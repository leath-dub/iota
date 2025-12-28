#ifndef MAP_H
#define MAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool (*cmp)(void *, void *);
    uint64_t (*hash)(void *);
} MapConfig;

typedef struct {
    const uint8_t *data;
    uint32_t length;
} bytes;

void *map_create(size_t key_size, size_t value_size, size_t slots,
                 MapConfig config);
void map_delete(void *values);
void *map_get(void *values, void *key);
void *map_get_or_insert(void **values, void *key, bool *inserted);
void *map_key_of(void *values, void *value);

struct MapHeader;

typedef struct {
    struct MapHeader *header;
    int32_t prev_index;
    void *current;
} MapCursor;

MapCursor map_cursor_create(void *values);
bool map_cursor_next(MapCursor *cursor);

void *map_bytes_get(void *values, bytes *key);
void *map_bytes_get_or_insert(void **values, bytes *key, bool *inserted);

#define MAP_DEFINE(name, K, V)                                                 \
    static inline V *name##_create(size_t slots) {                             \
        return map_create(sizeof(K), sizeof(V), slots,                         \
                          (MapConfig){                                         \
                              .cmp = NULL,                                     \
                              .hash = NULL,                                    \
                          });                                                  \
    }                                                                          \
    static inline V *name##_create_with_cfg(size_t slots, MapConfig cfg) {     \
        return map_create(sizeof(K), sizeof(V), slots, cfg);                   \
    }                                                                          \
    static inline void name##_delete(V *values) { map_delete(values); }        \
    static inline V *name##_get(V *values, K key) {                            \
        return map_get(values, (void *)&key);                                  \
    }                                                                          \
    static inline V *name##_get_or_insert(V **values, K key, bool *inserted) { \
        return map_get_or_insert((void **)values, (void *)&key, inserted);     \
    }                                                                          \
    static inline void name##_put(V **values, K key, V value) {                \
        *name##_get_or_insert(values, key, NULL) = value;                      \
    }

#define MAP_BYTES_DEFINE(name, V)                                        \
    static inline V *name##_create(size_t slots) {                       \
        return map_create(sizeof(bytes), sizeof(V), slots,               \
                          (MapConfig){                                   \
                              .cmp = NULL,                               \
                              .hash = NULL,                              \
                          });                                            \
    }                                                                    \
    static inline void name##_delete(V *values) { map_delete(values); }  \
    static inline V *name##_get(V *values, bytes key) {                  \
        return map_bytes_get(values, &key);                              \
    }                                                                    \
    static inline V *name##_get_or_insert(V **values, bytes key,         \
                                          bool *inserted) {              \
        return map_bytes_get_or_insert((void **)values, &key, inserted); \
    }                                                                    \
    static inline void name##_put(V **values, bytes key, V value) {      \
        *name##_get_or_insert(values, key, NULL) = value;                \
    }

#endif
