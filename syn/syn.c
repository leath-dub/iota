#include "syn.h"

// TODO: add IR that populates child parent node metadata.
// TODO: add delimiter tokens as part of the parsing context

#include <assert.h>
#include <string.h>

#include "../ast/ast.h"
#include "../lex/lex.h"

typedef struct {
  void *node;
  NodeID parent;
  bool root;
} NodeCtx;

static void *make_node(ParseCtx *c, NodeKind kind) {
  return new_node(&c->meta, &c->arena, kind);
}

static void *attr(ParseCtx *c, const char *name, void *node) {
  NodeChild *child = last_child(&c->meta, c->current);
  child->name.ok = true;
  child->name.value = name;
  return node;
}

static Tok token_attr(ParseCtx *c, const char *name, Tok tok) {
  add_child(&c->meta, c->current, child_token_named(name, tok));
  return tok;
}

static Tok token_attr_anon(ParseCtx *c, Tok tok) {
  add_child(&c->meta, c->current, child_token(tok));
  return tok;
}

static NodeCtx begin_node(ParseCtx *c, void *node) {
  NodeCtx ctx = {
      .root = false,
      .node = node,
      .parent = c->current,
  };
  c->current = *(NodeID *)ctx.node;
  return ctx;
}

static NodeCtx start_node(ParseCtx *c, NodeKind kind) {
  return begin_node(c, make_node(c, kind));
}

static void set_current_node(ParseCtx *c, NodeID id) { c->current = id; }

static void *end_node(ParseCtx *c, NodeCtx ctx) {
  assert(*(NodeID *)ctx.node == c->current);
  if (!ctx.root) {
    add_child(&c->meta, ctx.parent, child_node(c->current));
    set_current_node(c, ctx.parent);
  } else {
    assert(ctx.parent == c->current);
  }
  return ctx.node;
}

ParseCtx new_parse_ctx(SourceCode code) {
  return (ParseCtx){
      .lex = new_lexer(code),
      .arena = new_arena(),
      .current = 0,
  };
}

void parse_ctx_free(ParseCtx *c) {
  node_metadata_free(&c->meta);
  arena_free(&c->arena);
}

static ParseState set_marker(ParseCtx *c) { return (ParseState){c->lex}; }

static void backtrack(ParseCtx *c, ParseState marker) { c->lex = marker.lex; }

static Tok tnext(ParseCtx *c) {
  Tok tok;
  do {
    lex_consume(&c->lex);
    tok = lex_peek(&c->lex);
  } while (tok.t == T_CMNT);
  return tok;
}

static void next(ParseCtx *c) { (void)tnext(c); }

static Tok consume(ParseCtx *c) {
  Tok tok = lex_peek(&c->lex);
  next(c);
  return tok;
}

Tok at(ParseCtx *c) { return lex_peek(&c->lex); }

/*static*/
bool looking_at(ParseCtx *c, TokKind t) { return at(c).t == t; }

bool one_of(TokKind t, Toks toks) {
  for (u32 i = 0; i < toks.len; i++) {
    if (t == toks.items[i]) {
      return true;
    }
  }
  return false;
}

/*static*/
void advance(ParseCtx *c, Toks toks) {
  for (Tok tok = lex_peek(&c->lex); tok.t != T_EOF; tok = tnext(c)) {
    if (one_of(tok.t, toks)) {
      return;
    }
  }
}

/*static*/
void expected(ParseCtx *c, NodeID in, const char *msg) {
  Tok tok = lex_peek(&c->lex);
  reportf(c->lex.source, c->lex.cursor, "syntax error: expected %s, found %s",
          msg, tok_to_string[tok.t].data);
  add_node_flags(&c->meta, in, NFLAG_ERROR);
}

/*static*/
bool expect(ParseCtx *c, NodeID in, TokKind t) {
  Tok tok = lex_peek(&c->lex);
  bool match = tok.t == t;
  if (!match) {
    expected(c, in, tok_to_string[t].data);
  } else {
    next(c);
  }
  return match;
}

#define DECLARE_FOLLOW_SET(name, ...)                            \
  static const TokKind _FOLLOW_##name##_ITEMS[] = {__VA_ARGS__}; \
  static const Toks FOLLOW_##name = {                            \
      .items = (TokKind *)&_FOLLOW_##name##_ITEMS,               \
      .len = sizeof(_FOLLOW_##name##_ITEMS) / sizeof(TokKind),   \
  };

// TODO: maybe we need to be more dynamic with sync tokens

// These were manually calculated by looking at the tree-sitter grammar.
// I really fucking hope it was worth the effort.
DECLARE_FOLLOW_SET(IMPORT, T_IMPORT, T_LET, T_MUT, T_FUN, T_STRUCT, T_ENUM,
                   T_ERROR, T_UNION, T_TYPE, T_USE)
DECLARE_FOLLOW_SET(DECL, T_LET, T_MUT, T_FUN, T_STRUCT, T_ENUM, T_ERROR,
                   T_UNION)
DECLARE_FOLLOW_SET(VAR_BINDING, T_SCLN, T_EQ)
DECLARE_FOLLOW_SET(BINDING, T_COMMA, T_RPAR)
DECLARE_FOLLOW_SET(ALIAS_BINDING, T_COMMA, T_RBRC)
DECLARE_FOLLOW_SET(TYPE, T_COMMA, T_RPAR, T_SCLN, T_RBRC, T_LBRC, T_EQ)
DECLARE_FOLLOW_SET(IDENTS, T_RBRC)
DECLARE_FOLLOW_SET(ERRS, T_RBRC)
DECLARE_FOLLOW_SET(ERR, T_COMMA, T_RBRC)
DECLARE_FOLLOW_SET(FIELD, T_IDENT, T_RBRC)
DECLARE_FOLLOW_SET(STMTS, T_LBRC)
DECLARE_FOLLOW_SET(STMT, T_LET, T_MUT, T_FUN, T_STRUCT, T_ENUM, T_ERROR,
                   T_UNION, T_LBRC /*, TODO FIRST(expression) */)
DECLARE_FOLLOW_SET(ATOM, T_PLUS, T_MINUS, T_STAR, T_SLASH, T_EQ, T_EQEQ, T_NEQ,
                   T_AND, T_OR, T_RBRK, T_SCLN, T_RBRC)
// TODO: once we have an automated tool
// DECLARE_FOLLOW_SET(EXPRESSION, ?)

SourceFile *source_file(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_SOURCE_FILE);
  nc.root = true;
  SourceFile *n = nc.node;
  n->imports = imports(c);
  n->decls = decls(c);
  return end_node(c, nc);
}

Imports *imports(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_IMPORTS);
  Imports *n = nc.node;
  while (looking_at(c, T_IMPORT)) {
    APPEND(n, import(c));
  }
  arena_own(&c->arena, n->items, n->cap);
  return end_node(c, nc);
}

Import *import(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_IMPORT);
  Import *n = nc.node;
  assert(expect(c, n->id, T_IMPORT));
  if (looking_at(c, T_IDENT)) {
    n->aliased = true;
    n->alias = at(c);
    next(c);
  }
  Tok import_name = at(c);
  if (!expect(c, n->id, T_STR) || !expect(c, n->id, T_SCLN)) {
    advance(c, FOLLOW_IMPORT);
    return end_node(c, nc);
  }
  n->import_name = token_attr(c, "module", import_name);
  return end_node(c, nc);
}

Decls *decls(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_DECLS);
  Decls *n = nc.node;
  while (!looking_at(c, T_EOF)) {
    APPEND(n, decl(c));
  }
  arena_own(&c->arena, n->items, n->cap);
  return end_node(c, nc);
}

Decl *decl(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_DECL);
  Decl *n = nc.node;
  switch (at(c).t) {
    case T_LET:
    case T_MUT:
      n->t = DECL_VAR;
      n->var_decl = var_decl(c);
      break;
    case T_FUN:
      n->t = DECL_FN;
      n->fn_decl = fn_decl(c);
      break;
    case T_STRUCT:
      n->t = DECL_STRUCT;
      n->struct_decl = struct_decl(c);
      break;
    case T_ENUM:
      n->t = DECL_ENUM;
      n->enum_decl = enum_decl(c);
      break;
    case T_ERROR:
      n->t = DECL_ERR;
      n->err_decl = err_decl(c);
      break;
    default:
      expected(c, n->id, "start of declaration");
      advance(c, FOLLOW_DECL);
      break;
  }
  return end_node(c, nc);
}

StructDecl *struct_decl(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_STRUCT_DECL);
  StructDecl *n = nc.node;
  assert(consume(c).t == T_STRUCT);
  Tok name = at(c);
  if (!expect(c, n->id, T_IDENT)) {
    advance(c, FOLLOW_DECL);
    return end_node(c, nc);
  }
  n->ident = name;
  n->body = struct_body(c, FOLLOW_DECL);
  return end_node(c, nc);
}

StructBody *struct_body(ParseCtx *c, Toks follow) {
  NodeCtx nc = start_node(c, NODE_STRUCT_BODY);
  StructBody *n = nc.node;
  switch (at(c).t) {
    case T_LBRC:
      next(c);
      n->tuple_like = false;
      n->fields = fields(c);
      if (!expect(c, n->id, T_RBRC)) {
        advance(c, follow);
        return end_node(c, nc);
      }
      break;
    case T_LPAR:
      next(c);
      n->tuple_like = true;
      n->types = types(c);
      if (!expect(c, n->id, T_RPAR)) {
        advance(c, follow);
        return end_node(c, nc);
      }
      break;
    default:
      expected(c, n->id, "a struct or tuple body");
      advance(c, follow);
      break;
  }
  return end_node(c, nc);
}

EnumDecl *enum_decl(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_ENUM_DECL);
  EnumDecl *n = nc.node;
  assert(consume(c).t == T_ENUM);
  Tok name = at(c);
  if (!expect(c, n->id, T_IDENT)) {
    advance(c, FOLLOW_DECL);
    return end_node(c, nc);
  }
  n->ident = token_attr(c, "name", name);
  if (!expect(c, n->id, T_LBRC)) {
    advance(c, FOLLOW_DECL);
    return end_node(c, nc);
  }
  n->alts = idents(c);
  if (!expect(c, n->id, T_RBRC)) {
    advance(c, FOLLOW_DECL);
    return end_node(c, nc);
  }
  return end_node(c, nc);
}

ErrDecl *err_decl(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_ERR_DECL);
  ErrDecl *n = nc.node;
  assert(consume(c).t == T_ERROR);
  Tok name = at(c);
  if (!expect(c, n->id, T_IDENT)) {
    advance(c, FOLLOW_DECL);
    return end_node(c, nc);
  }
  n->ident = name;
  if (!expect(c, n->id, T_LBRC)) {
    advance(c, FOLLOW_DECL);
    return end_node(c, nc);
  }
  n->errs = errs(c);
  if (!expect(c, n->id, T_RBRC)) {
    advance(c, FOLLOW_DECL);
    return end_node(c, nc);
  }
  return end_node(c, nc);
}

VarDecl *var_decl(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_VAR_DECL);
  VarDecl *n = nc.node;
  Tok classifier = at(c);
  assert(classifier.t == T_MUT || classifier.t == T_LET);
  n->classifier = token_attr(c, "kind", consume(c));
  n->binding = attr(c, "binding", var_binding(c));
  if (!looking_at(c, T_EQ) && !looking_at(c, T_SCLN)) {
    // Must have a type
    n->type = type(c);
  }
  if (looking_at(c, T_EQ)) {
    n->init.ok = true;
    n->init.assign_token = consume(c);
    n->init.expr = attr(c, "value", expr(c, TOKS(T_SCLN)));
  }
  if (!expect(c, n->id, T_SCLN)) {
    advance(c, FOLLOW_DECL);
    return end_node(c, nc);
  }
  return end_node(c, nc);
}

VarBinding *var_binding(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_VAR_BINDING);
  VarBinding *n = nc.node;
  switch (at(c).t) {
    case T_LPAR:
      n->t = VAR_BINDING_UNPACK_TUPLE;
      n->unpack_tuple = unpack_tuple(c, FOLLOW_VAR_BINDING);
      break;
    case T_LBRC:
      n->t = VAR_BINDING_UNPACK_STRUCT;
      n->unpack_struct = unpack_struct(c, FOLLOW_VAR_BINDING);
      break;
    case T_IDENT: {
      Tok name = consume(c);
      if (looking_at(c, T_LPAR)) {
        next(c);
        n->t = VAR_BINDING_UNPACK_UNION;
        {
          NodeCtx destr_ctx = start_node(c, NODE_UNPACK_UNION);
          n->unpack_union = destr_ctx.node;
          n->unpack_union->tag = token_attr(c, "tag", name);
          n->unpack_union->binding = binding(c);
          (void)end_node(c, destr_ctx);
        }
        if (!expect(c, n->id, T_RPAR)) {
          advance(c, FOLLOW_VAR_BINDING);
          return end_node(c, nc);
        }
        return end_node(c, nc);
      }
      n->t = VAR_BINDING_BASIC;
      n->basic = token_attr(c, "name", name);
      break;
    }
    default:
      expected(c, n->id, "a variable binding");
      advance(c, FOLLOW_VAR_BINDING);
      break;
  }
  return end_node(c, nc);
}

Binding *binding(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_BINDING);
  Binding *n = nc.node;
  if (looking_at(c, T_STAR)) {
    n->ref = token_attr(c, "kind", consume(c));
  }
  Tok name = at(c);
  if (!expect(c, n->id, T_IDENT)) {
    advance(c, FOLLOW_BINDING);
    return end_node(c, nc);
  }
  n->ident = token_attr(c, "name", name);
  return end_node(c, nc);
}

AliasBinding *alias_binding(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_ALIAS_BINDING);
  AliasBinding *n = nc.node;
  n->binding = binding(c);
  if (looking_at(c, T_EQ)) {
    next(c);
    Tok name = at(c);
    if (!expect(c, n->id, T_IDENT)) {
      advance(c, FOLLOW_ALIAS_BINDING);
      return end_node(c, nc);
    }
    n->alias.ok = true;
    n->alias.value = token_attr(c, "alias", name);
  }
  return end_node(c, nc);
}

UnpackStruct *unpack_struct(ParseCtx *c, Toks follow) {
  NodeCtx nc = start_node(c, NODE_UNPACK_STRUCT);
  UnpackStruct *n = nc.node;
  if (!expect(c, n->id, T_LBRC)) {
    advance(c, follow);
    return end_node(c, nc);
  }
  n->bindings = alias_bindings(c, T_RBRC);
  if (!expect(c, n->id, T_RBRC)) {
    advance(c, follow);
    return end_node(c, nc);
  }
  return end_node(c, nc);
}

UnpackTuple *unpack_tuple(ParseCtx *c, Toks follow) {
  NodeCtx nc = start_node(c, NODE_UNPACK_TUPLE);
  UnpackTuple *n = nc.node;
  if (!expect(c, n->id, T_LPAR)) {
    advance(c, follow);
    return end_node(c, nc);
  }
  n->bindings = bindings(c, T_RPAR);
  if (!expect(c, n->id, T_RPAR)) {
    advance(c, follow);
    return end_node(c, nc);
  }
  return end_node(c, nc);
}

AliasBindings *alias_bindings(ParseCtx *c, TokKind end) {
  NodeCtx nc = start_node(c, NODE_ALIAS_BINDINGS);
  AliasBindings *n = nc.node;
  APPEND(n, alias_binding(c));
  while (looking_at(c, T_COMMA)) {
    next(c);
    // Ignore trailing comma
    if (looking_at(c, end)) {
      break;
    }
    APPEND(n, alias_binding(c));
  }
  arena_own(&c->arena, n->items, n->cap);
  return end_node(c, nc);
}

Bindings *bindings(ParseCtx *c, TokKind end) {
  NodeCtx nc = start_node(c, NODE_BINDINGS);
  Bindings *n = nc.node;
  APPEND(n, binding(c));
  while (looking_at(c, T_COMMA)) {
    next(c);
    // Ignore trailing comma
    if (looking_at(c, end)) {
      break;
    }
    APPEND(n, binding(c));
  }
  arena_own(&c->arena, n->items, n->cap);
  return end_node(c, nc);
}

FnDecl *fn_decl(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_FN_DECL);
  FnDecl *n = nc.node;
  assert(consume(c).t == T_FUN);
  // TODO type parameters
  Tok name = at(c);
  if (!expect(c, n->id, T_IDENT)) {
    advance(c, FOLLOW_DECL);
    return end_node(c, nc);
  }
  n->ident = token_attr(c, "name", name);
  if (!expect(c, n->id, T_LPAR)) {
    advance(c, FOLLOW_DECL);
    return end_node(c, nc);
  }
  n->params = fn_params(c);
  if (!expect(c, n->id, T_RPAR)) {
    advance(c, FOLLOW_DECL);
    return end_node(c, nc);
  }
  if (!looking_at(c, T_LBRC)) {
    n->return_type.ok = true;
    n->return_type.value = type(c);
  }
  n->body = comp_stmt(c);
  return end_node(c, nc);
}

FnParams *fn_params(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_FN_PARAMS);
  FnParams *n = nc.node;
  if (looking_at(c, T_RPAR)) {
    return end_node(c, nc);
  }
  APPEND(n, fn_param(c));
  while (looking_at(c, T_COMMA)) {
    next(c);
    // Ignore trailing comma
    if (looking_at(c, T_RPAR)) {
      break;
    }
    APPEND(n, fn_param(c));
  }
  arena_own(&c->arena, n->items, n->cap);
  return end_node(c, nc);
}

FnParam *fn_param(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_FN_PARAM);
  FnParam *n = nc.node;
  n->binding = var_binding(c);
  if (looking_at(c, T_DOTDOT)) {
    token_attr(c, "kind", at(c));
    next(c);
    n->variadic = true;
  }
  n->type = type(c);
  return end_node(c, nc);
}

Stmt *stmt(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_STMT);
  Stmt *n = nc.node;
  switch (at(c).t) {
    case T_FUN:
    case T_LET:
    case T_MUT:
    case T_STRUCT:
    case T_ENUM:
    case T_ERROR:
    case T_UNION:
      n->t = STMT_DECL;
      n->decl = decl(c);
      break;
    case T_LBRC:
      n->t = STMT_COMP;
      n->comp_stmt = comp_stmt(c);
      break;
    // case T_IF:
    //   n->t = STATEMENT_IF;
    //   n->if_statement = if
    //   break;
    // case T_WHILE:
    //   break;
    default:
      n->t = STMT_EXPR;
      n->expr = expr(c, TOKS(T_SCLN));
      if (!expect(c, n->id, T_SCLN)) {
        advance(c, FOLLOW_STMT);
        return end_node(c, nc);
      }
      break;
  }
  return end_node(c, nc);
}

CompStmt *comp_stmt(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_COMP_STMT);
  CompStmt *n = nc.node;
  if (!expect(c, n->id, T_LBRC)) {
    advance(c, FOLLOW_STMTS);
    return end_node(c, nc);
  }
  while (!looking_at(c, T_RBRC)) {
    if (looking_at(c, T_EOF)) {
      expected(c, n->id, "a statement");
      advance(c, FOLLOW_STMT);
      break;
    }
    APPEND(n, stmt(c));
  }
  arena_own(&c->arena, n->items, n->cap);
  next(c);
  return end_node(c, nc);
}

static bool starts_type(TokKind t) {
  switch (t) {
    case T_S8:
    case T_U8:
    case T_S16:
    case T_U16:
    case T_S32:
    case T_U32:
    case T_S64:
    case T_U64:
    case T_F32:
    case T_F64:
    case T_BOOL:
    case T_STRING:
    case T_ANY:
    case T_LBRK:
    case T_STRUCT:
    case T_UNION:
    case T_ENUM:
    case T_ERROR:
    case T_STAR:
    case T_FUN:
    case T_IDENT:
      return true;
    default:
      return false;
  }
}

Type *type(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_TYPE);
  Type *n = nc.node;
  switch (at(c).t) {
    case T_S8:
    case T_U8:
    case T_S16:
    case T_U16:
    case T_S32:
    case T_U32:
    case T_S64:
    case T_U64:
    case T_F32:
    case T_F64:
    case T_BOOL:
    case T_STRING:
    case T_ANY: {
      {
        NodeCtx builtin_ctx = start_node(c, NODE_BUILTIN_TYPE);
        BuiltinType *builtin_type = builtin_ctx.node;
        builtin_type->token = token_attr_anon(c, consume(c));
        n->t = TYPE_BUILTIN;
        n->builtin_type = end_node(c, builtin_ctx);
      }
      break;
    }
    case T_LBRK:
      n->t = TYPE_COLL;
      n->coll_type = coll_type(c);
      break;
    case T_STRUCT:
      n->t = TYPE_STRUCT;
      n->struct_type = struct_type(c);
      break;
    case T_UNION:
      n->t = TYPE_UNION;
      n->union_type = union_type(c);
      break;
    case T_ENUM:
      n->t = TYPE_ENUM;
      n->enum_type = enum_type(c);
      break;
    case T_ERROR:
      n->t = TYPE_ERR;
      n->err_type = err_type(c);
      break;
    case T_STAR:
      n->t = TYPE_PTR;
      n->ptr_type = ptr_type(c);
      break;
    case T_FUN:
      n->t = TYPE_FN;
      n->fn_type = fn_type(c);
      break;
    case T_IDENT:
      n->t = TYPE_SCOPED_IDENT;
      n->scoped_ident = scoped_ident(c, FOLLOW_TYPE);
      break;
    default:
      expected(c, n->id, "a type");
      advance(c, FOLLOW_TYPE);
      break;
  }
  return end_node(c, nc);
}

CollType *coll_type(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_COLL_TYPE);
  CollType *n = nc.node;
  assert(consume(c).t == T_LBRK);
  // For empty index, e.g.: []u32
  if (looking_at(c, T_RBRK)) {
    next(c);
    n->element_type = type(c);
    return end_node(c, nc);
  }
  n->index_expr.ok = true;
  n->index_expr.value = expr(c, TOKS(T_RBRK));
  if (!expect(c, n->id, T_RBRK)) {
    advance(c, FOLLOW_TYPE);
    return end_node(c, nc);
  }
  n->element_type = type(c);
  return end_node(c, nc);
}

StructType *struct_type(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_STRUCT_TYPE);
  StructType *n = nc.node;
  assert(consume(c).t == T_STRUCT);
  n->body = struct_body(c, FOLLOW_TYPE);
  return end_node(c, nc);
}

Fields *fields(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_FIELDS);
  Fields *n = nc.node;
  if (looking_at(c, T_RBRC)) {
    return end_node(c, nc);
  }
  APPEND(n, field(c));
  while (looking_at(c, T_COMMA)) {
    next(c);
    // Ignore trailing comma
    if (looking_at(c, T_RBRC)) {
      break;
    }
    APPEND(n, field(c));
  }
  arena_own(&c->arena, n->items, n->cap);
  return end_node(c, nc);
}

Field *field(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_FIELD);
  Field *n = nc.node;
  Tok tok = at(c);
  if (!expect(c, n->id, T_IDENT)) {
    advance(c, FOLLOW_FIELD);
    return end_node(c, nc);
  }
  n->ident = token_attr(c, "name", tok);
  n->type = type(c);
  return end_node(c, nc);
}

Types *types(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_TYPES);
  Types *n = nc.node;
  if (looking_at(c, T_RPAR)) {
    return end_node(c, nc);
  }
  APPEND(n, type(c));
  while (looking_at(c, T_COMMA)) {
    next(c);
    // Ignore trailing comma
    if (looking_at(c, T_RPAR)) {
      break;
    }
    APPEND(n, type(c));
  }
  arena_own(&c->arena, n->items, n->cap);
  return end_node(c, nc);
}

UnionType *union_type(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_UNION_TYPE);
  UnionType *n = nc.node;
  assert(consume(c).t == T_UNION);
  if (!expect(c, n->id, T_LBRC)) {
    advance(c, FOLLOW_TYPE);
    return end_node(c, nc);
  }
  n->fields = fields(c);
  if (!expect(c, n->id, T_RBRC)) {
    advance(c, FOLLOW_TYPE);
    return end_node(c, nc);
  }
  return end_node(c, nc);
}

EnumType *enum_type(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_ENUM_TYPE);
  EnumType *n = nc.node;
  assert(consume(c).t == T_ENUM);
  if (!expect(c, n->id, T_LBRC)) {
    advance(c, FOLLOW_TYPE);
    return end_node(c, nc);
  }
  n->alts = idents(c);
  if (!expect(c, n->id, T_RBRC)) {
    advance(c, FOLLOW_TYPE);
    return end_node(c, nc);
  }
  return end_node(c, nc);
}

ErrType *err_type(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_ERR_TYPE);
  ErrType *n = nc.node;
  assert(consume(c).t == T_ERROR);
  if (!expect(c, n->id, T_LBRC)) {
    advance(c, FOLLOW_TYPE);
    return end_node(c, nc);
  }
  n->errs = errs(c);
  if (!expect(c, n->id, T_RBRC)) {
    advance(c, FOLLOW_TYPE);
    return end_node(c, nc);
  }
  return end_node(c, nc);
}

Idents *idents(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_IDENTS);
  Idents *n = nc.node;
  bool start = true;
  while (!looking_at(c, T_RBRC)) {
    if (!start && !expect(c, n->id, T_COMMA)) {
      advance(c, FOLLOW_IDENTS);
      return end_node(c, nc);
    }
    // Ignore trailing comma
    if (looking_at(c, T_RBRC)) {
      break;
    }
    Tok identifier = at(c);
    if (!expect(c, n->id, T_IDENT)) {
      advance(c, FOLLOW_IDENTS);
      return end_node(c, nc);
    }
    APPEND(n, token_attr_anon(c, identifier));
    start = false;
  }
  arena_own(&c->arena, n->items, n->cap);
  return end_node(c, nc);
}

Errs *errs(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_ERRS);
  Errs *n = nc.node;
  bool start = true;
  while (!looking_at(c, T_RBRC)) {
    if (!start && !expect(c, n->id, T_COMMA)) {
      advance(c, FOLLOW_ERRS);
      return end_node(c, nc);
    }
    // Ignore trailing comma
    if (looking_at(c, T_RBRC)) {
      break;
    }
    APPEND(n, err(c));
    start = false;
  }
  arena_own(&c->arena, n->items, n->cap);
  return end_node(c, nc);
}

Err *err(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_ERR);
  Err *n = nc.node;
  if (looking_at(c, T_BANG)) {
    n->embedded = true;
    token_attr(c, "kind", consume(c));
  }
  n->scoped_ident = scoped_ident(c, FOLLOW_ERR);
  return end_node(c, nc);
}

PtrType *ptr_type(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_PTR_TYPE);
  PtrType *n = nc.node;
  assert(consume(c).t == T_STAR);
  if (looking_at(c, T_MUT)) {
    n->classifier = consume(c);
  }
  n->ref_type = type(c);
  return end_node(c, nc);
}

FnType *fn_type(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_FN_TYPE);
  FnType *n = nc.node;
  assert(consume(c).t == T_FUN);
  if (!expect(c, n->id, T_LPAR)) {
    advance(c, FOLLOW_TYPE);
    return end_node(c, nc);
  }
  n->params = types(c);
  if (!expect(c, n->id, T_RPAR)) {
    advance(c, FOLLOW_TYPE);
    return end_node(c, nc);
  }
  if (!one_of(at(c).t, FOLLOW_TYPE)) {
    n->return_type.ok = true;
    n->return_type.value = type(c);
  }
  return end_node(c, nc);
}

// Some rules that are used in many contexts take a follow set from the
// caller. This improves the error recovery as it has more context of what
// it should be syncing on than just "whatever can follow is really common rule"
ScopedIdent *scoped_ident(ParseCtx *c, Toks follow) {
  NodeCtx nc = start_node(c, NODE_SCOPED_IDENT);
  ScopedIdent *n = nc.node;
  Tok identifier = at(c);
  if (!expect(c, n->id, T_IDENT)) {
    advance(c, follow);
    return end_node(c, nc);
  }
  APPEND(n, token_attr_anon(c, identifier));
  while (looking_at(c, T_DOT)) {
    next(c);
    Tok identifier = at(c);
    if (!expect(c, n->id, T_IDENT)) {
      advance(c, follow);
      return end_node(c, nc);
    }
    APPEND(n, token_attr_anon(c, identifier));
  }
  arena_own(&c->arena, n->items, n->cap);
  return end_node(c, nc);
}

Init *init(ParseCtx *c, TokKind delim) {
  NodeCtx nc = start_node(c, NODE_INIT);
  Init *n = nc.node;
  // No items in list, just return
  if (looking_at(c, delim)) {
    return end_node(c, nc);
  }
  Toks follow_item = TOKS(T_COMMA, delim);
  APPEND(n, expr(c, follow_item));
  while (looking_at(c, T_COMMA)) {
    next(c);
    // Ignore trailing comma
    if (looking_at(c, delim)) {
      break;
    }
    APPEND(n, expr(c, follow_item));
  }
  arena_own(&c->arena, n->items, n->cap);
  return end_node(c, nc);
}

// Helpful resource for pratt parser:
// https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html#Pratt-parsing-the-general-shape

static bool is_prefix_op(TokKind t) {
  switch (t) {
    case T_AMP:
    case T_INC:
    case T_DEC:
    case T_STAR:
    case T_MINUS:
      return true;
    default:
      return false;
  }
}

static bool is_infix_op(TokKind t) {
  switch (t) {
    case T_PLUS:
    case T_MINUS:
    case T_PERC:
    case T_STAR:
    case T_SLASH:
    case T_EQ:
    case T_AND:
    case T_OR:
    case T_AMP:
    case T_PIPE:
    case T_NEQ:
    case T_EQEQ:
      return true;
    default:
      return false;
  }
}

static bool is_postfix_op(TokKind t) {
  switch (t) {
    case T_DOT:
    case T_LPAR:
    case T_LBRK:
    case T_INC:
    case T_DEC:
      return true;
    default:
      return false;
  }
}

typedef struct {
  u32 left;
  u32 right;
} Power;

typedef enum {
  PREC_POSTFIX,
  PREC_UNARY,
  PREC_MULTIPLICATIVE,
  PREC_ADDITIVE,
  PREC_SHIFT,
  PREC_RELATIONAL,
  PREC_EQUALITY,
  PREC_BAND,
  PREC_BXOR,
  PREC_BOR,
  PREC_AND,
  PREC_OR,
  PREC_ASSIGNMENT,
  PREC_COUNT,
} Precendence_Group;

static const Power binding_power_of[PREC_COUNT] = {
    [PREC_POSTFIX] = {23, 24},
    [PREC_UNARY] = {22, 21},
    [PREC_MULTIPLICATIVE] = {19, 20},
    [PREC_ADDITIVE] = {17, 18},
    [PREC_SHIFT] = {15, 16},
    [PREC_RELATIONAL] = {13, 14},
    [PREC_EQUALITY] = {11, 12},
    [PREC_BAND] = {9, 10},
    [PREC_BOR] = {7, 8},
    [PREC_AND] = {5, 6},
    [PREC_OR] = {3, 4},
    [PREC_ASSIGNMENT] = {2, 1},
};

static Index *index(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_INDEX);
  Index *n = nc.node;
  assert(consume(c).t == T_LBRK);

  n->t = INDEX_SINGLE;

  // e.g. [:10]
  if (looking_at(c, T_CLN)) {
    n->t = INDEX_RANGE;
    token_attr_anon(c, consume(c));
    if (!looking_at(c, T_RBRK)) {
      // Although it is not technically correct to have
      // ';' as a "valid" delimiter, it is used here to prevent
      // spurious errors caused by unmatched '['
      n->end.value = expr(c, TOKS(T_RBRK, T_SCLN));
      n->end.ok = true;
    }
    goto delim;
  }

  n->start.value = expr(c, TOKS(T_RBRK, T_CLN, T_SCLN));
  n->start.ok = true;

  if (looking_at(c, T_CLN)) {
    n->t = INDEX_RANGE;
    token_attr_anon(c, consume(c));
    if (!looking_at(c, T_RBRK)) {
      n->end.value = expr(c, TOKS(T_RBRK, T_SCLN));
      n->end.ok = true;
    }
    goto delim;
  }

delim:
  if (!expect(c, n->id, T_RBRK)) {
    // TODO: change to FOLLOW_EXPRESSION
    advance(c, FOLLOW_ATOM);
  }

  return end_node(c, nc);
}

static NodeCtx parse_postfix(ParseCtx *c, Expr *lhs) {
  NodeCtx expr_ctx = start_node(c, NODE_EXPR);
  Expr *expr = expr_ctx.node;

  switch (at(c).t) {
    case T_LBRK: {
      NodeCtx access_ctx = start_node(c, NODE_COLL_ACCESS);
      CollAccess *access = access_ctx.node;

      NodeCtx lhs_ctx = begin_node(c, lhs);
      access->lvalue = end_node(c, lhs_ctx);
      access->index = index(c);

      expr->t = EXPR_COLL_ACCESS;
      expr->coll_access = end_node(c, access_ctx);
      break;
    }
    case T_LPAR: {
      NodeCtx call_ctx = start_node(c, NODE_FN_CALL);
      FnCall *call = call_ctx.node;

      NodeCtx lhs_ctx = begin_node(c, lhs);
      call->lvalue = end_node(c, lhs_ctx);

      if (!expect(c, call->id, T_LPAR)) {
        advance(c, FOLLOW_ATOM);
        goto yield_call_expr;
      }

      call->args = attr(c, "args", init(c, T_RPAR));

      if (!expect(c, call->id, T_RPAR)) {
        advance(c, FOLLOW_ATOM);
      }

    yield_call_expr:
      expr->t = EXPR_FN_CALL;
      expr->fn_call = end_node(c, call_ctx);
      break;
    }
    case T_DOT: {
      NodeCtx access_ctx = start_node(c, NODE_FIELD_ACCESS);
      FieldAccess *access = access_ctx.node;

      NodeCtx lhs_ctx = begin_node(c, lhs);
      access->lvalue = attr(c, "lvalue", end_node(c, lhs_ctx));

      next(c);
      Tok field = at(c);
      if (!expect(c, access->id, T_IDENT)) {
        advance(c, FOLLOW_ATOM);
        goto yield_access_expr;
      }
      access->field = token_attr(c, "field", field);

    yield_access_expr:
      expr->t = EXPR_FIELD_ACCESS;
      expr->field_access = end_node(c, access_ctx);
      break;
    }
    default: {
      NodeCtx postfix_ctx = start_node(c, NODE_POSTFIX_EXPR);
      PostfixExpr *postfix = postfix_ctx.node;

      postfix->op = token_attr(c, "op", consume(c));
      NodeCtx lhs_ctx = begin_node(c, lhs);
      postfix->sub_expr = end_node(c, lhs_ctx);

      expr->t = EXPR_POSTFIX;
      expr->postfix_expr = end_node(c, postfix_ctx);
    }
  }

  return expr_ctx;
}

typedef struct {
  Power pow;
  bool ok;
} Maybe_Power;

static Maybe_Power infix_bpow(TokKind op) {
  switch (op) {
    case T_PLUS:
    case T_MINUS:
      return (Maybe_Power){binding_power_of[PREC_ADDITIVE], true};
    case T_PERC:
    case T_STAR:
    case T_SLASH:
      return (Maybe_Power){binding_power_of[PREC_MULTIPLICATIVE], true};
    case T_EQ:
      return (Maybe_Power){binding_power_of[PREC_ASSIGNMENT], true};
    case T_AND:
      return (Maybe_Power){binding_power_of[PREC_AND], true};
    case T_OR:
      return (Maybe_Power){binding_power_of[PREC_OR], true};
    case T_AMP:
      return (Maybe_Power){binding_power_of[PREC_BAND], true};
    case T_PIPE:
      return (Maybe_Power){binding_power_of[PREC_BOR], true};
    case T_NEQ:
    case T_EQEQ:
      return (Maybe_Power){binding_power_of[PREC_EQUALITY], true};
    default:
      return (Maybe_Power){.ok = false};
  }
}

static Maybe_Power prefix_bpow(TokKind op) {
  switch (op) {
    case T_AMP:
    case T_INC:
    case T_DEC:
    case T_STAR:
    case T_MINUS:
      return (Maybe_Power){binding_power_of[PREC_UNARY], true};
    default:
      return (Maybe_Power){.ok = false};
  }
}

static Maybe_Power postfix_bpow(TokKind op) {
  switch (op) {
    case T_DOT:
    case T_LPAR:
    case T_LBRK:
    case T_INC:
    case T_DEC:
      return (Maybe_Power){binding_power_of[PREC_POSTFIX], true};
    default:
      return (Maybe_Power){.ok = false};
      assert(false && "TODO");
  }
}

static NodeCtx expr_with_bpow(ParseCtx *c, u32 min_pow, Toks delim) {
  NodeCtx lhs_ctx = start_node(c, NODE_EXPR);
  Expr *lhs = lhs_ctx.node;

  Tok tok = at(c);

  if (is_prefix_op(tok.t)) {
    NodeCtx unary_ctx = start_node(c, NODE_UNARY_EXPR);
    UnaryExpr *unary_expr = unary_ctx.node;

    Power pow;
    Maybe_Power maybe_pow = prefix_bpow(tok.t);
    if (!maybe_pow.ok) {
      expected(c, unary_expr->id, "valid prefix operator");
      pow = (Power){0, 0};
    } else {
      pow = maybe_pow.pow;
    }

    unary_expr->op = token_attr(c, "op", consume(c));
    unary_expr->sub_expr = end_node(c, expr_with_bpow(c, pow.right, delim));

    lhs->t = EXPR_UNARY;
    lhs->unary_expr = end_node(c, unary_ctx);
  } else {
    if (tok.t == T_LPAR) {
      next(c);
      Toks new_delim;
      new_delim.len = delim.len;
      new_delim.items = malloc((delim.len + 1) * sizeof(TokKind));
      memcpy(new_delim.items, delim.items, delim.len * sizeof(TokKind));
      if (!one_of(T_RPAR, delim)) {
        new_delim.len = delim.len + 1;
        new_delim.items[delim.len] = T_RPAR;
      }

      // TODO: this effectively leaks an expression allocated in the arena
      // maybe we can just free the node here before recursing
      set_current_node(c, lhs_ctx.parent);
      lhs_ctx = expr_with_bpow(c, 0, new_delim);
      lhs = lhs_ctx.node;
      free(new_delim.items);

      if (!expect(c, lhs->id, T_RPAR)) {
        advance(c, delim);
        return lhs_ctx;
      }
      if (one_of(at(c).t, delim)) {
        return lhs_ctx;
      }
    } else {
      lhs->t = EXPR_ATOM;
      lhs->atom = atom(c);
    }
  }

  while (true) {
    Tok op = at(c);
    // TODO: do we need explicit delimiters or should we just return if we don't
    // see a infix or postfix operator?
    if (one_of(op.t, delim)) {
      break;
    }
    if (!is_infix_op(op.t) && !is_postfix_op(op.t)) {
      expected(c, lhs->id, "an infix or postfix operator");
      advance(c, FOLLOW_ATOM);
    }

    Maybe_Power maybe_pow = postfix_bpow(op.t);
    if (maybe_pow.ok) {
      Power pow = maybe_pow.pow;
      if (pow.left < min_pow) {
        break;
      }
      // Restore to the old parent, as the sub expression will now be its
      // child and lhs will be a child of the sub expression.
      set_current_node(c, lhs_ctx.parent);
      lhs_ctx = parse_postfix(c, lhs);
      lhs = lhs_ctx.node;
      continue;
    }

    maybe_pow = infix_bpow(op.t);
    if (!maybe_pow.ok) {
      break;
    }
    Power pow = maybe_pow.pow;
    if (pow.left < min_pow) {
      break;
    }

    // Really sorry if someone is coming here to understand this code.
    // There is a bit of gymnastics here to make sure the implicit child-parent
    // relationships maintained by the NodeCtx system are correct.
    //
    // The below code needs to make sure that the current lhs is not added as
    // a child of the current parent (stored in lhs_ctx.parent). It should
    // instead be added as a child of a new node which should become the new
    // descendant of current parent.
    set_current_node(c, lhs_ctx.parent);

    NodeCtx new_expr_ctx = start_node(c, NODE_EXPR);
    Expr *new_expr = new_expr_ctx.node;

    NodeCtx bin_ctx = start_node(c, NODE_BIN_EXPR);
    BinExpr *bin_expr = bin_ctx.node;

    bin_expr->op = token_attr(c, "op", op);
    NodeCtx old_lhs_ctx = begin_node(c, lhs);
    bin_expr->left = attr(c, "left", end_node(c, old_lhs_ctx));
    next(c);
    bin_expr->right =
        attr(c, "right", end_node(c, expr_with_bpow(c, pow.right, delim)));

    new_expr->t = EXPR_BIN;
    new_expr->bin_expr = end_node(c, bin_ctx);

    lhs = new_expr;
    lhs_ctx = new_expr_ctx;
  }

  return lhs_ctx;
}

Expr *expr(ParseCtx *c, Toks delim) {
  return end_node(c, expr_with_bpow(c, 0, delim));
}

Atom *atom(ParseCtx *c) {
  NodeCtx nc = start_node(c, NODE_ATOM);
  Atom *n = nc.node;
  Tok first = at(c);
  if (starts_type(at(c).t)) {
    NodeCtx lit_ctx = start_node(c, NODE_BRACED_LIT);
    BracedLit *braced_lit = lit_ctx.node;

    braced_lit->type.ok = true;
    ParseState marker = set_marker(c);
    braced_lit->type.value = type(c);

    if (first.t == T_IDENT && !looking_at(c, T_LBRC)) {
      // The first token was an identifier and we don't have
      // a '{'. This means that the identifier should be just
      // the token itself as the basic expression
      backtrack(c, marker);
      n->t = ATOM_TOKEN;
      c->current = n->id;
      n->token = token_attr_anon(c, consume(c));
      return end_node(c, nc);
    }

    if (!expect(c, braced_lit->id, T_LBRC)) {
      advance(c, FOLLOW_ATOM);
      n->t = ATOM_BRACED_LIT;
      n->braced_lit = end_node(c, lit_ctx);
      return end_node(c, nc);
    }

    braced_lit->init = init(c, T_RBRC);
    if (!expect(c, braced_lit->id, T_RBRC)) {
      advance(c, FOLLOW_ATOM);
    }

    n->t = ATOM_BRACED_LIT;
    n->braced_lit = end_node(c, lit_ctx);
    return end_node(c, nc);
  }
  switch (at(c).t) {
    case T_CONS: {
      next(c);

      NodeCtx lit_ctx = start_node(c, NODE_BRACED_LIT);
      BracedLit *braced_lit = lit_ctx.node;

      if (!expect(c, braced_lit->id, T_LPAR)) {
        advance(c, FOLLOW_ATOM);
        goto yield_braced_lit;
      }

      braced_lit->type.ok = true;
      braced_lit->type.value = type(c);

      if (!expect(c, braced_lit->id, T_RPAR)) {
        advance(c, FOLLOW_ATOM);
        goto yield_braced_lit;
      }

      if (!expect(c, braced_lit->id, T_LBRC)) {
        advance(c, FOLLOW_ATOM);
        goto yield_braced_lit;
      }

      braced_lit->init = init(c, T_RBRC);

      if (!expect(c, braced_lit->id, T_RBRC)) {
        advance(c, FOLLOW_ATOM);
        goto yield_braced_lit;
      }

    yield_braced_lit:
      n->t = ATOM_BRACED_LIT;
      n->braced_lit = end_node(c, lit_ctx);
      return end_node(c, nc);
    }
    case T_LBRC: {
      next(c);

      NodeCtx lit_ctx = start_node(c, NODE_BRACED_LIT);
      BracedLit *braced_lit = lit_ctx.node;

      braced_lit->init = init(c, T_RBRC);
      n->t = ATOM_BRACED_LIT;
      n->braced_lit = end_node(c, lit_ctx);

      if (!expect(c, n->id, T_RBRC)) {
        advance(c, FOLLOW_ATOM);
      }

      return end_node(c, nc);
    }
    case T_NUM:
    case T_CHAR:
    case T_STR:
      n->t = ATOM_TOKEN;
      n->token = token_attr_anon(c, consume(c));
      return end_node(c, nc);
    default:
      break;
  }

  expected(c, n->id, "an atom");
  advance(c, FOLLOW_ATOM);
  return end_node(c, nc);
}
