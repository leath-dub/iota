#include <assert.h>
#include <stdio.h>

#include "../ast/ast.h"
#include "../syn/syn.h"

int main(void) {
  string source = ztos("let _ = @foo++; let foo = 10;");
  Source_Code code = new_source_code(ztos("<string>"), source);

  Parse_Context pc = new_parse_context(code);

  Source_File *sf = source_file(&pc);
  (void)sf;

  Tree_Dump_Ctx dump_ctx = {
      .fs = stdout, .indent_level = 0, .indent_width = 2, .meta = &pc.meta};
  tree_dump(&dump_ctx, sf->id);
  //
  // printf("---\n");
  //
  // Dump_Out out = new_dump_out();
  // DUMP(&out, &pc, sf, source_file);

  source_code_free(&code);
  parse_context_free(&pc);
}
