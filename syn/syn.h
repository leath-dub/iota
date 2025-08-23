#ifndef SYN_H
#define SYN_H

#include "../ast/ast.h"
#include "../lex/lex.h"

// Uses as a mask
typedef enum {
  NFLAG_NONE = 0,
  NFLAG_ERROR = 1,
} Node_Flags;

typedef struct {
  Node_Flags *items;
  u32 cap;
  u32 len;
} Node_Flags_Data;

typedef struct {
  Lexer lex;
  Arena arena;
  Node_ID next_id;
  Node_Flags_Data flags;
} Parse_Context;

Parse_Context new_parse_context(Source_Code code);
void parse_context_free(Parse_Context *c);

Module *parse_module(Parse_Context *c);

Declaration *parse_decl(Parse_Context *c);
Compound_Statement *parse_compound_stmt(Parse_Context *c);
Expr *parse_expr(Parse_Context *c);

#define DUMP(o, pc, n)                          \
  _Generic((n),                                 \
      Module *: dump_mod,                       \
      Declaration *: dump_decl,                 \
      Variable_Declaration *: dump_var_decl,    \
      Type *: dump_type,                        \
      Function_Declaration *: dump_fun_decl,    \
      Argument_List *: dump_arg_list,           \
      Argument *: dump_arg,                     \
      Statement *: dump_stmt,                   \
      Compound_Statement *: dump_compound_stmt, \
      Statement_List *: dump_stmt_list,         \
      Expr *: dump_expr)(o, pc, n)

typedef struct {
  u32 indent;
  FILE *fs;
} Dump_Out;

void dumpf(Dump_Out *dmp, const char *fmt, ...) PRINTF_CHECK(2, 3);

Dump_Out new_dump_out(void);

// For debug and testing purposes
void dump_mod(Dump_Out *dmp, Parse_Context *c, Module *m);
void dump_decl(Dump_Out *dmp, Parse_Context *c, Declaration *d);
void dump_var_decl(Dump_Out *dmp, Parse_Context *c, Variable_Declaration *vd);
void dump_type(Dump_Out *dmp, Parse_Context *c, Type *t);
void dump_fun_decl(Dump_Out *dmp, Parse_Context *c, Function_Declaration *fd);
void dump_arg_list(Dump_Out *dmp, Parse_Context *c, Argument_List *al);
void dump_arg(Dump_Out *dmp, Parse_Context *c, Argument *a);
void dump_stmt(Dump_Out *dmp, Parse_Context *c, Statement *st);
void dump_compound_stmt(Dump_Out *dmp, Parse_Context *c,
                        Compound_Statement *cs);
void dump_stmt_list(Dump_Out *dmp, Parse_Context *c, Statement_List *sl);
void dump_expr(Dump_Out *dmp, Parse_Context *c, Expr *e);

#endif
