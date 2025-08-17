#include "mod.h"

#include <string.h>
#include <stdbool.h>

FILE *error_stream = NULL;

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
	return (Source_Code) {
		.file_path = file_path,
		.lines = new_lines(text),
		.text = text,
	};
}

Position line_and_column(Lines lines, u32 offset) {
	for (u32 i = 0; i < lines.len; i++) {
		if (lines.items[i] <= offset) {
			bool last_line = i == lines.len - 1;
			if (last_line || offset <= lines.items[i + 1]) {
				return (Position) {
					.line = i + 1,
					.column = offset - lines.items[i] + 1,
				};
			}
		}
	}
	PANICF("offset %d has no associated line", offset);
}

void source_code_free(Source_Code *code) {
	if (code->lines.items != NULL) {
		free(code->lines.items);
	}
	memset(code, 0, sizeof(*code));
}
