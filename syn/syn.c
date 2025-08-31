#include "syn.h"

#include <assert.h>

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

static Tok tnext(Parse_Context *c) {
  Tok tok;
  do {
    lex_consume(&c->lex);
    tok = lex_peek(&c->lex);
  } while (tok.t == T_CMNT);
  return tok;
}

static void next(Parse_Context *c) {
  (void)tnext(c);
}

static Tok consume(Parse_Context *c) {
  Tok tok = lex_peek(&c->lex);
  next(c);
  return tok;
}

Tok at(Parse_Context *c) {
  return lex_peek(&c->lex);
}

/*static*/
bool looking_at(Parse_Context *c, Tok_Kind t) {
  return at(c).t == t;
}

/*static*/
void advance(Parse_Context *c, Toks toks) {
  for (Tok tok = lex_peek(&c->lex); tok.t != T_EOF; tok = tnext(c)) {
    for (u32 i = 0; i < toks.len; i++) {
      if (tok.t == toks.items[i]) {
        return;
      }
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

// These were manually calculated by looking at the tree-sitter grammar.
// I really fucking hope it was worth the effort.
static const Toks FOLLOW_IMPORT = TOKS(T_IMPORT, T_LET, T_MUT, T_FUN, T_STRUCT, T_ENUM, T_ERROR, T_UNION, T_TYPE, T_USE);
static const Toks FOLLOW_DECLARATION = TOKS(T_LET, T_MUT, T_FUN, T_STRUCT, T_ENUM, T_ERROR, T_UNION, T_TYPE, T_USE);
static const Toks FOLLOW_VARIABLE_BINDING = TOKS(T_COMMA);
static const Toks FOLLOW_TYPE = TOKS(T_COMMA, T_RPAR, T_SCLN, T_RBRC, T_LBRC, T_EQ);
static const Toks FOLLOW_FIELD_LIST = TOKS(T_RBRC);
static const Toks FOLLOW_IDENTIFIER_LIST = TOKS(T_RBRC);
static const Toks FOLLOW_ERROR_LIST = TOKS(T_RBRC);
static const Toks FOLLOW_ERROR = TOKS(T_COMMA, T_RBRC);
static const Toks FOLLOW_FIELD = TOKS(T_IDENT, T_RBRC);
// TODO: once we have an automated tool
// static const Toks FOLLOW_EXPRESSION = TOKS();

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
    default:
      expected(c, n->id, "expected start of declaration");
      advance(c, FOLLOW_DECLARATION);
      break;
  }
  return n;
}

Variable_Declaration *variable_declaration(Parse_Context *c) {
  Variable_Declaration *n = NODE(c, Variable_Declaration);
  Tok classifier = at(c);
  assert(classifier.t == T_MUT || classifier.t == T_LET);
  n->classifier = classifier;
  next(c);
  n->variable_list = variable_list(c);
  if (looking_at(c, T_EQ)) {
    n->init.assign_token = at(c);
    n->init.expression = expression(c);
  }
  if (!expect(c, n->id, T_SCLN)) {
    advance(c, FOLLOW_DECLARATION);
    return n;
  }
  return n;
}

Variable_List *variable_list(Parse_Context *c) {
  Variable_List *n = NODE(c, Variable_List);
  Variable_Binding *binding = variable_binding(c);
  APPEND(n, binding);
  while (looking_at(c, T_COMMA)) {
    next(c);
    binding = variable_binding(c);
    APPEND(n, binding);
  }
  arena_own(&c->arena, n->items, n->cap);
  return n;
}

Variable_Binding *variable_binding(Parse_Context *c) {
  Variable_Binding *n = NODE(c, Variable_Binding);
  Tok name = at(c);
  if (!expect(c, n->id, T_IDENT)) {
      advance(c, FOLLOW_VARIABLE_BINDING);
      return n;
  }
  n->identifier = name;
  if (!looking_at(c, T_COMMA) && !looking_at(c, T_SCLN)) {
    n->type = type(c);
  }
  return n;
}

Function_Declaration *function_declaration(Parse_Context *c) {
  Function_Declaration *n = NODE(c, Function_Declaration);
  assert(false && "TODO");
  return n;
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
    case T_IDENT:
      n->t = TYPE_SCOPED_IDENTIFIER;
      n->scoped_identifier = scoped_identifier(c, FOLLOW_TYPE);
      break;
    default:
      expected(c, n->id, "expected a type");
      advance(c, FOLLOW_TYPE);
      break;
  }
  return n;
}

Collection_Type *collection_type(Parse_Context *c) {
  Collection_Type *n = NODE(c, Collection_Type);
  assert(consume(c).t == T_LBRK);
  n->index_expression = expression(c);
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
  switch (at(c).t) {
    case T_LBRC:
      next(c);
      n->tuple_like = false;
      n->field_list = field_list(c);
      if (!expect(c, n->id, T_RBRC)) {
        advance(c, FOLLOW_TYPE);
        return n;
      }
      break;
    case T_LPAR:
      next(c);
      n->tuple_like = true;
      n->type_list = type_list(c);
      if (!expect(c, n->id, T_RPAR)) {
        advance(c, FOLLOW_TYPE);
        return n;
      }
      break;
    default:
      expected(c, n->id, "expected a struct or tuple body");
      advance(c, FOLLOW_TYPE);
      break;
  }
  return n;
}

Field_List *field_list(Parse_Context *c) {
  Field_List *n = NODE(c, Field_List);
  while (!looking_at(c, T_RBRC)) {
    APPEND(n, field(c));
    if (!expect(c, n->id, T_SCLN)) {
      advance(c, FOLLOW_FIELD_LIST);
      return n;
    }
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

// TODO
Expression *expression(Parse_Context *c) {
  Expression *n = NODE(c, Expression);
  switch (at(c).t) {
    default:
      expected(c, n->id, "expected an expression");
      // TODO: follow set
      next(c);
      break;
  }
  return n;
}
