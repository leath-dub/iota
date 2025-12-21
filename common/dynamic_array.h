#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

void *da_create(size_t item_size);
void da_delete(void *da);
void da_append(void **da, void *item, size_t item_size);
void da_shrink(void **da, size_t item_size);
size_t da_length(void *da);

typedef struct {
    _Alignas(max_align_t) size_t length;
    size_t capacity;
} DaHeader;

static inline DaHeader *da_header_get(void *da) {
    return (DaHeader *)((uintptr_t)da - sizeof(DaHeader));
}

static inline void *da_at(void *da, size_t index, size_t item_size) {
    assert(index < da_header_get(da)->length && "index out of bounds");
    uint8_t *read_view = da;
    return &read_view[index * item_size];
}

static inline void *da_unsafe_at(void *da, size_t index, size_t item_size) {
    uint8_t *read_view = da;
    return &read_view[index * item_size];
}

#define DA_DEFINE(name, V)                                                     \
    static inline V *name##_create(void) { return da_create(sizeof(V)); }      \
    static inline void name##_delete(V *values) { da_delete((void *)values); } \
    static inline void name##_append(V **values, V item) {                     \
        da_append((void **)values, &item, sizeof(V));                          \
    }                                                                          \
    static inline V *name##_at(V *values, size_t index) {                      \
        return da_at((void **)values, index, sizeof(V));                       \
    }                                                                          \
    static inline V *name##_unsafe_at(V *values, size_t index) {               \
        return da_unsafe_at((void **)values, index, sizeof(V));                \
    }                                                                          \
    static inline void name##_shrink(V **values) {                             \
        da_shrink((void **)values, sizeof(V));                                 \
    }

#endif
