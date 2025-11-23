#include "../ast/ast.h"

#define SCOPE_ENTRY_SLOTS 32
#define GLOBAL_SCOPE_ENTRY_SLOTS 128

static void build_symbol_table_enter(void *_ctx, NodeMetadata *m, AnyNode node);
static void build_symbol_table_exit(void *_ctx, NodeMetadata *m, AnyNode node);

typedef struct {
    NodeMetadata *meta;
    Stack scope_node_ctx;
} SymbolTableCtx;

void do_build_symbol_table(NodeMetadata *m, AnyNode root) {
    SymbolTableCtx ctx = {
        .meta = m,
        .scope_node_ctx = stack_new(),
    };
    ast_traverse_dfs(&ctx, root, m,
                     (EnterExitVTable){
                         .enter = build_symbol_table_enter,
                         .exit = build_symbol_table_exit,
                     });
    // Make sure traversal properly popped everything correctly
    assert(ctx.scope_node_ctx.top == NULL);
}

static void subscope_start(SymbolTableCtx *ctx, string symbol, AnyNode node);

static void enter_source_file(SymbolTableCtx *ctx, SourceFile *source_file);
static void exit_source_file(SymbolTableCtx *ctx, SourceFile *source_file);

static void enter_struct_decl(SymbolTableCtx *ctx, StructDecl *decl);
static void exit_struct_decl(SymbolTableCtx *ctx, StructDecl *decl);

static void enter_field(SymbolTableCtx *ctx, Field *field);

static void enter_fn_decl(SymbolTableCtx *ctx, FnDecl *fn_decl);
static void exit_fn_decl(SymbolTableCtx *ctx, FnDecl *fn_decl);

static void build_symbol_table_enter(void *_ctx, NodeMetadata *m,
                                     AnyNode node) {
    (void)m;
    SymbolTableCtx *ctx = _ctx;
    switch (node.kind) {
        case NODE_SOURCE_FILE:
            enter_source_file(ctx, (SourceFile *)node.data);
            break;
        case NODE_STRUCT_DECL:
            enter_struct_decl(ctx, (StructDecl *)node.data);
            break;
        case NODE_FIELD:
            enter_field(ctx, (Field *)node.data);
            break;
        case NODE_FN_DECL:
            enter_fn_decl(ctx, (FnDecl *)node.data);
            break;
        default:
            break;
    }
}

static void build_symbol_table_exit(void *_ctx, NodeMetadata *m, AnyNode node) {
    (void)m;
    SymbolTableCtx *ctx = _ctx;
    switch (node.kind) {
        case NODE_SOURCE_FILE:
            exit_source_file(ctx, (SourceFile *)node.data);
            break;
        case NODE_STRUCT_DECL:
            exit_struct_decl(ctx, (StructDecl *)node.data);
            break;
        case NODE_FN_DECL:
            exit_fn_decl(ctx, (FnDecl *)node.data);
            break;
        default:
            break;
    }
}

static void enter_source_file(SymbolTableCtx *ctx, SourceFile *source_file) {
    AnyNode *node =
        stack_push(&ctx->scope_node_ctx, sizeof(AnyNode), _Alignof(AnyNode));
    *node = MAKE_ANY(source_file);
    (void)scope_attach(ctx->meta, *node);
}

static void exit_source_file(SymbolTableCtx *ctx, SourceFile *source_file) {
    (void)source_file;
    stack_pop(&ctx->scope_node_ctx);
}

static void scope_insert_enclosing(SymbolTableCtx *ctx, string symbol,
                                   AnyNode node) {
    AnyNode *enclosing_node = stack_top(&ctx->scope_node_ctx);
    Scope *enclosing_scope = scope_get(ctx->meta, *enclosing_node->data);
    assert(enclosing_scope);
    scope_insert(ctx->meta, enclosing_scope, symbol, node);
}

static void subscope_start(SymbolTableCtx *ctx, string symbol, AnyNode node) {
    assert(!stack_empty(&ctx->scope_node_ctx));
    scope_insert_enclosing(ctx, symbol, node);
    AnyNode *entry =
        stack_push(&ctx->scope_node_ctx, sizeof(AnyNode), _Alignof(AnyNode));
    *entry = node;
    (void)scope_attach(ctx->meta, *entry);
}

static void enter_struct_decl(SymbolTableCtx *ctx, StructDecl *decl) {
    AnyNode self = MAKE_ANY(decl);
    subscope_start(ctx, decl->ident.text, self);
}

static void exit_struct_decl(SymbolTableCtx *ctx, StructDecl *decl) {
    (void)decl;
    stack_pop(&ctx->scope_node_ctx);
}

static void enter_field(SymbolTableCtx *ctx, Field *field) {
    assert(!stack_empty(&ctx->scope_node_ctx));
    scope_insert_enclosing(ctx, field->ident.text, MAKE_ANY(field));
}

static void enter_fn_decl(SymbolTableCtx *ctx, FnDecl *fn_decl) {
    AnyNode self = MAKE_ANY(fn_decl);
    subscope_start(ctx, fn_decl->ident.text, self);
}

static void exit_fn_decl(SymbolTableCtx *ctx, FnDecl *fn_decl) {
    (void)fn_decl;
    stack_pop(&ctx->scope_node_ctx);
}
