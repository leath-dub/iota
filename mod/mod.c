#include "mod.h"

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

void errorf(SourceCode code, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(code.error_stream, fmt, args);
  va_end(args);
}

void reportf(SourceCode code, u32 at, const char *fmt, ...) {
  Position pos = line_and_column(code.lines, at);
  string path = code.file_path;
  string line = line_of(code, at);
  // Trim '\n'
  if (line.data[line.len - 1] == '\n') {
    line.len -= 1;
  }
  fprintf(code.error_stream, "%.*s:%d:%d:", path.len, path.data, pos.line,
          pos.column);
  va_list args;
  va_start(args, fmt);
  fprintf(code.error_stream, " ");
  vfprintf(code.error_stream, fmt, args);
  fprintf(code.error_stream, "\n");
  va_end(args);
  static char fbuf[1024];
  snprintf(fbuf, sizeof(fbuf), " %d |", pos.line);
  u32 flen = strlen(fbuf);
  fprintf(code.error_stream, "%s %.*s\n", fbuf, line.len, line.data);
  snprintf(fbuf, sizeof(fbuf), "%*s|", flen - 1, " ");
  fprintf(code.error_stream, "%s %*s^\n", fbuf, pos.column - 1, "");
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

SourceCode new_source_code(string file_path, string text) {
  return (SourceCode){
      .file_path = file_path,
      .lines = new_lines(text),
      .text = text,
      .error_stream = stderr,
  };
}

Position line_and_column(Lines lines, u32 offset) {
  for (u32 i = 0; i < lines.len - 1; i++) {
    if (lines.items[i] <= offset && offset < lines.items[i + 1]) {
      return (Position){
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

string line_of(SourceCode code, u32 offset) {
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

void source_code_free(SourceCode *code) {
  if (code->lines.items != NULL) {
    free(code->lines.items);
  }
  if (code->errors.items != NULL) {
    free(code->errors.items);
  }
  memset(code, 0, sizeof(*code));
}

void raise_error(SourceCode *code, Error error) {
  APPEND(&code->errors, error);
#ifdef ERROR_IMMEDIATE
  report_error(*code, error);
#endif
}

void raise_syntax_error(SourceCode *code, SyntaxError error) {
  raise_error(code, (Error){
                        .t = ERROR_SYNTAX,
                        .syntax_error = error,
                    });
}

void raise_lexical_error(SourceCode *code, LexicalError error) {
  raise_error(code, (Error){
                        .t = ERROR_LEXICAL,
                        .lexical_error = error,
                    });
}

void report_error(SourceCode code, Error error) {
  switch (error.t) {
    case ERROR_SYNTAX: {
      SyntaxError syntax_error = error.syntax_error;
      reportf(code, syntax_error.at, "syntax error: expected %s, found %s",
              syntax_error.expected, syntax_error.got);
      break;
    }
    case ERROR_LEXICAL: {
      LexicalError lexical_error = error.lexical_error;
      switch (lexical_error.t) {
        case LEXICAL_ERROR_INVALID_CHAR:
          reportf(code, lexical_error.at, "invalid character '%c'",
                  lexical_error.invalid_char);
          break;
        case LEXICAL_ERROR_TEXT:
          reportf(code, lexical_error.at, "%s", lexical_error.text);
          break;
        default:
          break;
      }
      break;
    }
    default:
      break;
  }
}

void report_all_errors(SourceCode code) {
  for (u32 i = 0; i < code.errors.len; i++) {
    report_error(code, code.errors.items[i]);
  }
}
