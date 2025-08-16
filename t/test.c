#include "test.h"

#include <stdio.h>
#include <stdlib.h>

void tassert(const char *file, const char *func, u32 line, const char *condstr, u32 condval) {
	if (!condval) {
		fprintf(stderr, "TEST(%s):%s:%d ASSERT(%s) FAILED\n", func, file, line, condstr);
		abort();
	}
}
