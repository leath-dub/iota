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
    u32 at;
    const char *expected;
    const char *got;
} SyntaxError;

typedef struct {
    u32 at;
    const char *message;
} SemanticError;

typedef enum {
    LEXICAL_ERROR_INVALID_CHAR,
    LEXICAL_ERROR_TEXT,
} LexicalErrorKind;

typedef struct {
    u32 at;
    LexicalErrorKind t;
    union {
        char invalid_char;
        const char *text;
    };
} LexicalError;

typedef enum {
    ERROR_SYNTAX,
    ERROR_LEXICAL,
    ERROR_SEMANTIC,
} ErrorKind;

typedef struct {
    ErrorKind t;
    union {
        SyntaxError syntax_error;
        LexicalError lexical_error;
        SemanticError semantic_error;
    };
} Error;

typedef struct {
    Error *items;
    u32 len;
    u32 cap;
} Errors;

typedef struct {
    string file_path;
    string text;
    Lines lines;
    FILE *error_stream;
    Arena error_arena;  // Used to store error message data
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

void raise_error(SourceCode *code, Error error);
void raise_syntax_error(SourceCode *code, SyntaxError error);
void raise_lexical_error(SourceCode *code, LexicalError error);
void raise_semantic_error(SourceCode *code, SemanticError error);
void report_error(SourceCode code, Error error);
void report_all_errors(SourceCode code);
// Like report_all_errors except it will also reset the list of errors
void flush_errors(SourceCode *code);

#endif
