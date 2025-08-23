// For fmemopen
#define _POSIX_C_SOURCE 200809L

#include "../lex/lex.h"

#include <string.h>

#include "../mod/mod.h"
#include "test.h"

Tok peek_and_consume(Lexer *l) {
  Tok tok = lex_peek(l);
  lex_consume(l);
  return tok;
}

// Broad stroke test
void test_basic(void) {
  string source = ztos(
      "fun main() {\n"
      "	mut x = 10;\n"
      "	let y = x * 12;\n"
      "	let x' = x;\n"
      "	x = 40;\n"
      "	if x == 20 {\n"
      "		// do this\n"
      "	} else {\n"
      "		// do that\n"
      "	}\n"
      "       let c = 'x';\n"
      "       let msg = \"Hello, world\";\n"
      "}");
  Source_Code code = new_source_code(ztos("<string>"), source);
  Lexer l = new_lexer(code);

  ASSERT(peek_and_consume(&l).t == T_FUN);
  ASSERT(peek_and_consume(&l).t == T_IDENT);
  ASSERT(peek_and_consume(&l).t == T_LPAR);
  ASSERT(peek_and_consume(&l).t == T_RPAR);

  ASSERT(peek_and_consume(&l).t == T_LBRC);

  ASSERT(peek_and_consume(&l).t == T_MUT);
  ASSERT(peek_and_consume(&l).t == T_IDENT);
  ASSERT(peek_and_consume(&l).t == T_EQ);
  ASSERT(peek_and_consume(&l).t == T_NUM);
  ASSERT(peek_and_consume(&l).t == T_SCLN);

  ASSERT(peek_and_consume(&l).t == T_LET);
  ASSERT(peek_and_consume(&l).t == T_IDENT);
  ASSERT(peek_and_consume(&l).t == T_EQ);
  ASSERT(peek_and_consume(&l).t == T_IDENT);
  ASSERT(peek_and_consume(&l).t == T_STAR);
  ASSERT(peek_and_consume(&l).t == T_NUM);
  ASSERT(peek_and_consume(&l).t == T_SCLN);

  ASSERT(peek_and_consume(&l).t == T_LET);
  ASSERT(peek_and_consume(&l).t == T_IDENT);
  ASSERT(peek_and_consume(&l).t == T_EQ);
  ASSERT(peek_and_consume(&l).t == T_IDENT);
  ASSERT(peek_and_consume(&l).t == T_SCLN);

  ASSERT(peek_and_consume(&l).t == T_IDENT);
  ASSERT(peek_and_consume(&l).t == T_EQ);
  ASSERT(peek_and_consume(&l).t == T_NUM);
  ASSERT(peek_and_consume(&l).t == T_SCLN);

  ASSERT(peek_and_consume(&l).t == T_IF);
  ASSERT(peek_and_consume(&l).t == T_IDENT);
  ASSERT(peek_and_consume(&l).t == T_EQEQ);
  ASSERT(peek_and_consume(&l).t == T_NUM);
  ASSERT(peek_and_consume(&l).t == T_LBRC);
  ASSERT(peek_and_consume(&l).t == T_CMNT);
  ASSERT(peek_and_consume(&l).t == T_RBRC);
  ASSERT(peek_and_consume(&l).t == T_ELSE);
  ASSERT(peek_and_consume(&l).t == T_LBRC);
  ASSERT(peek_and_consume(&l).t == T_CMNT);
  ASSERT(peek_and_consume(&l).t == T_RBRC);

  ASSERT(peek_and_consume(&l).t == T_LET);
  ASSERT(peek_and_consume(&l).t == T_IDENT);
  ASSERT(peek_and_consume(&l).t == T_EQ);
  ASSERT(peek_and_consume(&l).t == T_CHAR);
  ASSERT(peek_and_consume(&l).t == T_SCLN);

  ASSERT(peek_and_consume(&l).t == T_LET);
  ASSERT(peek_and_consume(&l).t == T_IDENT);
  ASSERT(peek_and_consume(&l).t == T_EQ);
  ASSERT(peek_and_consume(&l).t == T_STR);
  ASSERT(peek_and_consume(&l).t == T_SCLN);

  ASSERT(peek_and_consume(&l).t == T_RBRC);
  ASSERT(peek_and_consume(&l).t == T_EOF);

  source_code_free(&code);
}

void test_empty_file(void) {
  string source = ztos("");
  Source_Code code = new_source_code(ztos("<string>"), source);
  Lexer l = new_lexer(code);
  // 3 times for good measure :)
  ASSERT(peek_and_consume(&l).t == T_EOF);
  ASSERT(peek_and_consume(&l).t == T_EOF);
  ASSERT(peek_and_consume(&l).t == T_EOF);
  source_code_free(&code);
}

void test_just_whitespace(void) {
  // just whitespace
  string source = ztos("    \n\t  \t  ");
  Source_Code code = new_source_code(ztos("<string>"), source);
  Lexer l = new_lexer(code);
  ASSERT(peek_and_consume(&l).t == T_EOF);
  source_code_free(&code);
}

void test_single_token(void) {
  string source = ztos("x");
  Source_Code code = new_source_code(ztos("<string>"), source);
  Lexer l = new_lexer(code);
  ASSERT(peek_and_consume(&l).t == T_IDENT);
  ASSERT(peek_and_consume(&l).t == T_EOF);
  source_code_free(&code);
}

void test_keywords(void) {
  string source = ztos("funbar fun if ifdo struct structx");
  Source_Code code = new_source_code(ztos("<string>"), source);
  Lexer l = new_lexer(code);

  Tok tok = peek_and_consume(&l);
  ASSERT(tok.t == T_IDENT);
  ASSERT(strncmp(tok.text.data, "funbar", 6) == 0);

  tok = peek_and_consume(&l);
  ASSERT(tok.t == T_FUN);
  ASSERT(strncmp(tok.text.data, "fun", 3) == 0);

  tok = peek_and_consume(&l);
  ASSERT(tok.t == T_IF);
  ASSERT(strncmp(tok.text.data, "if", 2) == 0);

  tok = peek_and_consume(&l);
  ASSERT(tok.t == T_IDENT);
  ASSERT(strncmp(tok.text.data, "ifdo", 4) == 0);

  tok = peek_and_consume(&l);
  ASSERT(tok.t == T_STRUCT);
  ASSERT(strncmp(tok.text.data, "struct", 6) == 0);

  tok = peek_and_consume(&l);
  ASSERT(tok.t == T_IDENT);
  ASSERT(strncmp(tok.text.data, "structx", 7) == 0);

  ASSERT(peek_and_consume(&l).t == T_EOF);
  source_code_free(&code);
}

void test_number(void) {
  Source_Code code = new_source_code(ztos("<string>"), ztos("0"));
  Lexer l = new_lexer(code);
  Tok tok = peek_and_consume(&l);
  ASSERT(tok.t == T_NUM);
  ASSERT(tok.ival == 0);
  ASSERT(peek_and_consume(&l).t == T_EOF);
  source_code_free(&code);

  code = new_source_code(ztos("<string>"), ztos("1024"));
  l = new_lexer(code);
  tok = peek_and_consume(&l);
  ASSERT(tok.t == T_NUM);
  ASSERT(tok.ival == 1024);
  ASSERT(peek_and_consume(&l).t == T_EOF);
  source_code_free(&code);
}

// Test that the lexer can report error tokens but still recover
void test_error(void) {
  char *buf = NULL;
  usize len = 0;
  FILE *es = open_memstream(&buf, &len);

  Source_Code code = new_source_code(ztos("<string>"), ztos("'x"));
  code.errors.fs = es;
  Lexer l = new_lexer(code);
  ASSERT(peek_and_consume(&l).t == T_ILLEGAL);
  ASSERT(peek_and_consume(&l).t == T_IDENT);
  source_code_free(&code);

  code = new_source_code(ztos("<string>"), ztos("fun main($)"));
  code.errors.fs = es;
  l = new_lexer(code);
  ASSERT(peek_and_consume(&l).t == T_FUN);
  ASSERT(peek_and_consume(&l).t == T_IDENT);
  ASSERT(peek_and_consume(&l).t == T_LPAR);
  ASSERT(peek_and_consume(&l).t == T_ILLEGAL);
  ASSERT(peek_and_consume(&l).t == T_RPAR);
  source_code_free(&code);

  fflush(es);

  string output = ztos(buf);
  string expected_output = ztos(
      " 1 | 'x\n"
      "   | ^\n"
      "<string>:1:1: unmatched quote in character literal\n"
      " 1 | fun main($)\n"
      "   |          ^\n"
      "<string>:1:10: invalid character '$'\n");
  ASSERT_STREQL(output, expected_output);

  fclose(es);
  if (buf != NULL) {
    free(buf);
  }
}

int main(void) {
  test_basic();
  test_empty_file();
  test_just_whitespace();
  test_single_token();
  test_keywords();
  test_number();
  test_error();
}
