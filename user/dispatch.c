#include <inc/stdio.h>
#include <inc/msg.h>
#include <inc/unistd.h>
#include <inc/syscall.h>
#include <inc/vm.h>

#define DISP_MID 1

void
dispatch (int cnt, int64_t *opr, int64_t *res)
{
	char ks[2] = "0";
	int pids[100];
	int i;
	for (i = 0; i < cnt; i++) {
		pids[i] = fork();
		if (pids[i] == 0) {
			// child
			cprintf("[disp] fork child %x\n", i);
			ks[0] = '0' + i;
			char *ss[2] = {ks, NULL};
			int err = execv("add", ss);
			if (err < 0) {
				cprintf("[disp] fail\n");
			}
		} else {
			// parent, start child
			cprintf("[disp] fork parent %x\n", i);
			sys_get(SYS_COPY, pids[i], NULL, VM_USERLO, VM_USERLO, 0);
			sys_put(SYS_START, pids[i], NULL, VM_USERLO, VM_USERLO, 0);
		}
	}
	for (i = 0; i < cnt; i++) {
		cprintf("[disp] start send %x\n", i);
		size_t len = send('0' + i, opr + 2 * i, 2 * sizeof(int64_t));
		cprintf("[disp] send %x len %x\n", i, len);
	}
	for (i = 0; i < cnt; i++) {
		cprintf("[disp] start recv %x\n", i);
		size_t len = recv('0' + i, res + i, sizeof(int64_t));
		cprintf("[disp] recv %x len %x\n", i, len);
		cprintf("[disp] result %x\n", res[i]);
	}
	wait(NULL);
}

int
main (int argc, char **argv)
{
	listen(DISP_MID);
	int64_t opr[100];
	int64_t res[50];
	int cnt = 5;

	if (argc == 2) {
		// net_node == 1
		// act as gateway
		cnt = atoi(argv[1]);
		cprintf("[disp main] cnt %x (%s)\n", cnt, argv[1]);
		int i;
		for (i = 0; i < cnt * 2; i++) {
			opr[i] = i + 1;
		}

		send(DISP_MID | (2ULL << 56), opr, cnt * 2 * sizeof(int64_t));
		recv(DISP_MID | (2ULL << 56), res, cnt * sizeof(int64_t));

		for (i = 0; i < cnt; i++) {
			cprintf("[disp main] %x + %x = %x\n", opr[2 * i], opr[2 * i + 1], res[i]);
		}
	} else {
		// net_node == 2
		// act as dispatcher
		// give some time constraints
		tag_t t;
		t.cat = 1; t.level = LVL_DEFAULT; t.time = 5;
		sys_set_label(t);
		sys_set_clearance(t);
		while (1) {
			cnt = 50;
			size_t len = recv(DISP_MID | (1ULL << 56), opr, cnt * 2 * sizeof(int64_t));
			cnt = len / 2 / sizeof(int64_t);
			cprintf("[disp main] recv %x adds\n", cnt);

			dispatch(cnt, opr, res);

			len = send(DISP_MID | (1ULL << 56), res, cnt * sizeof(int64_t));
		}
	}

	return 0;
}
