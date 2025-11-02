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

#endif
