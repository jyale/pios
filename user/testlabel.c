#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/unistd.h>
#include <inc/string.h>
#include <inc/vm.h>
#include <inc/label.h>
#include <inc/args.h>
#include <inc/msg.h>

#define LBL 0xdad
#define LBL_CHILD 0xccc
#define TIC 20000000

void proctest()
{
	sys_get(SYS_PERM | SYS_RW, 0, NULL, NULL, (void *)VM_SCRATCHLO, 4096);
	int pid = fork();
	if (pid == 0) {
		char str[] = "abcde";
		char emp[] = "_____";
		char *tmp = (char *)VM_SCRATCHLO;
		
		// RET to parent
		memmove(tmp, str, 6);
		cprintf("RET\n\t%s\n", tmp);
		sys_ret();

		// read pushed string
		cprintf("pushed\n\t%s\n", tmp);
		// set lower label and clearance
		tag_t t;
		t.cat = 1; t.level = LVL_OUTONLY; t.time = 1;
		sys_set_label(t);
		sys_set_clearance(t);
		memmove(tmp, str, 6);
		cprintf("RET LO\n\t%s\n", tmp);
		sys_ret();

		// read pushed string
		cprintf("pushed\n\t%s\n", tmp);
		// set higher label and clearance
		t.cat = 1; t.level = LVL_INONLY; t.time = 1;
		sys_set_label(t);
		sys_set_clearance(t);
		memmove(tmp, str, 6);
		cprintf("RET HI\n\t%s\n", tmp);
		sys_ret();

		// read pushed string
		cprintf("pushed\n\t%s\n", tmp);
		cprintf("child done\n");
		
		// restore label and clearance
		t.cat = 1; t.level = LVL_DEFAULT; t.time = 0;
		sys_set_label(t);
		sys_set_clearance(t);
	} else {
		char str[] = "12345";
		char emp[] = "-----";
		char *tmp = (char *)VM_SCRATCHLO;

		// GET from child
		memmove(tmp, emp, 6);
		cprintf("before GET\n\t%s\n", tmp);
		sys_get(SYS_COPY, pid, NULL, tmp, tmp, 4096);
		cprintf("after GET\n\t%s\n", tmp);

		// PUT to child
		memmove(tmp, str, 6);
		cprintf("PUT\n\t%s\n", tmp);
		sys_put(SYS_COPY | SYS_START, pid, NULL, tmp, tmp, 4096);

		// GET from child (child has lower label)
		memmove(tmp, emp, 6);
		cprintf("before GET from LO\n\t%s\n", tmp);
		sys_get(SYS_COPY, pid, NULL, tmp, tmp, 4096);
		cprintf("after GET from LO\n\t%s\n", tmp);

		// PUT to child (child has lower label)
		memmove(tmp, str, 6);
		cprintf("PUT to LO\n\t%s\n", tmp);
		sys_put(SYS_COPY | SYS_START, pid, NULL, tmp, tmp, 4096);

		// GET from child (child has higher label)
		memmove(tmp, emp, 6);
		cprintf("before GET from HI\n\t%s\n", tmp);
		sys_get(SYS_COPY, pid, NULL, tmp, tmp, 4096);
		cprintf("after GET from HI\n\t%s\n", tmp);

		// PUT to child (child has higher label)
		memmove(tmp, str, 6);
		cprintf("PUT to HI\n\t%s\n", tmp);
		sys_put(SYS_COPY | SYS_START, pid, NULL, tmp, tmp, 4096);

		// let child finish first
		sys_get(0, pid, NULL, tmp, tmp, 0);
		cprintf("parent done\n");
	}
	wait(NULL);
}

// identical to proctest(), using send/recv instead of put/get
void rawtest()
{
	sys_get(SYS_PERM | SYS_RW, 0, NULL, NULL, (void *)VM_SCRATCHLO, 4096);
	int pid = fork();
	if (pid == 0) {
		char str[] = "abcde";
		char emp[] = "_____";
		char *tmp = (char *)VM_SCRATCHLO;
		sys_mid_register(LBL_CHILD);
		
		// RET to parent
		memmove(tmp, str, 6);
		cprintf("RET\n\t%s\n", tmp);
		sys_ret();

		// RECV
		cprintf("before RECV\n");
		sys_recv(LBL);
		cprintf("RECV\n\t%s\n", tmp);

		// set lower label and clearance
		tag_t t;
		t.cat = 1; t.level = LVL_OUTONLY; t.time = 1;
		sys_set_label(t);
		sys_set_clearance(t);
		memmove(tmp, str, 6);
		cprintf("RET LO\n\t%s\n", tmp);
		sys_ret();

		// RECV
		cprintf("before RECV\n");
		sys_recv(LBL);
		cprintf("RECV\n\t%s\n", tmp);

		// set higher label and clearance
		t.cat = 1; t.level = LVL_INONLY; t.time = 1;
		sys_set_label(t);
		sys_set_clearance(t);
		memmove(tmp, str, 6);
		cprintf("RET HI\n\t%s\n", tmp);
		sys_ret();

		// RECV
		cprintf("before RECV\n");
		sys_recv(LBL);
		cprintf("RECV\n\t%s\n", tmp);

		cprintf("child done\n");
		
		// restore label and clearance
		t.cat = 1; t.level = LVL_DEFAULT; t.time = 0;
		sys_set_label(t);
		sys_set_clearance(t);
	} else {
		char str[] = "12345";
		char emp[] = "-----";
		char *tmp = (char *)VM_SCRATCHLO;
		sys_mid_register(LBL);

		// GET from child
		memmove(tmp, emp, 6);
		cprintf("before GET\n\t%s\n", tmp);
		sys_get(SYS_COPY, pid, NULL, tmp, tmp, 4096);
		cprintf("after GET\n\t%s\n", tmp);

		// SEND to child
		sys_put(SYS_START, pid, NULL, tmp, tmp, 0);
		memmove(tmp, str, 6);
		cprintf("SEND\n\t%s\n", tmp);
		sys_send(LBL_CHILD, tmp, tmp, 4096);

		// GET from child (child has lower label)
		memmove(tmp, emp, 6);
		cprintf("before GET from LO\n\t%s\n", tmp);
		sys_get(SYS_COPY, pid, NULL, tmp, tmp, 4096);
		cprintf("after GET from LO\n\t%s\n", tmp);

		// SEND to child (child has lower label)
		sys_put(SYS_START, pid, NULL, tmp, tmp, 0);
		memmove(tmp, str, 6);
		cprintf("SEND to LO\n\t%s\n", tmp);
		sys_send(LBL_CHILD, tmp, tmp, 4096);

		// GET from child (child has higher label)
		memmove(tmp, emp, 6);
		cprintf("before GET from HI\n\t%s\n", tmp);
		sys_get(SYS_COPY, pid, NULL, tmp, tmp, 4096);
		cprintf("after GET from HI\n\t%s\n", tmp);

		// SEND to child (child has higher label)
		sys_put(SYS_START, pid, NULL, tmp, tmp, 0);
		memmove(tmp, str, 6);
		cprintf("SEND to HI\n\t%s\n", tmp);
		sys_send(LBL_CHILD, tmp, tmp, 4096);

		// let child finish first
		sys_get(0, pid, NULL, tmp, tmp, 0);
		cprintf("parent done\n");
	}
	wait(NULL);
}

void basictest ()
{
	sys_get(SYS_PERM | SYS_RW, 0, NULL, NULL, (void *)VM_SCRATCHLO, 4096);
	sys_mid_register(LBL);
	char *tmp = (char *)VM_SCRATCHLO;
	sys_send(LBL, tmp, tmp, 4096);
	sys_recv(LBL);
	sys_send(LBL_CHILD, tmp, tmp, 4096);
	sys_recv(LBL_CHILD);
}
#if 0
void forktest ()
{
	int pid = fork();
	if (pid == 0) {
		listen(LBL_CHILD);
		cprintf("child start\n");
		sys_ret();
		char str[] = "12345";
		char tmp[] = "__________";
		size_t len;

		len = send(LBL, str, 5);
		cprintf("child send len %llu (should send something)\n", len);

		len = recv(LBL, tmp, 10);
		cprintf("child recv len %llu %s (should recv somthing)\n", len, tmp);

		sys_ret();

		char tmp2[] = "__________";
		len = recv(LBL, tmp2, 10);
		cprintf("child recv len %llu %s (should recv somthing)\n", len, tmp2);

		len = send(LBL, str, 5);
		cprintf("child send len %llu (should send something)\n", len);
	} else {
		listen(LBL);
		cprintf("parent start\n");
		sys_get(SYS_COPY, pid, NULL, VM_USERLO, VM_USERLO, 0);
		sys_put(SYS_START, pid, NULL, VM_USERLO, VM_USERLO, 0);
		char str[] = "abcde";
		char tmp[] = "----------";
		size_t len;

		len = recv(LBL_CHILD, tmp, 10);
		cprintf("parent recv len %llu %s (should recv somthing)\n", len, tmp);

		len = send(LBL_CHILD, str, 5);
		cprintf("parent send len %llu (should send something)\n", len);

		sys_get(SYS_COPY, pid, NULL, VM_USERLO, VM_USERLO, 0);
		sys_put(SYS_START, pid, NULL, VM_USERLO, VM_USERLO, 0);

		char tmp2[] = "----------";
		len = send(LBL_CHILD, str, 5);
		cprintf("parent send len %llu (should send something)\n", len);

		len = recv(LBL_CHILD, tmp2, 10);
		cprintf("parent recv len %llu %s (should recv somthing)\n", len, tmp2);

		wait(NULL);
	}
}

void delaytest ()
{
	int pid = fork();
	if (pid == 0) {
		int flag = sys_mid_register(LBL_CHILD);
		char str[] = "12345";
		char tmp[] = "__________";
		size_t len;

		len = sys_msg_send(str, 5, LBL);
		cprintf("child send len %llu (should send something)\n", len);

		cprintf("child recv (should wait)\n");
		len = sys_msg_recv(tmp, 10, LBL);
		cprintf("child recv len %llu %s (should recv somthing)\n", len, tmp);
	} else {
		int flag = sys_mid_register(LBL);
		char str[] = "abcde";
		char tmp[] = "----------";
		size_t len;
		tag_t t;
		t.cat = 1; t.level = LVL_DEFAULT; t.time = 1;
		flag = sys_set_label(t);

		cprintf("parent recv (should wait)\n");
		len = sys_msg_recv(tmp, 10, LBL_CHILD);
		cprintf("parent recv len %llu %s (should recv somthing)\n", len, tmp);

		len = sys_msg_send(str, 5, LBL_CHILD);
		cprintf("parent send len %llu (should send something)\n", len);

		wait(NULL);
	}
}
#endif
void nettest (char cmd)
{
	sys_get(SYS_PERM | SYS_RW, 0, NULL, NULL, (void *)VM_SCRATCHLO, 4096);
	int flag = sys_mid_register(LBL);
	char *tmp = (void *)VM_SCRATCHLO;
	size_t len;
	if (cmd == 's') {
		memmove(tmp, "123456789", 10);
		cprintf("before send\n");
		sys_send(2ULL << 56 | LBL, tmp, tmp, 4096);
		cprintf("send len (should send somthing)\n");
	} else {
		memmove(tmp, "---------", 4096);
		cprintf("before recv\n");
		sys_recv(1ULL << 56 | LBL);
		cprintf("recv len %s (should recv something)\n", tmp);
	}
}
#if 0
void interactivetest ()
{
	int flag = sys_mid_register(LBL);
	char str[] = "1234567890";
	char tmp[] = "----------";
	size_t len;
	uint16_t node = sys_msg_node();
	uint16_t dstnode = 3 - node;
	cprintf("node %u\n", node);
	tag_t tag_default = {
		.cat = 1,
		.time = 0,
		.level = LVL_DEFAULT,
	};

	while (1) {
		char *buf;
		buf = readline("> ");
		cprintf("\n");
		if (strncmp(buf, "print ", 6) == 0) {
			// print label or clearance
			buf += 6;
			if (strncmp(buf, "label", 5) == 0) {
				sys_print_label();
			} else if (strncmp(buf, "clearance", 9) == 0) {
				sys_print_clearance();
			} else {
				cprintf("print: label clearance\n");
			}
		} else if (strncmp(buf, "set ", 4) == 0) {
			// set label or clearance
			buf += 4;
			if (strncmp(buf, "label ", 6) == 0) {
				buf += 6;
				if (strncmp(buf, "lo", 2) == 0) {
					tag_t tag = tag_default;
					tag.level = 0;
					sys_set_label(tag);
				} else if (strncmp(buf, "default", 7) == 0) {
					tag_t tag = tag_default;
					tag.level = 1;
					sys_set_label(tag);
				} else if (strncmp(buf, "hi", 2) == 0) {
					tag_t tag = tag_default;
					tag.level = 2;
					sys_set_label(tag);
				} else {
					cprintf("set label: lo hi default\n");
				}
			} else if (strncmp(buf, "clearance ", 10) == 0) {
				buf += 10;
				if (strncmp(buf, "lo", 2) == 0) {
					tag_t tag = tag_default;
					tag.level = 0;
					sys_set_clearance(tag);
				} else if (strncmp(buf, "default", 7) == 0) {
					tag_t tag = tag_default;
					tag.level = 1;
					sys_set_clearance(tag);
				} else if (strncmp(buf, "hi", 2) == 0) {
					tag_t tag = tag_default;
					tag.level = 2;
					sys_set_clearance(tag);
				} else {
					cprintf("set clearance: lo hi default\n");
				}
			} else {
				cprintf("set: label clearance\n");
			}
		} else if (strncmp(buf, "send ", 5) == 0) {
			// send message
			buf += 5;
			len = sys_msg_send_remote(buf, strlen(buf), LBL, dstnode);
		} else if (strncmp(buf, "recv", 4) == 0) {
			// recv message
			char str[1024];
			len = sys_msg_recv_remote(str, 1024, LBL, dstnode);
			str[len] = '\0';
			cprintf("%s\n", str);
		} else if (strncmp(buf, "bye", 3) == 0) {
			return;
		} else {
			// usage
			cprintf("command: print set send recv\n");
		}
	}
}
#endif

void usage ()
{
	cprintf("usage: testlabel [pbfdni]\n");
	cprintf("\tp: proctest\n");
	cprintf("\tr: rawtest\n");
	cprintf("\tb: basictest\n");
	cprintf("\tf: forktest\n");
	cprintf("\td: delaytest\n");
	cprintf("\tns: nettest - send (only for 1)\n");
	cprintf("\tnr: nettest - recv (only for 2)\n");
	cprintf("\ti: interactivetest\n");
}

int main (int argc, char **argv)
{
	ARGBEGIN{
	}ARGEND
	if (argc != 1) {
		usage();
	} else {
		switch (argv[0][0]) {
			case 'p':
				proctest();
				break;
			case 'r':
				rawtest();
				break;
			case 'b':
				basictest();
				break;
#if 0
			case 'f':
				forktest();
				break;
			case 'd':
				delaytest();
				break;
#endif
			case 'n':
				nettest(argv[0][1]);
				break;
#if 0
			case 'i':
				interactivetest();
				break;
#endif
			default:
				usage();
		}
	}
	return 0;
}
