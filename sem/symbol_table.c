#include <assert.h>

#include "../ast/ast.h"

static DfsCtrl build_symbol_table_enter(void *_ctx, AstNode *node);
static DfsCtrl build_symbol_table_exit(void *_ctx, AstNode *node);

typedef struct {
    Ast *ast;
    Stack scope_node_ctx;
} SymbolTableCtx;

void do_build_symbol_table(Ast *ast) {
    SymbolTableCtx ctx = {
        .ast = ast,
        .scope_node_ctx = stack_new(),
    };
    ast_traverse_dfs(&ctx, ast,
                     (EnterExitVTable){
                         .enter = build_symbol_table_enter,
                         .exit = build_symbol_table_exit,
                     });
    // Make sure traversal properly popped everything correctly.
    // NOTE: no cleanup is needed for the stack as segments are freed
    // once they are popped.
    assert(ctx.scope_node_ctx.top == NULL);
}

static void subscope_start(SymbolTableCtx *ctx, string symbol, AstNode *node);
static void enter_source_file(SymbolTableCtx *ctx, SourceFile *source_file);
static void exit_source_file(SymbolTableCtx *ctx, SourceFile *source_file);
static void enter_type_decl(SymbolTableCtx *ctx, TypeDecl *decl);
static void exit_type_decl(SymbolTableCtx *ctx, TypeDecl *decl);
static void enter_struct_field(SymbolTableCtx *ctx, StructField *field);
static void enter_fn_decl(SymbolTableCtx *ctx, FnDecl *fn_decl);
static void exit_fn_decl(SymbolTableCtx *ctx, FnDecl *fn_decl);
static void exit_var_decl(SymbolTableCtx *ctx, VarDecl *var_decl);
static void enter_if_stmt(SymbolTableCtx *ctx, IfStmt *if_stmt);
static void exit_if_stmt(SymbolTableCtx *ctx, IfStmt *if_stmt);
static void enter_comp_stmt(SymbolTableCtx *ctx, CompStmt *comp_stmt);
static void exit_comp_stmt(SymbolTableCtx *ctx, CompStmt *comp_stmt);
static void enter_while_stmt(SymbolTableCtx *ctx, WhileStmt *while_stmt);
static void exit_while_stmt(SymbolTableCtx *ctx, WhileStmt *while_stmt);

static DfsCtrl build_symbol_table_enter(void *_ctx, AstNode *node) {
    SymbolTableCtx *ctx = _ctx;
    switch (node->kind) {
        case NODE_SOURCE_FILE:
            enter_source_file(ctx, (SourceFile *)node);
            break;
        case NODE_TYPE_DECL:
            enter_type_decl(ctx, (TypeDecl *)node);
            break;
        case NODE_STRUCT_FIELD:
            enter_struct_field(ctx, (StructField *)node);
            break;
        case NODE_FN_DECL:
            enter_fn_decl(ctx, (FnDecl *)node);
            break;
        case NODE_IF_STMT:
            enter_if_stmt(ctx, (IfStmt *)node);
            break;
        case NODE_COMP_STMT:
            enter_comp_stmt(ctx, (CompStmt *)node);
            break;
        case NODE_WHILE_STMT:
            enter_while_stmt(ctx, (WhileStmt *)node);
            break;
        default:
            break;
    }
    return DFS_CTRL_KEEP_GOING;
}

static DfsCtrl build_symbol_table_exit(void *_ctx, AstNode *node) {
    SymbolTableCtx *ctx = _ctx;
    switch (node->kind) {
        case NODE_SOURCE_FILE:
            exit_source_file(ctx, (SourceFile *)node);
            break;
        case NODE_TYPE_DECL:
            exit_type_decl(ctx, (TypeDecl *)node);
            break;
        case NODE_FN_DECL:
            exit_fn_decl(ctx, (FnDecl *)node);
            break;
        case NODE_VAR_DECL:
            exit_var_decl(ctx, (VarDecl *)node);
            break;
        case NODE_IF_STMT:
            exit_if_stmt(ctx, (IfStmt *)node);
            break;
        case NODE_COMP_STMT:
            exit_comp_stmt(ctx, (CompStmt *)node);
            break;
        case NODE_WHILE_STMT:
            exit_while_stmt(ctx, (WhileStmt *)node);
            break;
        default:
            break;
    }
    return DFS_CTRL_KEEP_GOING;
}

static void enter_source_file(SymbolTableCtx *ctx, SourceFile *source_file) {
    AstNode **node = stack_push(&ctx->scope_node_ctx, sizeof(AstNode *),
                                _Alignof(AstNode *));
    *node = &source_file->head;
    ast_scope_set(ctx->ast, *node, ast_scope_create(ctx->ast));
}

static void exit_source_file(SymbolTableCtx *ctx, SourceFile *source_file) {
    (void)source_file;
    stack_pop(&ctx->scope_node_ctx);
}

static void scope_insert_enclosing(SymbolTableCtx *ctx, string symbol,
                                   AstNode *node) {
    AstNode **enclosing_node = stack_top(&ctx->scope_node_ctx);
    Scope *enclosing_scope = ast_scope_get(ctx->ast, *enclosing_node);
    assert(enclosing_scope);
    ast_scope_insert(ctx->ast, enclosing_scope, symbol, node);
}

static void subscope_start(SymbolTableCtx *ctx, string symbol, AstNode *node) {
    scope_insert_enclosing(ctx, symbol, node);

    AstNode **enclosing_node = stack_top(&ctx->scope_node_ctx);
    Scope *enclosing_scope = ast_scope_get(ctx->ast, *enclosing_node);
    assert(enclosing_scope);

    AstNode **entry = stack_push(&ctx->scope_node_ctx, sizeof(AstNode *),
                                 _Alignof(AstNode *));
    *entry = node;

    Scope *sub_scope = ast_scope_create(ctx->ast);
    sub_scope->enclosing_scope.ptr = enclosing_scope;
    ast_scope_set(ctx->ast, *entry, sub_scope);
}

static void anon_subscope_start(SymbolTableCtx *ctx, AstNode *node) {
    Scope *anon_scope = ast_scope_create(ctx->ast);
    if (!stack_empty(&ctx->scope_node_ctx)) {
        AstNode **enclosing_node = stack_top(&ctx->scope_node_ctx);
        anon_scope->enclosing_scope.ptr =
            ast_scope_get(ctx->ast, *enclosing_node);
    }
    ast_scope_set(ctx->ast, node, anon_scope);
    AstNode **entry = stack_push(&ctx->scope_node_ctx, sizeof(AstNode *),
                                 _Alignof(AstNode *));
    *entry = node;
}

static void enter_type_decl(SymbolTableCtx *ctx, TypeDecl *decl) {
    subscope_start(ctx, decl->name->token.text, &decl->head);
}

static void exit_type_decl(SymbolTableCtx *ctx, TypeDecl *decl) {
    (void)decl;
    stack_pop(&ctx->scope_node_ctx);
}

static void enter_struct_field(SymbolTableCtx *ctx, StructField *field) {
    scope_insert_enclosing(ctx, field->binding->name->token.text, &field->head);
}

static void enter_fn_decl(SymbolTableCtx *ctx, FnDecl *fn_decl) {
    subscope_start(ctx, fn_decl->name->token.text, &fn_decl->head);
}

static void exit_fn_decl(SymbolTableCtx *ctx, FnDecl *fn_decl) {
    (void)fn_decl;
    stack_pop(&ctx->scope_node_ctx);
}

static void exit_var_decl(SymbolTableCtx *ctx, VarDecl *var_decl) {
    Binding *binding = var_decl->binding;
    scope_insert_enclosing(ctx, binding->name->token.text, &var_decl->head);
}

static void enter_if_stmt(SymbolTableCtx *ctx, IfStmt *if_stmt) {
    anon_subscope_start(ctx, &if_stmt->head);
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
    AstNode *parent = comp_stmt->head.parent.ptr;
    assert(parent);
    if (parent->kind == NODE_STMT) {
        anon_subscope_start(ctx, &comp_stmt->head);
    }
}

static void exit_comp_stmt(SymbolTableCtx *ctx, CompStmt *comp_stmt) {
    AstNode *parent = comp_stmt->head.parent.ptr;
    assert(parent);
    if (parent->kind == NODE_STMT) {
        stack_pop(&ctx->scope_node_ctx);
    }
}

static void enter_while_stmt(SymbolTableCtx *ctx, WhileStmt *while_stmt) {
    anon_subscope_start(ctx, &while_stmt->head);
}

static void exit_while_stmt(SymbolTableCtx *ctx, WhileStmt *while_stmt) {
    (void)while_stmt;
    stack_pop(&ctx->scope_node_ctx);
}
