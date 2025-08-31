#include <assert.h>
#include <stdio.h>

#include "../ast/ast.h"
#include "../syn/syn.h"

int main(void) {
  string source = ztos(
    "let foo' struct {\n"
    "  x int;\n"
    "  y float;\n"
    "}, foo *string;"
    // "import im \"foo/xxx\";\n"
    // "import im' \"bar/xxx\";\n"
    // "let x any, y foo.bar.baz;\n"
    // "let x struct{ _ s32; _ string; };\n"
    // "let x *mut struct(s32, string);\n"
    // "let x enum { foo, bar };\n"
    // "let x error { !foo, bar };\n"
  );
  Source_Code code = new_source_code(ztos("<string>"), source);

  Parse_Context pc = new_parse_context(code);

  Source_File *sf = source_file(&pc);
  Dump_Out out = new_dump_out();
  DUMP(&out, &pc, sf, source_file);

  // Module *mod = parse_module(&pc);
  // DUMP(&out, &pc, mod);
  // printf("\n");
  // fflush(out.fs);
  // __builtin_trap();
  source_code_free(&code);
  parse_context_free(&pc);
}
