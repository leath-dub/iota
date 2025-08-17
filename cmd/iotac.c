#include <assert.h>
#include <stdio.h>

#include "../syn/syn.h"

int main(void) {
	Source_Code code = new_source_code(ztos("<string>"), ztos("  \nmut main()"));
	Parse_Context pc = {
		.arena = new_arena(),
		.lex = new_lexer(code),
		.next_id = 0,
	};
	Declaration *decl = parse_decl(&pc);
	(void)decl;
	source_code_free(&code);
	arena_free(&pc.arena);
}
