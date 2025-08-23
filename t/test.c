#include "test.h"

#include <stdio.h>
#include <stdlib.h>

void tassert(const char *file, const char *func, u32 line, const char *condstr,
             u32 condval) {
  if (!condval) {
    fprintf(stderr, "TEST(%s):%s:%d ASSERT(%s) FAILED\n", func, file, line,
            condstr);
    abort();
  }
}

bool strdiff(string str1, string str2) {
  u32 diff_len = MIN(str1.len, str2.len);
  if (streql(str1, str2)) {
    return true;
  }
  fprintf(stderr, "\n--- string 1\n");
  for (u32 i = 0; i < str1.len; i++) {
    char c = str1.data[i];
    if (i < diff_len) {
      fprintf(stderr, c == '\n' ? "%s %c\033[0m" : "%s%c\033[0m",
              str2.data[i] != c ? "\033[7m" : "", c);
    } else {
      fprintf(stderr, c == '\n' ? "%s·%c\033[0m" : "%s%c\033[0m", "\033[4m", c);
    }
  }
  fprintf(stderr, "\n--- string 2\n");
  for (u32 i = 0; i < str2.len; i++) {
    char c = str2.data[i];
    if (i < diff_len) {
      fprintf(stderr, c == '\n' ? "%s %c\033[0m" : "%s%c\033[0m",
              str1.data[i] != c ? "\033[7m" : "", c);
    } else {
      fprintf(stderr, c == '\n' ? "%s$%c\033[0m" : "%s%c\033[0m", "\033[4m", c);
    }
  }
  fprintf(stderr, "\n---\n");
  return false;
}
