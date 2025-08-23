#ifndef TEST_H_
#define TEST_H_

#include "../common/common.h"

void tassert(const char *file, const char *func, u32 line, const char *condstr,
             u32 condval);
bool strdiff(string str1, string str2);

#define ASSERT(cond) tassert(__FILE__, __func__, __LINE__, #cond, cond)
#define ASSERT_STREQL(str1, str2)                               \
  tassert(__FILE__, __func__, __LINE__, "string 1 == string 2", \
          strdiff(str1, str2))

#endif

// vim: ft=c
