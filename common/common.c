#include "common.h"

#include <assert.h>
#include <string.h>

string substr(string s, u32 start, u32 end) {
	assert(start < s.len);
	assert(end <= s.len);
	return (string) { .data = &s.data[start], .len = end - start };
}

string ztos(char *s) {
	return (string) {
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
