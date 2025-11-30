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

static DfsCtrl resolve_names_enter(void *_ctx, AnyNode node) {
    NameResCtx *ctx = _ctx;
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
            break;
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
            continue;
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

// static void sem_raise_at(NameResCtx *ctx, const char *message, u32 offset) {
//     raise_semantic_error(ctx->code,
//                          (SemanticError){.at = offset, .message = message});
// }
//
// // static void sem_raise(NameResCtx *ctx, NodeID id, const char *message) {
// //     u32 offset = get_node_offset(ctx->meta, id);
// //     sem_raise_at(ctx, message, offset);
// // }
//
// static Scope *current_scope(NameResCtx *ctx) {
//     return *(Scope **)stack_top(&ctx->scope_ctx);
// }
//
// static void enter_scoped_ident(NameResCtx *ctx, ScopedIdent *scoped_ident) {
//     assert(scoped_ident->len != 0);
//
//     Tok top = scoped_ident->items[0]->token;
//     if (top.t == T_EMPTY_STRING) {
//         // Inferred namespace based on a type context. Identifiers in certain
//         // contexts are deferred to type checking (e.g. field names). If you
//         // want to reference an identifier who's scope is dependent on the
//         // type context you can prefix with empty string scope (e.g. ::foo)
//         // to defer the name to be resolved based on the type context of the
//         // expression.
//         TODO("inferred namespace");
//         return;
//     }
//     assert(top.t == T_IDENT);
//
//     Scope *scope = current_scope(ctx);
//
//     for (u32 i = 0; i < scoped_ident->len; i++) {
//         Tok ident_tok = scoped_ident->items[i]->token;
//         assert(ident_tok.t == T_IDENT);
//         string ident = ident_tok.text;
//         ScopeEntry *entry = scope_lookup(
//             scope, ident, i == 0 ? LOOKUP_MODE_LEXICAL : LOOKUP_MODE_DIRECT);
//         if (!entry) {
//             if (i != 0) {
//                 // TODO: print better message about what scope it was
//                 unresolved
//                 // in
//             }
//             sem_raise_at(ctx, "unresolved identifier", ident_tok.offset);
//             return;
//         }
//         switch (entry->node.kind) {
//             case NODE_STRUCT_DECL: {
//                 Scope *type_scope = scope_get(ctx->meta, *entry->node.data);
//                 assert(type_scope);
//                 scope = type_scope;
//                 break;
//             }
//             default:
//                 TODO("this is just a test for structs only");
//                 return;
//         }
//     }
// }

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
