#include "../../ast/ast.h"
#include "../../common/common.h"
#include "../../syn/syn.h"

string read_all(FILE *fs) {
  struct {
    char *items;
    u32 cap;
    u32 len;
  } data = {0};

  char c;
  while ((c = fgetc(fs)) != EOF) {
    APPEND(&data, c);
  }

  return (string){
      .data = data.items,
      .len = data.len,
  };
}

int main(void) {
  string text = read_all(stdin);
  Source_Code code = new_source_code(ztos("<stdin>"), text);
  Parse_Context pc = new_parse_context(code);
  Source_File *file = source_file(&pc);

  Tree_Dump_Ctx dump_ctx = {
      .fs = stdout, .indent_level = 0, .indent_width = 2, .meta = &pc.meta};
  tree_dump(&dump_ctx, file->id);

  parse_context_free(&pc);
  source_code_free(&code);
  fflush(dump_ctx.fs);
  free(text.data);
}
