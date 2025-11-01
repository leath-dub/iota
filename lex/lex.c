#include "lex.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  string keyword;
  TokKind type;
} KeywordBinding;

string tok_to_string[TOK_KIND_COUNT] = {
#define KEYWORD(NAME, REPR) [T_##NAME] = ZTOS("keyword " REPR),
#define TOKEN(NAME, REPR) [T_##NAME] = ZTOS(REPR),
    EACH_TOKEN
#undef TOKEN
#undef KEYWORD
};

static const KeywordBinding keyword_to_kind[] = {
#define TOKEN(...)
#define KEYWORD(NAME, REPR) {ZTOS(REPR), T_##NAME},
    EACH_TOKEN
#undef KEYWORD
#undef TOKEN
};
static const u32 keyword_to_kind_count =
    sizeof(keyword_to_kind) / sizeof(KeywordBinding);

static Tok new_tok(Lexer *l, TokKind t, u32 len) {
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

Lexer new_lexer(SourceCode source) {
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
static bool ahead(Lexer *l, char c);

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
      if (ahead(l, ':')) {
        return new_tok(l, T_SCOPE, 2);
      }
      return new_tok(l, T_CLN, 1);
    case ',':
      return new_tok(l, T_COMMA, 1);
    case '=': {
      if (ahead(l, '=')) {
        return new_tok(l, T_EQEQ, 2);
      }
      return new_tok(l, T_EQ, 1);
    }
    case '+':
      if (ahead(l, '+')) {
        return new_tok(l, T_INC, 2);
      }
      return new_tok(l, T_PLUS, 1);
    case '-':
      if (ahead(l, '-')) {
        return new_tok(l, T_DEC, 2);
      }
      return new_tok(l, T_MINUS, 1);
    case '*':
      return new_tok(l, T_STAR, 1);
    case '|':
      return new_tok(l, T_PIPE, 1);
    case '&':
      return new_tok(l, T_AMP, 1);
    case '%':
      return new_tok(l, T_PERC, 1);
    case '.':
      if (ahead(l, '.')) {
        return new_tok(l, T_DOTDOT, 2);
      }
      return new_tok(l, T_DOT, 1);
    case '!': {
      if (ahead(l, '=')) {
        return new_tok(l, T_NEQ, 2);
      }
      return new_tok(l, T_BANG, 1);
    }
    case '/': {
      if (ahead(l, '/')) {
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
        KeywordBinding b = keyword_to_kind[i];
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

static bool ahead(Lexer *l, char c) {
  return l->cursor + 1 < l->source.text.len &&
         l->source.text.data[l->cursor + 1] == c;
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
