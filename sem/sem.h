#include "../ast/ast.h"

void do_build_symbol_table(NodeMetadata *m, AnyNode root);
void do_resolve_names(SourceCode *code, NodeMetadata *m, AnyNode root);
