#ifndef SYN_H
#define SYN_H

#include "../ast/ast.h"
#include "../lex/lex.h"

typedef struct {
  TokKind *items;
  u32 len;
} Toks;

// I know this is smelly but C is very limited for variadic args and not storing
// size with array is really limiting for building abstractions
#define TOKS(...)                                             \
  (Toks) {                                                    \
    .items = (TokKind[]){__VA_ARGS__},                        \
    .len = sizeof((TokKind[]){__VA_ARGS__}) / sizeof(TokKind) \
  }

typedef struct {
  Lexer lex;
  Arena arena;
  NodeMetadata meta;
  NodeID current;
} ParseCtx;

typedef struct {
  Lexer lex;
} ParseState;

void check_allocs(void);

ParseCtx new_parse_ctx(SourceCode code);
void parse_ctx_free(ParseCtx *c);

SourceFile *parse_source_file(ParseCtx *c);
Imports *parse_imports(ParseCtx *c);
Import *parse_import(ParseCtx *c);
Decls *parse_decls(ParseCtx *c);
Decl *parse_decl(ParseCtx *c);
StructDecl *parse_struct_decl(ParseCtx *c);
StructBody *parse_struct_body(ParseCtx *c, Toks follow);
EnumDecl *parse_enum_decl(ParseCtx *c);
ErrDecl *parse_err_decl(ParseCtx *c);
VarDecl *parse_var_decl(ParseCtx *c);
FnDecl *parse_fn_decl(ParseCtx *c);
FnParams *parse_fn_params(ParseCtx *c);
FnParam *parse_fn_param(ParseCtx *c);
VarBinding *parse_var_binding(ParseCtx *c);
Binding *parse_binding(ParseCtx *c);
AliasBinding *parse_alias_binding(ParseCtx *c);
AliasBindings *parse_alias_bindings(ParseCtx *c, TokKind end);
Bindings *parse_bindings(ParseCtx *c, TokKind end);
UnpackStruct *parse_unpack_struct(ParseCtx *c, Toks follow);
UnpackTuple *parse_unpack_tuple(ParseCtx *c, Toks follow);
UnpackUnion *parse_unpack_union(ParseCtx *c, Toks follow);
Stmt *parse_stmt(ParseCtx *c);
IfStmt *parse_if_stmt(ParseCtx *c);
Else *parse_else(ParseCtx *c);
Cond *parse_cond(ParseCtx *c);
UnionTagCond *parse_union_tag_cond(ParseCtx *c);
CompStmt *parse_comp_stmt(ParseCtx *c);
Type *parse_type(ParseCtx *c);
CollType *parse_coll_type(ParseCtx *c);
StructType *parse_struct_type(ParseCtx *c);
Types *parse_types(ParseCtx *c);
Fields *parse_fields(ParseCtx *c);
Field *parse_field(ParseCtx *c);
UnionType *parse_union_type(ParseCtx *c);
EnumType *parse_enum_type(ParseCtx *c);
ErrType *parse_err_type(ParseCtx *c);
PtrType *parse_ptr_type(ParseCtx *c);
FnType *parse_fn_type(ParseCtx *c);
Idents *parse_idents(ParseCtx *c);
Errs *parse_errs(ParseCtx *c);
Err *parse_err(ParseCtx *c);
ScopedIdent *parse_scoped_ident(ParseCtx *c, Toks follow);
Expr *parse_expr(ParseCtx *c);
Atom *parse_atom(ParseCtx *c);
Init *parse_init(ParseCtx *c, TokKind delim);

#endif
