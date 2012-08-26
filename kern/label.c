/*
 * labeling system
 */

#include <inc/assert.h>
#include <inc/string.h>

#include <kern/label.h>

tag_t tag_default = {
	.cat = 0xfffffff,
	.time = 0,
	.level = LVL_DEFAULT,
};

static int tag_level_leq[LVL_STAR + 1][LVL_STAR + 1] = {
//	 0  1  2  *  right / left
	{1, 1, 1, 0},     // 0
	{0, 1, 1, 0},     // 1
	{0, 0, 1, 0},     // 2
	{1, 1, 1, 1},     // *
};

static int tag_level_leq_hi[LVL_STAR + 1][LVL_STAR + 1] = {
//	 0  1  2 (*) right / left
	{1, 1, 1, 1},     // 0
	{0, 1, 1, 1},     // 1
	{0, 0, 1, 1},     // 2
	{0, 0, 0, 1},     //(*)
};

static tag_t
tag_leq (tag_t left, tag_t right)
{
	tag_t ret = tag_default;
	ret.level = !tag_level_leq[left.level][right.level];
	if (left.time > right.time) {
		ret.time = left.time;
	}
	return ret;
}

static tag_t
tag_leq_hi (tag_t left, tag_t right)
{
	tag_t ret = tag_default;
	ret.level = !tag_level_leq_hi[left.level][right.level];
	if (left.time > right.time) {
		ret.time = left.time;
	}
	return ret;
}

static tag_t
label_cmp (label_t *left, label_t *right, tag_t (*cmp)(tag_t, tag_t))
{
	int li = 0, ri = 0;
	tag_t ret;
	ret.level = 0; ret.time = 0;
//	cprintf("[label cmp] lcnt %u rcnt %u\n", left->cnt, right->cnt);
	while (1) {
		tag_t r;
//		cprintf("[label cmp] li %d {%llx : %x | %x} ri %d {%llx : %x | %x}\n", li, left->tags[li].cat, left->tags[li].level, left->tags[li].time, ri, right->tags[ri].cat, right->tags[ri].level, right->tags[ri].time);
		if (li >= left->cnt && ri >= right->cnt) {
			r = (*cmp)(left->tags[li], right->tags[ri]);
//			cprintf("[label cmp] li %d ri %d r {%x | %x}\n", li, ri, r.level, r.time);
			if (r.level > ret.level)
				ret.level = r.level;
			if (r.time > ret.time)
				ret.time = r.time;
			break;
		} else if (li >= left->cnt) {
			r = (*cmp)(left->tags[li], right->tags[ri]);
			ri++;
		} else if (ri >= right->cnt) {
			r = (*cmp)(left->tags[li], right->tags[ri]);
			li++;
		} else {
			if (left->tags[li].cat == right->tags[ri].cat) {
				r = (*cmp)(left->tags[li], right->tags[ri]);
				li++;
				ri++;
			} else if (left->tags[li].cat < right->tags[ri].cat) {
				r = (*cmp)(left->tags[li], right->tags[right->cnt]);
				li++;
			} else {
				r = (*cmp)(left->tags[left->cnt], right->tags[ri]);
				ri++;
			}
		}
//		cprintf("[label cmp] li %d ri %d r {%x | %x}\n", li, ri, r.level, r.time);
		if (r.level > ret.level)
			ret.level = r.level;
		if (r.time > ret.time)
			ret.time = r.time;
	}
	return ret;
}

tag_t
label_leq (label_t *left, label_t *right)
{
	return label_cmp(left, right, &tag_leq);
}

tag_t
label_leq_hi (label_t *left, label_t *right)
{
	return label_cmp(left, right, &tag_leq_hi);
}

void
label_pace (label_t *label)
{
	int i;
	for (i = 0; i <= label->cnt; i++) {
		label->tags[i].time = 0;
	}
}

void
label_init (label_t *label, tag_t tag)
{
	label->cnt = 0;
	tag.cat = 0;
	label->tags[0] = tag;
}

int
label_promote (label_t *label, tag_t tag)
{
	// find tight upper bound
	int lb = 0, ub = label->cnt;
	while (ub > lb) {
		int m = (lb + ub) / 2;
		if (tag.cat > label->tags[m].cat) {
			lb = m + 1;
		} else {
			ub = m;
		}
	}
	if (tag.cat == label->tags[ub].cat) {
		label->tags[ub] = tag;
		return 0;
	}
	if (label->cnt >= TAG_LIMIT)
		return -1;
	memcpy(&label->tags[ub + 1], &label->tags[ub], sizeof(tag_t) * (label->cnt + 1 - ub));
	label->tags[ub] = tag;
	label->cnt++;
	return 0;
}

uint64_t
label_time (uint8_t time)
{
	if (time == 0) {
		return 0;
	} else {
		return 0x10000000ULL * (1 << (time - 1));
	}
}

void
label_print (label_t *label)
{
	int i;
	for (i = 0; i < label->cnt; i++) {
		cprintf("{%llx : %x | %x} ", label->tags[i].cat, label->tags[i].level, label->tags[i].time);
	}
	cprintf("{default : %x | %x}\n", label->tags[label->cnt].level, label->tags[label->cnt].time);
}

void
label_check ()
{
	label_t l1, l2;
	tag_t t;
	// test label promotion
	t = tag_default;
	label_init(&l1, t);
	t.cat = 1; t.time = 1;
	label_promote(&l1, t);
	t.cat = 5; t.time = 5;
	label_promote(&l1, t);
	t.cat = 3; t.time = 3;
	label_promote(&l1, t);
	t.cat = 4; t.time = 4;
	label_promote(&l1, t);
	t.cat = 3; t.time = 0x33; t.level = LVL_STAR;
	label_promote(&l1, t);
	t.cat = 2; t.time = 2;
	label_promote(&l1, t);
//	label_print(&l1);
	assert(l1.cnt == 5);
	assert(l1.tags[0].cat == 1 && l1.tags[0].time == 1);
	assert(l1.tags[1].cat == 2 && l1.tags[1].time == 2);
	assert(l1.tags[2].cat == 3 && l1.tags[2].time == 0x33 && l1.tags[2].level == LVL_STAR);
	assert(l1.tags[3].cat == 4 && l1.tags[3].time == 4);
	assert(l1.tags[4].cat == 5 && l1.tags[4].time == 5);

	// basic test, no individual tag
	// l1 to be {1|0}
	t = tag_default;
	label_init(&l1, t);
//	label_print(&l1);
	t = label_leq(&l1, &l1);
	assert(!t.level && t.time == 0);
	t = label_leq_hi(&l1, &l1);
	assert(!t.level && t.time == 0);

	// l2 to be lo {0|0}
	t = tag_default;
	t.level = LVL_OUTONLY;
	label_init(&l2, t);
//	label_print(&l2);
	t = label_leq(&l1, &l2);
	assert(t.level && t.time == 0);
	t = label_leq_hi(&l1, &l2);
	assert(t.level && t.time == 0);
	t = label_leq(&l2, &l1);
	assert(!t.level && t.time == 0);
	t = label_leq_hi(&l2, &l1);
	assert(!t.level && t.time == 0);

	// l2 to be hi {2|33}
	t = tag_default;
	t.level = LVL_INONLY; t.time = 0x33;
	label_init(&l2, t);
//	label_print(&l2);
	t = label_leq(&l1, &l2);
	assert(!t.level && t.time == 0);
	t = label_leq_hi(&l1, &l2);
	assert(!t.level && t.time == 0);
	t = label_leq(&l2, &l1);
	assert(t.level && t.time == 0x33);
	t = label_leq_hi(&l2, &l1);
	assert(t.level && t.time == 0x33);

	// l2 to be * {3|0}
	t = tag_default;
	t.level = LVL_STAR;
	label_init(&l2, t);
//	label_print(&l2);
	t = label_leq(&l1, &l2);
	assert(t.level && t.time == 0);
	t = label_leq_hi(&l1, &l2);
	assert(!t.level && t.time == 0);
	t = label_leq(&l2, &l1);
	assert(!t.level && t.time == 0);
	t = label_leq_hi(&l2, &l1);
	assert(t.level && t.time == 0);

	// l2 to be {1:0|0, 2:3|0, 1|0}
	t = tag_default;
	label_init(&l2, t);
	t.cat = 1; t.level = LVL_OUTONLY;
	label_promote(&l2, t);
	t.cat = 2; t.level = LVL_INONLY;
	label_promote(&l2, t);
//	label_print(&l2);
	t = label_leq(&l1, &l2);
	assert(t.level && t.time == 0);
	t = label_leq_hi(&l1, &l2);
	assert(t.level && t.time == 0);
	t = label_leq(&l2, &l1);
	assert(t.level && t.time == 0);
	t = label_leq_hi(&l2, &l1);
	assert(t.level && t.time == 0);

	// l2 to be {1:0|11, 2:3|22, 1|0}
	t = tag_default;
	label_init(&l2, t);
	t.cat = 1; t.level = LVL_OUTONLY; t.time = 0x11;
	label_promote(&l2, t);
	t.cat = 2; t.level = LVL_INONLY; t.time = 0x22;
	label_promote(&l2, t);
//	label_print(&l2);
	t = label_leq(&l1, &l2);
	assert(t.level && t.time == 0);
	t = label_leq_hi(&l1, &l2);
	assert(t.level && t.time == 0);
	t = label_leq(&l2, &l1);
	assert(t.level && t.time == 0x22);
	t = label_leq_hi(&l2, &l1);
	assert(t.level && t.time == 0x22);

	// l1 to be {1:0|0, 2:3|0, 1|0}
	t = tag_default;
	label_init(&l1, t);
	t.cat = 1; t.level = LVL_OUTONLY;
	label_promote(&l1, t);
	t.cat = 2; t.level = LVL_INONLY;
	label_promote(&l1, t);
//	label_print(&l1);
	t = label_leq(&l1, &l2);
	assert(!t.level && t.time == 0);
	t = label_leq_hi(&l1, &l2);
	assert(!t.level && t.time == 0);
	t = label_leq(&l2, &l1);
	assert(!t.level && t.time == 0x22);
	t = label_leq_hi(&l2, &l1);
	assert(!t.level && t.time == 0x22);

	// l1 to be {1:0|0, 2:3|22, 1|0}
	t = tag_default;
	label_init(&l1, t);
	t.cat = 1; t.level = LVL_OUTONLY;
	label_promote(&l1, t);
	t.cat = 2; t.level = LVL_INONLY, t.time = 0x22;
	label_promote(&l1, t);
//	label_print(&l1);
	t = label_leq(&l1, &l2);
	assert(!t.level && t.time == 0);
	t = label_leq_hi(&l1, &l2);
	assert(!t.level && t.time == 0);
	t = label_leq(&l2, &l1);
	assert(!t.level && t.time == 0x11);
	t = label_leq_hi(&l2, &l1);
	assert(!t.level && t.time == 0x11);
	label_pace(&l1);
	assert(l1.tags[0].time == 0 && l1.tags[1].time == 0 && l1.tags[2].time == 0);

}
