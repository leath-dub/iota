#ifndef MOD_H
#define MOD_H

#include "../common/common.h"

// An array of line start offsets
typedef struct {
  u32 *items;
  u32 cap;
  u32 len;
} Lines;

typedef struct {
  FILE *fs;
} Errors;

typedef struct {
  string file_path;
  string text;
  Lines lines;
  Errors errors;
} Source_Code;

typedef struct {
  u32 line;    // 1-indexed
  u32 column;  // 1-indexed
} Position;

Source_Code new_source_code(string file_path, string text);
void source_code_free(Source_Code *code);

Position line_and_column(Lines lines, u32 offset);

void errorf(Source_Code code, const char *fmt, ...) PRINTF_CHECK(2, 3);
void report(Source_Code code, u32 at);
void reportf(Source_Code code, u32 at, const char *fmt, ...) PRINTF_CHECK(3, 4);

#endif
