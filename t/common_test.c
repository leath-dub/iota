#include "test.h"
#include "../common/common.h"

#include <stdlib.h>
#include <unistd.h>

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

int main(void) {
	test_arena();
}
