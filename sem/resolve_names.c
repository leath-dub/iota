#include "../ast/ast.h"

static DfsCtrl resolve_names_enter(void *_ctx, AnyNode node);
static DfsCtrl resolve_names_exit(void *_ctx, AnyNode node);

typedef struct {
    Stack scopes;
    Scope *global_scope;
    SourceCode *code;
    NodeMetadata *meta;
    NULLABLE_PTR(FnDecl) curr_fn;
    NULLABLE_PTR(VarDecl) curr_var_decl;
} NameResCtx;

void do_resolve_names(SourceCode *code, NodeMetadata *m, AnyNode root) {
    NameResCtx ctx = {
        .scopes = stack_new(),
        .meta = m,
        .code = code,
        .global_scope = NULL,
        .curr_fn.ptr = NULL,
        .curr_var_decl.ptr = NULL,
    };
    ast_traverse_dfs(&ctx, root, m,
                     (EnterExitVTable){
                         .enter = resolve_names_enter,
                         .exit = resolve_names_exit,
                     });
    // Make sure traversal properly popped everything correctly.
    // NOTE: no cleanup is needed for the stack as segments are freed
    // once they are popped.
    assert(ctx.scopes.top == NULL);
}

static void manage_scopes_enter_hook(NameResCtx *ctx, AnyNode node);
static void manage_scopes_exit_hook(NameResCtx *ctx, AnyNode node);
static void push_any_scope(NameResCtx *ctx, NodeID id);
static void pop_any_scope(NameResCtx *ctx, NodeID id);

static void resolve_ref(NameResCtx *ctx, ScopedIdent *scoped_ident);
static void check_top_level_decl(NameResCtx *ctx, Decl *decl);

static DfsCtrl resolve_names_enter(void *_ctx, AnyNode node) {
    NameResCtx *ctx = _ctx;
    bool top_level = ctx->curr_fn.ptr == NULL;
    if (top_level && node.kind == NODE_DECL) {
        check_top_level_decl(ctx, (Decl *)node.data);
    }
    manage_scopes_enter_hook(ctx, node);
    switch (node.kind) {
        case NODE_FN_DECL:
            assert(ctx->curr_fn.ptr == NULL && "TODO: support local functions");
            ctx->curr_fn.ptr = (FnDecl *)node.data;
            break;
        case NODE_VAR_DECL:
            assert(ctx->curr_var_decl.ptr == NULL);
            ctx->curr_var_decl.ptr = (VarDecl *)node.data;
            break;
        case NODE_SCOPED_IDENT:
            resolve_ref(ctx, (ScopedIdent *)node.data);
            return DFS_CTRL_KEEP_GOING;
        default:
            break;
    }
    return DFS_CTRL_KEEP_GOING;
}

static DfsCtrl resolve_names_exit(void *_ctx, AnyNode node) {
    NameResCtx *ctx = _ctx;
    manage_scopes_exit_hook(ctx, node);
    switch (node.kind) {
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

static void sem_raise(NameResCtx *ctx, u32 at, const char *banner,
                      const char *msg) {
    raise_semantic_error(ctx->code, (SemanticError){
                                        .at = at,
                                        .banner = banner,
                                        .message = msg,
                                    });
}

static bool ref_can_resolve_to(NameResCtx *ctx, ScopeLookup lookup, Ident *ref,
                               AnyNode cand) {
    bool ref_out_of_order =
        ctx->curr_fn.ptr != NULL && !(lookup.found_in == ctx->global_scope) &&
        get_node_offset(ctx->meta, *cand.data) > ref->token.offset;
    VarDecl *var = ctx->curr_var_decl.ptr;
    // Disallow: let x = x;
    //                   ^- 'x' cannot resolve to the lhs here
    bool is_curr_var_lhs = var != NULL && var->id == *cand.data;
    return !ref_out_of_order && !is_curr_var_lhs;
}

static void resolve_ref(NameResCtx *ctx, ScopedIdent *scoped_ident) {
    assert(scoped_ident->len != 0);

    Tok first = scoped_ident->items[0]->token;
    if (first.t == T_EMPTY_STRING) {
        // Inferred reference
        return;
    }
    assert(first.t == T_IDENT);

    Arena *erra = &ctx->code->error_arena;

    Scope *scope = get_curr_scope(ctx);
    for (u32 i = 0; i < scoped_ident->len; i++) {
        Ident *ident = scoped_ident->items[i];
        u32 defined_at = ident->token.offset;
        string ident_text = ident->token.text;

        if (!scope) {
            string supposed_scope = scoped_ident->items[i - 1]->token.text;
            sem_raise(ctx, defined_at, "error",
                      allocf(erra,
                             "cannot resolve '%.*s' in '%.*s' as '%.*s' does "
                             "not define a scope",
                             SPLAT(ident_text), SPLAT(supposed_scope),
                             SPLAT(supposed_scope)));
            break;
        }

        ScopeLookup lookup =
            scope_lookup(scope, ident_text,
                         i == 0 ? LOOKUP_MODE_LEXICAL : LOOKUP_MODE_DIRECT);
        if (!lookup.entry) {
            if (i != 0) {
                string parent = scoped_ident->items[i - 1]->token.text;
                sem_raise(ctx, defined_at, "error",
                          allocf(erra, "'%.*s' not found inside scope '%.*s'",
                                 SPLAT(ident_text), SPLAT(parent)));
            } else {
                sem_raise(ctx, defined_at, "error",
                          allocf(erra, "'%.*s' not found in current scope",
                                 SPLAT(ident_text)));
            }
            break;
        }
        scope = scope_get(ctx->meta, *lookup.entry->node.data);

        // Prevent access to the identifiers inside of a function when
        // not inside its body
        if (lookup.entry->node.kind == NODE_FN_DECL &&
            i != scoped_ident->len - 1) {
            FnDecl *res_fn = (FnDecl *)lookup.entry->node.data;
            FnDecl *curr_fn = ctx->curr_fn.ptr;
            if (curr_fn == NULL || curr_fn->id != res_fn->id) {
                sem_raise(ctx, defined_at, "error",
                          allocf(erra,
                                 "illegal access of scope '%.*s'; cannot "
                                 "access function scope outside of its body",
                                 SPLAT(res_fn->name->token.text)));
            }
        }

        // One or more candidates were found, now make sure one of them makes
        // sense
        ScopeEntry *it = lookup.entry;
        while (it) {
            if (ref_can_resolve_to(ctx, lookup, ident, it->node)) {
                set_resolved_node(ctx->meta, MAKE_ANY(ident), it->node);
                break;
            }
            it = it->shadows;
        }
        if (!it) {
            // Reached the end, no candidate was valid.
            sem_raise(ctx, defined_at, "error", "could not resolve name");
        }
    }
}

static void manage_scopes_enter_hook(NameResCtx *ctx, AnyNode node) {
    if (node.kind == NODE_SOURCE_FILE) {
        ctx->global_scope = scope_get(ctx->meta, *node.data);
        assert(ctx->global_scope);
    }
    push_any_scope(ctx, *node.data);
}

static void manage_scopes_exit_hook(NameResCtx *ctx, AnyNode node) {
    pop_any_scope(ctx, *node.data);
}

static void push_any_scope(NameResCtx *ctx, NodeID id) {
    Scope *scope = scope_get(ctx->meta, id);
    if (!scope) {
        return;
    }
    Scope **scope_ref =
        stack_push(&ctx->scopes, sizeof(Scope *), _Alignof(Scope *));
    *scope_ref = scope;
}

static void pop_any_scope(NameResCtx *ctx, NodeID id) {
    if (scope_get(ctx->meta, id)) {
        stack_pop(&ctx->scopes);
    }
}

typedef struct {
    Ident *ident;
    NodeID id;
} DeclDesc;

static DeclDesc get_decl_desc(Decl *decl) {
    switch (decl->t) {
        case DECL_STRUCT:
            return (DeclDesc){decl->struct_decl->name, decl->struct_decl->id};
        case DECL_ENUM:
            return (DeclDesc){decl->enum_decl->name, decl->enum_decl->id};
        case DECL_ERR:
            return (DeclDesc){decl->err_decl->name, decl->err_decl->id};
        case DECL_FN:
            return (DeclDesc){decl->fn_decl->name, decl->fn_decl->id};
        case DECL_UNION:
            return (DeclDesc){decl->union_decl->name, decl->union_decl->id};
        case DECL_VAR:
            assert(decl->var_decl->binding->t == VAR_BINDING_BASIC && "TODO");
            return (DeclDesc){decl->var_decl->binding->basic,
                              decl->var_decl->id};
    }
    assert(false && "unreachable");
}

static void check_top_level_decl(NameResCtx *ctx, Decl *decl) {
    assert(get_curr_scope(ctx) == ctx->global_scope);

    DeclDesc decl_desc = get_decl_desc(decl);

    ScopeLookup lookup = scope_lookup(
        ctx->global_scope, decl_desc.ident->token.text, LOOKUP_MODE_DIRECT);
    assert(lookup.entry);  // sanity check, this should be the case if symbol
                           // table has been built

    // Find the declaration position in shadow set
    ScopeEntry *it = lookup.entry;
    while (it) {
        if (decl_desc.id == *it->node.data) {
            break;
        }
        it = it->shadows;
    }
    assert(it);

    // Report shadow error if it is the second declaration with the same name.
    // Other shadows are ignored, we only provide error for the first one.
    ScopeEntry *shadowed = it->shadows;
    if (shadowed && shadowed->shadows == NULL) {
        Arena *erra = &ctx->code->error_arena;
        u32 prev_decl_offset = get_node_offset(ctx->meta, *shadowed->node.data);
        Position pos = line_and_column(ctx->code->lines, prev_decl_offset);
        sem_raise(
            ctx, decl_desc.ident->token.offset, "error",
            allocf(erra,
                   "declaration shadows previous declaration at %.*s:%d:%d",
                   SPLAT(ctx->code->file_path), pos.line, pos.column));
    }
}
