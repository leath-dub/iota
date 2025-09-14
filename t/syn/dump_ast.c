#include "../../ast/ast.h"
#include "../../syn/syn.h"
#include "../../common/common.h"

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

  return (string) {
    .data = data.items,
    .len = data.len,
  };
}

int main(void) {
  string text = read_all(stdin);
  Source_Code code = new_source_code(ztos("<stdin>"), text);
  Parse_Context pc = new_parse_context(code);
  Module *mod = parse_module(&pc);
  Dump_Out out = new_dump_out();
  DUMP(&out, &pc, mod);
  printf("\n");
  fflush(out.fs);
  free(text.data);
}
