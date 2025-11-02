#include "hm.h"

#include <assert.h>
#include <string.h>

#define FNV1A_64_OFFSET_BASIS (u64)0xcbf29ce484222325
#define FNV1A_64_PRIME (u64)0x100000001b3

static u64 fnv1a(string s) {
  u64 hash = FNV1A_64_OFFSET_BASIS;
  for (u32 i = 0; i < s.len; i++) {
    hash ^= s.data[i];
    hash *= FNV1A_64_PRIME;
  }
  return hash;
}

HashMap hm_unsafe_new(usize slots) {
  void *entries_data = calloc(slots, sizeof(Entry *));
  return (HashMap){
      .entries = (Entries){.items = entries_data, .len = slots},
      .arena = new_arena(),
      .value_count = 0,
  };
}

static Entry *hm_unsafe_alloc(HashMap *map, usize value_size) {
  map->value_count += 1;
  return arena_alloc(&map->arena, sizeof(Entry) + value_size, _Alignof(Entry));
}

Entry *hm_unsafe_ensure(HashMap *map, string key, usize value_size) {
  double load_factor = (double)map->value_count / (double)map->entries.len;
  if (load_factor >= .75) {
    HashMap new_map = hm_unsafe_new(map->entries.len * 2);
    for (u32 i = 0; i < map->entries.len; i++) {
      Entry *it = map->entries.items[i];
      while (it != NULL) {
        Entry *new_entry = hm_unsafe_ensure(&new_map, it->key, value_size);
        memcpy(&new_entry->data, &it->data, value_size);
        string *new_key_ref = (string *)&new_entry->key;
        *new_key_ref = it->key;
        it = it->next;
      }
    }
    hm_unsafe_free(map);
    *map = new_map;
  }

  assert(map->entries.len != 0);

  u64 hash = fnv1a(key) % map->entries.len;

  Entry **head = &map->entries.items[hash];

  if (*head == NULL) {
    Entry *new_entry = hm_unsafe_alloc(map, value_size);
    string *key_ref = (string *)&new_entry->key;
    *key_ref = key;
    new_entry->next = NULL;
    *head = new_entry;
    return new_entry;
  } else {
    Entry *it = *head;
    while (it->next != NULL) {
      if (streql(key, it->key)) {
        return it;
      }
      it = it->next;
    }
    if (streql(key, it->key)) {
      return it;
    }

    Entry *new_entry = hm_unsafe_alloc(map, value_size);
    // NOTE: We don't copy the key as this hash map is used for the symbol table
    // which most often references a string in the source code. We don't want
    // to make unnecessary copies if the source code is being kept around anyway
    // for error messages (if this changes we can change this here).
    string *key_ref = (string *)&new_entry->key;
    *key_ref = key;
    new_entry->next = NULL;
    it->next = new_entry;

    return new_entry;
  }
}

bool hm_unsafe_contains(HashMap *hm, string key) {
  u64 hash = fnv1a(key) % hm->entries.len;
  Entry *it = hm->entries.items[hash];
  while (it != NULL) {
    if (streql(key, it->key)) {
      return true;
    }
    it = it->next;
  }
  return false;
}

void hm_unsafe_free(HashMap *hm) {
  if (hm->entries.items != NULL) {
    free(hm->entries.items);
  }
  arena_free(&hm->arena);
}
