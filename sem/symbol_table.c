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

// static void subscope_start(SymbolTableCtx *ctx, string symbol, AstNode
// *node);
static void enter_source_file(SymbolTableCtx *ctx, SourceFile *source_file);
static void exit_source_file(SymbolTableCtx *ctx, SourceFile *source_file);
static void exit_type_decl(SymbolTableCtx *ctx, TypeDecl *decl);
static void enter_struct_type(SymbolTableCtx *ctx, StructType *struct_type);
static void exit_struct_type(SymbolTableCtx *ctx, StructType *struct_type);
static void enter_struct_field(SymbolTableCtx *ctx, StructField *field);
static void enter_tagged_union_type(SymbolTableCtx *ctx,
                                    TaggedUnionType *tu_type);
static void exit_tagged_union_type(SymbolTableCtx *ctx,
                                   TaggedUnionType *tu_type);
static void enter_enum_type(SymbolTableCtx *ctx, EnumType *en_type);
static void exit_enum_type(SymbolTableCtx *ctx, EnumType *en_type);
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
        case NODE_STRUCT_TYPE:
            enter_struct_type(ctx, (StructType *)node);
            break;
        case NODE_STRUCT_FIELD:
            enter_struct_field(ctx, (StructField *)node);
            break;
        case NODE_ENUM_TYPE:
            enter_enum_type(ctx, (EnumType *)node);
            break;
        case NODE_TAGGED_UNION_TYPE:
            enter_tagged_union_type(ctx, (TaggedUnionType *)node);
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
        case NODE_STRUCT_TYPE:
            exit_struct_type(ctx, (StructType *)node);
            break;
        case NODE_TAGGED_UNION_TYPE:
            exit_tagged_union_type(ctx, (TaggedUnionType *)node);
            break;
        case NODE_ENUM_TYPE:
            exit_enum_type(ctx, (EnumType *)node);
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
                                   AstNode *node, Scope *sub_scope) {
    AstNode **enclosing_node = stack_top(&ctx->scope_node_ctx);
    Scope *enclosing_scope = ast_scope_get(ctx->ast, *enclosing_node);
    assert(enclosing_scope);
    ast_scope_insert(ctx->ast, enclosing_scope, symbol, node, sub_scope);
}

// static void subscope_start(SymbolTableCtx *ctx, string symbol, AstNode *node)
// {
//     Scope *sub_scope = ast_scope_create(ctx->ast);
//     scope_insert_enclosing(ctx, symbol, node, sub_scope);
//
//     AstNode **enclosing_node = stack_top(&ctx->scope_node_ctx);
//     Scope *enclosing_scope = ast_scope_get(ctx->ast, *enclosing_node);
//     assert(enclosing_scope);
//
//     AstNode **entry = stack_push(&ctx->scope_node_ctx, sizeof(AstNode *),
//                                  _Alignof(AstNode *));
//     *entry = node;
//
//     sub_scope->enclosing_scope.ptr = enclosing_scope;
//     ast_scope_set(ctx->ast, *entry, sub_scope);
// }

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

static void enter_struct_type(SymbolTableCtx *ctx, StructType *struct_type) {
    anon_subscope_start(ctx, &struct_type->head);
}

static void exit_struct_type(SymbolTableCtx *ctx, StructType *struct_type) {
    (void)struct_type;
    stack_pop(&ctx->scope_node_ctx);
}

static void enter_tagged_union_type(SymbolTableCtx *ctx,
                                    TaggedUnionType *tu_type) {
    anon_subscope_start(ctx, &tu_type->head);
}

static Scope *type_scope_get(Ast *ast, Type *ty) {
    switch (ty->t) {
        case TYPE_STRUCT:
            return ast_scope_get(ast, &ty->struct_type->head);
        case TYPE_TAGGED_UNION:
            return ast_scope_get(ast, &ty->tagged_union_type->head);
        case TYPE_ENUM:
            return ast_scope_get(ast, &ty->enum_type->head);
        case TYPE_BUILTIN:
        case TYPE_COLL:
        case TYPE_TUPLE:
        case TYPE_ERR:
        case TYPE_PTR:
        case TYPE_FN:
        case TYPE_SCOPED_IDENT:
            return NULL;
    }
    assert(false && "unreachable");
}

static void exit_tagged_union_type(SymbolTableCtx *ctx,
                                   TaggedUnionType *tu_type) {
    (void)tu_type;
    stack_pop(&ctx->scope_node_ctx);
}

static void enter_enum_type(SymbolTableCtx *ctx, EnumType *en_type) {
    anon_subscope_start(ctx, &en_type->head);
    Idents *alts = en_type->alts;
    for (size_t i = 0; i < da_length(alts->head.children); i++) {
        Ident *id = child_ident_at(&alts->head, i);
        scope_insert_enclosing(ctx, id->token.text, &id->head, NULL);
    }
}

static void exit_enum_type(SymbolTableCtx *ctx, EnumType *en_type) {
    (void)en_type;
    stack_pop(&ctx->scope_node_ctx);
}

static void exit_type_decl(SymbolTableCtx *ctx, TypeDecl *decl) {
    scope_insert_enclosing(ctx, decl->name->token.text, &decl->head,
                           type_scope_get(ctx->ast, decl->type));
}

static void enter_struct_field(SymbolTableCtx *ctx, StructField *field) {
    scope_insert_enclosing(ctx, field->binding->name->token.text, &field->head,
                           NULL);
}

static void exit_fn_decl(SymbolTableCtx *ctx, FnDecl *fn_decl) {
    Scope *sub_scope = ast_scope_get(ctx->ast, &fn_decl->body->head);
    scope_insert_enclosing(ctx, fn_decl->name->token.text, &fn_decl->head,
                           sub_scope);
}

static void exit_var_decl(SymbolTableCtx *ctx, VarDecl *var_decl) {
    Binding *binding = var_decl->binding;
    scope_insert_enclosing(ctx, binding->name->token.text, &var_decl->head,
                           NULL);
}

static void enter_if_stmt(SymbolTableCtx *ctx, IfStmt *if_stmt) {
    anon_subscope_start(ctx, &if_stmt->head);
}

static void exit_if_stmt(SymbolTableCtx *ctx, IfStmt *if_stmt) {
    (void)if_stmt;
    stack_pop(&ctx->scope_node_ctx);
}

static void enter_comp_stmt(SymbolTableCtx *ctx, CompStmt *comp_stmt) {
    anon_subscope_start(ctx, &comp_stmt->head);
}

static void exit_comp_stmt(SymbolTableCtx *ctx, CompStmt *comp_stmt) {
    (void)comp_stmt;
    stack_pop(&ctx->scope_node_ctx);
}

static void enter_while_stmt(SymbolTableCtx *ctx, WhileStmt *while_stmt) {
    anon_subscope_start(ctx, &while_stmt->head);
}

static void exit_while_stmt(SymbolTableCtx *ctx, WhileStmt *while_stmt) {
    (void)while_stmt;
    stack_pop(&ctx->scope_node_ctx);
}
