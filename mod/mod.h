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
} SourceCode;

typedef struct {
  u32 line;    // 1-indexed
  u32 column;  // 1-indexed
} Position;

SourceCode new_source_code(string file_path, string text);
void source_code_free(SourceCode *code);

Position line_and_column(Lines lines, u32 offset);
string line_of(SourceCode code, u32 offset);

// TODO: add some way to restrict the number of errors on the same line - this
// is good to reduce spurious errors
void errorf(SourceCode code, const char *fmt, ...) PRINTF_CHECK(2, 3);
void reportf(SourceCode code, u32 at, const char *fmt, ...) PRINTF_CHECK(3, 4);

#endif
