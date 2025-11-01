#include <assert.h>
#include <stdio.h>

#include "../ast/ast.h"
#include "../syn/syn.h"

int main(void) {
  string source = ztos("if let Ok(*ro x) = foo(); {}");
  SourceCode code = new_source_code(ztos("<string>"), source);

  ParseCtx pc = new_parse_ctx(code);

  Stmt *root = parse_stmt(&pc);

  TreeDumpCtx dump_ctx = {
      .fs = stdout, .indent_level = 0, .indent_width = 2, .meta = &pc.meta};
  tree_dump(&dump_ctx, root->id);
  //
  // printf("---\n");
  //
  // Dump_Out out = new_dump_out();
  // DUMP(&out, &pc, sf, source_file);

  source_code_free(&code);
  parse_ctx_free(&pc);
}
