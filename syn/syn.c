#include "syn.h"

// TODO: add IR that populates child parent node metadata.
// TODO: add delimiter tokens as part of the parsing context

#include <assert.h>
#include <string.h>

#include "../ast/ast.h"
#include "../lex/lex.h"

typedef struct {
  void *node;
  Node_ID parent;
  bool root;
} Node_Context;

static void *make_node(Parse_Context *c, Node_Kind kind) {
  return new_node(&c->meta, &c->arena, kind);
}

static void *attr(Parse_Context *c, const char *name, void *node) {
  Node_Child *child = last_child(&c->meta, c->current);
  child->name.ok = true;
  child->name.value = name;
  return node;
}

static Tok token_attr(Parse_Context *c, const char *name, Tok tok) {
  add_child(&c->meta, c->current, child_token_named(name, tok));
  return tok;
}

static Tok token_attr_anon(Parse_Context *c, Tok tok) {
  add_child(&c->meta, c->current, child_token(tok));
  return tok;
}

static Node_Context begin_node(Parse_Context *c, void *node) {
  Node_Context ctx = {
      .root = false,
      .node = node,
      .parent = c->current,
  };
  c->current = *(Node_ID *)ctx.node;
  return ctx;
}

static Node_Context start_node(Parse_Context *c, Node_Kind kind) {
  return begin_node(c, make_node(c, kind));
}

static void set_current_node(Parse_Context *c, Node_ID id) { c->current = id; }

static void *end_node(Parse_Context *c, Node_Context ctx) {
  assert(*(Node_ID *)ctx.node == c->current);
  if (!ctx.root) {
    add_child(&c->meta, ctx.parent, child_node(c->current));
    set_current_node(c, ctx.parent);
  } else {
    assert(ctx.parent == c->current);
  }
  return ctx.node;
}

Parse_Context new_parse_context(Source_Code code) {
  return (Parse_Context){
      .lex = new_lexer(code),
      .arena = new_arena(),
      .current = 0,
  };
}

void parse_context_free(Parse_Context *c) {
  node_metadata_free(&c->meta);
  arena_free(&c->arena);
}

static Parse_State set_marker(Parse_Context *c) {
  return (Parse_State){c->lex};
}

static void backtrack(Parse_Context *c, Parse_State marker) {
  c->lex = marker.lex;
}

static Tok tnext(Parse_Context *c) {
  Tok tok;
  do {
    lex_consume(&c->lex);
    tok = lex_peek(&c->lex);
  } while (tok.t == T_CMNT);
  return tok;
}

static void next(Parse_Context *c) { (void)tnext(c); }

static Tok consume(Parse_Context *c) {
  Tok tok = lex_peek(&c->lex);
  next(c);
  return tok;
}

Tok at(Parse_Context *c) { return lex_peek(&c->lex); }

/*static*/
bool looking_at(Parse_Context *c, Tok_Kind t) { return at(c).t == t; }

bool one_of(Tok_Kind t, Toks toks) {
  for (u32 i = 0; i < toks.len; i++) {
    if (t == toks.items[i]) {
      return true;
    }
  }
  return false;
}

/*static*/
void advance(Parse_Context *c, Toks toks) {
  for (Tok tok = lex_peek(&c->lex); tok.t != T_EOF; tok = tnext(c)) {
    if (one_of(tok.t, toks)) {
      return;
    }
  }
}

/*static*/
void expected(Parse_Context *c, Node_ID in, const char *msg) {
  Tok tok = lex_peek(&c->lex);
  reportf(c->lex.source, c->lex.cursor, "syntax error: expected %s, found %s",
          msg, tok_to_string[tok.t].data);
  add_node_flags(&c->meta, in, NFLAG_ERROR);
}

/*static*/
bool expect(Parse_Context *c, Node_ID in, Tok_Kind t) {
  Tok tok = lex_peek(&c->lex);
  bool match = tok.t == t;
  if (!match) {
    expected(c, in, tok_to_string[t].data);
  } else {
    next(c);
  }
  return match;
}

#define DECLARE_FOLLOW_SET(name, ...)                             \
  static const Tok_Kind _FOLLOW_##name##_ITEMS[] = {__VA_ARGS__}; \
  static const Toks FOLLOW_##name = {                             \
      .items = (Tok_Kind *)&_FOLLOW_##name##_ITEMS,               \
      .len = sizeof(_FOLLOW_##name##_ITEMS) / sizeof(Tok_Kind),   \
  };

// TODO: maybe we need to be more dynamic with sync tokens

// These were manually calculated by looking at the tree-sitter grammar.
// I really fucking hope it was worth the effort.
DECLARE_FOLLOW_SET(IMPORT, T_IMPORT, T_LET, T_MUT, T_FUN, T_STRUCT, T_ENUM,
                   T_ERROR, T_UNION, T_TYPE, T_USE)
DECLARE_FOLLOW_SET(DECLARATION, T_LET, T_MUT, T_FUN, T_STRUCT, T_ENUM, T_ERROR,
                   T_UNION)
DECLARE_FOLLOW_SET(VARIABLE_BINDING, T_SCLN, T_EQ)
DECLARE_FOLLOW_SET(BINDING, T_COMMA, T_RPAR)
DECLARE_FOLLOW_SET(ALIASED_BINDING, T_COMMA, T_RBRC)
DECLARE_FOLLOW_SET(TYPE, T_COMMA, T_RPAR, T_SCLN, T_RBRC, T_LBRC, T_EQ)
DECLARE_FOLLOW_SET(IDENTIFIER_LIST, T_RBRC)
DECLARE_FOLLOW_SET(ERROR_LIST, T_RBRC)
DECLARE_FOLLOW_SET(ERROR, T_COMMA, T_RBRC)
DECLARE_FOLLOW_SET(FIELD, T_IDENT, T_RBRC)
DECLARE_FOLLOW_SET(STATEMENT_LIST, T_LBRC)
DECLARE_FOLLOW_SET(STATEMENT, T_LET, T_MUT, T_FUN, T_STRUCT, T_ENUM, T_ERROR,
                   T_UNION, T_LBRC /*, TODO FIRST(expression) */)
DECLARE_FOLLOW_SET(BASIC_EXPRESSION, T_PLUS, T_MINUS, T_STAR, T_SLASH, T_EQ,
                   T_EQEQ, T_NEQ, T_AND, T_OR, T_RBRK, T_SCLN, T_RBRC)
// TODO: once we have an automated tool
// DECLARE_FOLLOW_SET(EXPRESSION, ?)

Source_File *source_file(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_SOURCE_FILE);
  nc.root = true;
  Source_File *n = nc.node;
  n->imports = imports(c);
  n->declarations = declarations(c);
  return end_node(c, nc);
}

Imports *imports(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_IMPORTS);
  Imports *n = nc.node;
  while (looking_at(c, T_IMPORT)) {
    APPEND(n, import(c));
  }
  arena_own(&c->arena, n->items, n->cap);
  return end_node(c, nc);
}

Import *import(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_IMPORT);
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
  n->import_name = import_name;
  return end_node(c, nc);
}

Declarations *declarations(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_DECLARATIONS);
  Declarations *n = nc.node;
  while (!looking_at(c, T_EOF)) {
    APPEND(n, declaration(c));
  }
  arena_own(&c->arena, n->items, n->cap);
  return end_node(c, nc);
}

Declaration *declaration(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_DECLARATION);
  Declaration *n = nc.node;
  switch (at(c).t) {
    case T_LET:
    case T_MUT:
      n->t = DECLARATION_VARIABLE;
      n->variable_declaration = variable_declaration(c);
      break;
    case T_FUN:
      n->t = DECLARATION_FUNCTION;
      n->function_declaration = function_declaration(c);
      break;
    case T_STRUCT:
      n->t = DECLARATION_STRUCT;
      n->struct_declaration = struct_declaration(c);
      break;
    case T_ENUM:
      n->t = DECLARATION_ENUM;
      n->enum_declaration = enum_declaration(c);
      break;
    case T_ERROR:
      n->t = DECLARATION_ERROR;
      n->error_declaration = error_declaration(c);
      break;
    default:
      expected(c, n->id, "start of declaration");
      advance(c, FOLLOW_DECLARATION);
      break;
  }
  return end_node(c, nc);
}

Struct_Declaration *struct_declaration(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_STRUCT_DECLARATION);
  Struct_Declaration *n = nc.node;
  assert(consume(c).t == T_STRUCT);
  Tok name = at(c);
  if (!expect(c, n->id, T_IDENT)) {
    advance(c, FOLLOW_DECLARATION);
    return end_node(c, nc);
  }
  n->identifier = name;
  n->body = struct_body(c, FOLLOW_DECLARATION);
  return end_node(c, nc);
}

Struct_Body *struct_body(Parse_Context *c, Toks follow) {
  Node_Context nc = start_node(c, NODE_STRUCT_BODY);
  Struct_Body *n = nc.node;
  switch (at(c).t) {
    case T_LBRC:
      next(c);
      n->tuple_like = false;
      n->field_list = field_list(c);
      if (!expect(c, n->id, T_RBRC)) {
        advance(c, follow);
        return end_node(c, nc);
      }
      break;
    case T_LPAR:
      next(c);
      n->tuple_like = true;
      n->type_list = type_list(c);
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

Enum_Declaration *enum_declaration(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_ENUM_DECLARATION);
  Enum_Declaration *n = nc.node;
  assert(consume(c).t == T_ENUM);
  Tok name = at(c);
  if (!expect(c, n->id, T_IDENT)) {
    advance(c, FOLLOW_DECLARATION);
    return end_node(c, nc);
  }
  n->identifier = token_attr(c, "name", name);
  if (!expect(c, n->id, T_LBRC)) {
    advance(c, FOLLOW_DECLARATION);
    return end_node(c, nc);
  }
  n->enumerators = identifier_list(c);
  if (!expect(c, n->id, T_RBRC)) {
    advance(c, FOLLOW_DECLARATION);
    return end_node(c, nc);
  }
  return end_node(c, nc);
}

Error_Declaration *error_declaration(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_ERROR_DECLARATION);
  Error_Declaration *n = nc.node;
  assert(consume(c).t == T_ERROR);
  Tok name = at(c);
  if (!expect(c, n->id, T_IDENT)) {
    advance(c, FOLLOW_DECLARATION);
    return end_node(c, nc);
  }
  n->identifier = name;
  if (!expect(c, n->id, T_LBRC)) {
    advance(c, FOLLOW_DECLARATION);
    return end_node(c, nc);
  }
  n->errors = error_list(c);
  if (!expect(c, n->id, T_RBRC)) {
    advance(c, FOLLOW_DECLARATION);
    return end_node(c, nc);
  }
  return end_node(c, nc);
}

Variable_Declaration *variable_declaration(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_VARIABLE_DECLARATION);
  Variable_Declaration *n = nc.node;
  Tok classifier = at(c);
  assert(classifier.t == T_MUT || classifier.t == T_LET);
  n->classifier = token_attr(c, "kind", consume(c));
  n->binding = attr(c, "binding", variable_binding(c));
  if (!looking_at(c, T_EQ) && !looking_at(c, T_SCLN)) {
    // Must have a type
    n->type = type(c);
  }
  if (looking_at(c, T_EQ)) {
    n->init.ok = true;
    n->init.assign_token = consume(c);
    n->init.expression = attr(c, "value", expression(c, TOKS(T_SCLN)));
  }
  if (!expect(c, n->id, T_SCLN)) {
    advance(c, FOLLOW_DECLARATION);
    return end_node(c, nc);
  }
  return end_node(c, nc);
}

Variable_Binding *variable_binding(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_VARIABLE_BINDING);
  Variable_Binding *n = nc.node;
  switch (at(c).t) {
    case T_LPAR:
      n->t = VARIABLE_BINDING_DESTRUCTURE_TUPLE;
      n->destructure_tuple = destructure_tuple(c, FOLLOW_VARIABLE_BINDING);
      break;
    case T_LBRC:
      n->t = VARIABLE_BINDING_DESTRUCTURE_STRUCT;
      n->destructure_struct = destructure_struct(c, FOLLOW_VARIABLE_BINDING);
      break;
    case T_IDENT: {
      Tok name = consume(c);
      if (looking_at(c, T_LPAR)) {
        next(c);
        n->t = VARIABLE_BINDING_DESTRUCTURE_UNION;
        {
          Node_Context destr_ctx = start_node(c, NODE_DESTRUCTURE_UNION);
          n->destructure_union = destr_ctx.node;
          n->destructure_union->tag = token_attr(c, "tag", name);
          n->destructure_union->binding = binding(c);
          (void)end_node(c, destr_ctx);
        }
        if (!expect(c, n->id, T_RPAR)) {
          advance(c, FOLLOW_VARIABLE_BINDING);
          return end_node(c, nc);
        }
        return end_node(c, nc);
      }
      n->t = VARIABLE_BINDING_BASIC;
      n->basic = token_attr(c, "name", name);
      break;
    }
    default:
      expected(c, n->id, "a variable binding");
      advance(c, FOLLOW_VARIABLE_BINDING);
      break;
  }
  return end_node(c, nc);
}

Binding *binding(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_BINDING);
  Binding *n = nc.node;
  if (looking_at(c, T_STAR)) {
    n->reference = token_attr(c, "kind", consume(c));
  }
  Tok name = at(c);
  if (!expect(c, n->id, T_IDENT)) {
    advance(c, FOLLOW_BINDING);
    return end_node(c, nc);
  }
  n->identifier = token_attr(c, "name", name);
  return end_node(c, nc);
}

Aliased_Binding *aliased_binding(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_ALIASED_BINDING);
  Aliased_Binding *n = nc.node;
  n->binding = binding(c);
  if (looking_at(c, T_EQ)) {
    next(c);
    Tok name = at(c);
    if (!expect(c, n->id, T_IDENT)) {
      advance(c, FOLLOW_ALIASED_BINDING);
      return end_node(c, nc);
    }
    n->alias.ok = true;
    n->alias.value = token_attr(c, "alias", name);
  }
  return end_node(c, nc);
}

Destructure_Struct *destructure_struct(Parse_Context *c, Toks follow) {
  Node_Context nc = start_node(c, NODE_DESTRUCTURE_STRUCT);
  Destructure_Struct *n = nc.node;
  if (!expect(c, n->id, T_LBRC)) {
    advance(c, follow);
    return end_node(c, nc);
  }
  n->bindings = aliased_binding_list(c, T_RBRC);
  if (!expect(c, n->id, T_RBRC)) {
    advance(c, follow);
    return end_node(c, nc);
  }
  return end_node(c, nc);
}

Destructure_Tuple *destructure_tuple(Parse_Context *c, Toks follow) {
  Node_Context nc = start_node(c, NODE_DESTRUCTURE_TUPLE);
  Destructure_Tuple *n = nc.node;
  if (!expect(c, n->id, T_LPAR)) {
    advance(c, follow);
    return end_node(c, nc);
  }
  n->bindings = binding_list(c, T_RPAR);
  if (!expect(c, n->id, T_RPAR)) {
    advance(c, follow);
    return end_node(c, nc);
  }
  return end_node(c, nc);
}

Aliased_Binding_List *aliased_binding_list(Parse_Context *c, Tok_Kind end) {
  Node_Context nc = start_node(c, NODE_ALIASED_BINDING_LIST);
  Aliased_Binding_List *n = nc.node;
  APPEND(n, aliased_binding(c));
  while (looking_at(c, T_COMMA)) {
    next(c);
    // Ignore trailing comma
    if (looking_at(c, end)) {
      break;
    }
    APPEND(n, aliased_binding(c));
  }
  arena_own(&c->arena, n->items, n->cap);
  return end_node(c, nc);
}

Binding_List *binding_list(Parse_Context *c, Tok_Kind end) {
  Node_Context nc = start_node(c, NODE_BINDING_LIST);
  Binding_List *n = nc.node;
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

Function_Declaration *function_declaration(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_FUNCTION_DECLARATION);
  Function_Declaration *n = nc.node;
  assert(consume(c).t == T_FUN);
  // TODO type parameters
  Tok name = at(c);
  if (!expect(c, n->id, T_IDENT)) {
    advance(c, FOLLOW_DECLARATION);
    return end_node(c, nc);
  }
  n->identifier = token_attr(c, "name", name);
  if (!expect(c, n->id, T_LPAR)) {
    advance(c, FOLLOW_DECLARATION);
    return end_node(c, nc);
  }
  n->parameters = function_parameter_list(c);
  if (!expect(c, n->id, T_RPAR)) {
    advance(c, FOLLOW_DECLARATION);
    return end_node(c, nc);
  }
  if (!looking_at(c, T_LBRC)) {
    n->return_type.ok = true;
    n->return_type.value = type(c);
  }
  n->body = compound_statement(c);
  return end_node(c, nc);
}

Function_Parameter_List *function_parameter_list(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_FUNCTION_PARAMETER_LIST);
  Function_Parameter_List *n = nc.node;
  if (looking_at(c, T_RPAR)) {
    return end_node(c, nc);
  }
  APPEND(n, function_parameter(c));
  while (looking_at(c, T_COMMA)) {
    next(c);
    // Ignore trailing comma
    if (looking_at(c, T_RPAR)) {
      break;
    }
    APPEND(n, function_parameter(c));
  }
  arena_own(&c->arena, n->items, n->cap);
  return end_node(c, nc);
}

Function_Parameter *function_parameter(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_FUNCTION_PARAMETER);
  Function_Parameter *n = nc.node;
  n->binding = variable_binding(c);
  if (looking_at(c, T_DOTDOT)) {
    token_attr(c, "kind", at(c));
    next(c);
    n->variadic = true;
  }
  n->type = type(c);
  return end_node(c, nc);
}

Statement *statement(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_STATEMENT);
  Statement *n = nc.node;
  switch (at(c).t) {
    case T_FUN:
    case T_LET:
    case T_MUT:
    case T_STRUCT:
    case T_ENUM:
    case T_ERROR:
    case T_UNION:
      n->t = STATEMENT_DECLARATION;
      n->declaration = declaration(c);
      break;
    case T_LBRC:
      n->t = STATEMENT_COMPOUND;
      n->compound_statement = compound_statement(c);
      break;
    // case T_IF:
    //   n->t = STATEMENT_IF;
    //   n->if_statement = if
    //   break;
    // case T_WHILE:
    //   break;
    default:
      n->t = STATEMENT_EXPRESSION;
      n->expression = expression(c, TOKS(T_SCLN));
      if (!expect(c, n->id, T_SCLN)) {
        advance(c, FOLLOW_STATEMENT);
        return end_node(c, nc);
      }
      break;
  }
  return end_node(c, nc);
}

Compound_Statement *compound_statement(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_COMPOUND_STATEMENT);
  Compound_Statement *n = nc.node;
  if (!expect(c, n->id, T_LBRC)) {
    advance(c, FOLLOW_STATEMENT_LIST);
    return end_node(c, nc);
  }
  while (!looking_at(c, T_RBRC)) {
    if (looking_at(c, T_EOF)) {
      expected(c, n->id, "a statement");
      advance(c, FOLLOW_STATEMENT);
      break;
    }
    APPEND(n, statement(c));
  }
  arena_own(&c->arena, n->items, n->cap);
  next(c);
  return end_node(c, nc);
}

static bool starts_type(Tok_Kind t) {
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

Type *type(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_TYPE);
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
        Node_Context builtin_ctx = start_node(c, NODE_BUILTIN_TYPE);
        Builtin_Type *builtin_type = builtin_ctx.node;
        builtin_type->token = token_attr(c, "name", consume(c));
        n->t = TYPE_BUILTIN;
        n->builtin_type = end_node(c, builtin_ctx);
      }
      break;
    }
    case T_LBRK:
      n->t = TYPE_COLLECTION;
      n->collection_type = collection_type(c);
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
      n->t = TYPE_ERROR;
      n->error_type = error_type(c);
      break;
    case T_STAR:
      n->t = TYPE_POINTER;
      n->pointer_type = pointer_type(c);
      break;
    case T_FUN:
      n->t = TYPE_FUNCTION;
      n->function_type = function_type(c);
      break;
    case T_IDENT:
      n->t = TYPE_SCOPED_IDENTIFIER;
      n->scoped_identifier = scoped_identifier(c, FOLLOW_TYPE);
      break;
    default:
      expected(c, n->id, "a type");
      advance(c, FOLLOW_TYPE);
      break;
  }
  return end_node(c, nc);
}

Collection_Type *collection_type(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_COLLECTION_TYPE);
  Collection_Type *n = nc.node;
  assert(consume(c).t == T_LBRK);
  // For empty index, e.g.: []u32
  if (looking_at(c, T_RBRK)) {
    next(c);
    n->element_type = type(c);
    return end_node(c, nc);
  }
  n->index_expression.ok = true;
  n->index_expression.value = expression(c, TOKS(T_RBRK));
  if (!expect(c, n->id, T_RBRK)) {
    advance(c, FOLLOW_TYPE);
    return end_node(c, nc);
  }
  n->element_type = type(c);
  return end_node(c, nc);
}

Struct_Type *struct_type(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_STRUCT_TYPE);
  Struct_Type *n = nc.node;
  assert(consume(c).t == T_STRUCT);
  n->body = struct_body(c, FOLLOW_TYPE);
  return end_node(c, nc);
}

Field_List *field_list(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_FIELD_LIST);
  Field_List *n = nc.node;
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

Field *field(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_FIELD);
  Field *n = nc.node;
  Tok tok = at(c);
  if (!expect(c, n->id, T_IDENT)) {
    advance(c, FOLLOW_FIELD);
    return end_node(c, nc);
  }
  n->identifier = token_attr(c, "name", tok);
  n->type = type(c);
  return end_node(c, nc);
}

Type_List *type_list(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_TYPE_LIST);
  Type_List *n = nc.node;
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

Union_Type *union_type(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_UNION_TYPE);
  Union_Type *n = nc.node;
  assert(consume(c).t == T_UNION);
  if (!expect(c, n->id, T_LBRC)) {
    advance(c, FOLLOW_TYPE);
    return end_node(c, nc);
  }
  n->fields = field_list(c);
  if (!expect(c, n->id, T_RBRC)) {
    advance(c, FOLLOW_TYPE);
    return end_node(c, nc);
  }
  return end_node(c, nc);
}

Enum_Type *enum_type(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_ENUM_TYPE);
  Enum_Type *n = nc.node;
  assert(consume(c).t == T_ENUM);
  if (!expect(c, n->id, T_LBRC)) {
    advance(c, FOLLOW_TYPE);
    return end_node(c, nc);
  }
  n->enumerators = identifier_list(c);
  if (!expect(c, n->id, T_RBRC)) {
    advance(c, FOLLOW_TYPE);
    return end_node(c, nc);
  }
  return end_node(c, nc);
}

Error_Type *error_type(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_ERROR_TYPE);
  Error_Type *n = nc.node;
  assert(consume(c).t == T_ERROR);
  if (!expect(c, n->id, T_LBRC)) {
    advance(c, FOLLOW_TYPE);
    return end_node(c, nc);
  }
  n->errors = error_list(c);
  if (!expect(c, n->id, T_RBRC)) {
    advance(c, FOLLOW_TYPE);
    return end_node(c, nc);
  }
  return end_node(c, nc);
}

Identifier_List *identifier_list(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_IDENTIFIER_LIST);
  Identifier_List *n = nc.node;
  bool start = true;
  while (!looking_at(c, T_RBRC)) {
    if (!start && !expect(c, n->id, T_COMMA)) {
      advance(c, FOLLOW_IDENTIFIER_LIST);
      return end_node(c, nc);
    }
    // Ignore trailing comma
    if (looking_at(c, T_RBRC)) {
      break;
    }
    Tok identifier = at(c);
    if (!expect(c, n->id, T_IDENT)) {
      advance(c, FOLLOW_IDENTIFIER_LIST);
      return end_node(c, nc);
    }
    APPEND(n, token_attr_anon(c, identifier));
    start = false;
  }
  arena_own(&c->arena, n->items, n->cap);
  return end_node(c, nc);
}

Error_List *error_list(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_ERROR_LIST);
  Error_List *n = nc.node;
  bool start = true;
  while (!looking_at(c, T_RBRC)) {
    if (!start && !expect(c, n->id, T_COMMA)) {
      advance(c, FOLLOW_ERROR_LIST);
      return end_node(c, nc);
    }
    // Ignore trailing comma
    if (looking_at(c, T_RBRC)) {
      break;
    }
    APPEND(n, error(c));
    start = false;
  }
  arena_own(&c->arena, n->items, n->cap);
  return end_node(c, nc);
}

Error *error(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_ERROR);
  Error *n = nc.node;
  if (looking_at(c, T_BANG)) {
    n->embedded = true;
    token_attr(c, "kind", consume(c));
  }
  n->scoped_identifier = scoped_identifier(c, FOLLOW_ERROR);
  return end_node(c, nc);
}

Pointer_Type *pointer_type(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_POINTER_TYPE);
  Pointer_Type *n = nc.node;
  assert(consume(c).t == T_STAR);
  if (looking_at(c, T_MUT)) {
    n->classifier = consume(c);
  }
  n->referenced_type = type(c);
  return end_node(c, nc);
}

Function_Type *function_type(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_FUNCTION_TYPE);
  Function_Type *n = nc.node;
  assert(consume(c).t == T_FUN);
  if (!expect(c, n->id, T_LPAR)) {
    advance(c, FOLLOW_TYPE);
    return end_node(c, nc);
  }
  n->parameters = type_list(c);
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
Scoped_Identifier *scoped_identifier(Parse_Context *c, Toks follow) {
  Node_Context nc = start_node(c, NODE_SCOPED_IDENTIFIER);
  Scoped_Identifier *n = nc.node;
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

Initializer_List *initializer_list(Parse_Context *c, Tok_Kind delim) {
  Node_Context nc = start_node(c, NODE_INITIALIZER_LIST);
  Initializer_List *n = nc.node;
  // No items in list, just return
  if (looking_at(c, delim)) {
    return end_node(c, nc);
  }
  Toks follow_item = TOKS(T_COMMA, delim);
  APPEND(n, expression(c, follow_item));
  while (looking_at(c, T_COMMA)) {
    next(c);
    // Ignore trailing comma
    if (looking_at(c, delim)) {
      break;
    }
    APPEND(n, expression(c, follow_item));
  }
  arena_own(&c->arena, n->items, n->cap);
  return end_node(c, nc);
}

// Helpful resource for pratt parser:
// https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html#Pratt-parsing-the-general-shape

static bool is_prefix_op(Tok_Kind t) {
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

static bool is_infix_op(Tok_Kind t) {
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

static bool is_postfix_op(Tok_Kind t) {
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

static Index *index(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_INDEX);
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
      n->end.value = expression(c, TOKS(T_RBRK, T_SCLN));
      n->end.ok = true;
    }
    goto delim;
  }

  n->start.value = expression(c, TOKS(T_RBRK, T_CLN, T_SCLN));
  n->start.ok = true;

  if (looking_at(c, T_CLN)) {
    n->t = INDEX_RANGE;
    token_attr_anon(c, consume(c));
    if (!looking_at(c, T_RBRK)) {
      n->end.value = expression(c, TOKS(T_RBRK, T_SCLN));
      n->end.ok = true;
    }
    goto delim;
  }

delim:
  if (!expect(c, n->id, T_RBRK)) {
    // TODO: change to FOLLOW_EXPRESSION
    advance(c, FOLLOW_BASIC_EXPRESSION);
  }

  return end_node(c, nc);
}

static Node_Context parse_postfix(Parse_Context *c, Expression *lhs) {
  Node_Context expr_ctx = start_node(c, NODE_EXPRESSION);
  Expression *expr = expr_ctx.node;

  switch (at(c).t) {
    case T_LBRK: {
      Node_Context access_ctx = start_node(c, NODE_ARRAY_ACCESS_EXPRESSION);
      Array_Access_Expression *access = access_ctx.node;

      Node_Context lhs_ctx = begin_node(c, lhs);
      access->lvalue = end_node(c, lhs_ctx);
      access->index = index(c);

      expr->t = EXPRESSION_ARRAY_ACCESS;
      expr->array_access_expression = end_node(c, access_ctx);
      break;
    }
    case T_LPAR: {
      Node_Context call_ctx = start_node(c, NODE_FUNCTION_CALL_EXPRESSION);
      Function_Call_Expression *call = call_ctx.node;

      Node_Context lhs_ctx = begin_node(c, lhs);
      call->lvalue = end_node(c, lhs_ctx);

      if (!expect(c, call->id, T_LPAR)) {
        advance(c, FOLLOW_BASIC_EXPRESSION);
        goto yield_call_expr;
      }

      call->arguments = attr(c, "args", initializer_list(c, T_RPAR));

      if (!expect(c, call->id, T_RPAR)) {
        advance(c, FOLLOW_BASIC_EXPRESSION);
      }

    yield_call_expr:
      expr->t = EXPRESSION_FUNCTION_CALL;
      expr->function_call_expression = end_node(c, call_ctx);
      break;
    }
    case T_DOT: {
      Node_Context access_ctx = start_node(c, NODE_FIELD_ACCESS_EXPRESSION);
      Field_Access_Expression *access = access_ctx.node;

      Node_Context lhs_ctx = begin_node(c, lhs);
      access->lvalue = attr(c, "lvalue", end_node(c, lhs_ctx));

      next(c);
      Tok field = at(c);
      if (!expect(c, access->id, T_IDENT)) {
        advance(c, FOLLOW_BASIC_EXPRESSION);
        goto yield_access_expr;
      }
      access->field = token_attr(c, "field", field);

    yield_access_expr:
      expr->t = EXPRESSION_FIELD_ACCESS;
      expr->field_access_expression = end_node(c, access_ctx);
      break;
    }
    default: {
      Node_Context postfix_ctx = start_node(c, NODE_POSTFIX_EXPRESSION);
      Postfix_Expression *postfix = postfix_ctx.node;

      postfix->op = token_attr(c, "op", consume(c));
      Node_Context lhs_ctx = begin_node(c, lhs);
      postfix->inner_expression = end_node(c, lhs_ctx);

      expr->t = EXPRESSION_POSTFIX;
      expr->postfix_expression = end_node(c, postfix_ctx);
    }
  }

  return expr_ctx;
}

typedef struct {
  Power pow;
  bool ok;
} Maybe_Power;

static Maybe_Power infix_binding_power(Tok_Kind op) {
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

static Maybe_Power prefix_binding_power(Tok_Kind op) {
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

static Maybe_Power postfix_binding_power(Tok_Kind op) {
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

static Node_Context expression_power(Parse_Context *c, u32 min_pow,
                                     Toks delim) {
  Node_Context lhs_ctx = start_node(c, NODE_EXPRESSION);
  Expression *lhs = lhs_ctx.node;

  Tok tok = at(c);

  if (is_prefix_op(tok.t)) {
    Node_Context unary_ctx = start_node(c, NODE_UNARY_EXPRESSION);
    Unary_Expression *unary_expr = unary_ctx.node;

    Power pow;
    Maybe_Power maybe_pow = prefix_binding_power(tok.t);
    if (!maybe_pow.ok) {
      expected(c, unary_expr->id, "valid prefix operator");
      pow = (Power){0, 0};
    } else {
      pow = maybe_pow.pow;
    }

    unary_expr->op = token_attr(c, "op", consume(c));
    unary_expr->inner_expression =
        end_node(c, expression_power(c, pow.right, delim));

    lhs->t = EXPRESSION_UNARY;
    lhs->unary_expression = end_node(c, unary_ctx);
  } else {
    if (tok.t == T_LPAR) {
      next(c);
      Toks new_delim;
      new_delim.len = delim.len;
      new_delim.items = malloc((delim.len + 1) * sizeof(Tok_Kind));
      memcpy(new_delim.items, delim.items, delim.len * sizeof(Tok_Kind));
      if (!one_of(T_RPAR, delim)) {
        new_delim.len = delim.len + 1;
        new_delim.items[delim.len] = T_RPAR;
      }

      // TODO: this effectively leaks an expression allocated in the arena
      // maybe we can just free the node here before recursing
      set_current_node(c, lhs_ctx.parent);
      lhs_ctx = expression_power(c, 0, new_delim);
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
      lhs->t = EXPRESSION_BASIC;
      lhs->basic_expression = basic_expression(c);
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
      advance(c, FOLLOW_BASIC_EXPRESSION);
    }

    Maybe_Power maybe_pow = postfix_binding_power(op.t);
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

    maybe_pow = infix_binding_power(op.t);
    if (!maybe_pow.ok) {
      break;
    }
    Power pow = maybe_pow.pow;
    if (pow.left < min_pow) {
      break;
    }

    // Really sorry if someone is coming here to understand this code.
    // There is a bit of gymnastics here to make sure the implicit child-parent
    // relationships maintained by the Node_Context system are correct.
    //
    // The below code needs to make sure that the current lhs is not added as
    // a child of the current parent (stored in lhs_ctx.parent). It should
    // instead be added as a child of a new node which should become the new
    // descendant of current parent.
    set_current_node(c, lhs_ctx.parent);

    Node_Context new_expr_ctx = start_node(c, NODE_EXPRESSION);
    Expression *new_expr = new_expr_ctx.node;

    Node_Context bin_ctx = start_node(c, NODE_BINARY_EXPRESSION);
    Binary_Expression *bin_expr = bin_ctx.node;

    bin_expr->op = token_attr(c, "op", op);
    Node_Context old_lhs_ctx = begin_node(c, lhs);
    bin_expr->left = attr(c, "left", end_node(c, old_lhs_ctx));
    next(c);
    bin_expr->right =
        attr(c, "right", end_node(c, expression_power(c, pow.right, delim)));

    new_expr->t = EXPRESSION_BINARY;
    new_expr->binary_expression = end_node(c, bin_ctx);

    lhs = new_expr;
    lhs_ctx = new_expr_ctx;
  }

  return lhs_ctx;
}

Expression *expression(Parse_Context *c, Toks delim) {
  return end_node(c, expression_power(c, 0, delim));
}

Basic_Expression *basic_expression(Parse_Context *c) {
  Node_Context nc = start_node(c, NODE_BASIC_EXPRESSION);
  Basic_Expression *n = nc.node;
  Tok first = at(c);
  if (starts_type(at(c).t)) {
    Node_Context lit_ctx = start_node(c, NODE_BRACED_LITERAL);
    Braced_Literal *braced_lit = lit_ctx.node;

    braced_lit->type.ok = true;
    Parse_State marker = set_marker(c);
    braced_lit->type.value = type(c);

    if (first.t == T_IDENT && !looking_at(c, T_LBRC)) {
      // The first token was an identifier and we don't have
      // a '{'. This means that the identifier should be just
      // the token itself as the basic expression
      backtrack(c, marker);
      n->t = BASIC_EXPRESSION_TOKEN;
      c->current = n->id;
      n->token = token_attr(c, "atom", consume(c));
      return end_node(c, nc);
    }

    if (!expect(c, braced_lit->id, T_LBRC)) {
      advance(c, FOLLOW_BASIC_EXPRESSION);
      n->t = BASIC_EXPRESSION_BRACED_LIT;
      n->braced_lit = end_node(c, lit_ctx);
      return end_node(c, nc);
    }

    braced_lit->initializer = initializer_list(c, T_RBRC);
    if (!expect(c, braced_lit->id, T_RBRC)) {
      advance(c, FOLLOW_BASIC_EXPRESSION);
    }

    n->t = BASIC_EXPRESSION_BRACED_LIT;
    n->braced_lit = end_node(c, lit_ctx);
    return end_node(c, nc);
  }
  switch (at(c).t) {
    case T_CONS: {
      next(c);

      Node_Context lit_ctx = start_node(c, NODE_BRACED_LITERAL);
      Braced_Literal *braced_lit = lit_ctx.node;

      if (!expect(c, braced_lit->id, T_LPAR)) {
        advance(c, FOLLOW_BASIC_EXPRESSION);
        goto yield_braced_lit;
      }

      braced_lit->type.ok = true;
      braced_lit->type.value = type(c);

      if (!expect(c, braced_lit->id, T_RPAR)) {
        advance(c, FOLLOW_BASIC_EXPRESSION);
        goto yield_braced_lit;
      }

      if (!expect(c, braced_lit->id, T_LBRC)) {
        advance(c, FOLLOW_BASIC_EXPRESSION);
        goto yield_braced_lit;
      }

      braced_lit->initializer = initializer_list(c, T_RBRC);

      if (!expect(c, braced_lit->id, T_RBRC)) {
        advance(c, FOLLOW_BASIC_EXPRESSION);
        goto yield_braced_lit;
      }

    yield_braced_lit:
      n->t = BASIC_EXPRESSION_BRACED_LIT;
      n->braced_lit = end_node(c, lit_ctx);
      return end_node(c, nc);
    }
    case T_LBRC: {
      next(c);

      Node_Context lit_ctx = start_node(c, NODE_BRACED_LITERAL);
      Braced_Literal *braced_lit = lit_ctx.node;

      braced_lit->initializer = initializer_list(c, T_RBRC);
      n->t = BASIC_EXPRESSION_BRACED_LIT;
      n->braced_lit = end_node(c, lit_ctx);

      if (!expect(c, n->id, T_RBRC)) {
        advance(c, FOLLOW_BASIC_EXPRESSION);
      }

      return end_node(c, nc);
    }
    case T_NUM:
    case T_CHAR:
    case T_STR:
      n->t = BASIC_EXPRESSION_TOKEN;
      n->token = token_attr(c, "atom", consume(c));
      return end_node(c, nc);
    default:
      break;
  }

  expected(c, n->id, "a basic expression");
  advance(c, FOLLOW_BASIC_EXPRESSION);
  return end_node(c, nc);
}
