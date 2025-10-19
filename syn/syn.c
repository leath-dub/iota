#include "syn.h"

// TODO: add IR that populates child parent node metadata.
// TODO: add delimiter tokens as part of the parsing context

#include <assert.h>
#include <string.h>

#include "../ast/ast.h"
#include "../lex/lex.h"

#define NODE(pc, T) new_node(&(pc)->meta, &(pc)->arena, sizeof(T), _Alignof(T))

Parse_Context new_parse_context(Source_Code code) {
  return (Parse_Context){
      .lex = new_lexer(code),
      .arena = new_arena(),
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
  Source_File *n = NODE(c, Source_File);
  n->imports = imports(c);
  n->declarations = declarations(c);
  return n;
}

Imports *imports(Parse_Context *c) {
  Imports *n = NODE(c, Imports);
  while (looking_at(c, T_IMPORT)) {
    APPEND(n, import(c));
  }
  arena_own(&c->arena, n->items, n->cap);
  return n;
}

Import *import(Parse_Context *c) {
  Import *n = NODE(c, Import);
  assert(expect(c, n->id, T_IMPORT));
  if (looking_at(c, T_IDENT)) {
    n->aliased = true;
    n->alias = at(c);
    next(c);
  }
  Tok import_name = at(c);
  if (!expect(c, n->id, T_STR) || !expect(c, n->id, T_SCLN)) {
    advance(c, FOLLOW_IMPORT);
    return n;
  }
  n->import_name = import_name;
  return n;
}

Declarations *declarations(Parse_Context *c) {
  Declarations *n = NODE(c, Declarations);
  while (!looking_at(c, T_EOF)) {
    APPEND(n, declaration(c));
  }
  arena_own(&c->arena, n->items, n->cap);
  return n;
}

Declaration *declaration(Parse_Context *c) {
  Declaration *n = NODE(c, Declaration);
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
  return n;
}

Struct_Declaration *struct_declaration(Parse_Context *c) {
  Struct_Declaration *n = NODE(c, Struct_Declaration);
  assert(consume(c).t == T_STRUCT);
  Tok name = at(c);
  if (!expect(c, n->id, T_IDENT)) {
    advance(c, FOLLOW_DECLARATION);
    return n;
  }
  n->identifier = name;
  n->body = struct_body(c, FOLLOW_DECLARATION);
  return n;
}

Struct_Body *struct_body(Parse_Context *c, Toks follow) {
  Struct_Body *n = NODE(c, Struct_Body);
  switch (at(c).t) {
    case T_LBRC:
      next(c);
      n->tuple_like = false;
      n->field_list = field_list(c);
      if (!expect(c, n->id, T_RBRC)) {
        advance(c, follow);
        return n;
      }
      break;
    case T_LPAR:
      next(c);
      n->tuple_like = true;
      n->type_list = type_list(c);
      if (!expect(c, n->id, T_RPAR)) {
        advance(c, follow);
        return n;
      }
      break;
    default:
      expected(c, n->id, "a struct or tuple body");
      advance(c, follow);
      break;
  }
  return n;
}

Enum_Declaration *enum_declaration(Parse_Context *c) {
  Enum_Declaration *n = NODE(c, Enum_Declaration);
  assert(consume(c).t == T_ENUM);
  Tok name = at(c);
  if (!expect(c, n->id, T_IDENT)) {
    advance(c, FOLLOW_DECLARATION);
    return n;
  }
  n->identifier = name;
  if (!expect(c, n->id, T_LBRC)) {
    advance(c, FOLLOW_DECLARATION);
    return n;
  }
  n->enumerators = identifier_list(c);
  if (!expect(c, n->id, T_RBRC)) {
    advance(c, FOLLOW_DECLARATION);
    return n;
  }
  return n;
}

Error_Declaration *error_declaration(Parse_Context *c) {
  Error_Declaration *n = NODE(c, Error_Declaration);
  assert(consume(c).t == T_ERROR);
  Tok name = at(c);
  if (!expect(c, n->id, T_IDENT)) {
    advance(c, FOLLOW_DECLARATION);
    return n;
  }
  n->identifier = name;
  if (!expect(c, n->id, T_LBRC)) {
    advance(c, FOLLOW_DECLARATION);
    return n;
  }
  n->errors = error_list(c);
  if (!expect(c, n->id, T_RBRC)) {
    advance(c, FOLLOW_DECLARATION);
    return n;
  }
  return n;
}

Variable_Declaration *variable_declaration(Parse_Context *c) {
  Variable_Declaration *n = NODE(c, Variable_Declaration);
  Tok classifier = at(c);
  assert(classifier.t == T_MUT || classifier.t == T_LET);
  n->classifier = consume(c);
  n->binding = variable_binding(c);
  if (!looking_at(c, T_EQ) && !looking_at(c, T_SCLN)) {
    // Must have a type
    n->type = type(c);
  }
  if (looking_at(c, T_EQ)) {
    n->init.ok = true;
    n->init.assign_token = consume(c);
    n->init.expression = expression(c, TOKS(T_SCLN));
  }
  if (!expect(c, n->id, T_SCLN)) {
    advance(c, FOLLOW_DECLARATION);
    return n;
  }
  return n;
}

Variable_Binding *variable_binding(Parse_Context *c) {
  Variable_Binding *n = NODE(c, Variable_Binding);
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
        n->destructure_union = NODE(c, Destructure_Union);
        n->destructure_union->tag = name;
        n->destructure_union->binding = binding(c);
        if (!expect(c, n->id, T_RPAR)) {
          advance(c, FOLLOW_VARIABLE_BINDING);
          return n;
        }
        return n;
      }
      n->t = VARIABLE_BINDING_BASIC;
      n->basic = name;
      break;
    }
    default:
      expected(c, n->id, "a variable binding");
      advance(c, FOLLOW_VARIABLE_BINDING);
      break;
  }
  return n;
}

Binding *binding(Parse_Context *c) {
  Binding *n = NODE(c, Binding);
  if (looking_at(c, T_STAR)) {
    n->reference = consume(c);
  }
  Tok name = at(c);
  if (!expect(c, n->id, T_IDENT)) {
    advance(c, FOLLOW_BINDING);
    return n;
  }
  n->identifier = name;
  return n;
}

Aliased_Binding *aliased_binding(Parse_Context *c) {
  Aliased_Binding *n = NODE(c, Aliased_Binding);
  n->binding = binding(c);
  if (looking_at(c, T_EQ)) {
    next(c);
    Tok name = at(c);
    if (!expect(c, n->id, T_IDENT)) {
      advance(c, FOLLOW_ALIASED_BINDING);
      return n;
    }
    n->alias.ok = true;
    n->alias.value = name;
  }
  return n;
}

Destructure_Struct *destructure_struct(Parse_Context *c, Toks follow) {
  Destructure_Struct *n = NODE(c, Destructure_Struct);
  if (!expect(c, n->id, T_LBRC)) {
    advance(c, follow);
    return n;
  }
  n->bindings = aliased_binding_list(c, T_RBRC);
  if (!expect(c, n->id, T_RBRC)) {
    advance(c, follow);
    return n;
  }
  return n;
}

Destructure_Tuple *destructure_tuple(Parse_Context *c, Toks follow) {
  Destructure_Tuple *n = NODE(c, Destructure_Tuple);
  if (!expect(c, n->id, T_LPAR)) {
    advance(c, follow);
    return n;
  }
  n->bindings = binding_list(c, T_RPAR);
  if (!expect(c, n->id, T_RPAR)) {
    advance(c, follow);
    return n;
  }
  return n;
}

Aliased_Binding_List *aliased_binding_list(Parse_Context *c, Tok_Kind end) {
  Aliased_Binding_List *n = NODE(c, Aliased_Binding_List);
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
  return n;
}

Binding_List *binding_list(Parse_Context *c, Tok_Kind end) {
  Binding_List *n = NODE(c, Binding_List);
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
  return n;
}

Function_Declaration *function_declaration(Parse_Context *c) {
  Function_Declaration *n = NODE(c, Function_Declaration);
  assert(consume(c).t == T_FUN);
  // TODO type parameters
  Tok name = at(c);
  if (!expect(c, n->id, T_IDENT)) {
    advance(c, FOLLOW_DECLARATION);
    return n;
  }
  n->identifier = name;
  if (!expect(c, n->id, T_LPAR)) {
    advance(c, FOLLOW_DECLARATION);
    return n;
  }
  n->parameters = function_parameter_list(c);
  if (!expect(c, n->id, T_RPAR)) {
    advance(c, FOLLOW_DECLARATION);
    return n;
  }
  if (!looking_at(c, T_LBRC)) {
    n->return_type.ok = true;
    n->return_type.value = type(c);
  }
  n->body = compound_statement(c);
  return n;
}

Function_Parameter_List *function_parameter_list(Parse_Context *c) {
  Function_Parameter_List *n = NODE(c, Function_Parameter_List);
  if (looking_at(c, T_RPAR)) {
    return n;
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
  return n;
}

Function_Parameter *function_parameter(Parse_Context *c) {
  Function_Parameter *n = NODE(c, Function_Parameter);
  n->binding = variable_binding(c);
  if (looking_at(c, T_DOTDOT)) {
    next(c);
    n->variadic = true;
  }
  n->type = type(c);
  return n;
}

Statement *statement(Parse_Context *c) {
  Statement *n = NODE(c, Statement);
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
        return n;
      }
      break;
  }
  return n;
}

Compound_Statement *compound_statement(Parse_Context *c) {
  Compound_Statement *n = NODE(c, Compound_Statement);
  if (!expect(c, n->id, T_LBRC)) {
    advance(c, FOLLOW_STATEMENT_LIST);
    return n;
  }
  while (!looking_at(c, T_RBRC)) {
    if (looking_at(c, T_EOF)) {
      expected(c, n->id, "a statement");
      advance(c, FOLLOW_STATEMENT);
      break;
    }
    APPEND(n, statement(c));
  }
  next(c);
  return n;
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
  Type *n = NODE(c, Type);
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
      Builtin_Type *builtin_type = NODE(c, Builtin_Type);
      n->t = TYPE_BUILTIN;
      builtin_type->token = consume(c);
      n->builtin_type = builtin_type;
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
  return n;
}

Collection_Type *collection_type(Parse_Context *c) {
  Collection_Type *n = NODE(c, Collection_Type);
  assert(consume(c).t == T_LBRK);
  // For empty index, e.g.: []u32
  if (looking_at(c, T_RBRK)) {
    next(c);
    n->element_type = type(c);
    return n;
  }
  n->index_expression.ok = true;
  n->index_expression.value = expression(c, TOKS(T_RBRK));
  if (!expect(c, n->id, T_RBRK)) {
    advance(c, FOLLOW_TYPE);
    return n;
  }
  n->element_type = type(c);
  return n;
}

Struct_Type *struct_type(Parse_Context *c) {
  Struct_Type *n = NODE(c, Struct_Type);
  assert(consume(c).t == T_STRUCT);
  n->body = struct_body(c, FOLLOW_TYPE);
  return n;
}

Field_List *field_list(Parse_Context *c) {
  Field_List *n = NODE(c, Field_List);
  if (looking_at(c, T_RBRC)) {
    return n;
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
  return n;
}

Field *field(Parse_Context *c) {
  Field *n = NODE(c, Field);
  Tok tok = at(c);
  if (!expect(c, n->id, T_IDENT)) {
    advance(c, FOLLOW_FIELD);
    return n;
  }
  n->identifier = tok;
  n->type = type(c);
  return n;
}

Type_List *type_list(Parse_Context *c) {
  Type_List *n = NODE(c, Type_List);
  if (looking_at(c, T_RPAR)) {
    return n;
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
  return n;
}

Union_Type *union_type(Parse_Context *c) {
  Union_Type *n = NODE(c, Union_Type);
  assert(consume(c).t == T_UNION);
  if (!expect(c, n->id, T_LBRC)) {
    advance(c, FOLLOW_TYPE);
    return n;
  }
  n->fields = field_list(c);
  if (!expect(c, n->id, T_RBRC)) {
    advance(c, FOLLOW_TYPE);
    return n;
  }
  return n;
}

Enum_Type *enum_type(Parse_Context *c) {
  Enum_Type *n = NODE(c, Enum_Type);
  assert(consume(c).t == T_ENUM);
  if (!expect(c, n->id, T_LBRC)) {
    advance(c, FOLLOW_TYPE);
    return n;
  }
  n->enumerators = identifier_list(c);
  if (!expect(c, n->id, T_RBRC)) {
    advance(c, FOLLOW_TYPE);
    return n;
  }
  return n;
}

Error_Type *error_type(Parse_Context *c) {
  Error_Type *n = NODE(c, Error_Type);
  assert(consume(c).t == T_ERROR);
  if (!expect(c, n->id, T_LBRC)) {
    advance(c, FOLLOW_TYPE);
    return n;
  }
  n->errors = error_list(c);
  if (!expect(c, n->id, T_RBRC)) {
    advance(c, FOLLOW_TYPE);
    return n;
  }
  return n;
}

Identifier_List *identifier_list(Parse_Context *c) {
  Identifier_List *n = NODE(c, Identifier_List);
  bool start = true;
  while (!looking_at(c, T_RBRC)) {
    if (!start && !expect(c, n->id, T_COMMA)) {
      advance(c, FOLLOW_IDENTIFIER_LIST);
      return n;
    }
    // Ignore trailing comma
    if (looking_at(c, T_RBRC)) {
      break;
    }
    Tok identifier = at(c);
    if (!expect(c, n->id, T_IDENT)) {
      advance(c, FOLLOW_IDENTIFIER_LIST);
      return n;
    }
    APPEND(n, identifier);
    start = false;
  }
  arena_own(&c->arena, n->items, n->cap);
  return n;
}

Error_List *error_list(Parse_Context *c) {
  Error_List *n = NODE(c, Error_List);
  bool start = true;
  while (!looking_at(c, T_RBRC)) {
    if (!start && !expect(c, n->id, T_COMMA)) {
      advance(c, FOLLOW_ERROR_LIST);
      return n;
    }
    // Ignore trailing comma
    if (looking_at(c, T_RBRC)) {
      break;
    }
    APPEND(n, error(c));
    start = false;
  }
  arena_own(&c->arena, n->items, n->cap);
  return n;
}

Error *error(Parse_Context *c) {
  Error *n = NODE(c, Error);
  if (looking_at(c, T_BANG)) {
    n->embedded = true;
    next(c);
  }
  n->scoped_identifier = scoped_identifier(c, FOLLOW_ERROR);
  return n;
}

Pointer_Type *pointer_type(Parse_Context *c) {
  Pointer_Type *n = NODE(c, Pointer_Type);
  assert(consume(c).t == T_STAR);
  if (looking_at(c, T_MUT)) {
    n->classifier = consume(c);
  }
  n->referenced_type = type(c);
  return n;
}

Function_Type *function_type(Parse_Context *c) {
  Function_Type *n = NODE(c, Function_Type);
  assert(consume(c).t == T_FUN);
  if (!expect(c, n->id, T_LPAR)) {
    advance(c, FOLLOW_TYPE);
    return n;
  }
  n->parameters = type_list(c);
  if (!expect(c, n->id, T_RPAR)) {
    advance(c, FOLLOW_TYPE);
    return n;
  }
  if (!one_of(at(c).t, FOLLOW_TYPE)) {
    n->return_type.ok = true;
    n->return_type.value = type(c);
  }
  return n;
}

// Some rules that are used in many contexts take a follow set from the
// caller. This improves the error recovery as it has more context of what
// it should be syncing on than just "whatever can follow is really common rule"
Scoped_Identifier *scoped_identifier(Parse_Context *c, Toks follow) {
  Scoped_Identifier *n = NODE(c, Scoped_Identifier);
  Tok identifier = at(c);
  if (!expect(c, n->id, T_IDENT)) {
    advance(c, follow);
    return n;
  }
  APPEND(n, identifier);
  while (looking_at(c, T_DOT)) {
    next(c);
    Tok identifier = at(c);
    if (!expect(c, n->id, T_IDENT)) {
      advance(c, follow);
      return n;
    }
    APPEND(n, identifier);
  }
  arena_own(&c->arena, n->items, n->cap);
  return n;
}

Initializer_List *initializer_list(Parse_Context *c, Tok_Kind delim) {
  Initializer_List *n = NODE(c, Initializer_List);
  // No items in list, just return
  if (looking_at(c, delim)) {
    return n;
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
  return n;
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
  Index *n = NODE(c, Index);
  assert(consume(c).t == T_LBRK);

  n->t = INDEX_SINGLE;

  // e.g. [:10]
  if (looking_at(c, T_CLN)) {
    n->t = INDEX_RANGE;
    next(c);
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
    next(c);
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

  return n;
}

static Expression *parse_postfix(Parse_Context *c, Expression *lhs) {
  Expression *expr = NODE(c, Expression);
  switch (at(c).t) {
    case T_LBRK: {
      Array_Access_Expression *access = NODE(c, Array_Access_Expression);
      access->lvalue = lhs;
      access->index = index(c);
      expr->t = EXPRESSION_ARRAY_ACCESS;
      expr->array_access_expression = access;
      break;
    }
    case T_LPAR: {
      Function_Call_Expression *call = NODE(c, Function_Call_Expression);
      expr->t = EXPRESSION_FUNCTION_CALL;
      expr->function_call_expression = call;

      call->lvalue = lhs;
      if (!expect(c, call->id, T_LPAR)) {
        advance(c, FOLLOW_BASIC_EXPRESSION);
        break;
      }
      call->arguments = initializer_list(c, T_RPAR);
      if (!expect(c, call->id, T_RPAR)) {
        advance(c, FOLLOW_BASIC_EXPRESSION);
      }
      break;
    }
    case T_DOT: {
      Field_Access_Expression *access = NODE(c, Field_Access_Expression);
      expr->t = EXPRESSION_FIELD_ACCESS;
      expr->field_access_expression = access;

      access->lvalue = lhs;
      next(c);
      Tok field = at(c);
      if (!expect(c, access->id, T_IDENT)) {
        advance(c, FOLLOW_BASIC_EXPRESSION);
        break;
      }
      access->field = field;
      break;
    }
    default: {
      Expression *expr = NODE(c, Expression);
      expr->t = EXPRESSION_POSTFIX;
      expr->postfix_expression = NODE(c, Postfix_Expression);
      expr->postfix_expression->op = at(c);
      expr->postfix_expression->inner_expression = lhs;
    }
  }
  return expr;
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

static Expression *expression_power(Parse_Context *c, u32 min_pow, Toks delim) {
  Expression *lhs = NULL;

  Tok tok = at(c);

  if (is_prefix_op(tok.t)) {
    lhs = NODE(c, Expression);

    Maybe_Power maybe_pow = prefix_binding_power(tok.t);
    Power pow;
    Unary_Expression *n = NODE(c, Unary_Expression);
    if (!maybe_pow.ok) {
      expected(c, n->id, "valid prefix operator");
      pow = (Power){0, 0};
    } else {
      pow = maybe_pow.pow;
    }
    lhs->t = EXPRESSION_UNARY;
    n->op = consume(c);
    Expression *rhs = expression_power(c, pow.right, delim);
    n->inner_expression = rhs;
    lhs->unary_expression = n;
  } else {
    if (tok.t == T_LPAR) {
      next(c);
      Toks new_delim;
      new_delim.len = delim.len;
      new_delim.items = malloc(delim.len * sizeof(Tok_Kind));
      memcpy(new_delim.items, delim.items, delim.len * sizeof(Tok_Kind));
      if (!one_of(T_RPAR, delim)) {
        new_delim.len = delim.len + 1;
        new_delim.items[delim.len] = T_RPAR;
      }
      lhs = expression_power(c, 0, new_delim);
      free(new_delim.items);
      if (!expect(c, lhs->id, T_RPAR)) {
        advance(c, delim);
        return lhs;
      }
      if (one_of(at(c).t, delim)) {
        return lhs;
      }
    } else {
      lhs = NODE(c, Expression);
      lhs->t = EXPRESSION_BASIC;
      lhs->basic_expression = basic_expression(c);
    }
  }

  while (true) {
    Tok op = at(c);
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
      lhs = parse_postfix(c, lhs);
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

    next(c);
    Expression *rhs = expression_power(c, pow.right, delim);

    Expression *old_lhs = lhs;
    lhs = NODE(c, Expression);
    lhs->t = EXPRESSION_BINARY;
    lhs->binary_expression = NODE(c, Binary_Expression);
    lhs->binary_expression->op = op;
    lhs->binary_expression->left = old_lhs;
    lhs->binary_expression->right = rhs;
  }

  return lhs;
}

Expression *expression(Parse_Context *c, Toks delim) {
  return expression_power(c, 0, delim);
}

Basic_Expression *basic_expression(Parse_Context *c) {
  Basic_Expression *n = NODE(c, Basic_Expression);
  Tok first = at(c);
  if (starts_type(at(c).t)) {
    n->t = BASIC_EXPRESSION_BRACED_LIT;
    n->braced_lit = NODE(c, Braced_Literal);
    n->braced_lit->type.ok = true;
    Parse_State marker = set_marker(c);
    n->braced_lit->type.value = type(c);
    if (first.t == T_IDENT && !looking_at(c, T_LBRC)) {
      // The first token was an identifier and we don't have
      // a '{'. This means that the identifier should be just
      // the token itself as the basic expression
      backtrack(c, marker);
      n->t = BASIC_EXPRESSION_TOKEN;
      n->token = consume(c);
      return n;
    }
    if (!expect(c, n->braced_lit->id, T_LBRC)) {
      advance(c, FOLLOW_BASIC_EXPRESSION);
      return n;
    }
    n->braced_lit->initializer = initializer_list(c, T_RBRC);
    if (!expect(c, n->braced_lit->id, T_RBRC)) {
      advance(c, FOLLOW_BASIC_EXPRESSION);
    }
    return n;
  }
  switch (at(c).t) {
    case T_CONS: {
      next(c);
      n->t = BASIC_EXPRESSION_BRACED_LIT;
      n->braced_lit = NODE(c, Braced_Literal);
      if (!expect(c, n->braced_lit->id, T_LPAR)) {
        advance(c, FOLLOW_BASIC_EXPRESSION);
        return n;
      }
      n->braced_lit->type.ok = true;
      n->braced_lit->type.value = type(c);
      if (!expect(c, n->braced_lit->id, T_RPAR)) {
        advance(c, FOLLOW_BASIC_EXPRESSION);
        return n;
      }
      if (!expect(c, n->braced_lit->id, T_LBRC)) {
        advance(c, FOLLOW_BASIC_EXPRESSION);
        return n;
      }
      n->braced_lit->initializer = initializer_list(c, T_RBRC);
      if (!expect(c, n->braced_lit->id, T_RBRC)) {
        advance(c, FOLLOW_BASIC_EXPRESSION);
        return n;
      }
      return n;
    }
    case T_LBRC: {
      next(c);
      n->t = BASIC_EXPRESSION_BRACED_LIT;
      n->braced_lit = NODE(c, Braced_Literal);
      n->braced_lit->initializer = initializer_list(c, T_RBRC);
      if (!expect(c, n->id, T_RBRC)) {
        advance(c, FOLLOW_BASIC_EXPRESSION);
        return n;
      }
      return n;
    }
    case T_NUM:
    case T_CHAR:
    case T_STR:
      break;
    default:
      expected(c, n->id, "a basic expression");
      advance(c, FOLLOW_BASIC_EXPRESSION);
      n->t = BASIC_EXPRESSION_TOKEN;
      return n;
  }
  n->t = BASIC_EXPRESSION_TOKEN;
  n->token = consume(c);
  return n;
}
