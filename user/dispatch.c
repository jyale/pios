#include <inc/stdio.h>
#include <inc/msg.h>
#include <inc/unistd.h>
#include <inc/syscall.h>
#include <inc/vm.h>

#define DISP_MID 1

int
main (int argc, char **argv)
{
	listen(DISP_MID);
	char ks[2] = "0";
	int pids[10];
	int i;
	for (i = 0; i < 8; i++) {
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
	int pid = fork();
	for (i = 0; i < 8; i++) {
		cprintf("[disp] start send %x\n", i);
		int64_t opr[3] = {i + 1, i + 2, 0};
		size_t len = send('0' + i, opr, 2 * sizeof(int64_t));
		cprintf("[disp] send %x len %x\n", i, len);
	}
	for (i = 0; i < 8; i++) {
		int64_t res = 0;
		cprintf("[disp] start recv %x\n", i);
		size_t len = recv('0' + i, &res, sizeof(int64_t));
		cprintf("[disp] recv %x len %x\n", i, len);
		cprintf("[disp] result %x\n", res);
	}
	wait(NULL);
	return 0;
}
