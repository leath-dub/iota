#include <assert.h>
#include <stdio.h>

#include "../ast/ast.h"
#include "../syn/syn.h"

int main(void) {
  string source = ztos("let _ = @foo++; let foo = 10;");
  SourceCode code = new_source_code(ztos("<string>"), source);

  ParseCtx pc = new_parse_ctx(code);

  SourceFile *sf = source_file(&pc);
  (void)sf;

  TreeDumpCtx dump_ctx = {
      .fs = stdout, .indent_level = 0, .indent_width = 2, .meta = &pc.meta};
  tree_dump(&dump_ctx, sf->id);
  //
  // printf("---\n");
  //
  // Dump_Out out = new_dump_out();
  // DUMP(&out, &pc, sf, source_file);

  source_code_free(&code);
  parse_ctx_free(&pc);
}
