#include <assert.h>
#include <stdarg.h>

#include "syn.h"

void dumpf(Dump_Out *d, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  if (d->delimit) {
    fputc('\n', d->fs);
    d->delimit = false;
  }
  if (!d->is_field) {
    fprintf(d->fs, "%*s", d->indent * 2, "");
  } else {
    d->is_field = false;
  }
  vfprintf(d->fs, fmt, args);
  va_end(args);
}

void dump_rawf(Dump_Out *d, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(d->fs, fmt, args);
  va_end(args);
}

Dump_Out new_dump_out(void) {
  return (Dump_Out){
      .fs = stdout,
      .indent = 0,
      .is_field = false,
      .delimit = false,
  };
}

#define D(name, node) DUMP(d, c, node, name)

void dump_node(Dump_Out *d, Parse_Context *c, void *node, const char *name,
               Dumper dumper) {
  if (has_error(&c->meta, node)) {
    dumpf(d, "(ERROR %s)", name);
    return;
  }
  dumpf(d, "(%s", name);
  d->indent++;
  d->delimit = true;
  dumper(d, c, node);
  d->delimit = false;
  dump_rawf(d, ")");
  d->indent--;
}

void dump_source_file(Dump_Out *d, Parse_Context *c, Source_File *n) {
  D(imports, n->imports);
  dump_rawf(d, "\n");
  D(declarations, n->declarations);
}

void dump_imports(Dump_Out *d, Parse_Context *c, Imports *n) {
  for (u32 i = 0; i < n->len; i++) {
    D(import, n->items[i]);
    if (i != n->len - 1) {
      dump_rawf(d, "\n");
    }
  }
}

void dump_import(Dump_Out *d, Parse_Context *c, Import *n) {
  if (n->aliased) {
    dumpf(d, "alias: ");
    dump_field_tok(d, c, n->alias);
    dump_rawf(d, "\n");
  }
  dump_tok(d, c, n->import_name);
}

void dump_declarations(Dump_Out *d, Parse_Context *c, Declarations *n) {
  for (u32 i = 0; i < n->len; i++) {
    D(declaration, n->items[i]);
    if (i != n->len - 1) {
      dump_rawf(d, "\n");
    }
  }
}

void dump_declaration(Dump_Out *d, Parse_Context *c, Declaration *n) {
  switch (n->t) {
    case DECLARATION_VARIABLE:
      D(variable_declaration, n->variable_declaration);
      break;
    case DECLARATION_FUNCTION:
      D(function_declaration, n->function_declaration);
      break;
    case DECLARATION_STRUCT:
      D(struct_declaration, n->struct_declaration);
      break;
    case DECLARATION_ENUM:
      D(enum_declaration, n->enum_declaration);
      break;
    case DECLARATION_ERROR:
      D(error_declaration, n->error_declaration);
      break;
    case DECLARATION_UNION:
      D(union_declaration, n->union_declaration);
      break;
  }
}

void dump_variable_declaration(Dump_Out *d, Parse_Context *c,
                               Variable_Declaration *n) {
  D(variable_binding, n->binding);
  if (n->type) {
    dump_rawf(d, "\n");
    D(type, n->type);
  }
  if (n->init.ok) {
    dump_rawf(d, "\n");
    D(expression, n->init.expression);
  }
}

void dump_function_declaration(Dump_Out *d, Parse_Context *c,
                               Function_Declaration *n) {
  dumpf(d, "name: ");
  dump_field_tok(d, c, n->identifier);
  dump_rawf(d, "\n");
  D(function_parameter_list, n->parameters);
  if (n->return_type.ok) {
    dump_rawf(d, "\n");
    D(type, n->return_type.value);
  }
  dump_rawf(d, "\n");
  D(compound_statement, n->body);
}

void dump_struct_declaration(Dump_Out *d, Parse_Context *c,
                             Struct_Declaration *n) {
  dumpf(d, "name: ");
  dump_field_tok(d, c, n->identifier);
  dump_rawf(d, "\n");
  if (n->tuple_like) {
    D(type_list, n->type_list);
  } else {
    D(field_list, n->field_list);
  }
}

void dump_enum_declaration(Dump_Out *d, Parse_Context *c, Enum_Declaration *n) {
  dumpf(d, "name: ");
  dump_field_tok(d, c, n->identifier);
  dump_rawf(d, "\n");
  D(identifier_list, n->enumerators);
}

void dump_error_declaration(Dump_Out *d, Parse_Context *c,
                            Error_Declaration *n) {
  dumpf(d, "name: ");
  dump_field_tok(d, c, n->identifier);
  dump_rawf(d, "\n");
  D(error_list, n->errors);
}

void dump_union_declaration(Dump_Out *d, Parse_Context *c,
                            Union_Declaration *n) {
  dumpf(d, "name: ");
  dump_field_tok(d, c, n->identifier);
  dump_rawf(d, "\n");
  D(field_list, n->fields);
}

void dump_error_list(Dump_Out *d, Parse_Context *c, Error_List *n) {
  for (u32 i = 0; i < n->len; i++) {
    D(error, n->items[i]);
    if (i != n->len - 1) {
      dump_rawf(d, "\n");
    }
  }
}

void dump_error(Dump_Out *d, Parse_Context *c, Error *n) {
  dumpf(d, "embedded: %s\n", n->embedded ? "true" : "false");
  D(scoped_identifier, n->scoped_identifier);
}

void dump_field_list(Dump_Out *d, Parse_Context *c, Field_List *n) {
  for (u32 i = 0; i < n->len; i++) {
    D(field, n->items[i]);
    if (i != n->len - 1) {
      dump_rawf(d, "\n");
    }
  }
}

void dump_type_list(Dump_Out *d, Parse_Context *c, Type_List *n) {
  for (u32 i = 0; i < n->len; i++) {
    D(type, n->items[i]);
    if (i != n->len - 1) {
      dump_rawf(d, "\n");
    }
  }
}

void dump_field(Dump_Out *d, Parse_Context *c, Field *n) {
  dumpf(d, "name: ");
  dump_field_tok(d, c, n->identifier);
  dump_rawf(d, "\n");
  D(type, n->type);
}

void dump_function_parameter_list(Dump_Out *d, Parse_Context *c,
                                  Function_Parameter_List *n) {
  for (u32 i = 0; i < n->len; i++) {
    D(function_parameter, n->items[i]);
    if (i != n->len - 1) {
      dump_rawf(d, "\n");
    }
  }
}

void dump_function_parameter(Dump_Out *d, Parse_Context *c,
                             Function_Parameter *n) {
  dumpf(d, "name: ");
  d->is_field = true;
  D(variable_binding, n->binding);
  dump_rawf(d, "\n");
  dumpf(d, "variadic: %s\n", n->variadic ? "true" : "false");
  D(type, n->type);
}

void dump_compound_statement(Dump_Out *d, Parse_Context *c,
                             Compound_Statement *n) {
  for (u32 i = 0; i < n->len; i++) {
    D(statement, n->items[i]);
    if (i != n->len - 1) {
      dump_rawf(d, "\n");
    }
  }
}

void dump_statement(Dump_Out *d, Parse_Context *c, Statement *n) {
  switch (n->t) {
    case STATEMENT_COMPOUND:
      D(compound_statement, n->compound_statement);
      break;
    case STATEMENT_DECLARATION:
      D(declaration, n->declaration);
      break;
    case STATEMENT_IF:
      D(if_statement, n->if_statement);
      break;
    case STATEMENT_RETURN:
      D(return_statement, n->return_statement);
      break;
    case STATEMENT_EXPRESSION:
      D(expression, n->expression);
      break;
  }
}

void dump_if_statement(Dump_Out *d, Parse_Context *c, If_Statement *n) {
  D(condition, n->condition);
  dump_rawf(d, "\n");
  D(compound_statement, n->true_branch);
  if (n->else_branch.ok) {
    dump_rawf(d, "\n");
    D(else, n->else_branch.value);
  }
}

void dump_condition(Dump_Out *d, Parse_Context *c, Condition *n) {
  switch (n->t) {
    case CONDITION_UNION_TAG:
      D(union_tag_condition, n->union_tag);
      break;
    case CONDITION_EXPRESSION:
      D(expression, n->expression);
      break;
  }
}

void dump_union_tag_condition(Dump_Out *d, Parse_Context *c,
                              Union_Tag_Condition *n) {
  dumpf(d, "mut: %s\n", n->classifier.t == T_MUT ? "true" : "false");
  D(destructure_union, n->trigger);
  dump_rawf(d, "\n");
  D(expression, n->expression);
}

void dump_else(Dump_Out *d, Parse_Context *c, Else *n) {
  switch (n->t) {
    case ELSE_COMPOUND:
      D(compound_statement, n->compound_statement);
      break;
    case ELSE_IF:
      D(if_statement, n->if_statement);
      break;
  }
}

void dump_return_statement(Dump_Out *d, Parse_Context *c, Return_Statement *n) {
  D(expression, n->expression);
}

void dump_identifier_list(Dump_Out *d, Parse_Context *c, Identifier_List *n) {
  for (u32 i = 0; i < n->len; i++) {
    dump_tok(d, c, n->items[i]);
    if (i != n->len - 1) {
      dump_rawf(d, "\n");
    }
  }
}

void dump_variable_binding(Dump_Out *d, Parse_Context *c, Variable_Binding *n) {
  switch (n->t) {
    case VARIABLE_BINDING_BASIC:
      dump_tok(d, c, n->basic);
      break;
    case VARIABLE_BINDING_DESTRUCTURE_STRUCT:
      D(destructure_struct, n->destructure_struct);
      break;
    case VARIABLE_BINDING_DESTRUCTURE_TUPLE:
      D(destructure_tuple, n->destructure_struct);
      break;
    case VARIABLE_BINDING_DESTRUCTURE_UNION:
      D(destructure_union, n->destructure_union);
      break;
  }
}

void dump_binding(Dump_Out *d, Parse_Context *c, Binding *n) {
  if (n->reference.t == T_STAR) {
    dump_tok(d, c, n->reference);
    dump_rawf(d, "\n");
  }
  dump_tok(d, c, n->identifier);
}

void dump_binding_list(Dump_Out *d, Parse_Context *c, Binding_List *n) {
  for (u32 i = 0; i < n->len; i++) {
    D(binding, n->items[i]);
    if (i != n->len - 1) {
      dump_rawf(d, "\n");
    }
  }
}

void dump_aliased_binding(Dump_Out *d, Parse_Context *c, Aliased_Binding *n) {
  D(binding, n->binding);
  if (n->alias.ok) {
    dump_rawf(d, "\n");
    dumpf(d, "alias: ");
    d->is_field = true;
    dump_tok(d, c, n->alias.value);
  }
}

void dump_aliased_binding_list(Dump_Out *d, Parse_Context *c,
                               Aliased_Binding_List *n) {
  for (u32 i = 0; i < n->len; i++) {
    D(aliased_binding, n->items[i]);
    if (i != n->len - 1) {
      dump_rawf(d, "\n");
    }
  }
}

void dump_destructure_struct(Dump_Out *d, Parse_Context *c,
                             Destructure_Struct *n) {
  D(aliased_binding_list, n->bindings);
}

void dump_destructure_tuple(Dump_Out *d, Parse_Context *c,
                            Destructure_Tuple *n) {
  D(binding_list, n->bindings);
}

void dump_destructure_union(Dump_Out *d, Parse_Context *c,
                            Destructure_Union *n) {
  D(binding, n->binding);
  dump_rawf(d, "\n");
  dump_tok(d, c, n->tag);
}

void dump_type(Dump_Out *d, Parse_Context *c, Type *n) {
  switch (n->t) {
    case TYPE_BUILTIN:
      D(builtin_type, n->builtin_type);
      break;
    case TYPE_COLLECTION:
      D(collection_type, n->collection_type);
      break;
    case TYPE_STRUCT:
      D(struct_type, n->struct_type);
      break;
    case TYPE_UNION:
      D(union_type, n->union_type);
      break;
    case TYPE_ENUM:
      D(enum_type, n->enum_type);
      break;
    case TYPE_ERROR:
      D(error_type, n->error_type);
      break;
    case TYPE_POINTER:
      D(pointer_type, n->pointer_type);
      break;
    case TYPE_FUNCTION:
      D(function_type, n->function_type);
      break;
    case TYPE_SCOPED_IDENTIFIER:
      D(scoped_identifier, n->scoped_identifier);
      break;
  }
}

void dump_builtin_type(Dump_Out *d, Parse_Context *c, Builtin_Type *n) {
  dump_tok(d, c, n->token);
}

void dump_collection_type(Dump_Out *d, Parse_Context *c, Collection_Type *n) {
  dumpf(d, "index: ");
  d->is_field = true;
  D(expression, n->index_expression);
  dump_rawf(d, "\n");
  D(type, n->element_type);
}

void dump_struct_type(Dump_Out *d, Parse_Context *c, Struct_Type *n) {
  dumpf(d, "tuple: %s\n", n->tuple_like ? "true" : "false");
  if (n->tuple_like) {
    D(type_list, n->type_list);
  } else {
    D(field_list, n->field_list);
  }
}

void dump_union_type(Dump_Out *d, Parse_Context *c, Union_Type *n) {
  D(field_list, n->fields);
}

void dump_enum_type(Dump_Out *d, Parse_Context *c, Enum_Type *n) {
  D(identifier_list, n->enumerators);
}

void dump_error_type(Dump_Out *d, Parse_Context *c, Error_Type *n) {
  D(error_list, n->errors);
}

void dump_pointer_type(Dump_Out *d, Parse_Context *c, Pointer_Type *n) {
  dumpf(d, "mut: %s\n", n->classifier.t == T_MUT ? "true" : "false");
  D(type, n->referenced_type);
}

void dump_function_type(Dump_Out *d, Parse_Context *c, Function_Type *n) {
  D(type_list, n->parameters);
  if (n->return_type.ok) {
    dump_rawf(d, "\n");
    D(type, n->return_type.value);
  }
}

void dump_scoped_identifier(Dump_Out *d, Parse_Context *c,
                            Scoped_Identifier *n) {
  for (u32 i = 0; i < n->len; i++) {
    dump_tok(d, c, n->items[i]);
    if (i != n->len - 1) {
      dump_rawf(d, "\n");
    }
  }
}

void dump_expression(Dump_Out *d, Parse_Context *c, Expression *n) {
  switch (n->t) {
    case EXPRESSION_BASIC:
      D(basic_expression, n->basic_expression);
      break;
    case EXPRESSION_PARENTHESIZED:
      D(parenthesized_expression, n->parenthesized_expression);
      break;
    case EXPRESSION_COMPOSITE_LITERAL:
      D(composite_literal_expression, n->composite_literal_expression);
      break;
    case EXPRESSION_POSTFIX:
      D(postfix_expression, n->postfix_expression);
      break;
    case EXPRESSION_FUNCTION_CALL:
      D(function_call_expression, n->function_call_expression);
      break;
    case EXPRESSION_FIELD_ACCESS:
      D(field_access_expression, n->field_access_expression);
      break;
    case EXPRESSION_ARRAY_ACCESS:
      D(array_access_expression, n->array_access_expression);
      break;
    case EXPRESSION_UNARY:
      D(unary_expression, n->unary_expression);
      break;
    case EXPRESSION_BINARY:
      D(binary_expression, n->binary_expression);
      break;
  }
}

void dump_basic_expression(Dump_Out *d, Parse_Context *c, Basic_Expression *n) {
  switch (n->t) {
    case BASIC_EXPRESSION_BRACED_LIT:
      D(braced_literal, n->braced_lit);
      break;
    case BASIC_EXPRESSION_TOKEN:
      dump_tok(d, c, n->token);
      break;
  }
}

void dump_braced_literal(Dump_Out *d, Parse_Context *c, Braced_Literal *n) {
  if (n->type.ok) {
    D(type, n->type.value);
    dump_rawf(d, "\n");
  }
  D(initializer_list, n->initializer);
}

void dump_parenthesized_expression(Dump_Out *d, Parse_Context *c,
                                   Parenthesized_Expression *n) {
  D(expression, n->inner_expression);
}

void dump_composite_literal_expression(Dump_Out *d, Parse_Context *c,
                                       Composite_Literal_Expression *n) {
  D(expression, n->value);
  if (n->explicit_type.ok) {
    dump_rawf(d, "\n");
    D(type, n->explicit_type.value);
  }
}

void dump_postfix_expression(Dump_Out *d, Parse_Context *c,
                             Postfix_Expression *n) {
  dumpf(d, "op: ");
  dump_field_tok(d, c, n->op);
  dump_rawf(d, "\n");
  D(expression, n->inner_expression);
}

void dump_function_call_expression(Dump_Out *d, Parse_Context *c,
                                   Function_Call_Expression *n) {
  D(expression, n->lvalue);
  dump_rawf(d, "\n");
  D(initializer_list, n->arguments);
}

void dump_initializer_list(Dump_Out *d, Parse_Context *c, Initializer_List *n) {
  for (u32 i = 0; i < n->len; i++) {
    D(expression, n->items[i]);
    if (i != n->len - 1) {
      dump_rawf(d, "\n");
    }
  }
}

void dump_field_access_expression(Dump_Out *d, Parse_Context *c,
                                  Field_Access_Expression *n) {
  D(expression, n->lvalue);
  dump_rawf(d, "\nfield: ");
  dump_field_tok(d, c, n->field);
}

void dump_array_access_expression(Dump_Out *d, Parse_Context *c,
                                  Array_Access_Expression *n) {
  D(expression, n->lvalue);
  dump_rawf(d, "\n");
  D(index, n->index);
}

void dump_unary_expression(Dump_Out *d, Parse_Context *c, Unary_Expression *n) {
  dumpf(d, "op: ");
  dump_field_tok(d, c, n->op);
  dump_rawf(d, "\n");
  D(expression, n->inner_expression);
}

void dump_binary_expression(Dump_Out *d, Parse_Context *c,
                            Binary_Expression *n) {
  D(expression, n->left);
  dump_rawf(d, "\n");
  dumpf(d, "op: ");
  dump_field_tok(d, c, n->op);
  dump_rawf(d, "\n");
  D(expression, n->right);
}

void dump_index(Dump_Out *d, Parse_Context *c, Index *n) {
  if (n->t == INDEX_SINGLE) {
    D(expression, n->start.value);
    return;
  }
  if (n->start.ok) {
    dumpf(d, "start: ");
    d->is_field = true;
    D(expression, n->start.value);
    if (n->end.ok) {
      dump_rawf(d, "\n");
    }
  }
  if (n->end.ok) {
    dumpf(d, "end: ");
    d->is_field = true;
    D(expression, n->end.value);
  }
}

void dump_field_tok(Dump_Out *d, Parse_Context *c, Tok tok) {
  d->is_field = true;
  dump_tok(d, c, tok);
}

void dump_tok(Dump_Out *d, Parse_Context *c, Tok tok) {
  (void)c;
  dumpf(d, "%.*s", tok.text.len, tok.text.data);
}

//
// void dump_mod(Dump_Out *dmp, Parse_Context *c, Module *m) {
//   dumpf(dmp, "[");
//   for (u32 i = 0; i < m->len; i++) {
//     if (i != 0) {
//       dumpf(dmp, ", ");
//     }
//     DUMP(dmp, c, m->items[i]);
//   }
//   dumpf(dmp, "]");
// }
//
// // For debug and testing purposes
// void dump_decl(Dump_Out *dmp, Parse_Context *c, Declaration *d) {
//   if (c->flags.items[d->id] & NFLAG_ERROR) {
//     dumpf(dmp, "\"<Declaration>\"");
//     return;
//   }
//   switch (d->t) {
//     case DECL_VAR:
//       DUMP(dmp, c, d->var);
//       break;
//     case DECL_FUN:
//       DUMP(dmp, c, d->fun);
//       break;
//   }
// }
//
// void dump_var_decl(Dump_Out *dmp, Parse_Context *c, Variable_Declaration *vd)
// {
//   if (c->flags.items[vd->id] & NFLAG_ERROR) {
//     dumpf(dmp, "\"<Variable_Declaration>\"");
//     return;
//   }
//   dumpf(dmp, "{\"kind\": \"variable\", \"name\": \"%.*s\", \"is_mut\": %s",
//         vd->name.text.len, vd->name.text.data, vd->is_mut ? "true" :
//         "false");
//   if (vd->type || vd->init) {
//     dumpf(dmp, ", ");
//   }
//   if (vd->type) {
//     dumpf(dmp, "\"type\": ");
//     DUMP(dmp, c, vd->type);
//   }
//   if (vd->init) {
//     // Fuck JSON not supporting trailing comma!
//     if (vd->type) {
//       dumpf(dmp, ", ");
//     }
//     dumpf(dmp, "\"value\": ");
//     DUMP(dmp, c, vd->init);
//   }
//   dumpf(dmp, "}");
// }
//
// void dump_type(Dump_Out *dmp, Parse_Context *c, Type *t) {
//   if (c->flags.items[t->id] & NFLAG_ERROR) {
//     dumpf(dmp, "\"<Type>\"");
//     return;
//   }
//   assert(t->t == TYPE_BUILTIN);
//   switch (t->builtin) {
//     case BUILTIN_S32:
//       dumpf(dmp, "\"s32\"");
//       break;
//     default:
//       assert(false && "TODO");
//   }
// }
//
// void dump_fun_decl(Dump_Out *dmp, Parse_Context *c, Function_Declaration *fd)
// {
//   if (c->flags.items[fd->id] & NFLAG_ERROR) {
//     dumpf(dmp, "\"<Function>\"");
//     return;
//   }
//   dumpf(dmp, "{\"kind\": \"function\", \"name\": \"%.*s\", \"args\": ",
//         fd->name.text.len, fd->name.text.data);
//   DUMP(dmp, c, fd->args);
//   if (fd->return_type) {
//     dumpf(dmp, ", \"returns\": ");
//     DUMP(dmp, c, fd->return_type);
//   }
//   dumpf(dmp, ", \"body\": ");
//   DUMP(dmp, c, fd->body);
//   dumpf(dmp, "}");
// }
//
// void dump_arg_list(Dump_Out *dmp, Parse_Context *c, Argument_List *al) {
//   if (c->flags.items[al->id] & NFLAG_ERROR) {
//     dumpf(dmp, "\"<Argument_List>\"");
//     return;
//   }
//   dumpf(dmp, "[");
//   for (u32 i = 0; i < al->len; i++) {
//     if (i != 0) {
//       dumpf(dmp, ", ");
//     }
//     DUMP(dmp, c, &al->items[i]);
//   }
//   dumpf(dmp, "]");
// }
//
// void dump_arg(Dump_Out *dmp, Parse_Context *c, Argument *a) {
//   if (c->flags.items[a->id] & NFLAG_ERROR) {
//     dumpf(dmp, "\"<Arg>\"");
//     return;
//   }
//   dumpf(dmp, "{\"kind\": \"argument\", \"name\": \"%.*s\", \"type\": ",
//         a->name.text.len, a->name.text.data);
//   DUMP(dmp, c, a->type);
//   dumpf(dmp, "}");
// }
//
// void dump_stmt(Dump_Out *dmp, Parse_Context *c, Statement *st) {
//   if (c->flags.items[st->id] & NFLAG_ERROR) {
//     dumpf(dmp, "\"<Statement>\"");
//     return;
//   }
//   switch (st->t) {
//     case STMT_DECL:
//       DUMP(dmp, c, st->decl);
//       break;
//     case STMT_COMP:
//       DUMP(dmp, c, st->compound);
//       break;
//     case STMT_ASSIGN:
//       DUMP(dmp, c, st->assign);
//       break;
//     default:
//       assert(false && "TODO");
//   }
// }
//
// void dump_compound_stmt(Dump_Out *dmp, Parse_Context *c,
//                         Compound_Statement *cs) {
//   if (c->flags.items[cs->id] & NFLAG_ERROR) {
//     dumpf(dmp, "\"<Compound_Statement>\"");
//     return;
//   }
//   dumpf(dmp, "{\"kind\": \"compound\", \"statements\": [");
//   DUMP(dmp, c, cs->statements);
//   dumpf(dmp, "]}");
// }
//
// void dump_stmt_list(Dump_Out *dmp, Parse_Context *c, Statement_List *sl) {
//   if (c->flags.items[sl->id] & NFLAG_ERROR) {
//     dumpf(dmp, "\"<Statement_List>\"");
//     return;
//   }
//   for (u32 i = 0; i < sl->len; i++) {
//     if (i != 0) {
//       dumpf(dmp, ", ");
//     }
//     Statement *stmt = &sl->items[i];
//     DUMP(dmp, c, stmt);
//   }
// }
//
// void dump_assign_stmt(Dump_Out *dmp, Parse_Context *c, Assign_Statement *as)
// {
//   if (c->flags.items[as->id] & NFLAG_ERROR) {
//     dumpf(dmp, "\"<Assignment_Statement>\"");
//     return;
//   }
//   dumpf(dmp, "{\"kind\": \"assign\", \"lhs\": %.*s, \"rhs\":",
//   as->lhs.text.len, as->lhs.text.data); DUMP(dmp, c, as->rhs); dumpf(dmp,
//   "}");
// }
//
// void dump_expr(Dump_Out *dmp, Parse_Context *c, Expr *e) {
//   if (c->flags.items[e->id] & NFLAG_ERROR) {
//     dumpf(dmp, "\"<Expr>\"");
//     return;
//   }
//   assert(e->t == EXPR_LIT);
//   assert(e->literal.t == T_NUM);
//   dumpf(dmp, "%ld", e->literal.ival);
// }
