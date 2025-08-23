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

static void expected_one_of(Parse_Context *c, Tok_Kind *toks, u32 count) {
  Tok_Kind curr = lex_peek(&c->lex).t;
  report(c->lex.source, c->lex.cursor);
  string curr_tok_str = tok_to_string[curr];
  char *expected_text;
  if (count == 1) {
    expected_text = "expected";
  } else {
    expected_text = "expected:";
  }
  errorf(c->lex.source, "syntax error: unexpected %.*s, %s ", curr_tok_str.len,
         curr_tok_str.data, expected_text);
  for (u32 i = 0; i < count; i++) {
    string tok_str = tok_to_string[toks[i]];
    char *delim;
    if (i == 0) {
      delim = "";
    } else if (i == count - 1) {
      delim = " or ";
    } else {
      delim = ", ";
    }
    errorf(c->lex.source, "%s%.*s", delim, tok_str.len, tok_str.data);
  }
  errorf(c->lex.source, "\n");
}

static bool skip_if(Parse_Context *c, Tok_Kind t) {
  Tok tok = lex_peek(&c->lex);
  bool match = tok.t == t;
  if (match) {
    lex_consume(&c->lex);
  }
  return match;
}

static bool skip(Parse_Context *c, Tok_Kind t) {
  bool match = skip_if(c, t);
  if (!match) {
    Tok_Kind toks[] = {t};
    expected_one_of(c, toks, LEN(toks));
  }
  return match;
}

static void add_node_flags(Parse_Context *c, Node_ID id, Node_Flags flags) {
  c->flags.items[id] |= flags;
}

#define MAX_RECOVER_SCAN 20

static bool recover_at(Parse_Context *c, Tok_Kind *toks, u32 count) {
  Lexer tmp = c->lex;
  Tok tok = lex_peek(&tmp);
  for (u32 tc = 0; tc < MAX_RECOVER_SCAN && tok.t != T_EOF; tc++) {
    for (u32 i = 0; i < count; i++) {
      if (tok.t == toks[i]) {
        // safe to commit the scan as we found a recovery token
        c->lex = tmp;
        return true;
      }
    }
    lex_consume(&tmp);
    tok = lex_peek(&tmp);
  }
  return false;
}

typedef struct {
  Tok_Kind *items;
  u32 len;
} Toks;

typedef struct {
  Toks expected;
  Toks recovery;
} Expect_Args;

static bool expect(Parse_Context *c, Expect_Args args) {
  Tok tok = lex_peek(&c->lex);
  for (u32 i = 0; i < args.expected.len; i++) {
    if (tok.t == args.expected.items[i]) {
      return true;
    }
  }
  expected_one_of(c, args.expected.items, args.expected.len);
  if (recover_at(c, args.recovery.items, args.recovery.len)) {
    lex_consume(&c->lex);
  }
  return false;
}

// I know this is smelly but C is very limited for variadic args and not storing
// size with array is really limiting for building abstractions
#define TOKS(...)                                               \
  (Toks) {                                                      \
    .items = (Tok_Kind[]){__VA_ARGS__},                         \
    .len = sizeof((Tok_Kind[]){__VA_ARGS__}) / sizeof(Tok_Kind) \
  }
#define EXPECT(c, ...) expect(c, (Expect_Args){__VA_ARGS__})

Module *parse_module(Parse_Context *c) {
  Module *mod = NEW(&c->arena, Module);
  mod->id = new_node_id(c);
  while (lex_peek(&c->lex).t != T_EOF) {
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
      break;
    default: {
      Tok_Kind toks[] = {T_S32};
      expected_one_of(c, toks, LEN(toks));
      add_node_flags(c, type->id, NFLAG_ERROR);
      return type;
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
  if (tok.t == T_RPAR) {
    goto out;
  }

  do {
    tok = lex_peek(&c->lex);

    // Skip comma if not the first element
    if (args->len != 0 && tok.t == T_COMMA) {
      lex_consume(&c->lex);
      // Handle trailing comma
      if (lex_peek(&c->lex).t == T_RPAR) {
        goto out;
      }
      tok = lex_peek(&c->lex);
    }

    Argument arg = {0};
    arg.id = new_node_id(c);

    if (tok.t != T_IDENT) {
      Tok_Kind toks[] = {T_IDENT};
      expected_one_of(c, toks, LEN(toks));
      Tok_Kind recs[] = {T_COMMA, T_RPAR};
      if (!recover_at(c, recs, LEN(recs))) {
        // If we cannot locally recover from error, return to the caller
        // maybe the token makes sense to them!
        add_node_flags(c, args->id, NFLAG_ERROR);
        goto out;
      }
      skip_if(c, T_COMMA);
      add_node_flags(c, arg.id, NFLAG_ERROR);
      APPEND(args, arg);
      continue;
    }

    lex_consume(&c->lex);
    arg.name = tok;
    arg.type = parse_type(c);
    lex_consume(&c->lex);

    APPEND(args, arg);
  } while (lex_peek(&c->lex).t == T_COMMA);

out:
  arena_own(&c->arena, args->items, args->cap);
  return args;
}

If_Statement *parse_if_stmt(Parse_Context *c) {
  (void)c;
  return NULL;
}

Assign_Statement *parse_assign_stmt(Parse_Context *c) {
  (void)c;
  return NULL;
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

// statement_list : statement statement_list
//                |
// 			          ;
Statement_List *parse_stmt_list(Parse_Context *c) {
  Statement_List *list = NEW(&c->arena, Statement_List);
  list->id = new_node_id(c);
  while (lex_peek(&c->lex).t != T_RBRC) {  // follow set of <statement_list>
                                           // make sure to update if other legal
                                           // delimiters are added to grammar
    if (lex_peek(&c->lex).t == T_EOF) {
      // EOF is currently not legal follow token
      string eof = tok_to_string[T_EOF];
      reportf(c->lex.source, c->lex.cursor,
              "syntax error: unexpected %.*s while parsing statements", eof.len,
              eof.data);
      add_node_flags(c, list->id, NFLAG_ERROR);
      goto out;
    }
    APPEND(list, parse_stmt(c));
  }
out:
  arena_own(&c->arena, list->items, list->cap);
  return list;
}

// compound_statement : '{' statement_list '}'
// 				            ;
//
Compound_Statement *parse_compound_stmt(Parse_Context *c) {
  Compound_Statement *stmt = NEW(&c->arena, Compound_Statement);
  stmt->id = new_node_id(c);
  skip(c, T_LBRC);
  stmt->statements = parse_stmt_list(c);
  skip(c, T_RBRC);
  return stmt;
}

// function_declaration :
//  'fun' IDENT '(' function_argument_list ') type? compound_statement ;
Function_Declaration *parse_fun_decl(Parse_Context *c) {
  skip(c, T_FUN);
  Tok tok = lex_peek(&c->lex);

  Function_Declaration *decl = NEW(&c->arena, Function_Declaration);
  decl->id = new_node_id(c);

  if (tok.t != T_IDENT) {
    Tok_Kind toks[] = {T_IDENT};
    expected_one_of(c, toks, LEN(toks));
    Tok_Kind recs[] = {T_LPAR};
    if (!recover_at(c, recs, LEN(recs))) {
      return NULL;
    }
    add_node_flags(c, decl->id, NFLAG_ERROR);
  }
  lex_consume(&c->lex);

  decl->name = tok;
  skip(c, T_LPAR);
  decl->args = parse_fun_args(c);
  skip(c, T_RPAR);

  if (lex_peek(&c->lex).t != T_LBRC) {
    // if not '{', it must be a type
    decl->return_type = parse_type(c);
  }
  // If we still don't have '{', we must be missing a body
  if (lex_peek(&c->lex).t != T_LBRC) {
    reportf(c->lex.source, c->lex.cursor, "missing function body");
    add_node_flags(c, decl->id, NFLAG_ERROR);
    Tok_Kind recover_body[] = {T_LBRC, T_RBRC, T_SCLN};
    if (recover_at(c, recover_body, LEN(recover_body))) {
      // consume recovery tokens if they were hit
      lex_consume(&c->lex);
    }
    return decl;
  }
  decl->body = parse_compound_stmt(c);

  return decl;
}

// variable_declartion : ('let' | 'mut') IDENT type? '=' expr ';'
//                     | ('let' | 'mut') IDENT type? ';'
//                     ;
Variable_Declaration *parse_var_decl(Parse_Context *c) {
  Toks recovery = TOKS(T_SCLN, T_LBRC);

  Variable_Declaration *decl = NEW(&c->arena, Variable_Declaration);
  decl->id = new_node_id(c);

  if (!EXPECT(c, TOKS(T_MUT, T_LET), recovery)) {
    add_node_flags(c, decl->id, NFLAG_ERROR);
    return decl;
  }
  decl->is_mut = lex_peek(&c->lex).t == T_MUT;
  lex_consume(&c->lex);

  Tok tok = lex_peek(&c->lex);
  if (!EXPECT(c, TOKS(T_IDENT), recovery)) {
    add_node_flags(c, decl->id, NFLAG_ERROR);
    return decl;
  }
  lex_consume(&c->lex);
  decl->name = tok;

  tok = lex_peek(&c->lex);
  switch (tok.t) {
    case T_SCLN:
      return decl;
    case T_EQ:
      break;
    default:
      decl->type = parse_type(c);
      tok = lex_peek(&c->lex);
      break;
  }

  if (tok.t != T_EQ) {
    if (!EXPECT(c, TOKS(T_SCLN), recovery)) {
      add_node_flags(c, decl->id, NFLAG_ERROR);
    }
    return decl;
  }

  lex_consume(&c->lex);
  decl->init = parse_expr(c);
  EXPECT(c, TOKS(T_SCLN), recovery);
  lex_consume(&c->lex);

  return decl;
}

// declaration : variable_declaration
// 	       | function_declaration
// 	       ;
Declaration *parse_decl(Parse_Context *c) {
  Tok tok = lex_peek(&c->lex);

  Declaration *decl = NEW(&c->arena, Declaration);

  decl->id = new_node_id(c);

  if (!EXPECT(c, TOKS(T_FUN, T_MUT, T_LET), TOKS(T_SCLN, T_LBRC))) {
    add_node_flags(c, decl->id, NFLAG_ERROR);
    return decl;
  }

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
      break;
  }

  return decl;
}

// expr : NUM
//      ;
Expr *parse_expr(Parse_Context *c) {
  Expr *expr = NEW(&c->arena, Expr);
  expr->id = new_node_id(c);

  if (!EXPECT(c, TOKS(T_NUM), TOKS(T_SCLN, T_RBRC))) {
    add_node_flags(c, expr->id, NFLAG_ERROR);
    return expr;
  }

  expr->t = EXPR_LIT;
  expr->literal = lex_peek(&c->lex);
  lex_consume(&c->lex);

  return expr;
}
