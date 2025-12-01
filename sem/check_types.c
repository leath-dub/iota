#include "sem.h"

static DfsCtrl check_types_enter(void *_ctx, AnyNode node);
static DfsCtrl check_types_exit(void *_ctx, AnyNode node);

typedef struct {
    SourceCode *code;
    NodeMetadata *meta;
} TypeCheckCtx;

void do_check_types(SourceCode *code, NodeMetadata *m, AnyNode root) {
    TypeCheckCtx ctx = {
        .meta = m,
        .code = code,
    };
    ast_traverse_dfs(&ctx, root, m,
                     (EnterExitVTable){
                         .enter = check_types_enter,
                         .exit = check_types_exit,
                     });
}

static DfsCtrl check_types_enter(void *_ctx, AnyNode node) {
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

static DfsCtrl check_types_exit(void *_ctx, AnyNode node) {
    TypeCheckCtx *ctx = _ctx;
    switch (node.kind) {
        case NODE_VAR_DECL:
            check_var_decl(ctx, (VarDecl *)node.data);
            break;
        case NODE_ATOM:
            check_atom(ctx, (Atom *)node.data);
            break;
        // case EXPR_POSTFIX:
        //     check_postfix_expr(ctx, (PostfixExpr *)node.data);
        case NODE_CALL:
            check_call(ctx, (Call *)node.data);
            break;
        // case EXPR_FIELD_ACCESS:
        //     check_field_access(ctx, (FieldAccess *)node.data);
        //     break;
        // case EXPR_COLL_ACCESS:
        //     check_coll_access(ctx, (CollAccess *)node.data);
        //     break;
        // case EXPR_UNARY:
        //     check_unary_expr(ctx, (UnaryExpr *)node.data);
        //     break;
        case NODE_BIN_EXPR:
            check_bin_expr(ctx, (BinExpr *)node.data);
            break;
        default:
            break;
    }
    return DFS_CTRL_KEEP_GOING;
}

static TypeRepr *type_from_symbol(TypeCheckCtx *ctx, AnyNode symbol);
static AnyNode get_scoped_ident_symbol(TypeCheckCtx *ctx,
                                       ScopedIdent *scoped_ident);

static TypeRepr *type_from_token(TypeCheckCtx *ctx, Tok token) {
    TypeRepr *type = type_alloc(ctx->meta);
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
    TypeRepr *type = type_alloc(ctx->meta);
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
            TypeRepr *type = type_alloc(ctx->meta);
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
        case TYPE_UNION:
        case TYPE_ENUM:
        case TYPE_ERR:
            break;
    }
    assert(false && "TODO");
}

static TypeRepr *type_from_symbol(TypeCheckCtx *ctx, AnyNode symbol) {
    switch (symbol.kind) {
        case NODE_FN_DECL: {
            FnDecl *fn = (FnDecl *)symbol.data;

            TypeRepr *fn_type = type_alloc(ctx->meta);

            fn_type->t = STORAGE_FN;

            for (u32 i = 0; i < fn->params->len; i++) {
                FnParam *param = fn->params->items[i];
                FnTParam tparam = {
                    .type = type_from_tree(ctx, param->type),
                    .variadic = param->variadic,
                };
                APPEND(&fn_type->fn.params, tparam);
            }

            TypeRepr *return_type = NULL;
            if (fn->return_type.ptr != NULL) {
                return_type = type_from_tree(ctx, fn->return_type.ptr);
            } else {
                return_type = type_alloc(ctx->meta);
                return_type->t = STORAGE_UNIT;
            }

            fn_type->fn.return_type = return_type;

            return fn_type;
        }
        case NODE_VAR_DECL: {
            VarDecl *var = (VarDecl *)symbol.data;
            assert(var->binding->t == VAR_BINDING_BASIC && "TODO");
            TypeRepr *type = type_try_get(ctx->meta, var->id);
            assert(type);
            return type;
        }
        default:
            break;
    }
    assert(false && "TODO");
}

static AnyNode get_scoped_ident_symbol(TypeCheckCtx *ctx,
                                       ScopedIdent *scoped_ident) {
    assert(scoped_ident->items[0]->token.t != T_EMPTY_STRING &&
           "TODO inferred");
    AnyNode *node = try_get_resolved_node(
        ctx->meta, scoped_ident->items[scoped_ident->len - 1]->id);
    assert(node);
    return *node;
}

static void check_atom(TypeCheckCtx *ctx, Atom *atom) {
    switch (atom->t) {
        case ATOM_TOKEN:
            type_set(ctx->meta, MAKE_ANY(atom),
                     type_from_lit(ctx, atom->builtin_type));
            break;
        case ATOM_SCOPED_IDENT:
            type_set(ctx->meta, MAKE_ANY(atom),
                     type_from_symbol(ctx, get_scoped_ident_symbol(
                                               ctx, atom->scoped_ident)));
            break;
        default:
            break;
    }
}

static TypeRepr *type_of_expr(TypeCheckCtx *ctx, Expr *expr) {
    NodeChildren *children = get_node_children(ctx->meta, expr->id);
    NodeChild child = children->items[0];
    assert(child.t == CHILD_NODE);
    return type_try_get(ctx->meta, *child.node.data);
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
            type_set(ctx->meta, MAKE_ANY(call), type);
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
}

static void check_var_decl(TypeCheckCtx *ctx, VarDecl *var_decl) {
    TypeRepr *resolved_type = BAD_PTR;

    if (var_decl->init.ok) {
        resolved_type = type_of_expr(ctx, var_decl->init.expr);
    }

    if (var_decl->type) {
        TypeRepr *expected_type = type_from_tree(ctx, var_decl->type);
        if (resolved_type != BAD_PTR && expected_type->t != resolved_type->t) {
            sem_raise(ctx, get_node_offset(ctx->meta, var_decl->type->id),
                      "type error", "expression does not match declared type");
        }
        resolved_type = expected_type;
    }

    assert(resolved_type != BAD_PTR);

    // Set type inferred from right-hand side
    type_set(ctx->meta, MAKE_ANY(var_decl), resolved_type);
}
