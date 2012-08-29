/*
 * hash table for tifc label mappings
 *
 * structure
 * hash function returns 1byte = 8bit -> 256 buckets
 * each entry is 64bit+64bit = 16B
 *
 * entry (16B):
 *     0             8               16
 *     +-------------+---------------+
 *     | key (64bit) | value (64bit) |
 *     +-------------+---------------+
 *
 * bucket (1 page):
 *     0       8           16          4096
 *     +-------+-----------+-----N-----+
 *     | count | succ addr |  entries  |
 *     | 64bit |   64bit   | 16B x 255 |
 *     +-------+-----------+-----N-----+
 *
 * table (1 page):
 *     simply 256 entries (8bit for hashed key)
 *
 */

#ifndef PIOS_KERN_HASHTABLE_H
#define PIOS_KERN_HASHTABLE_H

#define HASH_BIT 8

typedef struct hashentry {
	uint64_t key;
	uint64_t value;
} hashentry;

#define HASH_ENTRY_SIZE 16
#define HASH_ENTRY_COUNT (1 << HASH_BIT)

typedef struct hashtable {
	hashentry entries[HASH_ENTRY_COUNT];
} hashtable;

#define BUCKET_SIZE(bucket) ((bucket)->entries[0].key)
#define BUCKET_NEXT(bucket) ((hashtable *)((bucket)->entries[0].value))
#define BUCKET_SET_SIZE(bucket, size) do { (bucket)->entries[0].key = (size); } while (0)
#define BUCKET_SET_NEXT(bucket, next) do { (bucket)->entries[0].value = (next); } while (0)

#define BUCKET_MAXKEY(bucket) ((bucket)->entries[BUCKET_SIZE(bucket)].key)
#define BUCKET_MAXVALUE(bucket) ((bucket)->entries[BUCKET_SIZE(bucket)].value)

hashtable *table_init ();
int table_insert (hashtable *table, uint64_t key, uint64_t value);
int table_find (hashtable *table, uint64_t key, uint64_t *value);
//void table_traverse(hashtable *table, void (*func)(proc *, uint64_t));

#endif // !PIOS_KERN_HASHTABLE_H
