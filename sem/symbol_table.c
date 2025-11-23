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
static void exit_var_decl(SymbolTableCtx *ctx, VarDecl *var_decl);
static void enter_if_stmt(SymbolTableCtx *ctx, IfStmt *if_stmt);
static void exit_if_stmt(SymbolTableCtx *ctx, IfStmt *if_stmt);
static void enter_comp_stmt(SymbolTableCtx *ctx, CompStmt *comp_stmt);
static void exit_comp_stmt(SymbolTableCtx *ctx, CompStmt *comp_stmt);
static void enter_while_stmt(SymbolTableCtx *ctx, WhileStmt *while_stmt);
static void exit_while_stmt(SymbolTableCtx *ctx, WhileStmt *while_stmt);

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
        case NODE_IF_STMT:
            enter_if_stmt(ctx, (IfStmt *)node.data);
            break;
        case NODE_COMP_STMT:
            enter_comp_stmt(ctx, (CompStmt *)node.data);
            break;
        case NODE_WHILE_STMT:
            enter_while_stmt(ctx, (WhileStmt *)node.data);
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
        case NODE_VAR_DECL:
            exit_var_decl(ctx, (VarDecl *)node.data);
            break;
        case NODE_IF_STMT:
            exit_if_stmt(ctx, (IfStmt *)node.data);
            break;
        case NODE_COMP_STMT:
            exit_comp_stmt(ctx, (CompStmt *)node.data);
            break;
        case NODE_WHILE_STMT:
            exit_while_stmt(ctx, (WhileStmt *)node.data);
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
    scope_insert_enclosing(ctx, symbol, node);

    AnyNode *enclosing_node = stack_top(&ctx->scope_node_ctx);
    Scope *enclosing_scope = scope_get(ctx->meta, *enclosing_node->data);
    assert(enclosing_scope);

    AnyNode *entry =
        stack_push(&ctx->scope_node_ctx, sizeof(AnyNode), _Alignof(AnyNode));
    *entry = node;

    Scope *sub_scope = scope_attach(ctx->meta, *entry);
    sub_scope->enclosing_scope.ptr = enclosing_scope;
}

static void anon_subscope_start(SymbolTableCtx *ctx, AnyNode node) {
    Scope *anon_scope = scope_attach(ctx->meta, node);
    if (!stack_empty(&ctx->scope_node_ctx)) {
        AnyNode *enclosing_node = stack_top(&ctx->scope_node_ctx);
        anon_scope->enclosing_scope.ptr =
            scope_get(ctx->meta, *enclosing_node->data);
    }
    AnyNode *entry =
        stack_push(&ctx->scope_node_ctx, sizeof(AnyNode), _Alignof(AnyNode));
    *entry = node;
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

static void exit_var_decl(SymbolTableCtx *ctx, VarDecl *var_decl) {
    VarBinding *binding = var_decl->binding;
    switch (binding->t) {
        case VAR_BINDING_BASIC:
            scope_insert_enclosing(ctx, binding->basic.text,
                                   MAKE_ANY(var_decl));
            break;
        default:
            assert(false && "TODO");
    }
}

static void enter_if_stmt(SymbolTableCtx *ctx, IfStmt *if_stmt) {
    anon_subscope_start(ctx, MAKE_ANY(if_stmt));
}

static void exit_if_stmt(SymbolTableCtx *ctx, IfStmt *if_stmt) {
    (void)if_stmt;
    stack_pop(&ctx->scope_node_ctx);
}

static void enter_comp_stmt(SymbolTableCtx *ctx, CompStmt *comp_stmt) {
    // If the parent is a generic statement, it is in a statement
    // context so it must be a bare compound statement. This means it is
    // not a direct child of a function declaration, if statement, etc.
    //
    // This needs to be special cased as only bare compound statements insert
    // their own anonymous scope.
    AnyNode parent = get_node_parent(ctx->meta, comp_stmt->id);
    if (parent.kind == NODE_STMT) {
        anon_subscope_start(ctx, MAKE_ANY(comp_stmt));
    }
}

static void exit_comp_stmt(SymbolTableCtx *ctx, CompStmt *comp_stmt) {
    AnyNode parent = get_node_parent(ctx->meta, comp_stmt->id);
    if (parent.kind == NODE_STMT) {
        stack_pop(&ctx->scope_node_ctx);
    }
}

static void enter_while_stmt(SymbolTableCtx *ctx, WhileStmt *while_stmt) {
    anon_subscope_start(ctx, MAKE_ANY(while_stmt));
}

static void exit_while_stmt(SymbolTableCtx *ctx, WhileStmt *while_stmt) {
    (void)while_stmt;
    stack_pop(&ctx->scope_node_ctx);
}
