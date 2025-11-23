#include "../ast/ast.h"

static void resolve_names_enter(void *_ctx, NodeMetadata *m, AnyNode node);
static void resolve_names_exit(void *_ctx, NodeMetadata *m, AnyNode node);

typedef struct {
    Stack scope_ctx;
    Scope *global_scope;
    SourceCode *code;
    NodeMetadata *meta;
} NameResCtx;

void do_resolve_names(SourceCode *code, NodeMetadata *m, AnyNode root) {
    NameResCtx ctx = {
        .scope_ctx = stack_new(),
        .meta = m,
        .code = code,
        .global_scope = NULL,
    };
    ast_traverse_dfs(&ctx, root, m,
                     (EnterExitVTable){
                         .enter = resolve_names_enter,
                         .exit = resolve_names_exit,
                     });
    // Make sure traversal properly popped everything correctly.
    // NOTE: no cleanup is needed for the stack as segments are freed
    // once they are popped.
    assert(ctx.scope_ctx.top == NULL);
}

static void enter_scoped_ident(NameResCtx *ctx, ScopedIdent *scoped_ident);

static void push_any_scope(NameResCtx *ctx, NodeID id) {
    Scope *scope = scope_get(ctx->meta, id);
    if (!scope) {
        return;
    }
    Scope **scope_ref =
        stack_push(&ctx->scope_ctx, sizeof(Scope *), _Alignof(Scope *));
    *scope_ref = scope;
}

static void pop_any_scope(NameResCtx *ctx, NodeID id) {
    if (scope_get(ctx->meta, id)) {
        stack_pop(&ctx->scope_ctx);
    }
}

static void resolve_names_enter(void *_ctx, NodeMetadata *m, AnyNode node) {
    (void)m;
    NameResCtx *ctx = _ctx;
    switch (node.kind) {
        case NODE_SCOPED_IDENT:
            enter_scoped_ident(ctx, (ScopedIdent *)node.data);
            break;
        case NODE_SOURCE_FILE:
            ctx->global_scope = scope_get(ctx->meta, *node.data);
            assert(ctx->global_scope);
            push_any_scope(ctx, *node.data);
            break;
        default:
            push_any_scope(ctx, *node.data);
            break;
    }
}

static void resolve_names_exit(void *_ctx, NodeMetadata *m, AnyNode node) {
    (void)m;
    NameResCtx *ctx = _ctx;
    pop_any_scope(ctx, *node.data);
}

static void sem_raise(NameResCtx *ctx, NodeID id, const char *message) {
    u32 offset = get_node_offset(ctx->meta, id);
    raise_semantic_error(ctx->code,
                         (SemanticError){.at = offset, .message = message});
}

static Scope *current_scope(NameResCtx *ctx) {
    return *(Scope **)stack_top(&ctx->scope_ctx);
}

static void enter_scoped_ident(NameResCtx *ctx, ScopedIdent *scoped_ident) {
    assert(scoped_ident->len != 0);

    Tok top = scoped_ident->items[0];
    if (top.t == T_EMPTY_STRING) {
        // Inferred namespace based on a type context. Identifiers in certain
        // contexts are deferred to type checking (e.g. field names). If you
        // want to reference an identifier who's scope is dependent on the
        // type context you can prefix with empty string scope (e.g. ::foo)
        // to defer the name to be resolved based on the type context of the
        // expression.
        TODO("inferred namespace");
        return;
    }
    assert(top.t == T_IDENT);

    Scope *scope = current_scope(ctx);

    ScopeEntry *entry = scope_lookup(scope, top.text, LOOKUP_MODE_LEXICAL);
    if (!entry) {
        sem_raise(ctx, scoped_ident->id, "unresolved identifier");
    }
}
