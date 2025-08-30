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

Tok at(Parse_Context *c) {
  return lex_peek(&c->lex);
}

/*static*/
bool looking_at(Parse_Context *c, Tok_Kind t) {
  return at(c).t == t;
}

typedef struct {
  Tok_Kind *items;
  u32 len;
} Toks;

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

// I know this is smelly but C is very limited for variadic args and not storing
// size with array is really limiting for building abstractions
#define TOKS(...)                                               \
  (Toks) {                                                      \
    .items = (Tok_Kind[]){__VA_ARGS__},                         \
    .len = sizeof((Tok_Kind[]){__VA_ARGS__}) / sizeof(Tok_Kind) \
  }

// These were manually calculated by looking at the tree-sitter grammar.
// I really fucking hope it was worth the effort.
static const Toks FOLLOW_IMPORT = TOKS(T_IMPORT, T_LET, T_MUT, T_FUN, T_STRUCT, T_ENUM, T_ERROR, T_UNION, T_TYPE, T_USE);
static const Toks FOLLOW_DECLARATION = TOKS(T_LET, T_MUT, T_FUN, T_STRUCT, T_ENUM, T_ERROR, T_UNION, T_TYPE, T_USE);
static const Toks FOLLOW_VARIABLE_DECLARATION = FOLLOW_DECLARATION;
static const Toks FOLLOW_VARIABLE_BINDING = TOKS(T_COMMA);
static const Toks FOLLOW_TYPE = TOKS(T_COMMA, T_RPAR, T_SCLN, T_RBRC, T_LBRC, T_EQ);
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
  n->variable_list = variable_list(c);
  if (looking_at(c, T_EQ)) {
    n->init.assign_token = at(c);
    n->init.expression = expression(c);
  }
  if (!expect(c, n->id, T_SCLN)) {
    advance(c, FOLLOW_VARIABLE_DECLARATION);
    return n;
  }
  return n;
}

Variable_List *variable_list(Parse_Context *c) {
  Variable_List *n = NODE(c, Variable_List);
  Variable_Binding *binding = variable_binding(c, T_COMMA);
  while (looking_at(c, T_COMMA)) {
    APPEND(n, binding);
    binding = variable_binding(c, T_COMMA);
  }
  return n;
}

Variable_Binding *variable_binding(Parse_Context *c, Tok_Kind delim) {
  Variable_Binding *n = NODE(c, Variable_Binding);
  Tok name = at(c);
  if (!expect(c, n->id, T_IDENT)) {
      advance(c, FOLLOW_VARIABLE_BINDING);
      return n;
  }
  n->identifier = name;
  if (!looking_at(c, delim)) {
    n->type = type(c);
  }
  return n;
}

// TODO
Type *type(Parse_Context *c) {
  Type *n = NODE(c, Type);
  switch (at(c).t) {
    default:
      expected(c, n->id, "expected a type");
      advance(c, FOLLOW_TYPE);
      break;
  }
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

// FOLLOW sets (make sure to keep these up to date!)
// static const Toks FOLLOW_DECL = TOKS(T_FUN, T_MUT, T_LET, T_IF, T_FOR, T_IDENT);
// static const Toks FOLLOW_STMT = TOKS(T_FUN, T_MUT, T_LET, T_IF, T_FOR, T_IDENT);
// static const Toks FOLLOW_EXPR = TOKS(T_SCLN);
//
// Module *parse_module(Parse_Context *c) {
//   Module *mod = NEW(&c->arena, Module);
//   mod->id = new_node_id(c);
//   while (!looking_at(c, T_EOF)) {
//     APPEND(mod, parse_decl(c));
//   }
//   arena_own(&c->arena, mod->items, mod->cap);
//   return mod;
// }
//
// // TODO: support more types
// Type *parse_type(Parse_Context *c) {
//   Tok tok = lex_peek(&c->lex);
//   Type *type = NEW(&c->arena, Type);
//   type->id = new_node_id(c);
//   type->t = TYPE_BUILTIN;
//   switch (tok.t) {
//     case T_S32:
//       type->builtin = BUILTIN_S32;
//       next(c);
//       break;
//     default: {
//       expected(c, type->id, "builtin type");
//       advance(c, TOKS(T_RPAR, T_EQ, T_COMMA));
//       break;
//     }
//   }
//   return type;
// }
//
// // function_argument_list : function_argument ','? function_argument_list
// //               		      |
// // 			  		            ;
// static Argument_List *parse_fun_args(Parse_Context *c) {
//   Tok tok = lex_peek(&c->lex);
//   Argument_List *args = NEW(&c->arena, Argument_List);
//   args->id = new_node_id(c);
//
//   while (looking_at(c, T_IDENT)) {
//     Argument arg = {0};
//     arg.id = new_node_id(c);
//     arg.name = tok;
//     next(c);
//     arg.type = parse_type(c);
//     if (looking_at(c, T_EOF)) {
//       break;
//     }
//     if (looking_at(c, T_COMMA)) {
//       tok = tnext(c);
//     }
//   }
//
//   arena_own(&c->arena, args->items, args->cap);
//
//   return args;
// }
//
// If_Statement *parse_if_stmt(Parse_Context *c) {
//   (void)c;
//   return NULL;
// }
//
// // assignment_statement : IDENT '=' NUM ';'
// Assign_Statement *parse_assign_stmt(Parse_Context *c) {
//   Assign_Statement *stmt = NEW(&c->arena, Assign_Statement);
//   stmt->id = new_node_id(c);
//   if (!expect(c, stmt->id, T_IDENT)) {
//     advance(c, FOLLOW_STMT);
//     return stmt;
//   }
//   stmt->lhs = lex_peek(&c->lex);
//   if (!expect(c, stmt->id, T_EQ)) {
//     advance(c, FOLLOW_STMT);
//     return stmt;
//   }
//   stmt->rhs = parse_expr(c);
//   if (looking_at(c, T_EOF)) {
//     add_node_flags(c, stmt->id, NFLAG_ERROR);
//     return stmt;
//   }
//   if (!expect(c, stmt->id, T_SCLN)) {
//     advance(c, FOLLOW_STMT);
//     return stmt;
//   }
//   return stmt;
// }
//
// // statement : declaration
// // 		       | compound_statement
// // 		       | assignment_statement
// // 		       | if_statement
// // 		       ;
// Statement parse_stmt(Parse_Context *c) {
//   Statement stmt;
//   stmt.id = new_node_id(c);
//   Tok tok = lex_peek(&c->lex);
//   switch (tok.t) {
//     case T_FUN:
//     case T_MUT:
//     case T_LET:
//       stmt.t = STMT_DECL;
//       stmt.decl = parse_decl(c);
//       break;
//     case T_LBRC:
//       stmt.t = STMT_COMP;
//       stmt.compound = parse_compound_stmt(c);
//       break;
//     case T_IF:
//       stmt.t = STMT_IF;
//       stmt.if_ = parse_if_stmt(c);
//       break;
//     default:
//       stmt.t = STMT_ASSIGN;
//       stmt.assign = parse_assign_stmt(c);
//       break;
//   }
//   return stmt;
// }
//
// //   while (lex_peek(&c->lex).t != T_RBRC) {  // follow set of <statement_list>
// //                                            // make sure to update if other legal
// //                                            // delimiters are added to grammar
// //     if (lex_peek(&c->lex).t == T_EOF) {
// //       // EOF is currently not legal follow token
// //       string eof = tok_to_string[T_EOF];
// //       reportf(c->lex.source, c->lex.cursor,
// //               "syntax error: unexpected %.*s while parsing statements", eof.len,
// //               eof.data);
// //       add_node_flags(c, list->id, NFLAG_ERROR);
// //       goto out;
// //     }
// //     APPEND(list, parse_stmt(c));
// //   }
// // out:
//
// // statement_list : statement statement_list
// //                |
// // 			          ;
// Statement_List *parse_stmt_list(Parse_Context *c) {
//   Statement_List *list = NEW(&c->arena, Statement_List);
//   list->id = new_node_id(c);
//   while (!looking_at(c, T_RBRC) && !looking_at(c, T_EOF)) {
//     APPEND(list, parse_stmt(c));
//   }
//   arena_own(&c->arena, list->items, list->cap);
//   return list;
// }
//
// // compound_statement : '{' statement_list '}'
// // 				            ;
// //
// Compound_Statement *parse_compound_stmt(Parse_Context *c) {
//   Compound_Statement *stmt = NEW(&c->arena, Compound_Statement);
//   stmt->id = new_node_id(c);
//   if (!expect(c, stmt->id, T_LBRC)) {
//     advance(c, FOLLOW_STMT);
//     return stmt;
//   }
//   stmt->statements = parse_stmt_list(c);
//   if (!expect(c, stmt->id, T_RBRC)) {
//     advance(c, FOLLOW_STMT);
//     return stmt;
//   }
//   return stmt;
// }
//
// // function_declaration :
// //  'fun' IDENT '(' function_argument_list ') type? compound_statement ;
// Function_Declaration *parse_fun_decl(Parse_Context *c) {
//   Function_Declaration *decl = NEW(&c->arena, Function_Declaration);
//   decl->id = new_node_id(c);
//
//   if (!expect(c, decl->id, T_FUN)) {
//     advance(c, FOLLOW_DECL);
//     return decl;
//   }
//   Tok tok = lex_peek(&c->lex);
//   if (!expect(c, decl->id, T_IDENT)) {
//     advance(c, FOLLOW_DECL);
//     return decl;
//   }
//   decl->name = tok;
//   if (!expect(c, decl->id, T_LPAR)) {
//     advance(c, FOLLOW_DECL);
//     return decl;
//   }
//   decl->args = parse_fun_args(c);
//   if (!expect(c, decl->id, T_RPAR)) {
//     advance(c, FOLLOW_DECL);
//     return decl;
//   }
//
//   if (!looking_at(c, T_LBRC)) {
//     // if not '{', it must be a type
//     decl->return_type = parse_type(c);
//   }
//   // if still not '{' early return
//   if (!looking_at(c, T_LBRC)) {
//     advance(c, FOLLOW_DECL);
//     return decl;
//   }
//   decl->body = parse_compound_stmt(c);
//
//   return decl;
// }
//
// // variable_declartion : ('let' | 'mut') IDENT type? '=' expr ';'
// //                     | ('let' | 'mut') IDENT type? ';'
// //                     ;
// Variable_Declaration *parse_var_decl(Parse_Context *c) {
//   Variable_Declaration *decl = NEW(&c->arena, Variable_Declaration);
//   decl->id = new_node_id(c);
//
//   switch (lex_peek(&c->lex).t) {
//     case T_MUT:
//       decl->is_mut = true;
//       break;
//     case T_LET:
//       decl->is_mut = false;
//       break;
//     default:
//       expected(c, decl->id, "expected let or mut");
//       advance(c, FOLLOW_DECL);
//       return decl;
//   }
//
//   Tok tok = tnext(c);
//   if (!expect(c, decl->id, T_IDENT)) {
//     advance(c, FOLLOW_DECL);
//     return decl;
//   }
//   decl->name = tok;
//
//   switch (lex_peek(&c->lex).t) {
//     case T_SCLN:
//       return decl;
//     case T_EQ:
//       break;
//     default:
//       decl->type = parse_type(c);
//       if (looking_at(c, T_EOF)) {
//         add_node_flags(c, decl->id, NFLAG_ERROR);
//         return decl;
//       }
//       break;
//   }
//
//   if (!expect(c, decl->id, T_EQ)) {
//     advance(c, FOLLOW_DECL);
//     return decl;
//   }
//
//   decl->init = parse_expr(c);
//   if (!expect(c, decl->id, T_SCLN)) {
//     advance(c, FOLLOW_DECL);
//     return decl;
//   }
//
//   return decl;
// }
//
// // declaration : variable_declaration
// // 	       | function_declaration
// // 	       ;
// Declaration *parse_decl(Parse_Context *c) {
//   Declaration *decl = NEW(&c->arena, Declaration);
//
//   decl->id = new_node_id(c);
//
//   Tok tok = lex_peek(&c->lex);
//   switch (tok.t) {
//     case T_FUN: {
//       decl->t = DECL_FUN;
//       decl->fun = parse_fun_decl(c);
//     } break;
//     case T_MUT:
//     case T_LET: {
//       decl->t = DECL_VAR;
//       decl->var = parse_var_decl(c);
//     } break;
//     default:
//       expected(c, decl->id, "expected start of declaration");
//       advance(c, FOLLOW_DECL);
//       break;
//   }
//
//   return decl;
// }
//
// // expr : NUM
// //      ;
// Expr *parse_expr(Parse_Context *c) {
//   Expr *expr = NEW(&c->arena, Expr);
//   expr->id = new_node_id(c);
//   expr->t = EXPR_LIT;
//   Tok tok = lex_peek(&c->lex);
//   if (!expect(c, expr->id, T_NUM)) {
//     advance(c, FOLLOW_EXPR);
//     return expr;
//   }
//   expr->literal = tok;
//   return expr;
// }
