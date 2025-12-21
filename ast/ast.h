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

#define EACH_NODE(USE)                                   \
    USE(SourceFile, SOURCE_FILE, "source_file")          \
    USE(Imports, IMPORTS, "imports")                     \
    USE(Decls, DECLS, "decls")                           \
    USE(Import, IMPORT, "import")                        \
    USE(Decl, DECL, "decl")                              \
    USE(VarDecl, VAR_DECL, "var_decl")                   \
    USE(FnDecl, FN_DECL, "fn_decl")                      \
    USE(FnMods, FN_MODS, "fn_mods")                      \
    USE(FnMod, FN_MOD, "fn_mod")                         \
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
    USE(AssignOrExpr, ASSIGN_OR_EXPR, "assign_or_expr")  \
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
    USE(Ident, IDENT, "ident")                           \
    USE(Expr, EXPR, "expr")                              \
    USE(Atom, ATOM, "atom")                              \
    USE(PostfixExpr, POSTFIX_EXPR, "postfix_expr")       \
    USE(Call, CALL, "call")                              \
    USE(CallArgs, CALL_ARGS, "call_args")                \
    USE(CallArg, CALL_ARG, "call_arg")                   \
    USE(FieldAccess, FIELD_ACCESS, "field_access")       \
    USE(CollAccess, COLL_ACCESS, "coll_access")          \
    USE(UnaryExpr, UNARY_EXPR, "unary_expr")             \
    USE(BinExpr, BIN_EXPR, "bin_expr")                   \
    USE(Index, INDEX, "index")

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
    u32 len;
    u32 cap;
    struct Import **items;
};

struct Import {
    AstNode head;
    struct ScopedIdent *module;
};

struct Decls {
    AstNode head;
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
    AstNode head;
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
    AstNode head;
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
    AstNode head;
    VarBindingKind t;
    union {
        struct Ident *basic;
        struct UnpackTuple *unpack_tuple;
        struct UnpackStruct *unpack_struct;
    };
};

struct AliasBinding {
    AstNode head;
    struct Binding *binding;
    MAYBE(Tok) alias;
};

struct AliasBindings {
    AstNode head;
    u32 len;
    u32 cap;
    struct AliasBinding **items;
};

struct Binding {
    AstNode head;
    struct Ident *name;
    MAYBE(Tok) ref;  // could be a '*' meaning the binding is a reference
    MAYBE(Tok) mod;
};

struct Bindings {
    AstNode head;
    u32 len;
    u32 cap;
    struct Binding **items;
};

struct UnpackTuple {
    AstNode head;
    struct Bindings *bindings;
};

struct UnpackStruct {
    AstNode head;
    struct AliasBindings *bindings;
};

struct UnpackUnion {
    AstNode head;
    struct Ident *tag;
    struct Binding *binding;
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
    u32 len;
    u32 cap;
    struct FnMod **items;
};

struct FnMod {
    AstNode head;
    Tok mod;
};

struct FnParams {
    AstNode head;
    u32 len;
    u32 cap;
    struct FnParam **items;
};

struct FnParam {
    AstNode head;
    struct VarBinding *binding;
    struct Type *type;
    bool variadic;
};

struct TypeParams {
    AstNode head;
    struct Types *types;
};

struct StructDecl {
    AstNode head;
    struct Ident *name;
    NULLABLE_PTR(struct TypeParams) type_params;
    struct StructBody *body;
};

struct StructBody {
    AstNode head;
    bool tuple_like;
    union {
        struct Types *types;
        struct Fields *fields;
    };
};

struct Fields {
    AstNode head;
    u32 len;
    u32 cap;
    struct Field **items;
};

struct Field {
    AstNode head;
    struct Ident *name;
    struct Type *type;
};

struct EnumDecl {
    AstNode head;
    struct Ident *name;
    struct Idents *alts;
};

struct Idents {
    AstNode head;
    u32 len;
    u32 cap;
    struct Ident **items;
};

struct ErrDecl {
    AstNode head;
    struct Ident *name;
    struct Errs *errs;
};

struct Errs {
    AstNode head;
    u32 len;
    u32 cap;
    struct Err **items;
};

struct Err {
    AstNode head;
    bool embedded;  // e.g. !Foo
    struct ScopedIdent *scoped_ident;
};

struct UnionDecl {
    AstNode head;
    struct Ident *name;
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

typedef enum {
    CASE_PATT_EXPR,
    CASE_PATT_DEFAULT,
    CASE_PATT_UNPACK_UNION,
} CasePattKind;

struct CasePatt {
    AstNode head;
    CasePattKind t;
    union {
        struct Expr *expr;
        Tok default_;
        struct UnpackUnion *unpack_union;
    };
};

struct CaseBranch {
    AstNode head;
    struct CasePatt *patt;
    struct Stmt *action;
};

struct CaseBranches {
    AstNode head;
    u32 len;
    u32 cap;
    struct CaseBranch **items;
};

struct CaseStmt {
    AstNode head;
    struct Expr *expr;
    struct CaseBranches *branches;
};

typedef enum {
    COND_UNION_TAG,
    COND_EXPR,
} CondKind;

struct Cond {
    AstNode head;
    CondKind t;
    union {
        struct UnionTagCond *union_tag;
        struct Expr *expr;
    };
};

struct UnionTagCond {
    AstNode head;
    struct UnpackUnion *trigger;
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
    u32 len;
    u32 cap;
    struct Stmt **items;
};

struct AssignOrExpr {
    AstNode head;
    struct Expr *lvalue;
    Tok assign_token;
    NULLABLE_PTR(struct Expr) rvalue;
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
    AstNode head;
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
    AstNode head;
    Tok token;  // just stores the keyword token
};

struct CollType {
    AstNode head;
    NULLABLE_PTR(struct Expr) index_expr;
    struct Type *element_type;
};

struct StructType {
    AstNode head;
    NULLABLE_PTR(struct TypeParams) type_params;
    struct StructBody *body;
};

struct Types {
    AstNode head;
    u32 len;
    u32 cap;
    struct Type **items;
};

struct UnionType {
    AstNode head;
    NULLABLE_PTR(struct TypeParams) type_params;
    struct Fields *fields;
};

struct EnumType {
    AstNode head;
    struct Idents *alts;
};

struct ErrType {
    AstNode head;
    struct Errs *errs;
};

struct PtrType {
    AstNode head;
    MAYBE(Tok) ro;
    struct Type *ref_type;
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
    u32 cap;
    u32 len;
    struct Ident **items;
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
    u32 len;
    u32 cap;
    struct CallArg **items;
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
                   "AST node must define 'id' field of type NodeID");

EACH_NODE(CHECK_NODE)

typedef struct ScopeEntry {
    AstNode *node;
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
    STORAGE_PTR,
    STORAGE_FN,
} TypeStorage;

struct TypeRepr;

typedef struct {
    struct TypeRepr *referent;
} PtrT;

typedef struct {
    bool variadic;
    struct TypeRepr *type;
} FnTParam;

typedef struct {
    FnTParam *params;
    struct TypeRepr *return_type;
} FnT;

DA_DEFINE(fnt_params, FnTParam)

typedef struct TypeRepr {
    TypeStorage t;
    union {
        FnT fn;
        PtrT ptr;
    };
} TypeRepr;

typedef struct {
    Arena *arena;
    Scope **scope;
    AstNode **resolves_to;
    TypeRepr **type;
} TreeData;

MAP_DEFINE(scope_map, AstNode *, Scope *)
MAP_DEFINE(resolves_to_map, Ident *, AstNode *)
MAP_DEFINE(type_map, AstNode *, TypeRepr *)

TreeData tree_data_create(Arena *a);
void tree_data_delete(TreeData td);

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

TypeRepr *type_create(Arena *a);
void ast_type_set(Ast *ast, AstNode *n, TypeRepr *type);
TypeRepr *ast_type_get(Ast *ast, AstNode *n);

Scope *ast_scope_create(Ast *ast);
void ast_scope_insert(Ast *ast, Scope *s, string name, AstNode *n);

AstNode *ast_node_create(Ast *ast, NodeKind kind);
void ast_node_child_add(AstNode *node, Child child);

// const char *node_kind_name(NodeKind kind);
// NodeMetadata new_node_metadata(void);
// void node_metadata_free(NodeMetadata *m);
// void *new_node(NodeMetadata *m, Arena *a, NodeKind kind);
// void add_node_flags(NodeMetadata *m, NodeID id, NodeFlag flags);
// bool has_error(NodeMetadata *m, void *node);
//
// void set_node_offset(NodeMetadata *m, NodeID id, u32 pos);
// u32 get_node_offset(NodeMetadata *m, NodeID id);
//
// void add_child(NodeMetadata *m, NodeID id, NodeChild child);
// NodeChild child_token(Tok token);
// NodeChild child_token_named(const char *name, Tok token);
// NodeChild child_node(AnyNode node);
// NodeChild child_node_named(const char *name, AnyNode node);
// void remove_child(NodeMetadata *m, NodeID from, NodeID child);
//
// NodeChildren *get_node_children(NodeMetadata *m, NodeID id);
// void set_node_parent(NodeMetadata *m, NodeID id, AnyNode parent);
// AnyNode get_node_parent(NodeMetadata *m, NodeID id);
// NodeChild *last_child(NodeMetadata *m, NodeID id);
//
// void *expect_node(NodeKind kind, AnyNode node);
//
// Scope *scope_attach(NodeMetadata *m, AnyNode node);
// void scope_insert(NodeMetadata *m, Scope *scope, string symbol, AnyNode
// node); Scope *scope_get(NodeMetadata *m, NodeID id);
//
// void set_resolved_node(NodeMetadata *m, AnyNode node, AnyNode resolved_node);
// AnyNode *try_get_resolved_node(NodeMetadata *m, NodeID id);
//
// TypeRepr *type_alloc(NodeMetadata *m);
// void type_set(NodeMetadata *m, AnyNode node, TypeRepr *type);
// TypeRepr *type_try_get(NodeMetadata *m, NodeID id);

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

#define NODE_GENERIC_CASE(NodeT, UPPER_NAME, _) NodeT * : NODE_##UPPER_NAME,

#define GET_NODE_KIND(NODE_PTR)                       \
    _Generic((NODE_PTR),                              \
        EACH_NODE(NODE_GENERIC_CASE) default: assert( \
                 false && "not a pointer to a node"))

#define REIFY_AS(node, type)                                             \
    (assert(node->kind == GET_NODE_KIND((type *)0x0) && "invalid cast"), \
     (type *)node)

#endif
