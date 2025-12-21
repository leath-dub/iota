#include "../ast/ast.h"

void do_build_symbol_table(Ast *ast);
void do_resolve_names(Ast *ast, SourceCode *code);
void do_check_types(Ast *ast, SourceCode *code);
