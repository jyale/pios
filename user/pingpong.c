#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/unistd.h>
#include <inc/vm.h>
#include <inc/label.h>
#include <inc/args.h>
#include <inc/time.h>

#include <inc/msg.h>

#define LBL 0xdad
#define LBL_CHILD 0xccc
#define TIC 20000000

#define MAXBLOCKSIZE 4194304*12

double nowTime(void) 
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0)
        perror("time: gettimeofday");
	double now = (double)tv.tv_sec
			+ (double)tv.tv_usec / 1000000.0;
    return now;
}

int pingPongBenchmark(int myid, int np)
{
    if (myid == 1) {
        int myproc = myid, size, other_proc, nprocs = np, i, last;
        double t0, t1, time;
        double max_rate = 0.0, min_latency = 10e6;

        double *a = VM_SCRATCHLO;
        sys_get(SYS_PERM | SYS_RW, 0, NULL, NULL, (void *)VM_SCRATCHLO, MAXBLOCKSIZE);

        for (i = 0; i < MAXBLOCKSIZE; i++) {
            a[i] = (double) i;
        }
        other_proc = 2;

        cprintf("Hello from %d of %d\n", myproc, nprocs);

        // Timer accuracy test 
        t0 = nowTime();
        t1 = nowTime();
        while (t1 == t0) t1 = nowTime();
        if (myproc == 1)
                cprintf("Timer accuracy of ~%f usecs\n\n", (t1 - t0) * 1000000);

        for (size = 8; size <= MAXBLOCKSIZE; size *= 2) {
            for (i = 0; i < size / 8; i++) {
            a[i] = (double) i;
        }
        last = size / 8 - 1;

        t0 = nowTime();

        send(other_proc, a, size);
        recv(other_proc, a, size);

        t1 = nowTime();
        time = 1.e6 * (t1 - t0);

        if ((a[0] != 1.0 || a[last] != last + 1)) {
            cprintf("ERROR - a[0] = %f a[%d] = %f\n", a[0], last, a[last]);
            exit (1);
        }
        for (i = 1; i < last - 1; i++)
            if (a[i] != (double) i)
            cprintf("ERROR - a[%d] = %f\n", i, a[i]);
        if (myproc == 1 && time > 0.000001) {
            cprintf(" %7d bytes took %9.0f usec (%8.3f MB/sec)\n", size, time, 2.0 * size / time);
            if (2 * size / time > max_rate) max_rate = 2 * size / time;
            if (time / 2 < min_latency) 
                min_latency = time / 2;
            } else if (myproc == 1) {
                cprintf(" %7d bytes took less than the timer accuracy\n", size);
            }
        }

        if (myproc == 1)
        cprintf("\n Max rate = %f MB/sec  Min latency = %f usec\n",
        max_rate, min_latency);
    } else {
        int myproc = myid, size, other_proc, nprocs = np, i, last;
        double t0, t1, time;
        double max_rate = 0.0, min_latency = 10e6;

        double *b = VM_SCRATCHLO;
        sys_get(SYS_PERM | SYS_RW, 0, NULL, NULL, (void *)VM_SCRATCHLO, MAXBLOCKSIZE);

        for (i = 0; i < MAXBLOCKSIZE; i++) {
            b[i] = 0.0;
        }

        other_proc = 1;

        cprintf("Hello from %d of %d\n", myproc, nprocs);

        // Timer accuracy test 
        t0 = nowTime();
        t1 = nowTime();

        while (t1 == t0) t1 = nowTime();

        for (size = 8; size <= MAXBLOCKSIZE; size *= 2) {
            for (i = 0; i < size / 8; i++) {
                b[i] = 0.0;
            }
            last = size / 8 - 1;

            t0 = nowTime();

            recv(other_proc, b, size);
            b[0] += 1.0;
            if (last != 0)
            b[last] += 1.0;
            send(other_proc, b, size);

            t1 = nowTime();
            time = 1.e6 * (t1 - t0);

            if ((b[0] != 1.0 || b[last] != last + 1)) {
                cprintf("ERROR - b[0] = %f b[%d] = %f\n", b[0], last, b[last]);
                exit (1);
            }
            for (i = 1; i < last - 1; i++)
                if (b[i] != (double) i)
                    cprintf("ERROR - b[%d] = %f\n", i, b[i]);
            if (myproc == 1 && time > 0.000001) {
                cprintf(" %7d bytes took %9.0f usec (%8.3f MB/sec)\n", size, time, 2.0 * size / time);
                if (2 * size / time > max_rate) max_rate = 2 * size / time;
                if (time / 2 < min_latency) min_latency = time / 2;
            } else if (myproc == 1) {
                cprintf(" %7d bytes took less than the timer accuracy\n", size);
            }
        }
    }
}



int ring(int myid, int np) {
    int token;
    if (myid != 1) {
        recv(myid - 1, &token, sizeof(int));
        if (token != -1) exit(-1);
        cprintf("Process %d received token %d from Process %d\n", myid, token, myid - 1);
    } else {
        token = -1;
    }
    send(myid%np+1, &token, sizeof(int));
    if (myid == 1) {
        recv(np, &token, sizeof(int));
        if (token != -1) exit(-1);
        cprintf("Process %d received token %d from Process %d\n", myid, token, np);
    }
}

int runtest(int np)
{
    int i;
    listen(1);
    if (np==1) {
        test(1, np);
        return 0;
    }

    for (i = 2; i <= np; i++) {
        int pid = fork();
        if (pid == 1) {
            sys_get(SYS_COPY, pid, NULL, VM_USERLO, VM_USERLO, 0);
            sys_put(SYS_START, pid, NULL, VM_USERLO, VM_USERLO, 0);
            if (i!=2) sys_ret();
            test(i-1, np);
            wait(NULL);
            return 0;
        } else if (pid == 0) {
            listen(i);
            if (i == np) {
                sys_ret();
                test(i, np);
                return 0;
            }
        }
	}
}

int test(int myid, int np) 
{
    sys_get(SYS_PERM | SYS_RW, 0, NULL, NULL, (void *)VM_SCRATCHLO, 419430400);

    pingPongBenchmark(myid, np);
}

int main (int argc, char **argv)
{
	ARGBEGIN{
	}ARGEND

    runtest(2);
	return 0;
}
