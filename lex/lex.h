#ifndef SCANNER_H_
#define SCANNER_H_

#include "../common/common.h"
#include "../mod/mod.h"
#include "uc.h"

#define EACH_TOKEN                          \
  TOKEN(EMPTY, "<empty token>")             \
  TOKEN(EOF, "<end of file>")               \
  TOKEN(CMNT, "<comment>")                  \
  TOKEN(SYNTHESIZED, "<synthesized token>") \
  TOKEN(ILLEGAL, "<illegal token>")         \
  TOKEN(EMPTY_STRING, "''")                 \
  TOKEN(LBRK, "'['")                        \
  TOKEN(RBRK, "']'")                        \
  TOKEN(LPAR, "'('")                        \
  TOKEN(RPAR, "')'")                        \
  TOKEN(LBRC, "'{'")                        \
  TOKEN(RBRC, "'}'")                        \
  TOKEN(SCLN, "';'")                        \
  TOKEN(CLN, "':'")                         \
  TOKEN(SCOPE, "'::'")                      \
  TOKEN(COMMA, "','")                       \
  TOKEN(BANG, "'!'")                        \
  TOKEN(QUEST, "'?'")                       \
  TOKEN(DOT, "'.'")                         \
  TOKEN(DOTDOT, "'..'")                     \
  TOKEN(PLUS, "'+'")                        \
  TOKEN(MINUS, "'-'")                       \
  TOKEN(STAR, "'*'")                        \
  TOKEN(SLASH, "'/'")                       \
  TOKEN(EQ, "'='")                          \
  TOKEN(EQEQ, "'=='")                       \
  TOKEN(NEQ, "'!='")                        \
  TOKEN(PIPE, "'|'")                        \
  TOKEN(AMP, "'&'")                         \
  TOKEN(PERC, "'%'")                        \
  TOKEN(INC, "'++'")                        \
  TOKEN(DEC, "'--'")                        \
  TOKEN(CHAR, "character")                  \
  TOKEN(STR, "string")                      \
  TOKEN(NUM, "number")                      \
  TOKEN(IDENT, "identifier")                \
  KEYWORD(NOT, "not")                       \
  KEYWORD(AND, "and")                       \
  KEYWORD(OR, "or")                         \
  KEYWORD(RO, "ro")                         \
  KEYWORD(FUN, "fun")                       \
  KEYWORD(IF, "if")                         \
  KEYWORD(ELSE, "else")                     \
  KEYWORD(FOR, "for")                       \
  KEYWORD(WHILE, "while")                   \
  KEYWORD(DEFER, "defer")                   \
  KEYWORD(STRUCT, "struct")                 \
  KEYWORD(UNION, "union")                   \
  KEYWORD(ENUM, "enum")                     \
  KEYWORD(LET, "let")                       \
  KEYWORD(TYPE, "type")                     \
  KEYWORD(IMPORT, "import")                 \
  KEYWORD(ERROR, "error")                   \
  KEYWORD(USE, "use")                       \
  KEYWORD(S8, "s8")                         \
  KEYWORD(U8, "u8")                         \
  KEYWORD(S16, "s16")                       \
  KEYWORD(U16, "u16")                       \
  KEYWORD(S32, "s32")                       \
  KEYWORD(U32, "u32")                       \
  KEYWORD(S64, "s64")                       \
  KEYWORD(U64, "u64")                       \
  KEYWORD(F32, "f32")                       \
  KEYWORD(F64, "f64")                       \
  KEYWORD(BOOL, "bool")                     \
  KEYWORD(RETURN, "return")                 \
  KEYWORD(STRING, "string")                 \
  KEYWORD(ANY, "any")                       \
  KEYWORD(CONS, "cons")

typedef enum {
#define TOKEN(NAME, ...) T_##NAME,
#define KEYWORD(NAME, ...) T_##NAME,
  EACH_TOKEN
#undef TOKEN
#undef KEYWORD
      TOK_KIND_COUNT,
} TokKind;

extern string tok_to_string[TOK_KIND_COUNT];

typedef struct {
  TokKind t;
  string text;
  u32 offset;
  union {
    u64 ival;
    f64 fval;
  };
} Tok;

typedef struct {
  SourceCode source;
  u32 cursor;
  Tok lookahead;  // is set every time `lex_peek` is called
} Lexer;

Lexer new_lexer(SourceCode source);
Tok lex_peek(Lexer *l);
void lex_consume(Lexer *l);

#endif

// vim: ft=c
