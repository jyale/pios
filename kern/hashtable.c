/*
 * hash table for tifc label mappings
 */

#include <kern/mem.h>
#include <kern/hashtable.h>

#include <inc/string.h>

static inline uint8_t
hash (uint64_t key)
{
	return key;
	uint8_t ret = 0;
	int i;
	for (i = 0; i < 8; i++) {
		ret ^= (key & 0xff);
		key >>= 2;
	}
	return ret;
}

/*
 * return self, 0 for done
 */
static hashtable *
bucket_insert (hashtable *bucket, uint64_t key, uint64_t value, uint64_t *key_overflow, uint64_t *value_overflow)
{
//	cprintf("\t\t[bucket ins] key %llx value %llx size %x max %llx next %x\n", key, value, BUCKET_SIZE(bucket), BUCKET_MAXKEY(bucket), BUCKET_NEXT(bucket));
//	int k;
//	cprintf("\t\t[bucket ins]");
//	for (k = 1; k <= BUCKET_SIZE(bucket); k++)
//		cprintf("\t%llx", bucket->entries[k].key);
//	cprintf("\n");
	// process overflow
	if (BUCKET_SIZE(bucket) >= HASH_ENTRY_COUNT - 1) {
		if (BUCKET_MAXKEY(bucket) < key) {
			*key_overflow = key;
			*value_overflow = value;
			return bucket;
		}
		*key_overflow = BUCKET_MAXKEY(bucket);
		*value_overflow = BUCKET_MAXVALUE(bucket);
	}
	int lb = 1, ub = BUCKET_SIZE(bucket) + 1;
	// find tight upper bound
	while (lb < ub) {
		int m = (lb + ub) / 2;
		if (key <= bucket->entries[m].key) {
			ub = m;
		} else {
			lb = m + 1;
		}
	}
//	cprintf("\t\t[bucket ins] ub %x\n", ub);
	// assign value if equal
	if (bucket->entries[ub].key == key) {
		bucket->entries[ub].value = value;
//		cprintf("\t\t[bucket ins] update %x\n", ub);
		return NULL;
	}
	// insert value
	hashtable *ret = bucket;
	if (BUCKET_SIZE(bucket) < HASH_ENTRY_COUNT - 1) {
		BUCKET_SET_SIZE(bucket, BUCKET_SIZE(bucket) + 1);
		ret = NULL;
	}
	memcpy(&bucket->entries[ub + 1], &bucket->entries[ub], sizeof(hashentry) * (BUCKET_SIZE(bucket) - ub));
	bucket->entries[ub].key = key;
	bucket->entries[ub].value = value;
//	cprintf("\t\t[bucket ins] insert %x size %x\n", ub, BUCKET_SIZE(bucket));
	return ret;
}

/*
 * return self, 0 for found
 * BUCKET_SIZE(bucket) < HASH_ENTRY_COUNT - 1 implies not found
 * BUCKET_MAXKEY(bucket) >= key implies not found
 */
static hashtable *
bucket_find (hashtable *bucket, uint64_t key, uint64_t *value)
{
//	cprintf("\t\t[bucket find] key %llx count %x max %llx next %x\n", key, BUCKET_SIZE(bucket), BUCKET_MAXKEY(bucket), BUCKET_NEXT(bucket));
//	int k;
//	cprintf("\t\t[bucket find]");
//	for (k = 1; k <= BUCKET_SIZE(bucket); k++)
//		cprintf("\t%llx", bucket->entries[k].key);
//	cprintf("\n");
	if (BUCKET_MAXKEY(bucket) < key) {
		return bucket;
	}
	int lb = 1, ub = BUCKET_SIZE(bucket) + 1;
	// find tight upper bound
	while (lb < ub) {
		int m = (lb + ub) / 2;
		if (key <= bucket->entries[m].key) {
			ub = m;
		} else {
			lb = m + 1;
		}
	}
//	cprintf("\t\t[bucket find] ub %x key %llx\n", ub, bucket->entries[ub].key);
	if (bucket->entries[ub].key == key) {
		*value = bucket->entries[ub].value;
		return NULL;
	}
	return bucket;
}
#if 0
static void
bucket_traverse(proc *p, hashtable *bucket, void (*func)(proc *, uint64_t))
{
	int i;
	for (i = 1; i <= BUCKET_SIZE(bucket); i++) {
		(*func)(p, bucket->entries[i].value);
	}
}
#endif
/*
 * table pointer should already be mapped
 */
hashtable *
table_alloc ()
{
	assert(sizeof(hashtable) == PAGESIZE);
	pageinfo *pi = mem_alloc();
	if (pi == NULL)
		return NULL;
	mem_incref(pi);
	hashtable *table = mem_pi2ptr(pi);
	memset(table, 0, sizeof(hashtable));
	return table;
}

int
table_insert (hashtable *table, uint64_t key, uint64_t value)
{
	int hashed_key = hash(key);
//	cprintf("\t[table ins] table %p key %llx -> %x value %llx\n", table, key, hashed_key, value);
	hashentry *entry = &table->entries[hashed_key];
	while (1) {
		if (entry->key == 0) {
			// allocate new bucket
			pageinfo *pi = mem_alloc();
			if (pi == NULL)
				return -1;
			mem_incref(pi);
			memset(mem_pi2ptr(pi), 0, PAGESIZE);
			entry->value = (intptr_t)mem_pi2ptr(pi);
			entry->key = 1;
//			cprintf("\t[table ins] new bucket %p\n", entry->value);
		}
		hashtable *bucket = (hashtable *)entry->value;
//		cprintf("\t[table ins] bucket %p\n", bucket);
		bucket = bucket_insert(bucket, key, value, &key, &value);
//		cprintf("\t[table ins] next bucket %p key %llx value %llx\n", bucket, key, value);
		if (!bucket)
			break;
		entry = bucket->entries;
	}
	return 0;
}

int
table_find (hashtable *table, uint64_t key, uint64_t *value)
{
	int hashed_key = hash(key);
//	cprintf("\t[table find] table %p key %llx -> %x\n", table, key, hashed_key);
	hashentry *entry = &table->entries[hashed_key];
//	cprintf("\t[table find] entry %llx -> %llx\n", entry->key, entry->value);
	while (1) {
		if (entry->value == 0) {
			return -1;
		}
		hashtable *bucket = (hashtable *)entry->value;
		bucket = bucket_find(bucket, key, value);
//		cprintf("\t[table find] next bucket %p key %llx value %llx\n", bucket, key, *value);
		if (!bucket)
			break;
		if (BUCKET_SIZE(bucket) < HASH_ENTRY_COUNT - 1)
			return -1;
		if (BUCKET_MAXKEY(bucket) >= key)
			return -1;
		entry = bucket->entries;
	}
	return 0;
}
#if 0
void
table_traverse(proc *p, hashtable *table, void (*func)(proc *, uint64_t))
{
	int i;
	for (i = 0; i < HASH_ENTRY_COUNT; i++)
	{
		hashentry *entry = &table->entries[i];
		while (1) {
			if (entry->value == 0)
				break;
			hashtable *bucket = (hashtable *)entry->value;
			if (p) {
				// translate user address to kernel address
				bucket = msg_va2ka(p, entry->value);
			}
			bucket_traverse(p, bucket, func);
			entry = bucket->entries;
		}
	}
}
#endif
