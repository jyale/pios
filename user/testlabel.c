#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/unistd.h>
#include <inc/vm.h>
#include <inc/label.h>
#include <inc/args.h>

#define LBL 0xdad
#define LBL_CHILD 0xccc
#define TIC 20000000

void proctest()
{
	sys_get(SYS_PERM | SYS_RW, 0, NULL, NULL, VM_SCRATCHLO, 4096);
	int pid = fork();
	if (pid == 0) {
		char str[] = "abcde";
		memmove(VM_SCRATCHLO, str, 5);
		cprintf("MOD %s\n", str);
		sys_ret();
		tag_t t;
		t.cat = 1; t.level = LVL_INONLY; t.time = 1;
		sys_set_label(t);
		char str2[] = "ABCDE";
		memmove(VM_SCRATCHLO, str2, 5);
		cprintf("MOD2 %s\n", str2);
		sys_ret();
		cprintf("child run\n");
		sys_ret();
	} else {
		char str[] = "12345";
		cprintf("BEFORE %s\n", str);
		sys_get(SYS_COPY, pid, NULL, VM_SCRATCHLO, VM_SCRATCHLO, 4096);
		memmove(str, VM_SCRATCHLO, 5);
		cprintf("AFTER %s\n", str);
		sys_put(SYS_START, pid, NULL, VM_USERLO, VM_USERLO, 0);
		sys_get(SYS_COPY, pid, NULL, VM_SCRATCHLO, VM_SCRATCHLO, 4096);
		memmove(str, VM_SCRATCHLO, 5);
		cprintf("AFTER2 %s\n", str);
		cprintf("waiting\n");
		sys_put(SYS_START, pid, NULL, VM_USERLO, VM_USERLO, 0);
		sys_get(0, pid, NULL, VM_USERLO, VM_USERLO, 0);
		cprintf("parent run\n");
	}
}
#if 0
void basictest ()
{
	int flag = sys_mid_register(LBL);
	char str[] = "1234567890";
	char tmp[] = "----------";
	size_t len;
	int i, j;
	len = sys_msg_send(str, 5, LBL);
	cprintf("send len %llu (should send somthing)\n", len);
	len = sys_msg_recv(tmp, 10, LBL);
	cprintf("recv len %llu %s (should recv something)\n", len, tmp);
	cprintf("recv (should wait forever)\n");
	len = sys_msg_recv(tmp, 10, LBL);
	cprintf("recv len %llu %s (should recv nothing)\n", len, tmp);
}

void forktest ()
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

		cprintf("parent recv (should wait)\n");
		len = sys_msg_recv(tmp, 10, LBL_CHILD);
		cprintf("parent recv len %llu %s (should recv somthing)\n", len, tmp);

		len = sys_msg_send(str, 5, LBL_CHILD);
		cprintf("parent send len %llu (should send something)\n", len);

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

void nettest ()
{
	int flag = sys_mid_register(LBL);
	char str[] = "1234567890";
	char tmp[] = "----------";
	size_t len;
	uint16_t node = sys_msg_node();
	cprintf("node %u\n", node);
	if (node == 1) {
		len = sys_msg_send_remote(str, 5, LBL, 2);
		cprintf("send len %llu (should send somthing)\n", len);
	} else {
		len = sys_msg_recv_remote(tmp, 10, LBL, 1);
		cprintf("recv len %llu %s (should recv something)\n", len, tmp);
	}
}

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
	cprintf("\tb: basictest\n");
	cprintf("\tf: forktest\n");
	cprintf("\td: delaytest\n");
	cprintf("\tn: nettest\n");
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
#if 0
			case 'b':
				basictest();
				break;
			case 'f':
				forktest();
				break;
			case 'd':
				delaytest();
				break;
			case 'n':
				nettest();
				break;
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
