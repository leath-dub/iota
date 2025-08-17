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
	string file_path;
	string text;
	Lines lines;
} Source_Code;

typedef struct {
	u32 line;   // 1-indexed
	u32 column; // 1-indexed
} Position;

Source_Code new_source_code(string file_path, string text);
void source_code_free(Source_Code *code);

Position line_and_column(Lines lines, u32 offset);

#define UNQOUTE(s) #s

// You can override the error stream, this is useful for tests
extern FILE *error_stream;

#define ERROR(code, at, msg) do { \
	if (error_stream == NULL) { \
		error_stream = stderr; \
	} \
	Position pos = line_and_column(code.lines, at); \
	fprintf(error_stream, "%.*s:%d:%d: " msg "\n", code.file_path.len, code.file_path.data, pos.line, pos.column); \
} while (0)

#define ERRORF(code, at, fmt, ...) do { \
	if (error_stream == NULL) { \
		error_stream = stderr; \
	} \
	Position pos = line_and_column(code.lines, at); \
	fprintf(error_stream, "%.*s:%d:%d: " fmt "\n", code.file_path.len, code.file_path.data, pos.line, pos.column, __VA_ARGS__); \
} while (0)

#define ERROR_POS(code, at) do { \
	if (error_stream == NULL) { \
		error_stream = stderr; \
	} \
	Position pos = line_and_column(code.lines, at); \
	fprintf(error_stream, "%.*s:%d:%d: ", code.file_path.len, code.file_path.data, pos.line, pos.column); \
} while (0)

#endif
