#include "dynamic_array.h"

#include <stdlib.h>
#include <string.h>

void *da_create(size_t item_size) {
    size_t capacity = 128;
    DaHeader *h = malloc(sizeof(DaHeader) + (capacity * item_size));
    h->length = 0;
    h->capacity = capacity;
    return (void *)((uintptr_t)h + sizeof(DaHeader));
}

void da_delete(void *da) { free(da_header_get(da)); }

void da_append(void **da, void *item, size_t item_size) {
    DaHeader *h = da_header_get(*da);
    if (h->length >= h->capacity) {
        void *alloc =
            realloc(h, sizeof(DaHeader) + (h->capacity * 2 * item_size));
        assert(alloc && "OOM");
        h = alloc;
        h->capacity *= 2;
        *da = (void *)((uintptr_t)h + sizeof(DaHeader));
    }
    uint8_t *write_view = *da;
    memcpy(&write_view[h->length * item_size], item, item_size);
    h->length++;
}

void da_pop(void *da) {
    DaHeader *h = da_header_get(da);
    h->length--;
}

void da_remove(void *da, size_t index, size_t item_size) {
    DaHeader *h = da_header_get(da);
    assert(index < h->length);

    size_t after = index + 1;
    size_t to_move = h->length - after;

    // Shift the bytes after index onto index itself
    uint8_t *v = da;
    memmove(&v[index * item_size], &v[after * item_size], to_move * item_size);

    h->length--;
}

size_t da_length(void *da) {
    if (da == NULL) {
        return 0;
    }
    return da_header_get(da)->length;
}

void da_shrink(void **da, size_t item_size) {
    DaHeader *h = da_header_get(*da);
    void *alloc = realloc(h, sizeof(DaHeader) + (h->length * item_size));
    assert(alloc && "OOM");
    h = alloc;
    h->capacity = h->length;
    *da = (void *)((uintptr_t)h + sizeof(DaHeader));
}
