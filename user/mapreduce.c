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

void map (void *inbuf, void *outbufs[])
{
	cprintf("[%llx map]\n", myid);
	uint16_t *inchar = inbuf;
	uint16_t cnt = *inchar;
	uint16_t i;
	uint64_t id;
	for (id = 1; id < masterid; id++) {
		uint16_t *outchar = outbufs[id];
		*outchar = 0;
	}
	for (i = 0; i < cnt; i++) {
		uint16_t tmp = inchar[i + 1];
		uint16_t *outchar = outbufs[tmp * (masterid - 1) / RAND_LIMIT + 1];
		outchar[0]++;
		outchar[outchar[0]] = tmp;
	}
}

void reduce (void *inbufs[], void *outbuf)
{
	cprintf("[%llx reduce]\n", myid);
	uint16_t cnts[RAND_LIMIT];
	uint64_t id;
	uint16_t i;
	for (i = 0; i < RAND_LIMIT; i++) cnts[i] = 0;
	for (id = 1; id < masterid; id ++) {
		cprintf("[%llx reduce] %llx\n", myid, id);
		uint16_t *inchar = inbufs[id];
		for (i = 0; i < inchar[0]; i++) {
			uint16_t tmp = inchar[i + 1];
			cnts[tmp]++;
		}
	}
	size_t len = 0;
	char *outchar = outbuf;
	for (i = 0; i < RAND_LIMIT; i++) {
		cprintf("[%llx reduce] output %x\n", myid, i);
		if (cnts[i] == 0) continue;
		if (i < 0xa) {
			outchar[len] = '0' + i;
		} else {
			outchar[len] = 'A' + i - 0xa;
		}
		len++;
		outchar[len] = ' ';
		len++;
		uint16_t cnt;
		for (cnt = 1; cnt <= cnts[i]; cnt++) {
			outchar[len] = '.';
			if (cnt % 5 == 0) outchar[len] = ':';
			if (cnt % 10 == 0) outchar[len] = '|';
			len++;
		}
		outchar[len] = '\n';
		len++;
	}
	outchar[len] = '\0';
}

static inline void srand (uint32_t x);
static inline uint16_t rand ();

void master ()
{
	int i;
	uint64_t id;
	uint16_t *buf = (uint16_t *)INBUF_BASE;
	listen(myid);
	srand(sys_time());
	for (id = 1; id < masterid; id++) {
		cprintf("[master to %llx]", id);
		buf[0] = 30;
		for (i = 0; i < buf[0]; i++) {
			uint16_t tmp = rand() % RAND_LIMIT;
			cprintf(" %x", tmp);
			buf[i + 1] = tmp;
		}
		cprintf("\n");
		send(REMOTE_ID(id,id), buf, BUF_SIZE);
	}
	for (id = 1; id < masterid; id++) {
		recv(REMOTE_ID(id,id), buf, BUF_SIZE);
		cprintf("[master from %llx]\n%s", id, buf);
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

	if (myid == masterid) {
		master();
		return 0;
	}

	void *mapin = (void *)INBUF_BASE;
	void *mapout[NODE_NUM];
	for (i = 0; i < NODE_NUM; i++) {
		mapout[i] = (void *)(OUTBUF_BASE + i * BUF_STEP);
	}

	void *reducein[NODE_NUM];
	void *reduceout = (void *)OUTBUF_BASE;
	for (i = 0; i < NODE_NUM; i++) {
		reducein[i] = (void *)(INBUF_BASE + i * BUF_STEP);
	}

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

static uint32_t seed = 0;

static inline void srand (uint32_t x)
{
	seed = x;
}

static inline uint16_t rand ()
{
	seed *= 1103515245;
	seed += 12345;
	return (uint16_t)(seed >> 15);
}
