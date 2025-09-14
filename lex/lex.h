#ifndef SCANNER_H_
#define SCANNER_H_

#include "../common/common.h"
#include "../mod/mod.h"
#include "uc.h"

typedef enum {
  T_EMPTY = 0,    // sentinal token value (used for initial state of lexer)
  T_EOF,          // EOF
  T_CMNT,         // comment
  T_SYNTHESIZED,  // not a real token, used when synthesizing a AST (the ast is
                  // more of a CST in the iota compiler
  T_ILLEGAL,
  T_LBRK,    // [
  T_RBRK,    // ]
  T_LPAR,    // (
  T_RPAR,    // )
  T_LBRC,    // {
  T_RBRC,    // }
  T_SCLN,    // ;
  T_CLN,     // :
  T_COMMA,   // ,
  T_BANG,    // !
  T_DOT,     // .
  T_DOTDOT,  // ..

  T_PLUS,   // +
  T_MINUS,  // -
  T_STAR,   // *
  T_SLASH,  // /
  T_EQ,     // =
  T_EQEQ,   // ==
  T_NEQ,    // !=

  T_CHAR,  // character
  T_STR,   // string
  T_NUM,   // number

  T_NOT,     // keyword: not
  T_AND,     // keyword: and
  T_OR,      // keyword: or
  T_FUN,     // keyword: fun
  T_IF,      // keyword: if
  T_ELSE,    // keyword: else
  T_FOR,     // keyword: for
  T_WHILE,   // keyword: while
  T_DEFER,   // keyword: defer
  T_STRUCT,  // keyword: struct
  T_UNION,   // keyword: union
  T_ENUM,    // keyword: enum
  T_LET,     // keyword: let
  T_MUT,     // keyword: mut
  T_TYPE,    // keyword: type
  T_IMPORT,  // keyword: import
  T_ERROR,   // keyword: error
  T_USE,     // keyword: use
  T_S8,      // keyword: s8
  T_U8,      // keyword: u8
  T_S16,     // keyword: s16
  T_U16,     // keyword: u16
  T_S32,     // keyword: s32
  T_U32,     // keyword: u32
  T_S64,     // keyword: s64
  T_U64,     // keyword: u64
  T_F32,     // keyword: f32
  T_F64,     // keyword: f64
  T_BOOL,    // keyword: bool
  T_STRING,  // keyword: string
  T_ANY,     // keyword: any
  T_IDENT,   // identifier

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
  Tok lookahead;  // is set every time `lex_peek` is called
} Lexer;

Lexer new_lexer(Source_Code source);
Tok lex_peek(Lexer *l);
void lex_consume(Lexer *l);

#endif

// vim: ft=c
