#include <assert.h>
#include <stdio.h>

#include "../ast/ast.h"
#include "../syn/syn.h"

int main(void) {
  string source = ztos(
      "fun add(ok(*y) ..Foo) s32 {\n"
      "    let x = 10[10 + 12:] + -11 * 10;\n"
      "    x = 1 * 10;\n"
      "    foo[10] = 30;\n"
      "}\n"
      // "let (*x);\n"
      // "let foo *mut s32;\n"
      // "let add fun(f32, f32) struct { value s32, ok bool };\n"
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
