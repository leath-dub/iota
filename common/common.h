#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef int8_t s8;
typedef uint8_t u8;
typedef int16_t s16;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint32_t u32;
typedef int64_t s64;
typedef uint64_t u64;
typedef intptr_t sptr;
typedef uintptr_t uptr;
typedef ptrdiff_t ssize;
typedef size_t usize;

_Static_assert(sizeof(float) == 4, "platform does not support IEEE 754");
_Static_assert(sizeof(double) == 8, "platform does not have 64-bit double");

typedef float f32;
typedef double f64;

typedef struct {
  char *data;
  u32 len;
} string;

// Return a sub string (view) of 's' from 'start' inclusive to 'end' exclusive
string substr(string s, u32 start, u32 end);
bool streql(string a, string b);

// Converts sentinal c string to 'string'
string ztos(char *s);

// Same as ztos (lower case) but only for 'char[<comptime known size>]'
#define ZTOS(s) {.data = (s), .len = sizeof(s) - 1}

u64 atou64(string s);

#define MIN(a, b) (a) < (b) ? (a) : (b)
#define MAX(a, b) (a) > (b) ? (a) : (b)

// Logging
void panic(const char *msg);
#define PANICF(fmt, ...)                              \
  do {                                                \
    fprintf(stderr, "panic: " fmt "\n", __VA_ARGS__); \
    abort();                                          \
  } while (0)
#define LOGF(fmt, ...) fprintf(stderr, fmt "\n", __VA_ARGS__)

#define AT(arr, index) \
  (assert(index < (arr).len && "index out of bounds"), &(arr).items[index])

// Generic dynamic array append. expects "arr" to have shape
// *struct { <int> len, <int> cap, <typeof(item)*> items }
#define APPEND(arr, ...)                                             \
  do {                                                               \
    if ((arr)->len >= (arr)->cap) {                                  \
      (arr)->cap = (arr)->cap ? (arr)->cap * 2 : 1;                  \
      void *newptr =                                                 \
          realloc((arr)->items, (arr)->cap * sizeof(*(arr)->items)); \
      if (newptr == NULL) {                                          \
        panic("out of memory");                                      \
      }                                                              \
      (arr)->items = newptr;                                         \
    }                                                                \
    (arr)->items[(arr)->len++] = (__VA_ARGS__);                      \
  } while (0)

// A flexible array member is not used as this arena
// supports externally allocated blocks which the arena takes ownership of
typedef struct {
  u32 cap;
  u32 used;
  u8 *alloc;
} Block;

typedef struct {
  u32 cap;
  u32 len;
  Block *items;
} Blocks;

typedef struct {
  u32 block_size;
  Blocks blocks;
} Arena;

Arena new_arena(void);
// If you don't care about alignment just pass _Alignof(max_align_t) or use NEW
// macro
void *arena_alloc(Arena *a, size_t size, uptr align);
void arena_reset(Arena *a);
void arena_free(Arena *a);
// Transfer the ownership of some memory allocated by malloc to the arena so it
// is freed with everything else. This is especially useful if you allocated
// dynamic arrays separate to the arena (good idea) and you still want the
// lifetime grouped with the arena.
void arena_own(Arena *a, void *alloc, u32 size);

#define NEW(a, type) arena_alloc(a, sizeof(type), _Alignof(type))
#define LEN(a) sizeof(a) / sizeof(*a)

#define PRINTF_CHECK(fmti, arg0)

#ifdef __GNUC__
#undef PRINTF_CHECK
#define PRINTF_CHECK(fmti, arg0) __attribute__((format(printf, fmti, arg0)))
#endif

#ifdef __clang__
#undef PRINTF_CHECK
#define PRINTF_CHECK(fmti, arg0) __attribute__((format(printf, fmti, arg0)))
#endif

#define MAYBE(T) \
  struct {       \
    bool ok;     \
    T value;     \
  }

#endif
