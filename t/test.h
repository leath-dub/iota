#ifndef TEST_H_
#define TEST_H_

#include "../common/common.h"

void tassert(const char *file, const char *func, u32 line, const char *condstr, u32 condval);

#define ASSERT(cond) tassert(__FILE__, __func__, __LINE__, #cond, cond);

#endif

// vim: ft=c
