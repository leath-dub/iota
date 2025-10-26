#ifndef SYN_H
#define SYN_H

#include "../ast/ast.h"
#include "../lex/lex.h"

typedef struct {
  Tok_Kind *items;
  u32 len;
} Toks;

// I know this is smelly but C is very limited for variadic args and not storing
// size with array is really limiting for building abstractions
#define TOKS(...)                                               \
  (Toks) {                                                      \
    .items = (Tok_Kind[]){__VA_ARGS__},                         \
    .len = sizeof((Tok_Kind[]){__VA_ARGS__}) / sizeof(Tok_Kind) \
  }

typedef struct {
  Lexer lex;
  Arena arena;
  Node_Metadata meta;
  Node_ID current;
} Parse_Context;

typedef struct {
  Lexer lex;
} Parse_State;

void check_allocs(void);

Parse_Context new_parse_context(Source_Code code);
void parse_context_free(Parse_Context *c);

Source_File *source_file(Parse_Context *c);
Imports *imports(Parse_Context *c);
Import *import(Parse_Context *c);
Declarations *declarations(Parse_Context *c);
Declaration *declaration(Parse_Context *c);
Struct_Declaration *struct_declaration(Parse_Context *c);
Struct_Body *struct_body(Parse_Context *c, Toks follow);
Enum_Declaration *enum_declaration(Parse_Context *c);
Error_Declaration *error_declaration(Parse_Context *c);
Variable_Declaration *variable_declaration(Parse_Context *c);
Function_Declaration *function_declaration(Parse_Context *c);
Function_Parameter_List *function_parameter_list(Parse_Context *c);
Function_Parameter *function_parameter(Parse_Context *c);
Variable_Binding *variable_binding(Parse_Context *c);
Binding *binding(Parse_Context *c);
Aliased_Binding *aliased_binding(Parse_Context *c);
Aliased_Binding_List *aliased_binding_list(Parse_Context *c, Tok_Kind end);
Binding_List *binding_list(Parse_Context *c, Tok_Kind end);
Destructure_Struct *destructure_struct(Parse_Context *c, Toks follow);
Destructure_Tuple *destructure_tuple(Parse_Context *c, Toks follow);
Statement *statement(Parse_Context *c);
Compound_Statement *compound_statement(Parse_Context *c);
Type *type(Parse_Context *c);
Collection_Type *collection_type(Parse_Context *c);
Struct_Type *struct_type(Parse_Context *c);
Type_List *type_list(Parse_Context *c);
Field_List *field_list(Parse_Context *c);
Field *field(Parse_Context *c);
Union_Type *union_type(Parse_Context *c);
Enum_Type *enum_type(Parse_Context *c);
Error_Type *error_type(Parse_Context *c);
Pointer_Type *pointer_type(Parse_Context *c);
Function_Type *function_type(Parse_Context *c);
Identifier_List *identifier_list(Parse_Context *c);
Error_List *error_list(Parse_Context *c);
Error *error(Parse_Context *c);
Scoped_Identifier *scoped_identifier(Parse_Context *c, Toks follow);
Expression *expression(Parse_Context *c, Toks delim);
Basic_Expression *basic_expression(Parse_Context *c);
Initializer_List *initializer_list(Parse_Context *c, Tok_Kind delim);

#endif
