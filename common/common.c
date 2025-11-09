#include "common.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

string substr(string s, u32 start, u32 end) {
  assert(start < s.len);
  assert(end <= s.len);
  return (string){.data = &s.data[start], .len = end - start};
}

bool streql(string a, string b) {
  if (a.len != b.len) {
    return false;
  }
  return memcmp(a.data, b.data, a.len) == 0;
}

string ztos(char *s) {
  return (string){
      .data = s,
      .len = strlen(s),
  };
}

static u64 powu64(u64 b, u64 exp) {
  u64 r = 1;
  for (u32 n = exp; n > 0; n--) {
    r *= b;
  }
  return r;
}

u64 atou64(string s) {
  u64 r = 0;
  for (u32 i = 0; i < s.len; i++) {
    r += (s.data[i] - '0') * powu64(10, s.len - i - 1);
  }
  return r;
}

void panic(const char *msg) {
  fprintf(stderr, "panic: %s\n", msg);
  fflush(stderr);
  abort();
}

Arena new_arena(void) {
  return (Arena){
      .block_size = sysconf(_SC_PAGESIZE),
      .blocks = (Blocks){.cap = 0, .len = 0, .items = NULL},
      .adopted_blocks = (Blocks){.cap = 0, .len = 0, .items = NULL},
  };
}

static inline uptr align_forward(uptr p, uptr align) {
  return (p + (align - 1)) & ~(align - 1);
}

// Trys to allocate size bytes in block, returning NULL if not successful
static inline void *try_alloc_in_block(Block *block, size_t size, uptr align) {
  uptr base_ptr = (uptr)&block->alloc[block->used];
  uptr aligned_ptr = align_forward(base_ptr, align);
  uptr padding = aligned_ptr - base_ptr;
  if (block->used + padding + size < block->cap) {
    block->used += padding + size;
    memset((void *)aligned_ptr, 0, size);
    return (void *)aligned_ptr;
  }
  return NULL;
}

void *arena_alloc(Arena *a, size_t size, uptr align) {
  if (size + align > a->block_size) {
    panic(
        "arena: tried to allocate something larger than blocksize.\n"
        "if you need a larger allocation, allocate it separately and"
        "transfer ownership to the arena using 'arena_own' function.");
  }

  if (a->blocks.items == NULL) {
    void *alloc = malloc(a->block_size);
    if (alloc == NULL) {
      panic("out of memory");
    }
    APPEND(&a->blocks, (Block){
                           .cap = a->block_size,
                           .used = 0,
                           .alloc = alloc,
                       });
  }

  for (u32 i = 0; i < a->blocks.len; i++) {
    void *p = try_alloc_in_block(&a->blocks.items[i], size, align);
    if (p != NULL) {
      return p;
    }
  }

  // No block currently has enough memory, create a new block
  // of double the size of the last allocated block.
  a->block_size *= 2;

  void *alloc = malloc(a->block_size);
  if (alloc == NULL) {
    panic("out of memory");
  }
  APPEND(&a->blocks, (Block){
                         .cap = a->block_size,
                         .used = 0,
                         .alloc = alloc,
                     });
  void *p =
      try_alloc_in_block(&a->blocks.items[a->blocks.len - 1], size, align);
  assert(p != NULL);
  return p;
}

void arena_reset(Arena *a) {
  for (u32 i = 0; i < a->blocks.len; i++) {
    a->blocks.items[i].used = 0;
  }
  for (u32 i = 0; i < a->adopted_blocks.len; i++) {
    a->adopted_blocks.items[i].used = 0;
  }
}

void arena_free(Arena *a) {
  for (u32 i = 0; i < a->blocks.len; i++) {
    const Block *block = &a->blocks.items[i];
    free(block->alloc);
  }
  if (a->blocks.items != NULL) {
    free(a->blocks.items);
  }
  a->blocks.items = NULL;
  a->blocks.cap = 0;
  a->blocks.len = 0;

  for (u32 i = 0; i < a->adopted_blocks.len; i++) {
    const Block *block = &a->adopted_blocks.items[i];
    free(block->alloc);
  }
  if (a->adopted_blocks.items != NULL) {
    free(a->adopted_blocks.items);
  }
  a->adopted_blocks.items = NULL;
  a->adopted_blocks.cap = 0;
  a->adopted_blocks.len = 0;
}

void arena_own(Arena *a, void *alloc, u32 size) {
  APPEND(&a->adopted_blocks, (Block){
                                 .cap = size,
                                 .used = size,
                                 .alloc = alloc,
                             });
}
