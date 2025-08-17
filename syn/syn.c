#include "syn.h"

#include <assert.h>

#include "../ast/ast.h"
#include "../lex/lex.h"

static Node_ID new_node_id(Parse_Context *c) {
	return c->next_id++;
}

static void expected_one_of(Parse_Context *c, u32 count, Tok_Kind *toks) {
	Tok_Kind curr = lex_peek(&c->lex).t;
	ERROR_POS(c->lex.source, c->lex.cursor);
	string curr_tok_str = tok_to_string[curr];
	char *expected_text;
	if (count == 1) {
		expected_text = "expected";
	} else {
		expected_text = "expected one of:";
	}
	fprintf(error_stream, "syntax error: unexpected %.*s, %s ", curr_tok_str.len, curr_tok_str.data, expected_text);
	for (u32 i = 0; i < count; i++) {
		string tok_str = tok_to_string[toks[i]];
		char *delim;
		if (i == 0) {
			delim = "";
		} else if (i == count - 1) {
			delim = " or ";
		} else {
			delim = ", ";
		}
		fprintf(error_stream, "%s%.*s", delim, tok_str.len, tok_str.data);
	}
	fprintf(error_stream, "\n");
}

Function_Declaration *parse_fun_decl(Parse_Context *c) {
	(void)c;
	return NULL;
}

Variable_Declaration *parse_var_decl(Parse_Context *c) {
	(void)c;
	return NULL;
}

// declaration : variable_declaration
// 	       | function_declaration
// 	       ;
Declaration *parse_decl(Parse_Context *c) {
	Tok tok = lex_peek(&c->lex);

	Declaration *decl = NEW(&c->arena, Declaration);
	decl->id = new_node_id(c);

	switch (tok.t) {
	case T_FUN: {
		decl->t = DECL_FUN;
		decl->fun = parse_fun_decl(c);
	} break;
	case T_MUT:
	case T_LET: {
		decl->t = DECL_VAR;
		decl->var = parse_var_decl(c);
	} break;
	default: {
		expected_one_of(c, ARRAY((Tok_Kind[]){ T_FUN, T_MUT, T_LET }));
		// TODO: add error meta data through parsing context
		return NULL;
	} break;
	}

	return decl;
}
