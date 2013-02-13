#include <inc/syscall.h>
#include <inc/msg.h>
#include <inc/vm.h>
#include <inc/string.h>
#include <inc/stdio.h>
#include <inc/stdlib.h>
#include <inc/unistd.h>

#define BUF_SIZE 4000
#define BUF_STEP 4096
#define NODE_NUM 0x10
#define INBUF_BASE VM_SCRATCHLO
#define OUTBUF_BASE (VM_SCRATCHLO + BUF_STEP * NODE_NUM)
#define REMOTE_ID(node,id) ((id) | ((node) << 56))

#define RAND_LIMIT 0x10

static uint64_t masterid;
static uint64_t myid;

struct Word {
    char word[32];
    uint16_t num;
};

void map (void *inbuf, void *outbufs[])
{
    // divide evently
	cprintf("[%llx map]\n", myid);
	char *inchar = inbuf;
    cprintf("inchar=%s\n", inchar+2);
    uint16_t *num = inchar;
	int cnt = *num;
    cprintf("debug 1: cnt=%d\n", cnt);
	int i;
	uint64_t id;
    // outchar points to buffers for different nodes
	for (id = 1; id < masterid; id++) {
		struct Word *outchar = outbufs[id];
		outchar[0].num = 0;
	}
    cprintf("debug 2\n");
    // iterate the inchar 
    int start = 2;
    int end = 2;
    char tmpStr[32];
	for (i = 2; i <= cnt+1; i++) {
        end = i;
        if (inchar[end] == ' ') {
            strncpy(tmpStr, inchar+start, end-1 - start + 1);
            tmpStr[end-start]='\0';
            // currently no divide 
            // struct Word *outchar = outbufs[(tmpStr[0]-'!') * (masterid-1) / ('~'-'!'+1) + 1];
            struct Word *outchar = outbufs[tmpStr[0] % (masterid-1) + 1];
            outchar[0].num ++;
            strcpy(outchar[outchar[0].num].word, tmpStr);
            outchar[outchar[0].num].num = 1;

            // move to the start of the next word
            while (inchar[++end] == ' ') {
            }
            i = end-1;
            start = end;
        }
	}
}

void reduce (void *inbufs[], void *outbuf)
{
    cprintf("---------------------reduce input-------------------\n");
	cprintf("[%llx reduce]\n", myid);
	struct Word *outchar = outbuf;
    outchar[0].num = 0;
	uint64_t id;
	uint16_t i, j;
    char tmpStr[32];
    // iterate the inbufs 1, 2, ...
	for (id = 1; id < masterid; id ++) {
		cprintf("[%llx reduce] %llx\n", myid, id);
		struct Word *inchar = inbufs[id];
		for (i = 0; i < inchar[0].num; i++) {
			strcpy(tmpStr, inchar[i + 1].word);
            // copy the first word
            if (outchar[0].num==0) {
                outchar[0].num++;
                outchar[outchar[0].num].num = 1;
                strcpy(outchar[outchar[0].num].word, tmpStr);
                cprintf("debug 1: word=%s, num=%d\n", outchar[outchar[0].num].word, outchar[outchar[0].num].num);
                continue;
            }
            for (j = 0; j < outchar[0].num; j++) {
                if (strcmp(outchar[j+1].word, tmpStr) == 0) {
                    outchar[j+1].num += inchar[i+1].num;
                    cprintf("debug 2: word=%s, num=%d\n", outchar[j+1].word, outchar[j+1].num);
                    break;
                }
                if (j+1 == outchar[0].num) {
                    outchar[0].num++;
                    outchar[outchar[0].num].num = 1;
                    strcpy(outchar[outchar[0].num].word, tmpStr);
                    cprintf("debug 3: word=%s, num=%d\n", outchar[outchar[0].num].word, outchar[outchar[0].num].num);
                    break;
                }
            }
		}
	}
    cprintf("---------------------reduce result------------------\n");
    for (i = 1; i <= outchar[0].num; i++) {
            cprintf("debug: word=%s, num=%d\n", outchar[i].word, outchar[i].num);
    }
}

void master ()
{
    char *str = "Determinator is an experimental multiprocessor, distributed OS that creates an environment in which anything an application computes is exactly repeatable. It consists of a microkernel and a set of user-space runtime libraries and applications. The microkernel provides a minimal API and execution environment, supporting a hierarchy of shared-nothing address spaces that can execute in parallel, but enforcing the guarantee that these spaced memory multithreading. A subset of Determinator comprises PIO of the core components of JOS. To our knowledge PIOS is the first instructional OS to include and emphasize increasingly important parallel/multicore and distributed OS programming practices in an undergraduate-level OS course. It was used to teach C";
    int strLen = strlen(str);

    // split 
    uint16_t span[NODE_NUM];
	int i;
    for (i=0; i<strLen; i++)
        cprintf("%c", str[i]);
    cprintf("\n");
    span[0] = masterid-1;
    span[1] = 0;
    for (i = 2; i <= masterid-1; i++) {
        int pos = (i-1) * strLen / (masterid - 1);
        while (!(str[pos]==' ' && str[pos+1]!=' ')) 
            ++pos;
        span[i] = pos+1;
    }
    span[masterid] = strLen;
    for (i = 0; i <= masterid-1; i++) {
        cprintf("span[%d]=%d", i, span[i]);
    }
    cprintf("\n");
        
    // send
	uint64_t id;
	char *buf = (char *)INBUF_BASE;
	listen(myid);
	for (id = 1; id < masterid; id++) {
		cprintf("[master to %llx]", id);
        uint16_t *num = buf;
        // the first 2 byte to store the size
		*num = span[id+1]-span[id]+1;
        cprintf("buf[0] = %d\n", *num);
        // fill the buf
		for (i = 1; i < span[id+1]-span[id]+1; i++) {
			char tmp = str[span[id]+i-1];
			cprintf("%c", tmp);
			buf[i + 1] = tmp;
		}
		cprintf("\n");
        cprintf("size for %d=%d\n", id, *num);
		send(REMOTE_ID(id,id), buf, BUF_SIZE);
	}
    // recv
	for (id = 1; id < masterid; id++) {
		recv(REMOTE_ID(id,id), buf, BUF_SIZE);
		cprintf("[master from %llx]\n%s", id, buf);

        struct Word *outchar = buf;
        for (i = 1; i <= outchar[0].num; i++) {
                cprintf("%s %d\n", outchar[i].word, outchar[i].num);
        }
	}
}

void sender (uint64_t dstid);

int main (int argc, char *argv[])
{
	uint16_t pids[NODE_NUM];

	int i;
	uint64_t id;

	masterid = atoi(argv[1]) + 1;
	myid = atoi(argv[2]);
	cprintf("[%llx/%llx]\n", myid, masterid - 1);

	sys_get(SYS_PERM|SYS_RW, 0, NULL, NULL, (void *)INBUF_BASE, 2 * NODE_NUM * BUF_STEP);

    // assign roles for master and slaves
	if (myid == masterid) {
		master();
		return 0;
	}

    // allocate spaces for map
	void *mapin = (void *)INBUF_BASE;
	void *mapout[NODE_NUM];
	for (i = 0; i < NODE_NUM; i++) {
		mapout[i] = (void *)(OUTBUF_BASE + i * BUF_STEP);
	}

    // allocate spaces for reduce 
	void *reducein[NODE_NUM];
	void *reduceout = (void *)OUTBUF_BASE;
	for (i = 0; i < NODE_NUM; i++) {
		reducein[i] = (void *)(INBUF_BASE + i * BUF_STEP);
	}

    // WHAT is sender for?
	for (id = 1; id < masterid; id++) {
		if (id == myid) continue;
		int pid = fork();
		if (pid == 0) {
			// child, act as sender
			sender(id);
		}
		pids[id] = pid;
	}

	// parent, act as worker
	listen(myid);
	cprintf("[%llx] init'd\n", myid);
	int cnt = 0;
	while (1) {
		cnt++;
		// receive task
		recv(REMOTE_ID(masterid,masterid), mapin, BUF_SIZE);
		cprintf("[%llx:%x] receive task\n", myid, cnt);

		// map
		map(mapin, mapout);
		cprintf("[%llx:%x] map done\n", myid, cnt);

		// send out map results
		for (id = 1; id < masterid; id++) {
			if (id == myid) {
				memmove(reducein[id], mapout[id], BUF_SIZE);
			} else {
				// wakes up sender
				cprintf("[%llx:%x] wake sender %llx pid %x addr %p\n", myid, cnt, id, pids[id], mapout[id]);
				procstate ps;
				sys_put(SYS_COPY|SYS_START, pids[id], &ps, mapout[id], mapout[id], BUF_STEP);
			}
		}
		cprintf("[%llx:%x] map results send start\n", myid, cnt);

		// receive map results
		for (id = 1; id < masterid; id++) {
			if (id == myid) continue;
			recv(REMOTE_ID(id,myid), reducein[id], BUF_SIZE);
		}
		cprintf("[%llx:%x] map results recv'd\n", myid, cnt);

		// wait for all sender to complete
		for (id = 1; id < masterid; id++) {
			if (id == myid) continue;
			sys_get(SYS_COPY, pids[id], NULL, mapout[id], mapout[id], 0);
		}
		cprintf("[%llx:%x] map results sent\n", myid, cnt);

		// reduce
		reduce(reducein, reduceout);
		cprintf("[%llx:%x] reduce done\n", myid, cnt);

		// return results
		send(REMOTE_ID(masterid,masterid), reduceout, BUF_SIZE);
		cprintf("[%llx:%x] task return\n", myid, cnt);
	}

	return 0;
}

void sender (uint64_t dstid) {
	// initialize
	listen(dstid);
	void *outbuf = (void *)(OUTBUF_BASE + dstid * BUF_STEP);
	cprintf("[%llx sender to %llx] init addr %p\n", myid, dstid, outbuf);
	sys_ret();

	while (1) {
		// parent wakes me up
		cprintf("[%llx sender to %llx] wake up\n", myid, dstid);
		send(REMOTE_ID(dstid,dstid), outbuf, BUF_SIZE);
		cprintf("[%llx sender to %llx] sent\n", myid, dstid);
		// wake up parent
		sys_ret();
	}
}
