#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stddef.h>

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

// Converts sentinal c string to 'string'
string ztos(char *s);

// Same as ztos (lower case) but only for 'char[<comptime known size>]'
#define ZTOS(s) { .data = (s), .len = sizeof(s) - 1 }

u64 atou64(string s);

#define MIN(a, b) (a) < (b) ? (a) : (b)
#define MAX(a, b) (a) > (b) ? (a) : (b)

#endif
