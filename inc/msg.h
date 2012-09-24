/*
 * user library for messages
 */

#ifndef PIOS_LIB_MSG_H
#define PIOS_LIB_MSG_H

#include <inc/types.h>

/*
 * message header
 */
typedef struct {
	uint64_t dst;
	uint64_t src;
	size_t len;
} msg_hdr_t;

typedef struct {
	msg_hdr_t hdr;
	unsigned char data[0];
} msg_t;

void listen(uint64_t mid);
size_t send(uint64_t dst, void *data, size_t size);
size_t recv(uint64_t src, void *data, size_t size);

#endif // !PIOS_LIB_MSG_H
