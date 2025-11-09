#define HM_INIT_SLOTS 4
#include "../common/common.h"

#include <stdlib.h>
#include <unistd.h>

#include "../common/hmtypes.h"
#include "test.h"

void test_arena(void) {
    // Just put some reasonable load on the allocator
    Arena a = new_arena();
    u32 last = (sysconf(_SC_PAGESIZE) * 5) / sizeof(u32);
    for (u32 i = 0; i < last; i++) {
        u32 *x = NEW(&a, u32);
        *x = i;
        ASSERT(*x == i);
    }
    u32 len = a.blocks.len;
    arena_reset(&a);
    for (u32 i = 0; i < last; i++) {
        u32 *x = NEW(&a, u32);
        // Ensure the memory is zeroed
        for (u32 j = 0; j < sizeof(u32); j++) {
            ASSERT(((u8 *)x)[j] == 0);
        }
        *x = i;
        ASSERT(*x == i);
    }
    // Should be able to deterministically insert the same data without
    // allocating any new blocks.
    ASSERT(a.blocks.len == len);
    arena_free(&a);

    // Since valgrind is ran on all the tests, a leak will get caught
    // if ownership did not actually get transferred such that the arena
    // frees the memory on `arena_free`.
    ASSERT(a.blocks.len == 0);
    u32 size = 10 * sizeof(int);
    void *alloc = malloc(size);
    arena_own(&a, alloc, size);
    arena_free(&a);
}

typedef struct Point {
    u32 x;
    u32 y;
    const char *name;
} Point;

void test_hashmap(void) {
    HashMapPoint pm = hm_point_new(HM_INIT_SLOTS);

    Point *point = hm_point_ensure(&pm, S("mypoint"));
    point->x = 20;
    point->y = 30;
    point->name = "hello";
    hm_point_put(&pm, S("other"), ((Point){.x = 10, .y = 12}));
    (void)hm_point_ensure(&pm, S("foo"));
    (void)hm_point_ensure(&pm, S("bar"));
    (void)hm_point_ensure(&pm, S("baz"));
    (void)hm_point_ensure(&pm, S("billy"));

    ASSERT(hm_point_contains(&pm, S("mypoint")));
    ASSERT(hm_point_contains(&pm, S("other")));
    ASSERT(hm_point_contains(&pm, S("foo")));
    ASSERT(hm_point_contains(&pm, S("bar")));
    ASSERT(hm_point_contains(&pm, S("baz")));
    ASSERT(hm_point_contains(&pm, S("billy")));

    Point *mypoint = hm_point_get(&pm, S("mypoint"));
    ASSERT(mypoint->x == 20);
    ASSERT(mypoint->y == 30);
    ASSERT_STREQL(ztos((char *)mypoint->name), S("hello"));

    Point *other = hm_point_get(&pm, S("other"));
    ASSERT(other->x == 10);
    ASSERT(other->y == 12);

    HashMapCursor cursor = hm_cursor_unsafe_new(&pm.base);
    ASSERT(hm_cursor_unsafe_next(&cursor) != NULL);
    ASSERT(hm_cursor_unsafe_next(&cursor) != NULL);
    ASSERT(hm_cursor_unsafe_next(&cursor) != NULL);
    ASSERT(hm_cursor_unsafe_next(&cursor) != NULL);
    ASSERT(hm_cursor_unsafe_next(&cursor) != NULL);
    ASSERT(hm_cursor_unsafe_next(&cursor) != NULL);
    ASSERT(hm_cursor_unsafe_next(&cursor) == NULL);

    hm_point_free(&pm);
}

int main(void) {
    test_arena();
    test_hashmap();
}
