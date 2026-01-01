#include <assert.h>

#include "../ast/ast.h"
#include "sem.h"

static DfsCtrl resolve_names_enter(void *_ctx, AstNode *node);
static DfsCtrl resolve_names_exit(void *_ctx, AstNode *node);

typedef struct {
    Ast *ast;
    Stack scopes;
    Scope *global_scope;
    SourceCode *code;
    NULLABLE_PTR(FnDecl) curr_fn;
    NULLABLE_PTR(VarDecl) curr_var_decl;
} NameResCtx;

void do_resolve_names(Ast *ast, SourceCode *code) {
    NameResCtx ctx = {
        .scopes = stack_new(),
        .ast = ast,
        .code = code,
        .global_scope = NULL,
        .curr_fn.ptr = NULL,
        .curr_var_decl.ptr = NULL,
    };
    ast_traverse_dfs(&ctx, ast,
                     (EnterExitVTable){
                         .enter = resolve_names_enter,
                         .exit = resolve_names_exit,
                     });
    // Make sure traversal properly popped everything correctly.
    // NOTE: no cleanup is needed for the stack as segments are freed
    // once they are popped.
    assert(ctx.scopes.top == NULL);
}

static void manage_scopes_enter_hook(NameResCtx *ctx, AstNode *node);
static void manage_scopes_exit_hook(NameResCtx *ctx, AstNode *node);
static void push_any_scope(NameResCtx *ctx, AstNode *n);
static void pop_any_scope(NameResCtx *ctx, AstNode *n);

static void resolve_ref(NameResCtx *ctx, ScopedIdent *scoped_ident);

typedef struct {
    Ident *ident;
    AstNode *resolves_to;
} DeclDesc;

static DeclDesc get_decl_desc(Decl *decl);
static void do_shadow_check(NameResCtx *ctx, DeclDesc decl_desc);
static Scope *get_curr_scope(NameResCtx *ctx);

static DfsCtrl resolve_names_enter(void *_ctx, AstNode *node) {
    NameResCtx *ctx = _ctx;
    manage_scopes_enter_hook(ctx, node);
    switch (node->kind) {
        case NODE_TYPE_DECL: {
            TypeDecl *td = (TypeDecl *)node;
            do_shadow_check(ctx, (DeclDesc){
                                     .ident = td->name,
                                     .resolves_to = &td->head,
                                 });
            break;
        }
        case NODE_DECL: {
            Decl *d = (Decl *)node;
            Scope *curr_scope = get_curr_scope(ctx);
            // Skip local variables (only things legal to shadow)
            if (d->t == DECL_VAR && curr_scope != ctx->global_scope) {
                break;
            }
            do_shadow_check(ctx, get_decl_desc(d));
            break;
        }
        case NODE_FN_DECL:
            assert(ctx->curr_fn.ptr == NULL && "TODO: support local functions");
            ctx->curr_fn.ptr = (FnDecl *)node;
            break;
        case NODE_VAR_DECL:
            assert(ctx->curr_var_decl.ptr == NULL);
            ctx->curr_var_decl.ptr = (VarDecl *)node;
            break;
        case NODE_SCOPED_IDENT:
            resolve_ref(ctx, (ScopedIdent *)node);
            break;
        default:
            break;
    }
    return DFS_CTRL_KEEP_GOING;
}

static DfsCtrl resolve_names_exit(void *_ctx, AstNode *node) {
    NameResCtx *ctx = _ctx;
    manage_scopes_exit_hook(ctx, node);
    switch (node->kind) {
        case NODE_FN_DECL:
            ctx->curr_fn.ptr = NULL;
            break;
        case NODE_VAR_DECL:
            ctx->curr_var_decl.ptr = NULL;
            break;
        default:
            break;
    }
    return DFS_CTRL_KEEP_GOING;
}

static Scope *get_curr_scope(NameResCtx *ctx) {
    return *(Scope **)stack_top(&ctx->scopes);
}

static bool ref_can_resolve_to(NameResCtx *ctx, ScopeLookup lookup, Ident *ref,
                               AstNode *cand) {
    bool ref_out_of_order = ctx->curr_fn.ptr != NULL &&
                            !(lookup.found_in == ctx->global_scope) &&
                            cand->offset > ref->token.offset;
    VarDecl *var = ctx->curr_var_decl.ptr;
    // Disallow: let x = x;
    //                   ^- 'x' cannot resolve to the lhs here
    bool is_curr_var_lhs = var != NULL && &var->head == cand;
    return !ref_out_of_order && !is_curr_var_lhs;
}

static void resolve_ref(NameResCtx *ctx, ScopedIdent *scoped_ident) {
    assert(da_length(scoped_ident->head.children) != 0);

    Tok first = child_ident_at(&scoped_ident->head, 0)->token;
    if (first.t == T_EMPTY_STRING) {
        // Inferred reference
        return;
    }
    assert(first.t == T_IDENT);

    AstNode *h = &scoped_ident->head;

    Scope *scope = get_curr_scope(ctx);
    for (u32 i = 0; i < da_length(h->children); i++) {
        Ident *ident = child_ident_at(&scoped_ident->head, i);
        u32 defined_at = ident->token.offset;
        string ident_text = ident->token.text;

        if (!scope) {
            string supposed_scope = child_ident_at(h, i - 1)->token.text;
            sem_raisef(
                ctx->ast, ctx->code, defined_at,
                "lookup error: cannot resolve '{s}' in '{s}' as '{s}' does "
                "not define a scope",
                ident_text, supposed_scope, supposed_scope);
            break;
        }

        ScopeLookup lookup =
            scope_lookup(scope, ident_text,
                         i == 0 ? LOOKUP_MODE_LEXICAL : LOOKUP_MODE_DIRECT);
        if (!lookup.entry) {
            if (i != 0) {
                string parent = child_ident_at(h, i - 1)->token.text;
                sem_raisef(ctx->ast, ctx->code, defined_at,
                           "lookup error: '{s}' not found inside scope '{s}'",
                           ident_text, parent);
            } else {
                sem_raisef(ctx->ast, ctx->code, defined_at,
                           "lookup error: '{s}' not found in current scope",
                           ident_text);
            }
            break;
        }

        // Prevent access to the identifiers inside of a function when
        // not inside its body
        if (lookup.entry->node->kind == NODE_FN_DECL &&
            i != da_length(h->children) - 1) {
            FnDecl *res_fn = (FnDecl *)lookup.entry->node;
            FnDecl *curr_fn = ctx->curr_fn.ptr;
            if (curr_fn == NULL || curr_fn != res_fn) {
                sem_raisef(ctx->ast, ctx->code, defined_at, "access error",
                           "illegal access of scope '{s}'; cannot "
                           "access function scope outside of its body",
                           res_fn->name->token.text);
            }
        }

        // One or more candidates were found, now make sure one of them makes
        // sense
        ScopeEntry *it = lookup.entry;
        while (it) {
            if (ref_can_resolve_to(ctx, lookup, ident, it->node)) {
                ast_resolves_to_set(ctx->ast, ident, it->node);
                break;
            }
            it = it->shadows;
        }
        if (!it) {
            // Reached the end, no candidate was valid.
            sem_raisef(ctx->ast, ctx->code, defined_at,
                       "error: could not resolve name");
        } else {
            scope = it->sub_scope.ptr;
        }
    }
}

static void manage_scopes_enter_hook(NameResCtx *ctx, AstNode *node) {
    if (node->kind == NODE_SOURCE_FILE) {
        ctx->global_scope = ast_scope_get(ctx->ast, node);
        assert(ctx->global_scope);
    }
    push_any_scope(ctx, node);
}

static void manage_scopes_exit_hook(NameResCtx *ctx, AstNode *node) {
    pop_any_scope(ctx, node);
}

static void push_any_scope(NameResCtx *ctx, AstNode *n) {
    Scope *scope = ast_scope_get(ctx->ast, n);
    if (!scope) {
        return;
    }
    Scope **scope_ref =
        stack_push(&ctx->scopes, sizeof(Scope *), _Alignof(Scope *));
    *scope_ref = scope;
}

static void pop_any_scope(NameResCtx *ctx, AstNode *n) {
    if (ast_scope_get(ctx->ast, n)) {
        stack_pop(&ctx->scopes);
    }
}

static DeclDesc get_decl_desc(Decl *decl) {
    switch (decl->t) {
        case DECL_FN:
            return (DeclDesc){decl->fn_decl->name, &decl->fn_decl->head};
        case DECL_VAR:
            return (DeclDesc){decl->var_decl->binding->name,
                              &decl->var_decl->head};
        case DECL_TYPE:
            return (DeclDesc){decl->type_decl->name, &decl->var_decl->head};
    }
    assert(false && "unreachable");
}

static void do_shadow_check(NameResCtx *ctx, DeclDesc decl_desc) {
    Scope *curr_scope = get_curr_scope(ctx);
    ScopeLookup lookup = scope_lookup(curr_scope, decl_desc.ident->token.text,
                                      LOOKUP_MODE_DIRECT);
    assert(lookup.entry);  // sanity check, this should be the case if symbol
                           // table has been built

    // Find the declaration position in shadow set
    ScopeEntry *it = lookup.entry;
    while (it) {
        if (decl_desc.resolves_to == it->node) {
            break;
        }
        it = it->shadows;
    }
    assert(it);

    // Report shadow error if it is the second declaration with the same name.
    // Other shadows are ignored, we only provide error for the first one.
    ScopeEntry *shadowed = it->shadows;
    if (shadowed && shadowed->shadows == NULL) {
        u32 prev_decl_offset = shadowed->node->offset;
        Position pos = line_and_column(ctx->code->lines, prev_decl_offset);
        sem_raisef(
            ctx->ast, ctx->code, decl_desc.resolves_to->offset,
            "error: declaration shadows previous declaration at {s}:{i}:{i}",
            ctx->code->file_path, pos.line, pos.column);
    }
}
