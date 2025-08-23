#include <assert.h>
#include <stdarg.h>

#include "syn.h"

void dumpf(Dump_Out *dmp, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(dmp->fs, fmt, args);
  va_end(args);
}

Dump_Out new_dump_out(void) {
  return (Dump_Out){
      .fs = stdout,
      .indent = 0,
  };
}

// For debug and testing purposes
void dump_decl(Dump_Out *dmp, Parse_Context *c, Declaration *d) {
  if (c->flags.items[d->id] & NFLAG_ERROR) {
    dumpf(dmp, "\"<Declaration>\"");
    return;
  }
  switch (d->t) {
    case DECL_VAR:
      DUMP(dmp, c, d->var);
      break;
    case DECL_FUN:
      DUMP(dmp, c, d->fun);
      break;
  }
}

void dump_var_decl(Dump_Out *dmp, Parse_Context *c, Variable_Declaration *vd) {
  if (c->flags.items[vd->id] & NFLAG_ERROR) {
    dumpf(dmp, "\"<Variable_Declaration>\"");
    return;
  }
  dumpf(dmp, "{\"kind\": \"variable\", \"name\": \"%.*s\", \"is_mut\": %s",
        vd->name.text.len, vd->name.text.data, vd->is_mut ? "true" : "false");
  if (vd->type || vd->init) {
    dumpf(dmp, ", ");
  }
  if (vd->type) {
    dumpf(dmp, "\"type\": ");
    DUMP(dmp, c, vd->type);
  }
  if (vd->init) {
    // Fuck JSON not supporting trailing comma!
    if (vd->type) {
      dumpf(dmp, ", ");
    }
    dumpf(dmp, "\"value\": ");
    DUMP(dmp, c, vd->init);
  }
  dumpf(dmp, "}");
}

void dump_type(Dump_Out *dmp, Parse_Context *c, Type *t) {
  if (c->flags.items[t->id] & NFLAG_ERROR) {
    dumpf(dmp, "\"<Type>\"");
    return;
  }
  assert(t->t == TYPE_BUILTIN);
  switch (t->builtin) {
    case BUILTIN_S32:
      dumpf(dmp, "\"s32\"");
      break;
    default:
      assert(false && "TODO");
  }
}

void dump_fun_decl(Dump_Out *dmp, Parse_Context *c, Function_Declaration *fd) {
  if (c->flags.items[fd->id] & NFLAG_ERROR) {
    dumpf(dmp, "\"<Function>\"");
    return;
  }
  dumpf(dmp, "{\"kind\": \"function\", \"name\": \"%.*s\", \"args\": ",
        fd->name.text.len, fd->name.text.data);
  DUMP(dmp, c, fd->args);
  if (fd->return_type) {
    dumpf(dmp, ", \"returns\": ");
    DUMP(dmp, c, fd->return_type);
  }
  dumpf(dmp, ", \"body\": ");
  DUMP(dmp, c, fd->body);
  dumpf(dmp, "}");
}

void dump_arg_list(Dump_Out *dmp, Parse_Context *c, Argument_List *al) {
  if (c->flags.items[al->id] & NFLAG_ERROR) {
    dumpf(dmp, "\"<Argument_List>\"");
    return;
  }
  dumpf(dmp, "[");
  for (u32 i = 0; i < al->len; i++) {
    if (i != 0) {
      dumpf(dmp, ", ");
    }
    DUMP(dmp, c, &al->items[i]);
  }
  dumpf(dmp, "]");
}

void dump_arg(Dump_Out *dmp, Parse_Context *c, Argument *a) {
  if (c->flags.items[a->id] & NFLAG_ERROR) {
    dumpf(dmp, "\"<Arg>\"");
    return;
  }
  dumpf(dmp, "{\"kind\": \"argument\", \"name\": \"%.*s\", \"type\": ",
        a->name.text.len, a->name.text.data);
  DUMP(dmp, c, a->type);
  dumpf(dmp, "}");
}

void dump_stmt(Dump_Out *dmp, Parse_Context *c, Statement *st) {
  if (c->flags.items[st->id] & NFLAG_ERROR) {
    dumpf(dmp, "\"<Statement>\"");
    return;
  }
  switch (st->t) {
    case STMT_DECL:
      DUMP(dmp, c, st->decl);
      break;
    case STMT_COMP:
      DUMP(dmp, c, st->compound);
      break;
    default:
      assert(false && "TODO");
  }
}

void dump_compound_stmt(Dump_Out *dmp, Parse_Context *c,
                        Compound_Statement *cs) {
  if (c->flags.items[cs->id] & NFLAG_ERROR) {
    dumpf(dmp, "\"<Compound_Statement>\"");
    return;
  }
  dumpf(dmp, "{\"kind\": \"compound\", \"statements\": [");
  DUMP(dmp, c, cs->statements);
  dumpf(dmp, "]}");
}

void dump_stmt_list(Dump_Out *dmp, Parse_Context *c, Statement_List *sl) {
  if (c->flags.items[sl->id] & NFLAG_ERROR) {
    dumpf(dmp, "\"<Statement_List>\"");
    return;
  }
  for (u32 i = 0; i < sl->len; i++) {
    if (i != 0) {
      dumpf(dmp, ", ");
    }
    Statement *stmt = &sl->items[i];
    DUMP(dmp, c, stmt);
  }
}

void dump_expr(Dump_Out *dmp, Parse_Context *c, Expr *e) {
  if (c->flags.items[e->id] & NFLAG_ERROR) {
    dumpf(dmp, "\"<Expr>\"");
    return;
  }
  assert(e->t == EXPR_LIT);
  assert(e->literal.t == T_NUM);
  dumpf(dmp, "%ld", e->literal.ival);
}
