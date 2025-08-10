#include "test.h"
#include <stdio.h>
#include <stdlib.h>

void tassert(const char *file, const char *func, int line, const char *condstr, int condval) {
	if (!condval) {
		fprintf(stderr, "TEST(%s):%s:%d ASSERT(%s) FAILED\n", func, file, line, condstr);
		abort();
	}
}
