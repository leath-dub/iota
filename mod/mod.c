#include "mod.h"

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

void errorf(Source_Code code, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(code.errors.fs, fmt, args);
  va_end(args);
}

void report(Source_Code code, u32 at) {
  Position pos = line_and_column(code.lines, at);
  string path = code.file_path;
  string line = line_of(code, at);
  // Trim '\n'
  if (line.data[line.len - 1] == '\n') {
    line.len -= 1;
  }
  static char fbuf[1024];
  snprintf(fbuf, sizeof(fbuf), " %d |", pos.line);
  u32 flen = strlen(fbuf);
  fprintf(code.errors.fs, "%s %.*s\n", fbuf, line.len, line.data);
  snprintf(fbuf, sizeof(fbuf), "%*s|", flen - 1, " ");
  fprintf(code.errors.fs, "%s %*s^\n", fbuf, pos.column - 1, "");
  fprintf(code.errors.fs, "%.*s:%d:%d:", path.len, path.data, pos.line,
          pos.column);
}

void reportf(Source_Code code, u32 at, const char *fmt, ...) {
  report(code, at);
  va_list args;
  va_start(args, fmt);
  fprintf(code.errors.fs, " ");
  vfprintf(code.errors.fs, fmt, args);
  fprintf(code.errors.fs, "\n");
  va_end(args);
}

static Lines new_lines(string text) {
  Lines lines = {0};
  APPEND(&lines, 0);
  for (u32 i = 0; i < text.len; i++) {
    if (text.data[i] == '\n' && i + 1 < text.len) {
      APPEND(&lines, i + 1);
    }
  }
  return lines;
}

Source_Code new_source_code(string file_path, string text) {
  return (Source_Code){
      .file_path = file_path,
      .lines = new_lines(text),
      .text = text,
      .errors =
          (Errors){
              .fs = stderr,
          },
  };
}

Position line_and_column(Lines lines, u32 offset) {
  for (u32 i = 0; i < lines.len - 1; i++) {
    if (lines.items[i] <= offset && offset < lines.items[i + 1]) {
      return (Position) {
        .line = i + 1,
        .column = offset - lines.items[i] + 1,
      };
    }
  }
  // Must be in the last line
  return (Position){
    .line = lines.len,
    .column = offset - lines.items[lines.len - 1] + 1,
  };
}

string line_of(Source_Code code, u32 offset) {
  Position pos = line_and_column(code.lines, offset);
  u32 line_start = code.lines.items[pos.line - 1];
  u32 line_end;
  if (pos.line >= code.lines.len) {
    line_end = code.text.len;
  } else {
    line_end = code.lines.items[pos.line];
  }
  return substr(code.text, line_start, line_end);
}

void source_code_free(Source_Code *code) {
  if (code->lines.items != NULL) {
    free(code->lines.items);
  }
  memset(code, 0, sizeof(*code));
}
