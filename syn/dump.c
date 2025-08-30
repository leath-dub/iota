#include <assert.h>
#include <stdarg.h>

#include "syn.h"

void dumpf(Dump_Out *d, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  if (!d->is_field) {
    fprintf(d->fs, "%*s", d->indent * 2, "");
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
  };
}

#define D(name, node) DUMP(d, c, node, name)

void dump_node(Dump_Out *d, Parse_Context *c, void *node, const char *name, Dumper dumper) {
  if (has_error(&c->meta, node)) {
    dumpf(d, "(error)");
    return;
  }
  dumpf(d, "(%s\n", name);
  d->indent++;
  dumper(d, c, node);
  dumpf(d, ")");
  d->indent--;
}

void dump_source_file(Dump_Out *d, Parse_Context *c, Source_File *n) {
  D(imports, n->imports);
  dump_rawf(d, "\n");
  D(declarations, n->imports);
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
    d->is_field = true;
    dump_tok(d, c, n->alias);
    d->is_field = false;
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
    case DECLARATION_ALIAS:
      D(alias_declaration, n->alias_declaration);
      break;
    case DECLARATION_USE:
      D(dump_use_declaration, n->use_declaration);
      break;
  }
}

void dump_variable_declaration(Dump_Out *d, Parse_Context *c, Variable_Declaration *n) {
  D(variable_list, n->variable_list);
  dump_rawf(d, ")");
}

void dump_function_declaration(Dump_Out *d, Parse_Context *c, Function_Declaration *n) {
  (void)d;
  (void)c;
  (void)n;
}

void dump_struct_declaration(Dump_Out *d, Parse_Context *c, Struct_Declaration *n) {
  (void)d;
  (void)c;
  (void)n;
}

void dump_enum_declaration(Dump_Out *d, Parse_Context *c, Enum_Declaration *n) {
  (void)d;
  (void)c;
  (void)n;
}

void dump_error_declaration(Dump_Out *d, Parse_Context *c, Error_Declaration *n) {
  (void)d;
  (void)c;
  (void)n;
}

void dump_union_declaration(Dump_Out *d, Parse_Context *c, Union_Declaration *n) {
  (void)d;
  (void)c;
  (void)n;
}

void dump_alias_declaration(Dump_Out *d, Parse_Context *c, Alias_Declaration *n) {
  (void)d;
  (void)c;
  (void)n;
}

void dump_use_declaration(Dump_Out *d, Parse_Context *c, Use_Declaration *n) {
  (void)d;
  (void)c;
  (void)n;
}

void dump_variable_list(Dump_Out *d, Parse_Context *c, Variable_List *n) {
  for (u32 i = 0; i < n->len; i++) {
    D(variable_binding, n->items[i]);
    if (i != n->len - 1) {
      dump_rawf(d, "\n");
    }
  }
}

void dump_variable_binding(Dump_Out *d, Parse_Context *c, Variable_Binding *n) {
  dumpf(d, "name: ");
  d->is_field = true;
  dump_tok(d, c, n->identifier);
  d->is_field = false;
  D(type, n->type);
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
    case TYPE_SCOPED_IDENTIFIER:
      D(scoped_identifier, n->scoped_identifier);
      break;
  }
}

void dump_builtin_type(Dump_Out *d, Parse_Context *c, Builtin_Type *n) {
  (void)d;
  (void)c;
  (void)n;
}

void dump_collection_type(Dump_Out *d, Parse_Context *c, Collection_Type *n) {
  (void)d;
  (void)c;
  (void)n;
}

void dump_struct_type(Dump_Out *d, Parse_Context *c, Struct_Type *n) {
  (void)d;
  (void)c;
  (void)n;
}

void dump_union_type(Dump_Out *d, Parse_Context *c, Union_Type *n) {
  (void)d;
  (void)c;
  (void)n;
}

void dump_enum_type(Dump_Out *d, Parse_Context *c, Enum_Type *n) {
  (void)d;
  (void)c;
  (void)n;
}

void dump_error_type(Dump_Out *d, Parse_Context *c, Error_Type *n) {
  (void)d;
  (void)c;
  (void)n;
}

void dump_pointer_type(Dump_Out *d, Parse_Context *c, Pointer_Type *n) {
  (void)d;
  (void)c;
  (void)n;
}

void dump_scoped_identifier(Dump_Out *d, Parse_Context *c, Scoped_Identifier *n) {
  (void)d;
  (void)c;
  (void)n;
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
// void dump_var_decl(Dump_Out *dmp, Parse_Context *c, Variable_Declaration *vd) {
//   if (c->flags.items[vd->id] & NFLAG_ERROR) {
//     dumpf(dmp, "\"<Variable_Declaration>\"");
//     return;
//   }
//   dumpf(dmp, "{\"kind\": \"variable\", \"name\": \"%.*s\", \"is_mut\": %s",
//         vd->name.text.len, vd->name.text.data, vd->is_mut ? "true" : "false");
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
// void dump_fun_decl(Dump_Out *dmp, Parse_Context *c, Function_Declaration *fd) {
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
// void dump_assign_stmt(Dump_Out *dmp, Parse_Context *c, Assign_Statement *as) {
//   if (c->flags.items[as->id] & NFLAG_ERROR) {
//     dumpf(dmp, "\"<Assignment_Statement>\"");
//     return;
//   }
//   dumpf(dmp, "{\"kind\": \"assign\", \"lhs\": %.*s, \"rhs\":", as->lhs.text.len, as->lhs.text.data);
//   DUMP(dmp, c, as->rhs);
//   dumpf(dmp, "}");
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
