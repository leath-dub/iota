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

SourceFile *source_file(ParseCtx *c);
Imports *imports(ParseCtx *c);
Import *import(ParseCtx *c);
Decls *decls(ParseCtx *c);
Decl *decl(ParseCtx *c);
StructDecl *struct_decl(ParseCtx *c);
StructBody *struct_body(ParseCtx *c, Toks follow);
EnumDecl *enum_decl(ParseCtx *c);
ErrDecl *err_decl(ParseCtx *c);
VarDecl *var_decl(ParseCtx *c);
FnDecl *fn_decl(ParseCtx *c);
FnParams *fn_params(ParseCtx *c);
FnParam *fn_param(ParseCtx *c);
VarBinding *var_binding(ParseCtx *c);
Binding *binding(ParseCtx *c);
AliasBinding *alias_binding(ParseCtx *c);
AliasBindings *alias_bindings(ParseCtx *c, TokKind end);
Bindings *bindings(ParseCtx *c, TokKind end);
UnpackStruct *unpack_struct(ParseCtx *c, Toks follow);
UnpackTuple *unpack_tuple(ParseCtx *c, Toks follow);
Stmt *stmt(ParseCtx *c);
CompStmt *comp_stmt(ParseCtx *c);
Type *type(ParseCtx *c);
CollType *coll_type(ParseCtx *c);
StructType *struct_type(ParseCtx *c);
Types *types(ParseCtx *c);
Fields *fields(ParseCtx *c);
Field *field(ParseCtx *c);
UnionType *union_type(ParseCtx *c);
EnumType *enum_type(ParseCtx *c);
ErrType *err_type(ParseCtx *c);
PtrType *ptr_type(ParseCtx *c);
FnType *fn_type(ParseCtx *c);
Idents *idents(ParseCtx *c);
Errs *errs(ParseCtx *c);
Err *err(ParseCtx *c);
ScopedIdent *scoped_ident(ParseCtx *c, Toks follow);
Expr *expr(ParseCtx *c, Toks delim);
Atom *atom(ParseCtx *c);
Init *init(ParseCtx *c, TokKind delim);

#endif
