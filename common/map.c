#include "map.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef uint64_t Hash;

// This struct needs maximum alignment, so we can safely get the header pointer
// given the value array pointer, e.g. with:
//
// ```c
// T *v = ...;
// (uintptr_t)v - sizeof(MapHeader)
// ```
typedef struct MapHeader {
    _Alignas(max_align_t) size_t total_slots;
    size_t occupied_slots;
    size_t value_size;
    size_t key_size;
} MapHeader;

// The data section for the hash map is made up of 3 different arrays:
//
// * Value array: T*
// * Key array: K*
// * Hash array: H*
//
// The overall allocation layout looks like this:
//
// [ HEADER ][ VALUES ... ]< ALIGN? >[ KEYS ... ]< ALIGN? >[ HASHES ... ]

static inline uintptr_t align_forward(uintptr_t p, uintptr_t align) {
    return (p + (align - 1)) & ~(align - 1);
}

static MapHeader *map_get_header(void *values) {
    return (MapHeader *)((uintptr_t)values - sizeof(MapHeader));
}

static uint8_t *map_get_values(MapHeader *header) {
    return (uint8_t *)((uintptr_t)header + sizeof(MapHeader));
}

static uint8_t *map_get_keys(MapHeader *header) {
    uintptr_t unaligned = (uintptr_t)map_get_values(header) +
                          (header->value_size * header->total_slots);
    return (uint8_t *)align_forward(unaligned, sizeof(max_align_t));
}

static Hash *map_get_hashes(MapHeader *header) {
    uintptr_t unaligned = (uintptr_t)map_get_keys(header) +
                          (header->key_size * header->total_slots);
    return (Hash *)align_forward(unaligned, _Alignof(Hash));
}

void *map_create(size_t key_size, size_t value_size, size_t slots) {
    uint8_t *block =
        calloc(sizeof(MapHeader) + (value_size * slots) + sizeof(max_align_t) +
                   (key_size * slots) + _Alignof(Hash) + (sizeof(Hash) * slots),
               1);
    assert(block);
    MapHeader *header = (MapHeader *)block;
    header->total_slots = slots;
    header->occupied_slots = 0;
    header->value_size = value_size;
    header->key_size = key_size;
    return map_get_values(header);
}

void map_delete(void *values) { free(map_get_header(values)); }

#define FNV1A_64_OFFSET_BASIS (uint64_t)0xcbf29ce484222325
#define FNV1A_64_PRIME (uint64_t)0x100000001b3

static uint64_t fnv1a(const uint8_t *key_data, size_t key_size) {
    uint64_t hash = FNV1A_64_OFFSET_BASIS;
    for (uint32_t i = 0; i < key_size; i++) {
        hash ^= key_data[i];
        hash *= FNV1A_64_PRIME;
    }
    return hash;
}

static Hash map_hash(MapHeader *header, void *key) {
    Hash hash = fnv1a((uint8_t *)key, header->key_size);
    // 0 is used as a sentinel value to represent unoccupied slot
    return hash == 0 ? 1 : hash;
}

static bool map_slot_empty(MapHeader *header, uint32_t i) {
    return map_get_hashes(header)[i] == 0;
}

static bool map_slot_key_is(MapHeader *header, uint32_t i, void *key,
                            Hash hash) {
    if (map_get_hashes(header)[i] != hash) {
        return false;
    }
    // Only compare if hash is the same
    return memcmp(&map_get_keys(header)[i * header->key_size], (uint8_t *)key,
                  header->key_size) == 0;
}

static uint32_t map_find_slot(MapHeader *header, void *key, Hash hash) {
    uint32_t i = hash % header->total_slots;
    while (!map_slot_empty(header, i) &&
           !map_slot_key_is(header, i, key, hash)) {
        i = (i + 1) % header->total_slots;
    }
    return i;
}

void *map_get(void *values, void *key) {
    MapHeader *header = map_get_header(values);
    Hash hash = map_hash(header, key);
    uint32_t i = map_find_slot(header, key, hash);
    if (map_slot_empty(header, i)) {
        return NULL;
    }
    return &map_get_values(header)[i * header->value_size];
}

static void map_ensure_slot(void **values) {
    MapHeader *header = map_get_header(*values);

    // Load factor >= .5
    if (header->occupied_slots >= (header->total_slots / 2)) {
        uint8_t *new_values = map_create(header->key_size, header->value_size,
                                         header->total_slots * 2);

        uint8_t *old_keys = map_get_keys(header);
        uint8_t *old_values = map_get_values(header);

        MapHeader *new_header = map_get_header(new_values);
        Hash *old_hashes = map_get_hashes(header);
        Hash *new_hashes = map_get_hashes(new_header);
        uint8_t *new_keys = map_get_keys(new_header);

        for (uint32_t j = 0; j < header->total_slots; j++) {
            if (!map_slot_empty(header, j)) {
                uint32_t insert_at = map_find_slot(
                    new_header, &old_keys[j * header->key_size], old_hashes[j]);
                uint8_t *value =
                    &new_values[insert_at * new_header->value_size];
                memcpy(value, &old_values[j * header->value_size],
                       header->value_size);
                memcpy(&new_keys[insert_at * header->key_size],
                       &old_keys[j * header->key_size], header->key_size);
                new_hashes[insert_at] = old_hashes[j];
                new_header->occupied_slots++;
            }
        }

        assert(new_header->occupied_slots == header->occupied_slots);

        map_delete(old_values);
        *values = new_values;
    }
}

void *map_get_or_insert(void **values, void *key, bool *inserted) {
    map_ensure_slot(values);

    MapHeader *header = map_get_header(*values);

    Hash hash = map_hash(header, key);
    uint32_t i = map_find_slot(header, key, hash);
    if (inserted) {
        *inserted = false;
    }

    if (map_slot_empty(header, i)) {
        memcpy(&map_get_keys(header)[i * header->key_size], key,
               header->key_size);
        map_get_hashes(header)[i] = hash;
        header->occupied_slots++;
        if (inserted) {
            *inserted = true;
        }
    }

    return &map_get_values(header)[i * header->value_size];
}

void *map_key_of(void *values, void *value) {
    MapHeader *header = map_get_header(values);
    uint32_t index =
        ((uintptr_t)value - (uintptr_t)values) / header->value_size;
    return &map_get_keys(header)[index * header->key_size];
}

static Hash map_bytes_hash(bytes *key) {
    Hash hash = fnv1a(key->data, key->length);
    // 0 is used as a sentinel value to represent unoccupied slot
    return hash == 0 ? 1 : hash;
}

static bool map_bytes_slot_key_is(MapHeader *header, uint32_t i, bytes *key,
                                  Hash hash) {
    if (map_get_hashes(header)[i] != hash) {
        return false;
    }
    bytes *slot_key = (bytes *)&map_get_keys(header)[i * header->key_size];
    // Only compare if hash is the same
    if (slot_key->length != key->length) {
        return false;
    }
    return memcmp(slot_key->data, key->data, key->length) == 0;
}

static uint32_t map_bytes_find_slot(MapHeader *header, bytes *key, Hash hash) {
    uint32_t i = hash % header->total_slots;
    while (!map_slot_empty(header, i) &&
           !map_bytes_slot_key_is(header, i, key, hash)) {
        i = (i + 1) % header->total_slots;
    }
    return i;
}

void *map_bytes_get(void *values, bytes *key) {
    MapHeader *header = map_get_header(values);
    Hash hash = map_bytes_hash(key);
    uint32_t i = map_bytes_find_slot(header, key, hash);
    if (map_slot_empty(header, i)) {
        return NULL;
    }
    return &map_get_values(header)[i * header->value_size];
}

void *map_bytes_get_or_insert(void **values, bytes *key, bool *inserted) {
    map_ensure_slot(values);

    MapHeader *header = map_get_header(*values);

    Hash hash = map_bytes_hash(key);
    uint32_t i = map_bytes_find_slot(header, key, hash);
    if (inserted) {
        *inserted = false;
    }

    if (map_slot_empty(header, i)) {
        memcpy(&map_get_keys(header)[i * header->key_size], key,
               header->key_size);
        map_get_hashes(header)[i] = hash;
        header->occupied_slots++;
        if (inserted) {
            *inserted = true;
        }
    }

    return &map_get_values(header)[i * header->value_size];
}

MapCursor map_cursor_create(void *values) {
    return (MapCursor){.header = map_get_header(values), .prev_index = -1};
}

bool map_cursor_next(MapCursor *cursor) {
    if (cursor->prev_index >= (int32_t)cursor->header->total_slots) {
        return false;
    }

    cursor->prev_index++;
    for (; cursor->prev_index < (int32_t)cursor->header->total_slots;
         cursor->prev_index++) {
        if (!map_slot_empty(cursor->header, cursor->prev_index)) {
            cursor->current = &map_get_values(
                cursor
                    ->header)[cursor->prev_index * cursor->header->value_size];
            return true;
        }
    }

    return false;
}
