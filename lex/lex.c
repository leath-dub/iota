#include "lex.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  string keyword;
  Tok_Kind type;
} Keyword_Binding;

string tok_to_string[TOK_KIND_COUNT] = {
    [T_LBRK] = ZTOS("'['"),
    [T_RBRK] = ZTOS("']'"),
    [T_LPAR] = ZTOS("'('"),
    [T_RPAR] = ZTOS("')'"),
    [T_LBRC] = ZTOS("'{'"),
    [T_RBRC] = ZTOS("'}'"),
    [T_SCLN] = ZTOS("';'"),
    [T_CLN] = ZTOS("':'"),
    [T_COMMA] = ZTOS("','"),

    [T_PLUS] = ZTOS("'+'"),
    [T_MINUS] = ZTOS("'-'"),
    [T_STAR] = ZTOS("'*'"),
    [T_SLASH] = ZTOS("'/'"),
    [T_EQ] = ZTOS("'='"),
    [T_EQEQ] = ZTOS("'=='"),
    [T_NEQ] = ZTOS("'!='"),

    [T_CHAR] = ZTOS("character"),
    [T_STR] = ZTOS("string"),
    [T_NUM] = ZTOS("number"),

    [T_NOT] = ZTOS("not"),
    [T_AND] = ZTOS("and"),
    [T_OR] = ZTOS("or"),
    [T_FUN] = ZTOS("fun"),
    [T_IF] = ZTOS("if"),
    [T_ELSE] = ZTOS("else"),
    [T_FOR] = ZTOS("for"),
    [T_DEFER] = ZTOS("defer"),
    [T_STRUCT] = ZTOS("struct"),
    [T_UNION] = ZTOS("union"),
    [T_ENUM] = ZTOS("enum"),
    [T_LET] = ZTOS("let"),
    [T_MUT] = ZTOS("mut"),
    [T_TYPE] = ZTOS("type"),
    [T_S32] = ZTOS("s32"),
    [T_IDENT] = ZTOS("identifier"),

    [T_EOF] = ZTOS("EOF"),
    [T_CMNT] = ZTOS("comment"),
    [T_EMPTY] = ZTOS("<empty token>"),
    [T_ILLEGAL] = ZTOS("<illegal token>"),
};

static const Keyword_Binding keyword_to_kind[] = {
    {ZTOS("not"), T_NOT},       {ZTOS("and"), T_AND},
    {ZTOS("or"), T_OR},         {ZTOS("fun"), T_FUN},
    {ZTOS("if"), T_IF},         {ZTOS("else"), T_ELSE},
    {ZTOS("for"), T_FOR},       {ZTOS("defer"), T_DEFER},
    {ZTOS("struct"), T_STRUCT}, {ZTOS("union"), T_UNION},
    {ZTOS("enum"), T_ENUM},     {ZTOS("let"), T_LET},
    {ZTOS("mut"), T_MUT},       {ZTOS("type"), T_TYPE},
    {ZTOS("s32"), T_S32},
};
static const u32 keyword_to_kind_count =
    sizeof(keyword_to_kind) / sizeof(Keyword_Binding);

static Tok new_tok(Lexer *l, Tok_Kind t, u32 len) {
  Tok r = (Tok){
      .t = t,
      .text = t != T_EOF ? substr(l->source.text, l->cursor, l->cursor + len)
                         : (string){.data = NULL, .len = 0},
      .offset = l->cursor,
  };
  l->lookahead = r;
  return r;
}

static void skipws(Lexer *l) {
  char c = l->source.text.data[l->cursor];
tok:
  switch (c) {
    case '\n':
    case '\t':
    case '\r':
    case ' ':
      l->cursor++;
      if (l->cursor >= l->source.text.len) {
        return;
      }
      c = l->source.text.data[l->cursor];
      goto tok;
  }
}

Lexer new_lexer(Source_Code source) {
  Lexer l = {
      .source = source,
      .cursor = 0,
      .lookahead = {.t = T_EMPTY},
  };
  if (source.text.len != 0) {
    skipws(&l);
  }
  return l;
}

static size_t scan_id(const char *txt);
static size_t scan_num(Lexer *l);
static Tok eval_num(Lexer *l, string num_text);

Tok lex_peek(Lexer *l) {
  // Cursor is at the same position as cached token just return that token.
  if (l->cursor == l->lookahead.offset && l->lookahead.t != T_EMPTY) {
    return l->lookahead;
  }
  if (l->cursor >= l->source.text.len) {
    return new_tok(l, T_EOF, 0);
  }
  char c = l->source.text.data[l->cursor];
  switch (c) {
    case '[':
      return new_tok(l, T_LBRK, 1);
    case ']':
      return new_tok(l, T_RBRK, 1);
    case '(':
      return new_tok(l, T_LPAR, 1);
    case ')':
      return new_tok(l, T_RPAR, 1);
    case '{':
      return new_tok(l, T_LBRC, 1);
    case '}':
      return new_tok(l, T_RBRC, 1);
    case ';':
      return new_tok(l, T_SCLN, 1);
    case ':':
      return new_tok(l, T_CLN, 1);
    case ',':
      return new_tok(l, T_COMMA, 1);
    case '=': {
      if (l->cursor + 1 < l->source.text.len &&
          l->source.text.data[l->cursor + 1] == '=') {
        return new_tok(l, T_EQEQ, 2);
      }
      return new_tok(l, T_EQ, 1);
    }
    case '*':
      return new_tok(l, T_STAR, 1);
    case '/': {
      if (l->cursor + 1 < l->source.text.len &&
          l->source.text.data[l->cursor + 1] == '/') {
        // Its a comment look for '\n' or eof
        u32 peek = l->cursor;
        char c = l->source.text.data[peek];
        while (c != '\n' && peek < l->source.text.len) {
          c = l->source.text.data[++peek];
        }
        return new_tok(l, T_CMNT, peek - l->cursor);
      }
      return new_tok(l, T_SLASH, 1);
    }
    case '\'': {
      // TODO: escape codes
      u32 peek = l->cursor + 1;
      char c = l->source.text.data[peek];
      while (c != '\'' && peek < l->source.text.len) {
        c = l->source.text.data[++peek];
      }
      if (c != '\'') {
        reportf(l->source, l->cursor, "unmatched quote in character literal");
        return new_tok(l, T_ILLEGAL, 1);
      }
      if (peek - l->cursor - 1 != 1) {
        reportf(l->source, l->cursor,
                "extraneous elements in character literal");
        return new_tok(l, T_ILLEGAL, peek - l->cursor + 1);
      }
      return new_tok(l, T_CHAR, peek - l->cursor + 1);
    }
    case '"': {
      // TODO: escape codes
      u32 peek = l->cursor + 1;
      char c = l->source.text.data[peek];
      while (c != '"' && peek < l->source.text.len) {
        c = l->source.text.data[++peek];
      }
      if (c != '"') {
        reportf(l->source, l->cursor, "unmatched quote in string literal");
        return new_tok(l, T_ILLEGAL, 1);
      }
      return new_tok(l, T_STR, peek - l->cursor + 1);
    }
    default: {
      // Unlike identifiers, numbers work with only ascii
      if ('0' <= c && c <= '9') {
        size_t len = scan_num(l);
        if (len == 0) {
          return new_tok(l, T_ILLEGAL, 1);
        }
        return eval_num(l, substr(l->source.text, l->cursor, l->cursor + len));
      }
      size_t len = scan_id(&l->source.text.data[l->cursor]);
      if (len == 0) {
        reportf(l->source, l->cursor, "invalid character '%c'", c);
        return new_tok(l, T_ILLEGAL, 1);
      }
      string id_text = substr(l->source.text, l->cursor, l->cursor + len);
      for (u32 i = 0; i < keyword_to_kind_count; i++) {
        Keyword_Binding b = keyword_to_kind[i];
        if (id_text.len == b.keyword.len &&
            strncmp(id_text.data, b.keyword.data, b.keyword.len) == 0) {
          return new_tok(l, b.type, len);
        }
      }
      return new_tok(l, T_IDENT, len);
    }
  }
  panic("unreachable state");
}

void lex_consume(Lexer *l) {
  // set cursor position to be pointing to the last byte of the current
  // token.
  l->cursor = l->lookahead.offset + l->lookahead.text.len;
  if (l->cursor >= l->source.text.len) {
    return;
  }
  skipws(l);
}

static size_t scan_num(Lexer *l) {
  // TODO: different base integers + floating point support
  u32 peek = l->cursor;
  char c = l->source.text.data[peek];
  while ('0' <= c && c <= '9' && peek < l->source.text.len) {
    c = l->source.text.data[++peek];
  }
  string text = substr(l->source.text, l->cursor, peek);
  if (text.data[0] == '0' && text.len != 1) {
    reportf(l->source, l->cursor,
            "0 is illegal at start of multi-digit number");
    return 0;
  }
  return peek - l->cursor;
}

static Tok eval_num(Lexer *l, string num_text) {
  Tok r = (Tok){
      .t = T_NUM,
      .text = num_text,
      .offset = l->cursor,
      .ival = atou64(num_text),
  };
  l->lookahead = r;
  return r;
}

// return the length of the matching identifier (0 if no match)
// TODO: convert the unicode procedures to use our own "string" type
static size_t scan_id(const char *txt) {
  const char *it = txt;
  u32 n = 0, l = 0;
  rune r;
  while (*it) {
    n = chartorune(&r, it);
    if (n == 0) {
      fprintf(stderr, "TODO: propagate utf8 decode error");
      abort();
    }
    bool is_id = it == txt ? id_start(r) : id_continue(r);
    if (!is_id) {
      break;
    }
    it += n;
    l += n;
  }
  // allow a trailing "'" if exists
  if (it && *it == '\'') {
    return l + 1;
  }
  return l;
}
