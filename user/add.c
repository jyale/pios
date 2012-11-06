#include <inc/stdio.h>
#include <inc/msg.h>
#include <inc/syscall.h>

#define DISP_MID 1

int
main (int argc, char **argv)
{
	cprintf("[add %c]\n", argv[0][0]);
	uint64_t msgid = argv[0][0];
	listen(msgid);
	sys_ret();
	int64_t opr[2];
	size_t len = recv(DISP_MID, opr, 2 * sizeof(int64_t));
	cprintf("[add %c] recv len %x\n", argv[0][0], len);
	cprintf("[add %c] %x + %x\n", argv[0][0], opr[0], opr[1]);
	opr[0] += opr[1];
	len = send(DISP_MID, opr, sizeof(int64_t));
	cprintf("[add %c] send len %x\n", argv[0][0], len);
	return 0;
}
