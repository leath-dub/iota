#define HM_INIT_SLOTS 4
#include "../common/common.h"

#include <stdlib.h>
#include <unistd.h>

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

typedef struct {
  u32 x;
  u32 y;
  const char *name;
} Point;

typedef MAP(Point) PointMap;

void test_hashmap(void) {
  PointMap map = NEW_MAP(Point, PointMap);
  (void)HM_ENSURE(Point, &map, ztos("mypoint"));
  (void)HM_ENSURE(Point, &map, ztos("other"));
  (void)HM_ENSURE(Point, &map, ztos("foo"));
  (void)HM_ENSURE(Point, &map, ztos("bar"));
  (void)HM_ENSURE(Point, &map, ztos("baz"));
  (void)HM_ENSURE(Point, &map, ztos("billy"));

  ASSERT(hm_contains(&map, ztos("mypoint")));
  ASSERT(hm_contains(&map, ztos("other")));
  ASSERT(hm_contains(&map, ztos("foo")));
  ASSERT(hm_contains(&map, ztos("bar")));
  ASSERT(hm_contains(&map, ztos("baz")));
  ASSERT(hm_contains(&map, ztos("billy")));

  // Entry *entry = HM_ENSURE(Point, &map, ztos("mypoint"));
  // Point *point = (Point *)entry->data;
  // point->x = 10;
  // point->y = 12;
  // point->name = "hello";
  hm_free(&map);
}

int main(void) {
  test_arena();
  test_hashmap();
}
