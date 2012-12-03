/*
 * user library for messages
 */

#include <inc/vm.h>
#include <inc/syscall.h>
#include <inc/string.h>
#include <inc/msg.h>

static int ready = 0;

#ifndef PAGESIZE
#define PAGESIZE 4096
#endif // !PAGESIZE

#define MSG_SEND_BASE VM_MSGLO
#define MSG_RECV_BASE (VM_MSGLO + (VM_MSGHI - VM_MSGLO) / 2)

void
listen(uint64_t mid)
{
	ready = 1;
	sys_mid_register(mid);
	sys_get(SYS_PERM | SYS_RW, 0, NULL, NULL, (void *)MSG_SEND_BASE, PAGESIZE);
	sys_get(SYS_PERM | SYS_RW, 0, NULL, NULL, (void *)MSG_RECV_BASE, PAGESIZE);
}

size_t
send(uint64_t dst, void *data, size_t size)
{
	if (!ready)
		return 0;

	msg_t *msg = (msg_t *)MSG_SEND_BASE;
	// allocate enough space
	size_t len = msg->hdr.len + sizeof(msg_hdr_t);
	len = ROUNDUP(len, PAGESIZE);
	size_t newlen = size + sizeof(msg_hdr_t);
	newlen = ROUNDUP(newlen, PAGESIZE);
	if (newlen > len) {
		sys_get(SYS_PERM | SYS_RW, 0, NULL, NULL, (void *)(MSG_SEND_BASE + len), newlen - len);
	}

	// copy
	memcpy(msg->data, data, size);
	msg->hdr.len = size;
	msg_t *msg_dst = (msg_t *)MSG_RECV_BASE;
	sys_send(dst, msg, msg_dst, newlen);
	return size;
}

size_t
recv(uint64_t src, void *data, size_t size)
{
	if (!ready)
		return 0;

	msg_t *msg = (msg_t *)MSG_RECV_BASE;
	// allocate enough space
	size_t len = msg->hdr.len + sizeof(msg_hdr_t);
	len = ROUNDUP(len, PAGESIZE);
	size_t newlen = size + sizeof(msg_hdr_t);
	newlen = ROUNDUP(newlen, PAGESIZE);
//	cprintf("[lib recv] %x needed %x allocated\n", newlen, len);
	if (newlen > len) {
		sys_get(SYS_PERM | SYS_RW, 0, NULL, NULL, (void *)(MSG_RECV_BASE + len), newlen - len);
	}

	// copy
	memset(msg, 0, msg->hdr.len + sizeof(msg_hdr_t));
//	cprintf("[lib recv] start recv %p\n", src);
	sys_recv(src);
//	cprintf("[lib recv] done recv %x\n", msg->hdr.len);
	if (size > msg->hdr.len) {
		size = msg->hdr.len;
	}
	memcpy(data, msg->data, size);
	return size;
}
