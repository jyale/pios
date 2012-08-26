/*
 * labeling system
 */

#ifndef PIOS_INC_LABEL_H
#define PIOS_INC_LABEL_H

#include <inc/types.h>
#include <inc/cdefs.h>

#define TAG_LIMIT 32

#define LVL_OUTONLY	0
#define LVL_DEFAULT	1
#define LVL_INONLY	2
#define LVL_STAR	3

typedef struct {
	uint64_t cat : 56; // can't be 0 (reserved) or 0xfffffff (default)
	uint8_t time : 6;
	uint8_t level : 2;
} tag_t;

extern tag_t tag_default;

typedef struct {
	size_t cnt;
	tag_t tags[TAG_LIMIT + 1];
} gcc_packed label_t;

void label_init(label_t *label, tag_t tag);
int label_promote(label_t *label, tag_t tag);

#endif // !PIOS_INC_LABEL_H

