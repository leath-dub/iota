#ifndef SCANNER_H_
#define SCANNER_H_

#include "uc.h"

#include "../common/common.h"
#include "../mod/mod.h"

typedef enum {
	T_LBRK,   // [
	T_RBRK,   // ]
	T_LPAR,   // (
	T_RPAR,   // )
	T_LBRC,   // {
	T_RBRC,   // }
	T_SCLN,   // ;
	T_CLN,    // :
	T_COMMA,  // ,

	T_PLUS,   // +
	T_MINUS,  // -
	T_STAR,   // *
	T_SLASH,  // /
	T_EQ,     // =
	T_EQEQ,   // ==
	T_NEQ,    // !=

	T_CHAR,   // character
	T_STR,    // string
	T_NUM,    // number

	T_NOT,    // keyword: not
	T_AND,    // keyword: and
	T_OR,     // keyword: or
	T_FUN,    // keyword: fun
	T_IF,     // keyword: if
	T_ELSE,   // keyword: else
	T_FOR,    // keyword: for
	T_DEFER,  // keyword: defer
	T_STRUCT, // keyword: struct
	T_UNION,  // keyword: union
	T_ENUM,   // keyword: enum
	T_LET,    // keyword: let
	T_MUT,    // keyword: mut
	T_TYPE,   // keyword: type
	T_S32,    // keyword: s32
	T_IDENT,  // identifier

	T_EOF,    // EOF
	T_CMNT,   // comment
	T_EMPTY,  // sentinal token value (used for initial state of lexer)
	T_ILLEGAL,

	TOK_KIND_COUNT,
} Tok_Kind;

extern string tok_to_string[TOK_KIND_COUNT];

typedef struct {
	Tok_Kind t;
	string text;
	u32 offset;
	union {
		u64 ival;
		f64 fval;
	};
} Tok;

typedef struct {
	Source_Code source;
	u32 cursor;
	Tok lookahead; // is set every time `lex_peek` is called
} Lexer;

Lexer new_lexer(Source_Code source);
Tok lex_peek(Lexer *l);
void lex_consume(Lexer *l);

#endif

// vim: ft=c
