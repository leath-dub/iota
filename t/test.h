#ifndef TEST_H_
#define TEST_H_

void tassert(const char *file, const char *func, int line, const char *condstr, int condval);

#define ASSERT(cond) tassert(__FILE__, __func__, __LINE__, #cond, cond);

#endif

// vim: ft=c
