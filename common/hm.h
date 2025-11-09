#ifndef HM_H
#define HM_H

#include "common.h"

typedef struct Entry {
    struct Entry *next;
    const string key;
    u8 data[];
} Entry;

typedef struct {
    Entry **items;
    u32 len;
} Entries;

typedef struct {
    Entries entries;
    Arena arena;  // For entries in the buckets
    u32 value_count;
} HashMap;

// Try not to use the following directly, they are not type safe
HashMap hm_unsafe_new(usize slots);
Entry *hm_unsafe_ensure(HashMap *hm, string key, usize value_size);
bool hm_unsafe_contains(HashMap *hm, string key);
void hm_unsafe_free(HashMap *hm);

typedef struct {
    u32 entry_index;
    Entry *current_entry;
    const HashMap *map;
    bool finished;
} HashMapCursor;

HashMapCursor hm_cursor_unsafe_new(const HashMap *map);
Entry *hm_cursor_unsafe_next(HashMapCursor *cursor);

#endif
