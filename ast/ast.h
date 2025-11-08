#ifndef AST_H
#define AST_H

#include "../common/common.h"
#include "../common/hmtypes.h"
#include "../lex/lex.h"

typedef u32 NodeID;

// There are a bunch of places where we need to do something for every AST node
// type:
//
// * Forward declare all AST nodes - so we don't have to care about order of
// nodes
// * `typedef` all AST nodes - so no need for consumers of `ast` module to type
//   `struct` prefix on each type
// * Add enumerator in Node_Kind enum
//
// For these reasons, The weird and arcane trick known as "X macro" is used
// below (see here for more: info https://en.wikipedia.org/wiki/X_macro).

#define EACH_NODE(USE)                                 \
  USE(SourceFile, SOURCE_FILE, "source_file")          \
  USE(Imports, IMPORTS, "imports")                     \
  USE(Decls, DECLS, "decls")                           \
  USE(Import, IMPORT, "import")                        \
  USE(Decl, DECL, "decl")                              \
  USE(VarDecl, VAR_DECL, "var_decl")                   \
  USE(FnDecl, FN_DECL, "fn_decl")                      \
  USE(TypeParams, TYPE_PARAMS, "type_params")          \
  USE(StructDecl, STRUCT_DECL, "struct_decl")          \
  USE(StructBody, STRUCT_BODY, "struct_body")          \
  USE(EnumDecl, ENUM_DECL, "enum_decl")                \
  USE(ErrDecl, ERR_DECL, "err_decl")                   \
  USE(UnionDecl, UNION_DECL, "union_decl")             \
  USE(VarBinding, VAR_BINDING, "var_binding")          \
  USE(UnpackTuple, UNPACK_TUPLE, "unpack_tuple")       \
  USE(UnpackStruct, UNPACK_STRUCT, "unpack_struct")    \
  USE(UnpackUnion, UNPACK_UNION, "unpack_union")       \
  USE(Binding, BINDING, "binding")                     \
  USE(Bindings, BINDINGS, "bindings")                  \
  USE(AliasBinding, ALIAS_BINDING, "alias_binding")    \
  USE(AliasBindings, ALIAS_BINDINGS, "alias_bindings") \
  USE(FnParams, FN_PARAMS, "fn_params")                \
  USE(FnParam, FN_PARAM, "fn_param")                   \
  USE(Types, TYPES, "types")                           \
  USE(Fields, FIELDS, "fields")                        \
  USE(Field, FIELD, "field")                           \
  USE(Idents, IDENTS, "idents")                        \
  USE(Errs, ERRS, "errs")                              \
  USE(Err, ERR, "err")                                 \
  USE(Stmt, STMT, "stmt")                              \
  USE(IfStmt, IF_STMT, "if_stmt")                      \
  USE(WhileStmt, WHILE_STMT, "while_stmt")             \
  USE(CasePatt, CASE_PATT, "case_patt")                \
  USE(CaseBranch, CASE_BRANCH, "case_branch")          \
  USE(CaseBranches, CASE_BRANCHES, "case_branches")    \
  USE(CaseStmt, CASE_STMT, "case_stmt")                \
  USE(Cond, COND, "cond")                              \
  USE(UnionTagCond, UNION_TAG_COND, "union_tag_cond")  \
  USE(ReturnStmt, RETURN_STMT, "return_stmt")          \
  USE(DeferStmt, DEFER_STATEMENT, "defer_stmt")        \
  USE(CompStmt, COMP_STMT, "comp_stmt")                \
  USE(Else, ELSE, "else")                              \
  USE(Type, TYPE, "type")                              \
  USE(BuiltinType, BUILTIN_TYPE, "builtin_type")       \
  USE(CollType, COLL_TYPE, "coll_type")                \
  USE(StructType, STRUCT_TYPE, "struct_type")          \
  USE(UnionType, UNION_TYPE, "union_type")             \
  USE(EnumType, ENUM_TYPE, "enum_type")                \
  USE(ErrType, ERR_TYPE, "err_type")                   \
  USE(PtrType, PTR_TYPE, "ptr_type")                   \
  USE(FnType, FN_TYPE, "fn_type")                      \
  USE(ScopedIdent, SCOPED_IDENT, "scoped_ident")       \
  USE(Expr, EXPR, "expr")                              \
  USE(Atom, ATOM, "atom")                              \
  USE(Designator, DESIGNATOR, "designator")            \
  USE(PostfixExpr, POSTFIX_EXPR, "postfix_expr")       \
  USE(FnCall, FN_CALL, "fn_call")                      \
  USE(Init, INIT, "init")                              \
  USE(FieldAccess, FIELD_ACCESS, "field_access")       \
  USE(CollAccess, COLL_ACCESS, "coll_access")          \
  USE(UnaryExpr, UNARY_EXPR, "unary_expr")             \
  USE(BinExpr, BIN_EXPR, "bin_expr")                   \
  USE(BracedLit, BRACED_LIT, "braced_lit")             \
  USE(Index, INDEX, "index")

#define FWD_DECL_NODE(NODE, ...) struct NODE;

EACH_NODE(FWD_DECL_NODE)

struct SourceFile {
  NodeID id;
  struct Imports *imports;
  struct Decls *decls;
};

struct Imports {
  NodeID id;
  u32 len;
  u32 cap;
  struct Import **items;
};

struct Import {
  NodeID id;
  struct ScopedIdent *module;
};

struct Decls {
  NodeID id;
  u32 len;
  u32 cap;
  struct Decl **items;
};

typedef enum {
  DECL_VAR,
  DECL_FN,
  DECL_STRUCT,
  DECL_ENUM,
  DECL_ERR,
  DECL_UNION,
} DeclKind;

struct Decl {
  NodeID id;
  DeclKind t;
  union {
    struct VarDecl *var_decl;
    struct FnDecl *fn_decl;
    struct StructDecl *struct_decl;
    struct EnumDecl *enum_decl;
    struct ErrDecl *err_decl;
    struct UnionDecl *union_decl;
  };
};

struct VarDecl {
  NodeID id;
  struct VarBinding *binding;
  struct Type *type;  // nullable
  struct {
    Tok assign_token;
    struct Expr *expr;
    bool ok;
  } init;
};

typedef enum {
  VAR_BINDING_BASIC,
  VAR_BINDING_UNPACK_TUPLE,
  VAR_BINDING_UNPACK_STRUCT,
} VarBindingKind;

struct VarBinding {
  NodeID id;
  VarBindingKind t;
  union {
    Tok basic;
    struct UnpackTuple *unpack_tuple;
    struct UnpackStruct *unpack_struct;
  };
};

struct AliasBinding {
  NodeID id;
  struct Binding *binding;
  MAYBE(Tok) alias;
};

struct AliasBindings {
  NodeID id;
  u32 len;
  u32 cap;
  struct AliasBinding **items;
};

struct Binding {
  NodeID id;
  Tok ident;
  MAYBE(Tok) ref;  // could be a '*' meaning the binding is a reference
  MAYBE(Tok) mod;
};

struct Bindings {
  NodeID id;
  u32 len;
  u32 cap;
  struct Binding **items;
};

struct UnpackTuple {
  NodeID id;
  struct Bindings *bindings;
};

struct UnpackStruct {
  NodeID id;
  struct AliasBindings *bindings;
};

struct UnpackUnion {
  NodeID id;
  Tok tag;
  struct Binding *binding;
};

struct FnDecl {
  NodeID id;
  Tok ident;
  NULLABLE_PTR(struct TypeParams) type_params;
  struct FnParams *params;
  NULLABLE_PTR(struct Type) return_type;
  struct CompStmt *body;
};

struct FnParams {
  NodeID id;
  u32 len;
  u32 cap;
  struct FnParam **items;
};

struct FnParam {
  NodeID id;
  struct VarBinding *binding;
  struct Type *type;
  bool variadic;
};

struct TypeParams {
  NodeID id;
  struct Types *types;
};

struct StructDecl {
  NodeID id;
  Tok ident;
  NULLABLE_PTR(struct TypeParams) type_params;
  struct StructBody *body;
};

struct StructBody {
  NodeID id;
  bool tuple_like;
  union {
    struct Types *types;
    struct Fields *fields;
  };
};

struct Fields {
  NodeID id;
  u32 len;
  u32 cap;
  struct Field **items;
};

struct Field {
  NodeID id;
  Tok ident;
  struct Type *type;
};

struct EnumDecl {
  NodeID id;
  Tok ident;
  struct Idents *alts;
};

struct Idents {
  NodeID id;
  u32 len;
  u32 cap;
  Tok *items;
};

struct ErrDecl {
  NodeID id;
  Tok ident;
  struct Errs *errs;
};

struct Errs {
  NodeID id;
  u32 len;
  u32 cap;
  struct Err **items;
};

struct Err {
  NodeID id;
  bool embedded;  // e.g. !Foo
  struct ScopedIdent *scoped_ident;
};

struct UnionDecl {
  NodeID id;
  Tok ident;
  NULLABLE_PTR(struct TypeParams) type_params;
  struct Fields *alts;
};

typedef enum {
  STMT_DECL,
  STMT_IF,
  STMT_WHILE,
  STMT_CASE,
  STMT_RETURN,
  STMT_COMP,
  STMT_EXPR,
} StmtKind;

struct Stmt {
  NodeID id;
  StmtKind t;
  union {
    struct Decl *decl;
    struct IfStmt *if_stmt;
    struct WhileStmt *while_stmt;
    struct CaseStmt *case_stmt;
    struct ReturnStmt *return_stmt;
    struct Defer_Statement *defer_stmt;
    struct CompStmt *comp_stmt;
    struct Expr *expr;
  };
};

struct IfStmt {
  NodeID id;
  struct Cond *cond;
  struct CompStmt *true_branch;
  NULLABLE_PTR(struct Else) else_branch;
};

struct WhileStmt {
  NodeID id;
  struct Cond *cond;
  struct CompStmt *true_branch;
};

typedef enum {
  CASE_PATT_LIT,
  CASE_PATT_NAME,
  CASE_PATT_DEFAULT,
} CasePattKind;

struct CasePatt {
  NodeID id;
  CasePattKind t;
  union {
    Tok lit;
    Tok default_;
    struct {
      struct ScopedIdent *ident;
      NULLABLE_PTR(struct Binding) binding;
    } name;
  };
};

struct CaseBranch {
  NodeID id;
  struct CasePatt *patt;
  struct Stmt *action;
};

struct CaseBranches {
  NodeID id;
  u32 len;
  u32 cap;
  struct CaseBranch **items;
};

struct CaseStmt {
  NodeID id;
  struct Expr *expr;
  struct CaseBranches *branches;
};

typedef enum {
  COND_UNION_TAG,
  COND_EXPR,
} CondKind;

struct Cond {
  NodeID id;
  CondKind t;
  union {
    struct UnionTagCond *union_tag;
    struct Expr *expr;
  };
};

struct UnionTagCond {
  NodeID id;
  struct UnpackUnion *trigger;
  Tok assign_token;
  struct Expr *expr;
};

typedef enum {
  ELSE_IF,
  ELSE_COMP,
} ElseKind;

struct Else {
  NodeID id;
  ElseKind t;
  union {
    struct IfStmt *if_stmt;
    struct CompStmt *comp_stmt;
  };
};

struct ReturnStmt {
  NodeID id;
  NULLABLE_PTR(struct Expr) expr;
};

struct DeferStmt {
  NodeID id;
  struct Stmt *stmt;
};

struct CompStmt {
  NodeID id;
  u32 len;
  u32 cap;
  struct Stmt **items;
};

typedef enum {
  TYPE_BUILTIN,
  TYPE_COLL,
  TYPE_STRUCT,
  TYPE_UNION,
  TYPE_ENUM,
  TYPE_ERR,
  TYPE_PTR,
  TYPE_FN,
  TYPE_SCOPED_IDENT,
} TypeKind;

struct Type {
  NodeID id;
  TypeKind t;
  union {
    struct BuiltinType *builtin_type;
    struct CollType *coll_type;
    struct StructType *struct_type;
    struct UnionType *union_type;
    struct EnumType *enum_type;
    struct ErrType *err_type;
    struct PtrType *ptr_type;
    struct FnType *fn_type;
    struct ScopedIdent *scoped_ident;
  };
};

struct BuiltinType {
  NodeID id;
  Tok token;  // just stores the keyword token
};

struct CollType {
  NodeID id;
  NULLABLE_PTR(struct Expr) index_expr;
  struct Type *element_type;
};

struct StructType {
  NodeID id;
  NULLABLE_PTR(struct TypeParams) type_params;
  struct StructBody *body;
};

struct Types {
  NodeID id;
  u32 len;
  u32 cap;
  struct Type **items;
};

struct UnionType {
  NodeID id;
  NULLABLE_PTR(struct TypeParams) type_params;
  struct Fields *fields;
};

struct EnumType {
  NodeID id;
  struct Idents *alts;
};

struct ErrType {
  NodeID id;
  struct Errs *errs;
};

struct PtrType {
  NodeID id;
  MAYBE(Tok) ro;
  struct Type *ref_type;
};

struct FnType {
  NodeID id;
  struct Types *params;
  NULLABLE_PTR(struct Type) return_type;
};

struct ScopedIdent {
  NodeID id;
  u32 cap;
  u32 len;
  Tok *items;
};

typedef enum {
  EXPR_ATOM,
  EXPR_POSTFIX,
  EXPR_FN_CALL,
  EXPR_FIELD_ACCESS,
  EXPR_COLL_ACCESS,
  EXPR_UNARY,
  EXPR_BIN,
} ExprKind;

struct Expr {
  NodeID id;
  ExprKind t;
  union {
    struct Atom *atom;
    struct PostfixExpr *postfix_expr;
    struct FnCall *fn_call;
    struct FieldAccess *field_access;
    struct CollAccess *coll_access;
    struct UnaryExpr *unary_expr;
    struct BinExpr *bin_expr;
  };
};

typedef enum {
  ATOM_TOKEN,
  ATOM_BRACED_LIT,
  ATOM_DESIGNATOR,
  ATOM_SCOPED_IDENT,
} AtomKind;

struct Atom {
  NodeID id;
  AtomKind t;
  union {
    Tok token;  // Stores: identifier, number, string, enum, nil
    struct ScopedIdent *scoped_ident;
    struct Designator *designator;
    struct BracedLit *braced_lit;
  };
};

struct Designator {
  NodeID id;
  struct ScopedIdent *ident;
  NULLABLE_PTR(struct Init) init;
};

struct BracedLit {
  NodeID id;
  NULLABLE_PTR(struct Type) type;
  struct Init *init;
};

struct FnCall {
  NodeID id;
  struct Expr *lvalue;
  struct Init *args;
};

struct Init {
  NodeID id;
  u32 len;
  u32 cap;
  struct Expr **items;
};

struct PostfixExpr {
  NodeID id;
  struct Expr *sub_expr;
  Tok op;
};

struct FieldAccess {
  NodeID id;
  struct Expr *lvalue;
  Tok field;
};

struct CollAccess {
  NodeID id;
  struct Expr *lvalue;
  struct Index *index;
};

typedef enum {
  INDEX_SINGLE,
  INDEX_RANGE,
} IndexKind;

struct Index {
  NodeID id;
  IndexKind t;
  NULLABLE_PTR(struct Expr) start;
  NULLABLE_PTR(struct Expr) end;
};

struct UnaryExpr {
  NodeID id;
  Tok op;
  struct Expr *sub_expr;
};

struct BinExpr {
  NodeID id;
  Tok op;
  struct Expr *left;
  struct Expr *right;
};

#define TYPEDEF_NODE(NODE, ...) typedef struct NODE NODE;

EACH_NODE(TYPEDEF_NODE)

#define DECL_NODE_KIND(_, UPPER_NAME, ...) NODE_##UPPER_NAME,

typedef enum {
  EACH_NODE(DECL_NODE_KIND) NODE_KIND_COUNT,
} NodeKind;

typedef enum {
  NFLAG_NONE = 0,
  NFLAG_ERROR = 1,
} NodeFlag;

typedef struct {
  NodeFlag *items;
  u32 cap;
  u32 len;
} NodeFlags;

typedef struct {
  const char **items;
  u32 cap;
  u32 len;
} NodeNames;

typedef enum {
  CHILD_TOKEN,
  CHILD_NODE,
} ChildKind;

typedef struct {
  NodeID *data;
  NodeKind kind;
} AnyNode;

typedef struct {
  ChildKind t;
  union {
    Tok token;
    AnyNode node;
  };
  NULLABLE_PTR(const char) name;
} NodeChild;

typedef struct {
  NodeChild *items;
  u32 cap;
  u32 len;
} NodeChildren;

typedef struct {
  NodeChildren *items;
  u32 cap;
  u32 len;
} NodeTree;

typedef struct Scope {
  HashMapScopeEntry table;
  NULLABLE_PTR(struct Scope) parent_scope;
} Scope;

typedef struct ScopeEntry {
  union {
    AnyNode node;
    Scope *scope;
  };
  struct ScopeEntry *shadows;
} ScopeEntry;

typedef struct {
  NodeID next_id;
  NodeFlags flags;
  NodeTree tree;
  NodeNames names;
  HashMapScope scopes;
} NodeMetadata;

const char *node_kind_name(NodeKind kind);

NodeMetadata new_node_metadata(void);
void node_metadata_free(NodeMetadata *m);
void *new_node(NodeMetadata *m, Arena *a, NodeKind kind);
void add_node_flags(NodeMetadata *m, NodeID id, NodeFlag flags);
bool has_error(NodeMetadata *m, void *node);

void add_child(NodeMetadata *m, NodeID id, NodeChild child);
NodeChild child_token(Tok token);
NodeChild child_token_named(const char *name, Tok token);
NodeChild child_node(AnyNode node);
NodeChild child_node_named(const char *name, AnyNode node);
void remove_child(NodeMetadata *m, NodeID from, NodeID child);

const char *get_node_name(NodeMetadata *m, NodeID id);
NodeChildren *get_node_children(NodeMetadata *m, NodeID id);
NodeChild *last_child(NodeMetadata *m, NodeID id);

void *expect_node(NodeKind kind, AnyNode node);

typedef enum {
  PRE_ORDER,
  POST_ORDER,
} TraversalOrder;

void ast_traverse_dfs(TraversalOrder order, void *ctx, AnyNode root,
                      NodeMetadata *m,
                      void (*visit_fun)(void *ctx, NodeMetadata *m,
                                        AnyNode node));

typedef struct {
  FILE *fs;
  u32 indent_level;
  u8 indent_width;
  NodeMetadata *meta;
} TreeDumpCtx;

void tree_dump(TreeDumpCtx *ctx, NodeID id);

#define NODE_GENERIC_CASE(NodeT, UPPER_NAME, _) NodeT * : NODE_##UPPER_NAME,

#define GET_NODE_KIND(NODE_PTR)                             \
  _Generic((NODE_PTR),                                      \
      EACH_NODE(NODE_GENERIC_CASE) default: assert(false && \
                                                   "not a pointer to a node"))

#define MAKE_ANY(NODE_PTR) \
  (AnyNode) { (NodeID *)(NODE_PTR), GET_NODE_KIND(NODE_PTR) }

#endif
