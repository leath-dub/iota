#include "syn.h"

#include <assert.h>

#include "../ast/ast.h"
#include "../lex/lex.h"

static Node_ID new_node_id(Parse_Context *c) {
  Node_ID next = c->next_id++;
  APPEND(&c->flags, 0);
  return next;
}

Parse_Context new_parse_context(Source_Code code) {
  return (Parse_Context){
      .lex = new_lexer(code),
      .arena = new_arena(),
      .next_id = 0,
  };
}

void parse_context_free(Parse_Context *c) {
  if (c->flags.items != NULL) {
    free(c->flags.items);
  }
  arena_free(&c->arena);
}

// static void expected_one_of(Parse_Context *c, Tok_Kind *toks, u32 count) {
//   Tok_Kind curr = lex_peek(&c->lex).t;
//   report(c->lex.source, c->lex.cursor);
//   string curr_tok_str = tok_to_string[curr];
//   char *expected_text;
//   if (count == 1) {
//     expected_text = "expected";
//   } else {
//     expected_text = "expected:";
//   }
//   errorf(c->lex.source, "syntax error: unexpected %.*s, %s ", curr_tok_str.len,
//          curr_tok_str.data, expected_text);
//   for (u32 i = 0; i < count; i++) {
//     string tok_str = tok_to_string[toks[i]];
//     char *delim;
//     if (i == 0) {
//       delim = "";
//     } else if (i == count - 1) {
//       delim = " or ";
//     } else {
//       delim = ", ";
//     }
//     errorf(c->lex.source, "%s%.*s", delim, tok_str.len, tok_str.data);
//   }
//   errorf(c->lex.source, "\n");
// }

// static bool skip_if(Parse_Context *c, Tok_Kind t) {
//   Tok tok = lex_peek(&c->lex);
//   bool match = tok.t == t;
//   if (match) {
//     lex_consume(&c->lex);
//   }
//   return match;
// }
//
// static bool skip(Parse_Context *c, Tok_Kind t) {
//   bool match = skip_if(c, t);
//   if (!match) {
//     Tok_Kind toks[] = {t};
//     expected_one_of(c, toks, LEN(toks));
//   }
//   return match;
// }

static void add_node_flags(Parse_Context *c, Node_ID id, Node_Flags flags) {
  c->flags.items[id] |= flags;
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

static bool looking_at(Parse_Context *c, Tok_Kind t) {
  return lex_peek(&c->lex).t == t;
}

typedef struct {
  Tok_Kind *items;
  u32 len;
} Toks;

static void advance(Parse_Context *c, Toks toks) {
  for (Tok tok = lex_peek(&c->lex); tok.t != T_EOF; tok = tnext(c)) {
    for (u32 i = 0; i < toks.len; i++) {
      if (tok.t == toks.items[i]) {
        return;
      }
    }
  }
}

static void expected(Parse_Context *c, Node_ID in, const char *msg) {
  Tok tok = lex_peek(&c->lex);
  reportf(c->lex.source, c->lex.cursor, "syntax error: expected %s, found %s",
          msg, tok_to_string[tok.t].data);
  add_node_flags(c, in, NFLAG_ERROR);
}

static bool expect(Parse_Context *c, Node_ID in, Tok_Kind t) {
  Tok tok = lex_peek(&c->lex);
  bool match = tok.t == t;
  if (!match) {
    expected(c, in, tok_to_string[t].data);
  } else {
    next(c);
  }
  return match;
}


// #define MAX_RECOVER_SCAN 20

// static bool recover_at(Parse_Context *c, Tok_Kind *toks, u32 count) {
//   Lexer tmp = c->lex;
//   Tok tok = lex_peek(&tmp);
//   for (u32 tc = 0; tc < MAX_RECOVER_SCAN && tok.t != T_EOF; tc++) {
//     for (u32 i = 0; i < count; i++) {
//       if (tok.t == toks[i]) {
//         // safe to commit the scan as we found a recovery token
//         c->lex = tmp;
//         return true;
//       }
//     }
//     lex_consume(&tmp);
//     tok = lex_peek(&tmp);
//   }
//   return false;
// }

// typedef struct {
//   Toks expected;
//   Toks recovery;
// } Expect_Args;

// static bool expect(Parse_Context *c, Expect_Args args) {
//   Tok tok = lex_peek(&c->lex);
//   for (u32 i = 0; i < args.expected.len; i++) {
//     if (tok.t == args.expected.items[i]) {
//       return true;
//     }
//   }
//   expected_one_of(c, args.expected.items, args.expected.len);
//   if (recover_at(c, args.recovery.items, args.recovery.len)) {
//     lex_consume(&c->lex);
//   }
//   return false;
// }

// I know this is smelly but C is very limited for variadic args and not storing
// size with array is really limiting for building abstractions
#define TOKS(...)                                               \
  (Toks) {                                                      \
    .items = (Tok_Kind[]){__VA_ARGS__},                         \
    .len = sizeof((Tok_Kind[]){__VA_ARGS__}) / sizeof(Tok_Kind) \
  }
// #define EXPECT(c, ...) expect(c, (Expect_Args){__VA_ARGS__})

// FOLLOW sets (make sure to keep these up to date!)
static const Toks FOLLOW_DECL = TOKS(T_FUN, T_MUT, T_LET, T_IF, T_FOR, T_IDENT);
static const Toks FOLLOW_STMT = TOKS(T_FUN, T_MUT, T_LET, T_IF, T_FOR, T_IDENT);
static const Toks FOLLOW_EXPR = TOKS(T_SCLN);

Module *parse_module(Parse_Context *c) {
  Module *mod = NEW(&c->arena, Module);
  mod->id = new_node_id(c);
  while (!looking_at(c, T_EOF)) {
    APPEND(mod, parse_decl(c));
  }
  arena_own(&c->arena, mod->items, mod->cap);
  return mod;
}

// TODO: support more types
Type *parse_type(Parse_Context *c) {
  Tok tok = lex_peek(&c->lex);
  Type *type = NEW(&c->arena, Type);
  type->id = new_node_id(c);
  type->t = TYPE_BUILTIN;
  switch (tok.t) {
    case T_S32:
      type->builtin = BUILTIN_S32;
      next(c);
      break;
    default: {
      expected(c, type->id, "builtin type");
      advance(c, TOKS(T_RPAR, T_EQ, T_COMMA));
      break;
    }
  }
  return type;
}

// function_argument_list : function_argument ','? function_argument_list
//               		      |
// 			  		            ;
static Argument_List *parse_fun_args(Parse_Context *c) {
  Tok tok = lex_peek(&c->lex);
  Argument_List *args = NEW(&c->arena, Argument_List);
  args->id = new_node_id(c);

  while (looking_at(c, T_IDENT)) {
    Argument arg = {0};
    arg.id = new_node_id(c);
    arg.name = tok;
    next(c);
    arg.type = parse_type(c);
    if (looking_at(c, T_EOF)) {
      break;
    }
    if (looking_at(c, T_COMMA)) {
      tok = tnext(c);
    }
  }

  arena_own(&c->arena, args->items, args->cap);

  return args;
}

If_Statement *parse_if_stmt(Parse_Context *c) {
  (void)c;
  return NULL;
}

// assignment_statement : IDENT '=' NUM ';'
Assign_Statement *parse_assign_stmt(Parse_Context *c) {
  Assign_Statement *stmt = NEW(&c->arena, Assign_Statement);
  stmt->id = new_node_id(c);
  if (!expect(c, stmt->id, T_IDENT)) {
    advance(c, FOLLOW_STMT);
    return stmt;
  }
  stmt->lhs = lex_peek(&c->lex);
  if (!expect(c, stmt->id, T_EQ)) {
    advance(c, FOLLOW_STMT);
    return stmt;
  }
  stmt->rhs = parse_expr(c);
  if (looking_at(c, T_EOF)) {
    add_node_flags(c, stmt->id, NFLAG_ERROR);
    return stmt;
  }
  if (!expect(c, stmt->id, T_SCLN)) {
    advance(c, FOLLOW_STMT);
    return stmt;
  }
  return stmt;
}

// statement : declaration
// 		       | compound_statement
// 		       | assignment_statement
// 		       | if_statement
// 		       ;
Statement parse_stmt(Parse_Context *c) {
  Statement stmt;
  stmt.id = new_node_id(c);
  Tok tok = lex_peek(&c->lex);
  switch (tok.t) {
    case T_FUN:
    case T_MUT:
    case T_LET:
      stmt.t = STMT_DECL;
      stmt.decl = parse_decl(c);
      break;
    case T_LBRC:
      stmt.t = STMT_COMP;
      stmt.compound = parse_compound_stmt(c);
      break;
    case T_IF:
      stmt.t = STMT_IF;
      stmt.if_ = parse_if_stmt(c);
      break;
    default:
      stmt.t = STMT_ASSIGN;
      stmt.assign = parse_assign_stmt(c);
      break;
  }
  return stmt;
}

//   while (lex_peek(&c->lex).t != T_RBRC) {  // follow set of <statement_list>
//                                            // make sure to update if other legal
//                                            // delimiters are added to grammar
//     if (lex_peek(&c->lex).t == T_EOF) {
//       // EOF is currently not legal follow token
//       string eof = tok_to_string[T_EOF];
//       reportf(c->lex.source, c->lex.cursor,
//               "syntax error: unexpected %.*s while parsing statements", eof.len,
//               eof.data);
//       add_node_flags(c, list->id, NFLAG_ERROR);
//       goto out;
//     }
//     APPEND(list, parse_stmt(c));
//   }
// out:

// statement_list : statement statement_list
//                |
// 			          ;
Statement_List *parse_stmt_list(Parse_Context *c) {
  Statement_List *list = NEW(&c->arena, Statement_List);
  list->id = new_node_id(c);
  while (!looking_at(c, T_RBRC) && !looking_at(c, T_EOF)) {
    APPEND(list, parse_stmt(c));
  }
  arena_own(&c->arena, list->items, list->cap);
  return list;
}

// compound_statement : '{' statement_list '}'
// 				            ;
//
Compound_Statement *parse_compound_stmt(Parse_Context *c) {
  Compound_Statement *stmt = NEW(&c->arena, Compound_Statement);
  stmt->id = new_node_id(c);
  if (!expect(c, stmt->id, T_LBRC)) {
    advance(c, FOLLOW_STMT);
    return stmt;
  }
  stmt->statements = parse_stmt_list(c);
  if (!expect(c, stmt->id, T_RBRC)) {
    advance(c, FOLLOW_STMT);
    return stmt;
  }
  return stmt;
}

// function_declaration :
//  'fun' IDENT '(' function_argument_list ') type? compound_statement ;
Function_Declaration *parse_fun_decl(Parse_Context *c) {
  Function_Declaration *decl = NEW(&c->arena, Function_Declaration);
  decl->id = new_node_id(c);

  if (!expect(c, decl->id, T_FUN)) {
    advance(c, FOLLOW_DECL);
    return decl;
  }
  Tok tok = lex_peek(&c->lex);
  if (!expect(c, decl->id, T_IDENT)) {
    advance(c, FOLLOW_DECL);
    return decl;
  }
  decl->name = tok;
  if (!expect(c, decl->id, T_LPAR)) {
    advance(c, FOLLOW_DECL);
    return decl;
  }
  decl->args = parse_fun_args(c);
  if (!expect(c, decl->id, T_RPAR)) {
    advance(c, FOLLOW_DECL);
    return decl;
  }

  if (!looking_at(c, T_LBRC)) {
    // if not '{', it must be a type
    decl->return_type = parse_type(c);
  }
  // if still not '{' early return
  if (!looking_at(c, T_LBRC)) {
    advance(c, FOLLOW_DECL);
    return decl;
  }
  decl->body = parse_compound_stmt(c);

  return decl;
}

// variable_declartion : ('let' | 'mut') IDENT type? '=' expr ';'
//                     | ('let' | 'mut') IDENT type? ';'
//                     ;
Variable_Declaration *parse_var_decl(Parse_Context *c) {
  Variable_Declaration *decl = NEW(&c->arena, Variable_Declaration);
  decl->id = new_node_id(c);

  switch (lex_peek(&c->lex).t) {
    case T_MUT:
      decl->is_mut = true;
      break;
    case T_LET:
      decl->is_mut = false;
      break;
    default:
      expected(c, decl->id, "expected let or mut");
      advance(c, FOLLOW_DECL);
      return decl;
  }

  Tok tok = tnext(c);
  if (!expect(c, decl->id, T_IDENT)) {
    advance(c, FOLLOW_DECL);
    return decl;
  }
  decl->name = tok;

  switch (lex_peek(&c->lex).t) {
    case T_SCLN:
      return decl;
    case T_EQ:
      break;
    default:
      decl->type = parse_type(c);
      if (looking_at(c, T_EOF)) {
        add_node_flags(c, decl->id, NFLAG_ERROR);
        return decl;
      }
      break;
  }

  if (!expect(c, decl->id, T_EQ)) {
    advance(c, FOLLOW_DECL);
    return decl;
  }

  decl->init = parse_expr(c);
  if (!expect(c, decl->id, T_SCLN)) {
    advance(c, FOLLOW_DECL);
    return decl;
  }

  return decl;
}

// declaration : variable_declaration
// 	       | function_declaration
// 	       ;
Declaration *parse_decl(Parse_Context *c) {
  Declaration *decl = NEW(&c->arena, Declaration);

  decl->id = new_node_id(c);

  Tok tok = lex_peek(&c->lex);
  switch (tok.t) {
    case T_FUN: {
      decl->t = DECL_FUN;
      decl->fun = parse_fun_decl(c);
    } break;
    case T_MUT:
    case T_LET: {
      decl->t = DECL_VAR;
      decl->var = parse_var_decl(c);
    } break;
    default:
      expected(c, decl->id, "expected start of declaration");
      advance(c, FOLLOW_DECL);
      break;
  }

  return decl;
}

// expr : NUM
//      ;
Expr *parse_expr(Parse_Context *c) {
  Expr *expr = NEW(&c->arena, Expr);
  expr->id = new_node_id(c);
  expr->t = EXPR_LIT;
  Tok tok = lex_peek(&c->lex);
  if (!expect(c, expr->id, T_NUM)) {
    advance(c, FOLLOW_EXPR);
    return expr;
  }
  expr->literal = tok;
  return expr;
}
