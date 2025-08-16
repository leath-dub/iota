#include "test.h"
#include "../lex/lex.h"

#include <string.h>

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
	Lexer l = new_lexer(source);
	
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
}

void test_empty_file(void) {
	string source = ztos("");
	Lexer l = new_lexer(source);
	// 3 times for good measure :)
	ASSERT(peek_and_consume(&l).t == T_EOF);
	ASSERT(peek_and_consume(&l).t == T_EOF);
	ASSERT(peek_and_consume(&l).t == T_EOF);
}

void test_just_whitespace(void) {
	// just whitespace
	string source = ztos("    \n\t  \t  ");
	Lexer l = new_lexer(source);
	ASSERT(peek_and_consume(&l).t == T_EOF);
}

void test_single_token(void) {
	string source = ztos("x");
	Lexer l = new_lexer(source);
	ASSERT(peek_and_consume(&l).t == T_IDENT);
	ASSERT(peek_and_consume(&l).t == T_EOF);
}

void test_keywords(void) {
	string source = ztos("funbar fun if ifdo struct structx");
	Lexer l = new_lexer(source);

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
}

void test_number(void) {
	Lexer l = new_lexer(ztos("0"));
	Tok tok = peek_and_consume(&l);
	ASSERT(tok.t == T_NUM);
	ASSERT(tok.ival == 0);
	ASSERT(peek_and_consume(&l).t == T_EOF);

	l = new_lexer(ztos("1024"));
	tok = peek_and_consume(&l);
	ASSERT(tok.t == T_NUM);
	ASSERT(tok.ival == 1024);
	ASSERT(peek_and_consume(&l).t == T_EOF);
}

int main(void) {
	test_basic();
	test_empty_file();
	test_just_whitespace();
	test_single_token();
	test_keywords();
	test_number();
}
