#include <assert.h>

#include "sem.h"

static DfsCtrl check_types_enter(void *_ctx, AstNode *node);
static DfsCtrl check_types_exit(void *_ctx, AstNode *node);

typedef struct {
    Ast *ast;
    SourceCode *code;
} TypeCheckCtx;

void do_check_types(Ast *ast, SourceCode *code) {
    TypeCheckCtx ctx = {
        .ast = ast,
        .code = code,
    };
    ast_traverse_dfs(&ctx, ast,
                     (EnterExitVTable){
                         .enter = check_types_enter,
                         .exit = check_types_exit,
                     });
}

static DfsCtrl check_types_enter(void *_ctx, AstNode *node) {
    (void)_ctx;
    (void)node;
    // TODO: add "inferred_scope" context on entry of certain nodes:
    //
    // e.g. - function call argument
    //      - variable declaration
    //      - return statement
    //      - case statement
    return DFS_CTRL_KEEP_GOING;
}

static void check_atom(TypeCheckCtx *ctx, Atom *atom);
// static void check_postfix_expr(TypeCheckCtx *ctx, PostfixExpr *postfix_expr);
static void check_call(TypeCheckCtx *ctx, Call *call);
// static void check_field_access(TypeCheckCtx *ctx, FieldAccess *field_access);
// static void check_coll_access(TypeCheckCtx *ctx, CollAccess *coll_access);
// static void check_unary_expr(TypeCheckCtx *ctx, UnaryExpr *unary_expr);
static void check_bin_expr(TypeCheckCtx *ctx, BinExpr *bin_expr);
static void check_var_decl(TypeCheckCtx *ctx, VarDecl *var_decl);

static DfsCtrl check_types_exit(void *_ctx, AstNode *node) {
    TypeCheckCtx *ctx = _ctx;
    switch (node->kind) {
        case NODE_VAR_DECL:
            check_var_decl(ctx, (VarDecl *)node);
            break;
        case NODE_ATOM:
            check_atom(ctx, (Atom *)node);
            break;
        // case EXPR_POSTFIX:
        //     check_postfix_expr(ctx, (PostfixExpr *)node);
        case NODE_CALL:
            check_call(ctx, (Call *)node);
            break;
        // case EXPR_FIELD_ACCESS:
        //     check_field_access(ctx, (FieldAccess *)node);
        //     break;
        // case EXPR_COLL_ACCESS:
        //     check_coll_access(ctx, (CollAccess *)node);
        //     break;
        // case EXPR_UNARY:
        //     check_unary_expr(ctx, (UnaryExpr *)node);
        //     break;
        case NODE_BIN_EXPR:
            check_bin_expr(ctx, (BinExpr *)node);
            break;
        default:
            break;
    }
    return DFS_CTRL_KEEP_GOING;
}

static TypeRepr *type_from_symbol(TypeCheckCtx *ctx, AstNode *symbol);
static AstNode *get_scoped_ident_symbol(TypeCheckCtx *ctx,
                                        ScopedIdent *scoped_ident);

static TypeRepr *type_from_token(TypeCheckCtx *ctx, Tok token) {
    TypeRepr *type = type_create(ctx->ast->arena);
    switch (token.t) {
        case T_U8:
            type->t = STORAGE_U8;
            break;
        case T_S8:
            type->t = STORAGE_S8;
            break;
        case T_U16:
            type->t = STORAGE_U16;
            break;
        case T_S16:
            type->t = STORAGE_S16;
            break;
        case T_U32:
            type->t = STORAGE_U32;
            break;
        case T_S32:
            type->t = STORAGE_S32;
            break;
        case T_U64:
            type->t = STORAGE_U64;
            break;
        case T_S64:
            type->t = STORAGE_S64;
            break;
        case T_F32:
            type->t = STORAGE_F32;
            break;
        case T_F64:
            type->t = STORAGE_F64;
            break;
        case T_UNIT:
            type->t = STORAGE_UNIT;
            break;
        default:
            assert(false && "TODO");
    }
    return type;
}

static TypeRepr *type_from_lit(TypeCheckCtx *ctx, Tok lit) {
    TypeRepr *type = type_create(ctx->ast->arena);
    switch (lit.t) {
        case T_NUM:
            type->t = STORAGE_S32;
            break;  // TODO validate number literal
        default:
            assert(false && "TODO");
    }
    return type;
}

static TypeRepr *type_from_tree(TypeCheckCtx *ctx, Type *tree) {
    switch (tree->t) {
        case TYPE_BUILTIN:
            return type_from_token(ctx, tree->builtin_type->token);
        case TYPE_PTR: {
            TypeRepr *type = type_create(ctx->ast->arena);
            type->t = STORAGE_PTR;
            type->ptr.referent = type_from_tree(ctx, tree->ptr_type->ref_type);
            return type;
        }
        case TYPE_SCOPED_IDENT:
            return type_from_symbol(
                ctx, get_scoped_ident_symbol(ctx, tree->scoped_ident));
        case TYPE_FN:
        case TYPE_COLL:
        case TYPE_STRUCT:
        case TYPE_TUPLE:
        case TYPE_TAGGED_UNION:
        case TYPE_ENUM:
        case TYPE_ERR:
            break;
    }
    assert(false && "TODO");
}

static TypeRepr *type_from_symbol(TypeCheckCtx *ctx, AstNode *symbol) {
    switch (symbol->kind) {
        case NODE_FN_DECL: {
            FnDecl *fn = (FnDecl *)symbol;

            TypeRepr *fn_type = type_create(ctx->ast->arena);

            fn_type->t = STORAGE_FN;

            AstNode *h = &fn->params->head;

            for (u32 i = 0; i < da_length(h->children); i++) {
                FnParam *param = child_fn_param_at(h, i);
                FnTParam tparam = {
                    .type = type_from_tree(ctx, param->binding->type),
                    .variadic = param->variadic,
                };
                fnt_params_append(&fn_type->fn.params, tparam);
            }

            TypeRepr *return_type = NULL;
            if (fn->return_type.ptr != NULL) {
                return_type = type_from_tree(ctx, fn->return_type.ptr);
            } else {
                return_type = type_create(ctx->ast->arena);
                return_type->t = STORAGE_UNIT;
            }

            fn_type->fn.return_type = return_type;

            return fn_type;
        }
        case NODE_VAR_DECL: {
            VarDecl *var = (VarDecl *)symbol;
            TypeRepr *type = ast_type_get(ctx->ast, &var->head);
            assert(type);
            return type;
        }
        default:
            break;
    }
    assert(false && "TODO");
}

static AstNode *get_scoped_ident_symbol(TypeCheckCtx *ctx,
                                        ScopedIdent *scoped_ident) {
    AstNode *h = &scoped_ident->head;
    assert(child_ident_at(h, 0)->token.t != T_EMPTY_STRING && "TODO inferred");
    AstNode *node = ast_resolves_to_get(
        ctx->ast, child_ident_at(h, da_length(h->children) - 1));
    assert(node);
    return node;
}

static void check_atom(TypeCheckCtx *ctx, Atom *atom) {
    switch (atom->t) {
        case ATOM_TOKEN:
            ast_type_set(ctx->ast, &atom->head,
                         type_from_lit(ctx, atom->builtin_type));
            break;
        case ATOM_SCOPED_IDENT:
            ast_type_set(ctx->ast, &atom->head,
                         type_from_symbol(ctx, get_scoped_ident_symbol(
                                                   ctx, atom->scoped_ident)));
            break;
        default:
            break;
    }
}

static TypeRepr *type_of_expr(TypeCheckCtx *ctx, Expr *expr) {
    Child *children = expr->head.children;
    assert(da_length(children) != 0);
    Child *child = children_at(children, 0);
    assert(child->t == CHILD_NODE);
    return ast_type_get(ctx->ast, child->node);
}

static void sem_raise(TypeCheckCtx *ctx, u32 at, const char *banner,
                      const char *msg) {
    raise_semantic_error(ctx->code, (SemanticError){
                                        .at = at,
                                        .banner = banner,
                                        .message = msg,
                                    });
}

// static void check_postfix_expr(TypeCheckCtx *ctx, PostfixExpr *postfix_expr);

static void check_call(TypeCheckCtx *ctx, Call *call) {
    if (call->callable->t == EXPR_ATOM) {
        Atom *atom = call->callable->atom;
        if (atom->t == ATOM_BUILTIN_TYPE) {
            // Cast to builtin type
            TypeRepr *type = type_from_token(ctx, atom->builtin_type);
            ast_type_set(ctx->ast, &call->head, type);
        }
    }
}

// static void check_field_access(TypeCheckCtx *ctx, FieldAccess *field_access);
// static void check_coll_access(TypeCheckCtx *ctx, CollAccess *coll_access);
// static void check_unary_expr(TypeCheckCtx *ctx, UnaryExpr *unary_expr);

static void check_bin_expr(TypeCheckCtx *ctx, BinExpr *bin_expr) {
    TypeRepr *lt = type_of_expr(ctx, bin_expr->left);
    TypeRepr *rt = type_of_expr(ctx, bin_expr->right);
    if (!lt || !rt) {
        TODO("type not resolved");
        return;
    }

    // Arena *erra = &ctx->code->error_arena;

    if (lt->t != rt->t) {
        sem_raise(ctx, bin_expr->op.offset, "type error", "mismatched types");
    }

    ast_type_set(ctx->ast, &bin_expr->head, lt);
}

static void check_var_decl(TypeCheckCtx *ctx, VarDecl *var_decl) {
    TypeRepr *resolved_type = BAD_PTR;

    if (var_decl->init.ptr != NULL) {
        resolved_type = type_of_expr(ctx, var_decl->init.ptr);
    }

    Type *type = var_decl->binding->type.ptr;
    if (type != NULL) {
        TypeRepr *expected_type = type_from_tree(ctx, type);
        if (resolved_type != BAD_PTR && expected_type->t != resolved_type->t) {
            sem_raise(ctx, var_decl->head.offset, "type error",
                      "expression does not match declared type");
        }
        resolved_type = expected_type;
    }

    assert(resolved_type != BAD_PTR);

    // Set type inferred from right-hand side
    ast_type_set(ctx->ast, &var_decl->head, resolved_type);
}
