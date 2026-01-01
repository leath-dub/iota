#ifndef AST_H
#define AST_H

#include "../common/common.h"
#include "../common/dynamic_array.h"
#include "../common/map.h"
#include "../lex/lex.h"
#include "../mod/mod.h"

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

#define EACH_NODE(USE)                                         \
    USE(SourceFile, SOURCE_FILE, source_file)                  \
    USE(Imports, IMPORTS, imports)                             \
    USE(Decls, DECLS, decls)                                   \
    USE(Import, IMPORT, import)                                \
    USE(Decl, DECL, decl)                                      \
    USE(VarDecl, VAR_DECL, var_decl)                           \
    USE(FnDecl, FN_DECL, fn_decl)                              \
    USE(FnMods, FN_MODS, fn_mods)                              \
    USE(FnMod, FN_MOD, fn_mod)                                 \
    USE(TypeParams, TYPE_PARAMS, type_params)                  \
    USE(TypeDecl, TYPE_DECL, type_decl)                        \
    USE(Binding, BINDING, binding)                             \
    USE(TypedBinding, TYPED_BINDING, typed_binding)            \
    USE(TypedBindings, TYPED_BINDINGS, typed_bindings)         \
    USE(FnParams, FN_PARAMS, fn_params)                        \
    USE(FnParam, FN_PARAM, fn_param)                           \
    USE(Types, TYPES, types)                                   \
    USE(Idents, IDENTS, idents)                                \
    USE(Stmt, STMT, stmt)                                      \
    USE(IfStmt, IF_STMT, if_stmt)                              \
    USE(WhileStmt, WHILE_STMT, while_stmt)                     \
    USE(CasePatt, CASE_PATT, case_patt)                        \
    USE(CaseBranch, CASE_BRANCH, case_branch)                  \
    USE(CaseBranches, CASE_BRANCHES, case_branches)            \
    USE(CaseStmt, CASE_STMT, case_stmt)                        \
    USE(Cond, COND, cond)                                      \
    USE(UnionReduceCond, UNION_REDUCE_COND, union_reduce_cond) \
    USE(ReturnStmt, RETURN_STMT, return_stmt)                  \
    USE(DeferStmt, DEFER_STATEMENT, defer_stmt)                \
    USE(CompStmt, COMP_STMT, comp_stmt)                        \
    USE(Else, ELSE, else)                                      \
    USE(AssignOrExpr, ASSIGN_OR_EXPR, assign_or_expr)          \
    USE(Type, TYPE, type)                                      \
    USE(BuiltinType, BUILTIN_TYPE, builtin_type)               \
    USE(CollType, COLL_TYPE, coll_type)                        \
    USE(TupleType, TUPLE_TYPE, tuple_type)                     \
    USE(StructType, STRUCT_TYPE, struct_type)                  \
    USE(StructFields, STRUCT_FIELDS, struct_fields)            \
    USE(StructField, STRUCT_FIELD, struct_field)               \
    USE(TaggedUnionType, TAGGED_UNION_TYPE, tagged_union_type) \
    USE(UnionAlts, UNION_ALTS, union_alts)                     \
    USE(UnionAlt, UNION_ALT, union_alt)                        \
    USE(EnumType, ENUM_TYPE, enum_type)                        \
    USE(PtrType, PTR_TYPE, ptr_type)                           \
    USE(ErrType, ERR_TYPE, err_type)                           \
    USE(FnType, FN_TYPE, fn_type)                              \
    USE(ScopedIdent, SCOPED_IDENT, scoped_ident)               \
    USE(Ident, IDENT, ident)                                   \
    USE(Expr, EXPR, expr)                                      \
    USE(Atom, ATOM, atom)                                      \
    USE(PostfixExpr, POSTFIX_EXPR, postfix_expr)               \
    USE(Call, CALL, call)                                      \
    USE(CallArgs, CALL_ARGS, call_args)                        \
    USE(CallArg, CALL_ARG, call_arg)                           \
    USE(FieldAccess, FIELD_ACCESS, field_access)               \
    USE(CollAccess, COLL_ACCESS, coll_access)                  \
    USE(UnaryExpr, UNARY_EXPR, unary_expr)                     \
    USE(BinExpr, BIN_EXPR, bin_expr)                           \
    USE(Index, INDEX, index)

#define FWD_DECL_NODE(NODE, ...) struct NODE;

EACH_NODE(FWD_DECL_NODE)

#define DECL_NODE_KIND(_, UPPER_NAME, ...) NODE_##UPPER_NAME,

typedef enum {
    EACH_NODE(DECL_NODE_KIND) NODE_KIND_COUNT,
} NodeKind;

struct AstNode;

typedef enum {
    CHILD_TOKEN,
    CHILD_NODE,
} ChildKind;

typedef struct {
    ChildKind t;
    union {
        Tok token;
        struct AstNode *node;
    };
    NULLABLE_PTR(const char) name;
} Child;

typedef struct AstNode {
    NodeKind kind;
    Child *children;
    NULLABLE_PTR(struct AstNode) parent;
    size_t offset;
    bool has_error;
} AstNode;

DA_DEFINE(children, Child)

struct SourceFile {
    AstNode head;
    struct Imports *imports;
    struct Decls *decls;
};

struct Imports {
    AstNode head;
};

struct Import {
    AstNode head;
    struct ScopedIdent *module;
};

struct Decls {
    AstNode head;
};

typedef enum {
    DECL_VAR,
    DECL_FN,
    DECL_TYPE,
} DeclKind;

struct Decl {
    AstNode head;
    DeclKind t;
    union {
        struct VarDecl *var_decl;
        struct FnDecl *fn_decl;
        struct TypeDecl *type_decl;
    };
};

struct VarDecl {
    AstNode head;
    struct Binding *binding;
    Tok assign_token;
    NULLABLE_PTR(struct Expr) init;
};

struct Binding {
    AstNode head;
    Tok qualifier;
    struct Ident *name;
    NULLABLE_PTR(struct Type) type;
};

struct TypedBinding {
    AstNode head;
    Tok qualifier;
    struct Ident *name;
    struct Type *type;
};

struct FnDecl {
    AstNode head;
    struct Ident *name;
    NULLABLE_PTR(struct TypeParams) type_params;
    struct FnParams *params;
    struct FnMods *mods;
    NULLABLE_PTR(struct Type) return_type;
    struct CompStmt *body;
};

struct FnMods {
    AstNode head;
};

struct FnMod {
    AstNode head;
    Tok mod;
};

struct FnParams {
    AstNode head;
};

struct FnParam {
    AstNode head;
    struct TypedBinding *binding;
    bool variadic;
};

struct TypeParams {
    AstNode head;
};

struct TupleType {
    AstNode head;
    struct Types *types;
};

struct StructType {
    AstNode head;
    struct StructFields *fields;
};

struct StructFields {
    AstNode head;
};

struct StructField {
    AstNode head;
    struct TypedBinding *binding;
    Tok assign_token;
    NULLABLE_PTR(struct Expr) default_value;
};

struct TypedBindings {
    AstNode head;
};

struct Idents {
    AstNode head;
};

typedef enum {
    STMT_DECL,
    STMT_IF,
    STMT_WHILE,
    STMT_CASE,
    STMT_RETURN,
    STMT_COMP,
    STMT_EXPR,
    STMT_ASSIGN_OR_EXPR,
} StmtKind;

struct Stmt {
    AstNode head;
    StmtKind t;
    union {
        struct Decl *decl;
        struct IfStmt *if_stmt;
        struct WhileStmt *while_stmt;
        struct CaseStmt *case_stmt;
        struct ReturnStmt *return_stmt;
        struct DeferStmt *defer_stmt;
        struct CompStmt *comp_stmt;
        struct AssignOrExpr *assign_or_expr;
    };
};

struct IfStmt {
    AstNode head;
    struct Cond *cond;
    struct CompStmt *true_branch;
    NULLABLE_PTR(struct Else) else_branch;
};

struct WhileStmt {
    AstNode head;
    struct Cond *cond;
    struct CompStmt *true_branch;
};

// Binding

typedef enum {
    CASE_PATT_EXPR,
    CASE_PATT_DEFAULT,
    CASE_PATT_TYPE,
    CASE_PATT_BINDING,
} CasePattKind;

struct CasePatt {
    AstNode head;
    CasePattKind t;
    union {
        struct Expr *expr;
        Tok default_;
        struct Type *type;
        struct TypedBinding *binding;
    };
};

struct CaseBranch {
    AstNode head;
    struct CasePatt *patt;
    struct Stmt *action;
};

struct CaseBranches {
    AstNode head;
};

struct CaseStmt {
    AstNode head;
    struct Expr *expr;
    struct CaseBranches *branches;
};

typedef enum {
    COND_UNION_REDUCE,
    COND_EXPR,
} CondKind;

struct Cond {
    AstNode head;
    CondKind t;
    union {
        struct UnionReduceCond *union_reduce;
        struct Expr *expr;
    };
};

struct UnionReduceCond {
    AstNode head;
    struct TypedBinding *trigger;
    Tok assign_token;
    struct Expr *expr;
};

typedef enum {
    ELSE_IF,
    ELSE_COMP,
} ElseKind;

struct Else {
    AstNode head;
    ElseKind t;
    union {
        struct IfStmt *if_stmt;
        struct CompStmt *comp_stmt;
    };
};

struct ReturnStmt {
    AstNode head;
    NULLABLE_PTR(struct Expr) expr;
};

struct DeferStmt {
    AstNode head;
    struct Stmt *stmt;
};

struct CompStmt {
    AstNode head;
};

struct AssignOrExpr {
    AstNode head;
    struct Expr *lvalue;
    Tok assign_token;
    NULLABLE_PTR(struct Expr) rvalue;
};

struct TypeDecl {
    AstNode head;
    struct Ident *name;
    struct Type *type;
};

typedef enum {
    TYPE_BUILTIN,
    TYPE_COLL,
    TYPE_TUPLE,
    TYPE_STRUCT,
    TYPE_TAGGED_UNION,
    TYPE_ENUM,
    TYPE_ERR,
    TYPE_PTR,
    TYPE_FN,
    TYPE_SCOPED_IDENT,
} TypeKind;

struct Type {
    AstNode head;
    TypeKind t;
    union {
        struct BuiltinType *builtin_type;
        struct CollType *coll_type;
        struct TupleType *tuple_type;
        struct StructType *struct_type;
        struct TaggedUnionType *tagged_union_type;
        struct EnumType *enum_type;
        struct ErrType *err_type;
        struct PtrType *ptr_type;
        struct FnType *fn_type;
        struct ScopedIdent *scoped_ident;
    };
};

struct BuiltinType {
    AstNode head;
    Tok token;  // just stores the keyword token
};

struct CollType {
    AstNode head;
    NULLABLE_PTR(struct Expr) index_expr;
    struct Type *element_type;
};

struct Types {
    AstNode head;
};

struct TaggedUnionType {
    AstNode head;
    struct UnionAlts *alts;
};

struct UnionAlts {
    AstNode head;
};

typedef enum {
    UNION_ALT_TYPE,
    UNION_ALT_INLINE_DECL,
} UnionAltKind;

struct UnionAlt {
    AstNode head;
    UnionAltKind t;
    union {
        struct Type *type;
        struct TypeDecl *inline_decl;
    };
};

struct EnumType {
    AstNode head;
    struct Idents *alts;
};

struct ErrType {
    AstNode head;
    struct Type *type;
};

struct PtrType {
    AstNode head;
    MAYBE(Tok) ro;
    struct Type *points_to;
};

struct FnType {
    AstNode head;
    struct Types *params;
    NULLABLE_PTR(struct Type) return_type;
};

struct Ident {
    AstNode head;
    Tok token;
};

struct ScopedIdent {
    AstNode head;
};

typedef enum {
    EXPR_ATOM,
    EXPR_POSTFIX,
    EXPR_CALL,
    EXPR_FIELD_ACCESS,
    EXPR_COLL_ACCESS,
    EXPR_UNARY,
    EXPR_BIN,
} ExprKind;

struct Expr {
    AstNode head;
    ExprKind t;
    union {
        struct Atom *atom;
        struct PostfixExpr *postfix_expr;
        struct Call *call;
        struct FieldAccess *field_access;
        struct CollAccess *coll_access;
        struct UnaryExpr *unary_expr;
        struct BinExpr *bin_expr;
    };
};

typedef enum {
    ATOM_TOKEN,
    ATOM_BUILTIN_TYPE,
    ATOM_SCOPED_IDENT,
} AtomKind;

struct Atom {
    AstNode head;
    AtomKind t;
    union {
        Tok token;
        Tok builtin_type;
        struct ScopedIdent *scoped_ident;
    };
};

struct Call {
    AstNode head;
    struct Expr *callable;
    struct CallArgs *args;
};

struct CallArgs {
    AstNode head;
};

struct CallArg {
    AstNode head;
    NULLABLE_PTR(struct Ident) name;
    Tok assign_token;
    struct Expr *value;
};

struct PostfixExpr {
    AstNode head;
    struct Expr *sub_expr;
    Tok op;
};

struct FieldAccess {
    AstNode head;
    struct Expr *lvalue;
    struct Ident *field;
};

struct CollAccess {
    AstNode head;
    struct Expr *lvalue;
    struct Index *index;
};

typedef enum {
    INDEX_SINGLE,
    INDEX_RANGE,
} IndexKind;

struct Index {
    AstNode head;
    IndexKind t;
    NULLABLE_PTR(struct Expr) start;
    NULLABLE_PTR(struct Expr) end;
};

struct UnaryExpr {
    AstNode head;
    Tok op;
    struct Expr *sub_expr;
};

struct BinExpr {
    AstNode head;
    Tok op;
    struct Expr *left;
    struct Expr *right;
};

#define TYPEDEF_NODE(NODE, ...) typedef struct NODE NODE;

EACH_NODE(TYPEDEF_NODE)

// This lets us not accidently add a AST node without the id field at the top
#define CHECK_NODE(NODE, ...)                                    \
    _Static_assert(sizeof(((NODE *)0)->head) == sizeof(AstNode), \
                   "AST node must define 'head' field of type AstNode");

EACH_NODE(CHECK_NODE)

struct Scope;

typedef struct ScopeEntry {
    AstNode *node;
    NULLABLE_PTR(struct Scope) sub_scope;
    struct ScopeEntry *shadows;
} ScopeEntry;

typedef struct Scope {
    AstNode *self;
    ScopeEntry **table;
    NULLABLE_PTR(struct Scope) enclosing_scope;
} Scope;

MAP_BYTES_DEFINE(scope_entry_map, ScopeEntry *)

typedef enum {
    STORAGE_U8,
    STORAGE_S8,
    STORAGE_U16,
    STORAGE_S16,
    STORAGE_U32,
    STORAGE_S32,
    STORAGE_U64,
    STORAGE_S64,
    STORAGE_F32,
    STORAGE_F64,
    STORAGE_UNIT,
    STORAGE_STRING,
    STORAGE_BOOL,
    STORAGE_PTR,
    STORAGE_FN,
    STORAGE_TUPLE,
    STORAGE_STRUCT,
    STORAGE_TAGGED_UNION,
    STORAGE_ENUM,
    STORAGE_ALIAS,
} TypeStorage;

typedef u64 TypeId;

#define INVALID_TYPE (TypeId)(0)

struct TypeRepr;

typedef struct {
    TypeId points_to;
} TypePtr;

typedef struct {
    bool variadic;
    TypeId type;
} TypeFnParam;

typedef struct {
    TypeFnParam *params;
    TypeId return_type;
} TypeFn;

DA_DEFINE(type_fn_params, TypeFnParam)

typedef struct {
    string name;
    TypeId type;
} TypeField;

typedef struct {
    TypeField *fields;
} TypeStruct;

DA_DEFINE(type_fields, TypeField)

typedef struct {
    string *alts;
} TypeEnum;

DA_DEFINE(type_enum_alts, string)

typedef struct {
    TypeId *types;
} TypeTuple;

typedef struct {
    TypeId *types;
} TypeTaggedUnion;

DA_DEFINE(types, TypeId)

typedef struct {
    TypeDecl *type_decl;
    TypeId aliases;
} TypeAlias;

typedef struct TypeRepr {
    TypeStorage t;
    union {
        TypeFn fn_type;
        TypePtr ptr_type;
        TypeAlias alias_type;
        TypeStruct struct_type;
        TypeTuple tuple_type;
        TypeTaggedUnion tagged_union_type;
        TypeEnum enum_type;
    };
} TypeRepr;

typedef struct {
    Arena *arena;
    Scope **scope;
    AstNode **resolves_to;
    TypeId *type;
    TypeId *builtin_type;
    TypeRepr *type_data;
} TreeData;

MAP_DEFINE(scope_map, AstNode *, Scope *)
MAP_DEFINE(resolves_to_map, Ident *, AstNode *)
MAP_DEFINE(type_map, AstNode *, TypeId)
MAP_DEFINE(builtin_type_map, TokKind, TypeId)
DA_DEFINE(type_data, TypeRepr)

TreeData tree_data_create(Arena *a);
void tree_data_delete(TreeData td);

#define FWD_DECL_CHILD_ACCESS(NODE, _UPPER_NAME, lower_name) \
    NODE *child_##lower_name##_at(AstNode *n, size_t index);

EACH_NODE(FWD_DECL_CHILD_ACCESS)

Child child_token_create(Tok tok);
Child child_token_named_create(const char *name, Tok tok);
Child child_node_create(AstNode *n);
Child child_node_named_create(const char *name, AstNode *n);

const char *node_kind_to_string(NodeKind kind);

typedef struct {
    Arena *arena;
    AstNode *root;
    TreeData tree_data;
} Ast;

Ast ast_create(Arena *a);
void ast_delete(Ast ast);

void ast_scope_set(Ast *ast, AstNode *n, Scope *scope);
Scope *ast_scope_get(Ast *ast, AstNode *n);

void ast_resolves_to_set(Ast *ast, Ident *ident, AstNode *to);
AstNode *ast_resolves_to_get(Ast *ast, Ident *ident);
AstNode *ast_resolves_to_get_scoped(Ast *ast, ScopedIdent *scoped_ident);

TypeId ast_type_create(Ast *ast);
void ast_type_set(Ast *ast, AstNode *n, TypeId tid);
TypeId ast_type_get(Ast *ast, AstNode *n);
TypeRepr *ast_type_repr(Ast *ast, TypeId id);

Scope *ast_scope_create(Ast *ast);
void ast_scope_insert(Ast *ast, Scope *s, string name, AstNode *n,
                      Scope *sub_scope);

AstNode *ast_node_create(Ast *ast, NodeKind kind);
void ast_node_child_add(AstNode *node, Child child);

void ast_node_reparent(AstNode *child, AstNode *new_parent);

typedef enum {
    LOOKUP_MODE_LEXICAL,
    LOOKUP_MODE_DIRECT,
} ScopeLookupMode;

typedef struct {
    ScopeEntry *entry;
    Scope *found_in;
} ScopeLookup;

ScopeLookup scope_lookup(Scope *scope, string name, ScopeLookupMode mode);

typedef enum {
    DFS_CTRL_KEEP_GOING,
    DFS_CTRL_SKIP_SUBTREE,
} DfsCtrl;

typedef struct {
    DfsCtrl (*enter)(void *ctx, AstNode *node);
    DfsCtrl (*exit)(void *ctx, AstNode *node);
} EnterExitVTable;

void ast_traverse_dfs(void *ctx, Ast *ast, EnterExitVTable vtable);

typedef struct {
    FILE *fs;
    u32 indent_level;
    u8 indent_width;
    Ast *ast;
} TreeDumpCtx;

void dump_tree(TreeDumpCtx *ctx, AstNode *n);
void dump_symbols(Ast *ast, const SourceCode *code);
void dump_types(Ast *ast);

void fmt_type(char *buf, size_t size, Ast *ast, TypeRepr repr);

#define NODE_GENERIC_CASE(NodeT, UPPER_NAME, _) NodeT * : NODE_##UPPER_NAME,

#define GET_NODE_KIND(NODE_PTR)                       \
    _Generic((NODE_PTR),                              \
        EACH_NODE(NODE_GENERIC_CASE) default: assert( \
                 false && "not a pointer to a node"))

#define REIFY_AS(node, type)                                             \
    (assert(node->kind == GET_NODE_KIND((type *)0x0) && "invalid cast"), \
     (type *)node)

#endif
