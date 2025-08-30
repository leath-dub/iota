#ifndef SYN_H
#define SYN_H

#include "../ast/ast.h"
#include "../lex/lex.h"

// Uses as a mask

typedef struct {
  Lexer lex;
  Arena arena;
  Node_Metadata meta;
} Parse_Context;

Parse_Context new_parse_context(Source_Code code);
void parse_context_free(Parse_Context *c);

Source_File *source_file(Parse_Context *c);
Imports *imports(Parse_Context *c);
Import *import(Parse_Context *c);
Declarations *declarations(Parse_Context *c);
Declaration *declaration(Parse_Context *c);
Variable_Declaration *variable_declaration(Parse_Context *c);
Variable_List *variable_list(Parse_Context *c);
Variable_Binding *variable_binding(Parse_Context *c, Tok_Kind delim);
Type *type(Parse_Context *c);
Expression *expression(Parse_Context *c);

// Declaration *parse_decl(Parse_Context *c);
// Compound_Statement *parse_compound_stmt(Parse_Context *c);
// Expr *parse_expr(Parse_Context *c);

/*
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
      Assign_Statement *: dump_assign_stmt,     \
      Statement_List *: dump_stmt_list,         \
      Expr *: dump_expr)(o, pc, n)
*/

typedef struct {
  u32 indent;
  FILE *fs;
  bool is_field;
} Dump_Out;

void dumpf(Dump_Out *dmp, const char *fmt, ...) PRINTF_CHECK(2, 3);
void dump_rawf(Dump_Out *d, const char *fmt, ...) PRINTF_CHECK(2, 3);

Dump_Out new_dump_out(void);
typedef void (*Dumper)(Dump_Out *d, Parse_Context *c, void *node);

void dump_node(Dump_Out *d, Parse_Context *c, void *node, const char *name, Dumper dumper);
void dump_source_file(Dump_Out *d, Parse_Context *c, Source_File *n);
void dump_imports(Dump_Out *d, Parse_Context *c, Imports *n);
void dump_import(Dump_Out *d, Parse_Context *c, Import *n);
void dump_declarations(Dump_Out *d, Parse_Context *c, Declarations *n);
void dump_declaration(Dump_Out *d, Parse_Context *c, Declaration *n);
void dump_variable_declaration(Dump_Out *d, Parse_Context *c, Variable_Declaration *n);
void dump_function_declaration(Dump_Out *d, Parse_Context *c, Function_Declaration *n);
void dump_struct_declaration(Dump_Out *d, Parse_Context *c, Struct_Declaration *n);
void dump_enum_declaration(Dump_Out *d, Parse_Context *c, Enum_Declaration *n);
void dump_error_declaration(Dump_Out *d, Parse_Context *c, Error_Declaration *n);
void dump_union_declaration(Dump_Out *d, Parse_Context *c, Union_Declaration *n);
void dump_alias_declaration(Dump_Out *d, Parse_Context *c, Alias_Declaration *n);
void dump_use_declaration(Dump_Out *d, Parse_Context *c, Use_Declaration *n);
void dump_variable_list(Dump_Out *d, Parse_Context *c, Variable_List *n);
void dump_variable_binding(Dump_Out *d, Parse_Context *c, Variable_Binding *n);
void dump_builtin_type(Dump_Out *d, Parse_Context *c, Builtin_Type *n);
void dump_collection_type(Dump_Out *d, Parse_Context *c, Collection_Type *n);
void dump_struct_type(Dump_Out *d, Parse_Context *c, Struct_Type *n);
void dump_union_type(Dump_Out *d, Parse_Context *c, Union_Type *n);
void dump_enum_type(Dump_Out *d, Parse_Context *c, Enum_Type *n);
void dump_error_type(Dump_Out *d, Parse_Context *c, Error_Type *n);
void dump_pointer_type(Dump_Out *d, Parse_Context *c, Pointer_Type *n);
void dump_scoped_identifier(Dump_Out *d, Parse_Context *c, Scoped_Identifier *n);
void dump_tok(Dump_Out *d, Parse_Context *c, Tok tok);

#define DUMP(d, c, node, name) dump_node(d, c, node, #name, (Dumper)dump_##name);

// For debug and testing purposes
// void dump_mod(Dump_Out *dmp, Parse_Context *c, Module *m);
// void dump_decl(Dump_Out *dmp, Parse_Context *c, Declaration *d);
// void dump_var_decl(Dump_Out *dmp, Parse_Context *c, Variable_Declaration *vd);
// void dump_type(Dump_Out *dmp, Parse_Context *c, Type *t);
// void dump_fun_decl(Dump_Out *dmp, Parse_Context *c, Function_Declaration *fd);
// void dump_arg_list(Dump_Out *dmp, Parse_Context *c, Argument_List *al);
// void dump_arg(Dump_Out *dmp, Parse_Context *c, Argument *a);
// void dump_stmt(Dump_Out *dmp, Parse_Context *c, Statement *st);
// void dump_compound_stmt(Dump_Out *dmp, Parse_Context *c,
//                         Compound_Statement *cs);
// void dump_assign_stmt(Dump_Out *dmp, Parse_Context *c, Assign_Statement *as);
// void dump_stmt_list(Dump_Out *dmp, Parse_Context *c, Statement_List *sl);
// void dump_expr(Dump_Out *dmp, Parse_Context *c, Expr *e);

#endif
