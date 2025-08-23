#include <assert.h>
#include <stdio.h>

#include "../ast/ast.h"
#include "../syn/syn.h"

int main(void) {
  Source_Code code =
      new_source_code(ztos("<string>"), ztos("fun main(x s32, foo s32) {\n"
                                             "  let x = 10;\n"
                                             "  fun foo() {\n"
                                             "    {\n"
                                             "      fun bar() {}\n"
                                             "    }\n"
                                             "  }\n"
                                             "  fun bar() {}\n"
                                             "}"));
  Parse_Context pc = new_parse_context(code);
  Declaration *decl = parse_decl(&pc);
  Dump_Out out = new_dump_out();
  DUMP(&out, &pc, decl);
  __builtin_trap();
  (void)decl;
  source_code_free(&code);
  parse_context_free(&pc);
}
