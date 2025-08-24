#ifndef AST_H
#define AST_H

#include "../common/common.h"
#include "../lex/lex.h"

struct Type;
struct Expr;
struct Statement;
struct If_Statement;
struct Assign_Statement;
struct Statement_List;
struct Compound_Statement;
struct Else_Branch;
struct Argument;
struct Argument_List;
struct Function_Declaration;
struct Variable_Declaration;
struct Declaration;

// For storing metadata like the resolved type or the location (span) of
// ast nodes we store a handle.
typedef u32 Node_ID;

typedef enum {
  BUILTIN_S32,
} Builtin_Type;

typedef enum {
  TYPE_BUILTIN,
} Type_Kind;

typedef struct Type {
  Node_ID id;
  Type_Kind t;
  // TODO: add more types
  union {
    Builtin_Type builtin;
  };
} Type;

typedef enum {
  EXPR_ADD,
  EXPR_MUL,
  EXPR_SUB,
  EXPR_NEG,
  EXPR_POS,
  EXPR_LIT,
} Expr_Kind;

typedef struct Expr {
  Node_ID id;
  Expr_Kind t;
  union {
    struct {
      Tok op;
      struct Expr *operand;
    } unary;
    struct {
      Tok op;
      struct Expr *left;
      struct Expr *right;
    } binary;
    Tok literal;
    // TODO: add postfix expressions like array access and slicing + function
    // calls
  };
} Expr;

typedef struct Statement_List {
  Node_ID id;
  struct Statement *items;
  u32 cap;
  u32 len;
} Statement_List;

typedef struct Compound_Statement {
  Node_ID id;
  Statement_List *statements;
} Compound_Statement;

typedef enum {
  ELSE_IF,
  ELSE_COMPOUND,
} Else_Branch_Kind;

typedef struct Else_Branch {
  Node_ID id;
  Else_Branch_Kind t;
  union {
    struct If_Statement *if_;
    Compound_Statement *statements;
  };
} Else_Branch;

typedef struct If_Statement {
  Node_ID id;
  Expr *condition;
  Compound_Statement *true_;
  Else_Branch *false_;
} If_Statement;

typedef struct Assign_Statement {
  Node_ID id;
  Tok lhs;  // TODO: allow *x = 10 or x[0] = 10
  Expr *rhs;
} Assign_Statement;

typedef enum {
  STMT_IF,
  STMT_ASSIGN,
  STMT_DECL,
  STMT_COMP,
} Statement_Kind;

typedef struct Statement {
  Node_ID id;
  Statement_Kind t;
  union {
    If_Statement *if_;
    Assign_Statement *assign;
    struct Declaration *decl;
    struct Compound_Statement *compound;
  };
} Statement;

typedef struct Argument {
  Node_ID id;
  Tok name;
  Type *type;
} Argument;

typedef struct Argument_List {
  Node_ID id;
  Argument *items;
  u32 cap;
  u32 len;
} Argument_List;

typedef struct Variable_Declaration {
  Node_ID id;
  Tok name;
  Type *type;  // nullable *
  Expr *init;  // nullable *
  bool is_mut;
} Variable_Declaration;

typedef struct Function_Declaration {
  Node_ID id;
  Tok name;
  Argument_List *args;
  Type *return_type;
  Compound_Statement *body;
} Function_Declaration;

typedef enum {
  DECL_VAR,
  DECL_FUN,
} Declaration_Kind;

typedef struct Declaration {
  Node_ID id;
  Declaration_Kind t;
  union {
    Variable_Declaration *var;
    Function_Declaration *fun;
  };
} Declaration;

typedef struct {
  Node_ID id;
  Declaration **items;
  u32 len;
  u32 cap;
} Module;

#endif
