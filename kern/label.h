/*
 * labeling system
 */

#ifndef PIOS_KERN_LABEL_H
#define PIOS_KERN_LABEL_H

#include <inc/label.h>

/*
 * return tag_t
 * level / time == 0 means relation is good
 */
tag_t label_leq(label_t *left, label_t *right);
tag_t label_leq_hi(label_t *left, label_t *right);

uint64_t label_time(uint8_t time);
void label_pace(label_t *label);

void label_print(label_t *label);

void label_check();

#endif // !PIOS_KERN_LABEL_H

