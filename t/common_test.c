#define HM_INIT_SLOTS 4
#include "../common/common.h"

#include <stdlib.h>
#include <unistd.h>

#include "../common/map.h"
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

MAP_BYTES_DEFINE(point_map, Point)

#define BYTES(s) {.data = (uint8_t *)(s), .length = sizeof(s) - 1}
#define B(s) ((bytes)BYTES(s))

void test_hashmap(void) {
    Point *pm = point_map_create(HM_INIT_SLOTS);

    Point *point = point_map_get_or_insert(&pm, B("mypoint"), NULL);
    point->x = 20;
    point->y = 30;
    point->name = "hello";
    *point_map_get_or_insert(&pm, B("other"), NULL) = (Point){.x = 10, .y = 12};
    (void)point_map_get_or_insert(&pm, B("foo"), NULL);
    (void)point_map_get_or_insert(&pm, B("bar"), NULL);
    (void)point_map_get_or_insert(&pm, B("baz"), NULL);
    (void)point_map_get_or_insert(&pm, B("billy"), NULL);

    ASSERT(point_map_get(pm, B("mypoint")) != NULL);
    ASSERT(point_map_get(pm, B("other")) != NULL);
    ASSERT(point_map_get(pm, B("foo")) != NULL);
    ASSERT(point_map_get(pm, B("bar")) != NULL);
    ASSERT(point_map_get(pm, B("baz")) != NULL);
    ASSERT(point_map_get(pm, B("billy")) != NULL);

    Point *mypoint = point_map_get(pm, B("mypoint"));
    ASSERT(mypoint != NULL);
    ASSERT(mypoint->x == 20);
    ASSERT(mypoint->y == 30);
    ASSERT_STREQL(ztos((char *)mypoint->name), S("hello"));

    Point *other = point_map_get(pm, B("other"));
    ASSERT(other != NULL);
    ASSERT(other->x == 10);
    ASSERT(other->y == 12);

    MapCursor cursor = map_cursor_create(pm);
    ASSERT(map_cursor_next(&cursor));
    ASSERT(map_cursor_next(&cursor));
    ASSERT(map_cursor_next(&cursor));
    ASSERT(map_cursor_next(&cursor));
    ASSERT(map_cursor_next(&cursor));
    ASSERT(map_cursor_next(&cursor));
    ASSERT(!map_cursor_next(&cursor));

    point_map_delete(pm);
}

void test_stack(void) {
    Stack stack = stack_new();

    Point *ref = stack_push(&stack, sizeof(Point), _Alignof(Point));
    ref->x = 10;
    ref->y = 20;

    ref = stack_top(&stack);
    ASSERT(ref->x == 10);
    ASSERT(ref->y == 20);

    ref = stack_push(&stack, sizeof(Point), _Alignof(Point));
    ref->x = 434;
    ref->y = 12;

    ref = stack_top(&stack);
    ASSERT(ref->x == 434);
    ASSERT(ref->y == 12);

    stack_pop(&stack);

    ref = stack_top(&stack);
    ASSERT(ref->x == 10);
    ASSERT(ref->y == 20);

    stack_pop(&stack);
    ASSERT(stack.top == NULL);
}

int main(void) {
    test_arena();
    test_hashmap();
    test_stack();
}
