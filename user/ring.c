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
    ring(myid, np);
}

int main (int argc, char **argv)
{
	ARGBEGIN{
	}ARGEND

    runtest(atoi(argv[0]));
	return 0;
}
