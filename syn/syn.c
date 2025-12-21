#include "syn.h"

// TODO: add delimiter tokens as part of the parsing context

#include <assert.h>
#include <string.h>

#include "../ast/ast.h"
#include "../lex/lex.h"
#include "../mod/mod.h"

typedef struct {
    AstNode *node;
    AstNode *parent;
} NodeCtx;

static void *attr(ParseCtx *c, const char *name, void *node);
static Tok token_attr(ParseCtx *c, const char *name, Tok tok);
static Tok token_attr_anon(ParseCtx *c, Tok tok);
static NodeCtx begin_node(ParseCtx *c, AstNode *node);
static NodeCtx start_node(ParseCtx *c, NodeKind kind);
static void set_current_node(ParseCtx *c, AstNode *node);
static void *end_node(ParseCtx *c, NodeCtx ctx);
static Tok tnext(ParseCtx *c);
static void next(ParseCtx *c);
static Tok consume(ParseCtx *c);
static Tok at(ParseCtx *c);
static bool looking_at(ParseCtx *c, TokKind t);
static bool one_of(TokKind t, Toks toks);
static void advance(ParseCtx *c, Toks toks);
static void expected(ParseCtx *c, AstNode *in, const char *msg);
static bool expect_one_of(ParseCtx *c, AstNode *in, Toks toks,
                          const char *message);
static bool skip_if(ParseCtx *c, AstNode *in, TokKind t);
// static void sync_if_none_of(ParseCtx *c, Toks toks);
static bool expect(ParseCtx *c, AstNode *in, TokKind t);
static ParseState set_marker(ParseCtx *c);
static void backtrack(ParseCtx *c, ParseState marker);

typedef AstNode *(*ParseFn)(ParseCtx *);
static void ensure_progress(ParseCtx *c, ParseFn parse_fn);

ParseCtx parse_ctx_create(Ast *ast, SourceCode *code) {
    return (ParseCtx){
        .lex = new_lexer(code),
        .arena = new_arena(),
        .ast = ast,
        .current = NULL,
        .panic_mode = false,
    };
}

void parse_ctx_delete(ParseCtx *c) { arena_free(&c->arena); }

SourceFile *parse_source_file(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_SOURCE_FILE);
    SourceFile *n = (SourceFile *)nc.node;
    n->imports = parse_imports(c);
    n->decls = parse_decls(c);
    return end_node(c, nc);
}

Imports *parse_imports(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_IMPORTS);
    while (looking_at(c, T_IMPORT)) {
        ensure_progress(c, (ParseFn)parse_import);
    }
    return end_node(c, nc);
}

Import *parse_import(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_IMPORT);
    Import *n = (Import *)nc.node;
    assert(skip_if(c, &n->head, T_IMPORT));
    n->module = parse_scoped_ident(c);
    if (!skip_if(c, &n->head, T_SCLN)) {
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

Decls *parse_decls(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_DECLS);
    while (!looking_at(c, T_EOF)) {
        ensure_progress(c, (ParseFn)parse_decl);
    }
    return end_node(c, nc);
}

Decl *parse_decl(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_DECL);
    Decl *n = (Decl *)nc.node;
again:
    switch (at(c).t) {
        case T_LET:
            n->t = DECL_VAR;
            n->var_decl = parse_var_decl(c);
            break;
        case T_FUN:
            n->t = DECL_FN;
            n->fn_decl = parse_fn_decl(c);
            break;
        case T_STRUCT:
            n->t = DECL_STRUCT;
            n->struct_decl = parse_struct_decl(c);
            break;
        case T_ENUM:
            n->t = DECL_ENUM;
            n->enum_decl = parse_enum_decl(c);
            break;
        case T_UNION:
            n->t = DECL_UNION;
            n->union_decl = parse_union_decl(c);
            break;
        case T_ERROR:
            n->t = DECL_ERR;
            n->err_decl = parse_err_decl(c);
            break;
        default:
            if (expect_one_of(
                    c, &n->head,
                    TOKS(T_LET, T_FUN, T_STRUCT, T_ENUM, T_UNION, T_ERROR),
                    "start of declaration")) {
                goto again;
            }
            break;
    }
    return end_node(c, nc);
}

StructDecl *parse_struct_decl(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_STRUCT_DECL);
    StructDecl *n = (StructDecl *)nc.node;
    assert(consume(c).t == T_STRUCT);
    n->name = parse_ident(c);
    n->body = parse_struct_body(c);
    return end_node(c, nc);
}

StructBody *parse_struct_body(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_STRUCT_BODY);
    StructBody *n = (StructBody *)nc.node;
again:
    switch (at(c).t) {
        case T_LBRC:
            next(c);
            n->tuple_like = false;
            n->fields = parse_fields(c);
            if (!skip_if(c, &n->head, T_RBRC)) {
                return end_node(c, nc);
            }
            break;
        case T_LPAR:
            next(c);
            n->tuple_like = true;
            n->types = parse_types(c);
            if (!skip_if(c, &n->head, T_RPAR)) {
                return end_node(c, nc);
            }
            if (!skip_if(c, &n->head, T_SCLN)) {
                return end_node(c, nc);
            }
            break;
        default:
            if (!expect_one_of(c, &n->head, TOKS(T_LBRC, T_LPAR),
                               "a struct or tuple body")) {
                goto again;
            }
            break;
    }
    return end_node(c, nc);
}

EnumDecl *parse_enum_decl(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_ENUM_DECL);
    EnumDecl *n = (EnumDecl *)nc.node;
    assert(consume(c).t == T_ENUM);
    n->name = parse_ident(c);
    if (!skip_if(c, &n->head, T_LBRC)) {
        return end_node(c, nc);
    }
    n->alts = parse_idents(c);
    if (!skip_if(c, &n->head, T_RBRC)) {
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

UnionDecl *parse_union_decl(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_UNION_DECL);
    UnionDecl *n = (UnionDecl *)nc.node;
    assert(consume(c).t == T_UNION);
    n->name = parse_ident(c);
    if (!skip_if(c, &n->head, T_LBRC)) {
        return end_node(c, nc);
    }
    n->alts = parse_fields(c);
    if (!skip_if(c, &n->head, T_RBRC)) {
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

ErrDecl *parse_err_decl(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_ERR_DECL);
    ErrDecl *n = (ErrDecl *)nc.node;
    assert(consume(c).t == T_ERROR);
    n->name = parse_ident(c);
    if (!skip_if(c, &n->head, T_LBRC)) {
        return end_node(c, nc);
    }
    n->errs = parse_errs(c);
    if (!skip_if(c, &n->head, T_RBRC)) {
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

VarDecl *parse_var_decl(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_VAR_DECL);
    VarDecl *n = (VarDecl *)nc.node;
    assert(consume(c).t == T_LET);
    n->binding = attr(c, "binding", parse_var_binding(c));
    if (!looking_at(c, T_EQ) && !looking_at(c, T_SCLN)) {
        // Must have a type
        n->type = parse_type(c);
    }
    // sync_if_none_of(c, TOKS(T_EQ, T_SCLN));
    if (looking_at(c, T_EQ)) {
        n->init.ok = true;
        n->init.assign_token = consume(c);
        n->init.expr = attr(c, "value", parse_expr(c));
    }
    if (!skip_if(c, &n->head, T_SCLN)) {
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

VarBinding *parse_var_binding(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_VAR_BINDING);
    VarBinding *n = (VarBinding *)nc.node;
again:
    switch (at(c).t) {
        case T_LPAR:
            n->t = VAR_BINDING_UNPACK_TUPLE;
            n->unpack_tuple = parse_unpack_tuple(c);
            break;
        case T_LBRC:
            n->t = VAR_BINDING_UNPACK_STRUCT;
            n->unpack_struct = parse_unpack_struct(c);
            break;
        case T_IDENT: {
            n->t = VAR_BINDING_BASIC;
            n->basic = parse_ident(c);
            break;
        }
        default:
            if (expect_one_of(c, &n->head, TOKS(T_LPAR, T_LBRC, T_IDENT),
                              "a variable binding")) {
                goto again;
            }
            break;
    }
    return end_node(c, nc);
}

Binding *parse_binding(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_BINDING);
    Binding *n = (Binding *)nc.node;
    if (looking_at(c, T_STAR)) {
        n->ref.ok = true;
        n->ref.value = token_attr(c, "kind", consume(c));
        if (looking_at(c, T_RO)) {
            n->ref.ok = true;
            n->mod.value = token_attr(c, "modifier", consume(c));
        }
    }
    n->name = parse_ident(c);
    return end_node(c, nc);
}

AliasBinding *parse_alias_binding(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_ALIAS_BINDING);
    AliasBinding *n = (AliasBinding *)nc.node;
    n->binding = parse_binding(c);
    if (looking_at(c, T_EQ)) {
        next(c);
        if (!expect(c, &n->head, T_IDENT)) {
            return end_node(c, nc);
        }
        Tok name = consume(c);
        n->alias.ok = true;
        n->alias.value = token_attr(c, "alias", name);
    }
    return end_node(c, nc);
}

UnpackStruct *parse_unpack_struct(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_UNPACK_STRUCT);
    UnpackStruct *n = (UnpackStruct *)nc.node;
    if (!skip_if(c, &n->head, T_LBRC)) {
        return end_node(c, nc);
    }
    n->bindings = parse_alias_bindings(c, T_RBRC);
    if (!skip_if(c, &n->head, T_RBRC)) {
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

UnpackTuple *parse_unpack_tuple(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_UNPACK_TUPLE);
    UnpackTuple *n = (UnpackTuple *)nc.node;
    if (!skip_if(c, &n->head, T_LPAR)) {
        return end_node(c, nc);
    }
    n->bindings = parse_bindings(c, T_RPAR);
    if (!skip_if(c, &n->head, T_RPAR)) {
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

UnpackUnion *parse_unpack_union(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_UNPACK_UNION);
    UnpackUnion *n = (UnpackUnion *)nc.node;

    n->tag = parse_ident(c);

    if (!skip_if(c, &n->head, T_LPAR)) {
        return end_node(c, nc);
    }
    n->binding = parse_binding(c);
    if (!skip_if(c, &n->head, T_RPAR)) {
        return end_node(c, nc);
    }

    return end_node(c, nc);
}

AliasBindings *parse_alias_bindings(ParseCtx *c, TokKind end) {
    NodeCtx nc = start_node(c, NODE_ALIAS_BINDINGS);
    (void)parse_alias_binding(c);
    while (looking_at(c, T_COMMA)) {
        next(c);
        // Ignore trailing comma
        if (looking_at(c, end)) {
            break;
        }
        (void)parse_alias_binding(c);
    }
    return end_node(c, nc);
}

Bindings *parse_bindings(ParseCtx *c, TokKind end) {
    NodeCtx nc = start_node(c, NODE_BINDINGS);
    (void)parse_binding(c);
    while (looking_at(c, T_COMMA)) {
        next(c);
        // Ignore trailing comma
        if (looking_at(c, end)) {
            break;
        }
        (void)parse_binding(c);
    }
    return end_node(c, nc);
}

FnDecl *parse_fn_decl(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_FN_DECL);
    FnDecl *n = (FnDecl *)nc.node;
    assert(consume(c).t == T_FUN);
    // TODO type parameters
    n->name = parse_ident(c);
    if (!skip_if(c, &n->head, T_LPAR)) {
        return end_node(c, nc);
    }
    n->params = parse_fn_params(c);
    if (!skip_if(c, &n->head, T_RPAR)) {
        return end_node(c, nc);
    }
    n->mods = parse_mods(c);
    if (looking_at(c, T_ARROW)) {
        next(c);
        n->return_type.ptr = parse_type(c);
    }
    n->body = parse_comp_stmt(c);
    return end_node(c, nc);
}

static bool is_mod(TokKind t) {
    switch (t) {
        case T_EXTERN:
            return true;
        default:
            return false;
    }
}

FnMods *parse_mods(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_FN_MODS);
    while (is_mod(at(c).t)) {
        NodeCtx mod_ctx = start_node(c, NODE_FN_MOD);
        FnMod *mod = (FnMod *)mod_ctx.node;
        mod->mod = token_attr_anon(c, consume(c));
        (void)end_node(c, mod_ctx);
    }
    return end_node(c, nc);
}

FnParams *parse_fn_params(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_FN_PARAMS);
    if (looking_at(c, T_RPAR)) {
        return end_node(c, nc);
    }
    (void)parse_fn_param(c);
    while (looking_at(c, T_COMMA)) {
        next(c);
        // Ignore trailing comma
        if (looking_at(c, T_RPAR)) {
            break;
        }
        (void)parse_fn_param(c);
    }
    return end_node(c, nc);
}

FnParam *parse_fn_param(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_FN_PARAM);
    FnParam *n = (FnParam *)nc.node;
    n->binding = parse_var_binding(c);
    if (looking_at(c, T_DOTDOT)) {
        token_attr(c, "kind", at(c));
        next(c);
        n->variadic = true;
    }
    n->type = parse_type(c);
    return end_node(c, nc);
}

Stmt *parse_stmt(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_STMT);
    Stmt *n = (Stmt *)nc.node;
    switch (at(c).t) {
        case T_FUN:
        case T_LET:
        case T_STRUCT:
        case T_ENUM:
        case T_ERROR:
        case T_UNION:
            n->t = STMT_DECL;
            n->decl = parse_decl(c);
            break;
        case T_LBRC:
            n->t = STMT_COMP;
            n->comp_stmt = parse_comp_stmt(c);
            break;
        case T_IF:
            n->t = STMT_IF;
            n->if_stmt = parse_if_stmt(c);
            break;
        case T_RETURN:
            n->t = STMT_RETURN;
            n->return_stmt = parse_return_stmt(c);
            break;
        case T_WHILE:
            n->t = STMT_WHILE;
            n->while_stmt = parse_while_stmt(c);
            break;
        case T_CASE:
            n->t = STMT_CASE;
            n->case_stmt = parse_case_stmt(c);
            break;
        default:
            n->t = STMT_ASSIGN_OR_EXPR;
            n->assign_or_expr = parse_assign_or_expr(c);
            if (!skip_if(c, &n->head, T_SCLN)) {
                return end_node(c, nc);
            }
            break;
    }
    return end_node(c, nc);
}

CompStmt *parse_comp_stmt(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_COMP_STMT);
    CompStmt *n = (CompStmt *)nc.node;
    if (!skip_if(c, &n->head, T_LBRC)) {
        return end_node(c, nc);
    }
    while (!looking_at(c, T_RBRC)) {
        if (looking_at(c, T_EOF)) {
            expected(c, &n->head, "a statement");
            break;
        }
        (void)parse_stmt(c);
    }
    next(c);
    return end_node(c, nc);
}

IfStmt *parse_if_stmt(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_IF_STMT);
    IfStmt *n = (IfStmt *)nc.node;
    assert(consume(c).t == T_IF);
    n->cond = parse_cond(c);
    n->true_branch = parse_comp_stmt(c);
    if (looking_at(c, T_ELSE)) {
        n->else_branch.ptr = parse_else(c);
    }
    return end_node(c, nc);
}

WhileStmt *parse_while_stmt(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_WHILE_STMT);
    WhileStmt *n = (WhileStmt *)nc.node;
    assert(consume(c).t == T_WHILE);
    n->cond = parse_cond(c);
    n->true_branch = parse_comp_stmt(c);
    return end_node(c, nc);
}

CasePatt *parse_case_patt(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_CASE_PATT);
    CasePatt *n = (CasePatt *)nc.node;
    switch (at(c).t) {
        case T_LET:
            next(c);
            n->t = CASE_PATT_UNPACK_UNION;
            n->unpack_union = parse_unpack_union(c);
            break;
        case T_ELSE:
            n->t = CASE_PATT_DEFAULT;
            n->default_ = token_attr_anon(c, consume(c));
            break;
        default:
            n->t = CASE_PATT_EXPR;
            n->expr = parse_expr(c);
            break;
    }
    return end_node(c, nc);
}

CaseBranch *parse_case_branch(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_CASE_BRANCH);
    CaseBranch *n = (CaseBranch *)nc.node;
    n->patt = parse_case_patt(c);
    if (!skip_if(c, &n->head, T_ARROW)) {
        return end_node(c, nc);
    }
    n->action = parse_stmt(c);
    return end_node(c, nc);
}

CaseBranches *parse_case_branches(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_CASE_BRANCHES);
    CaseBranches *n = (CaseBranches *)nc.node;
    if (!skip_if(c, &n->head, T_LBRC)) {
        return end_node(c, nc);
    }
    while (!looking_at(c, T_RBRC)) {
        if (looking_at(c, T_EOF)) {
            expected(c, &n->head, "a case branch");
            break;
        }
        (void)parse_case_branch(c);
    }
    next(c);
    return end_node(c, nc);
}

CaseStmt *parse_case_stmt(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_CASE_STMT);
    CaseStmt *n = (CaseStmt *)nc.node;
    assert(consume(c).t == T_CASE);
    n->expr = parse_expr(c);
    n->branches = parse_case_branches(c);
    return end_node(c, nc);
}

ReturnStmt *parse_return_stmt(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_RETURN_STMT);
    ReturnStmt *n = (ReturnStmt *)nc.node;
    assert(consume(c).t == T_RETURN);
    if (looking_at(c, T_SCLN)) {
        return end_node(c, nc);
    }
    n->expr.ptr = parse_expr(c);
    if (!skip_if(c, &n->head, T_SCLN)) {
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

Else *parse_else(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_ELSE);
    Else *n = (Else *)nc.node;
    assert(consume(c).t == T_ELSE);
    if (at(c).t == T_IF) {
        n->t = ELSE_IF;
        n->if_stmt = parse_if_stmt(c);
    } else {
        n->t = ELSE_COMP;
        n->comp_stmt = parse_comp_stmt(c);
    }
    return end_node(c, nc);
}

AssignOrExpr *parse_assign_or_expr(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_ASSIGN_OR_EXPR);
    AssignOrExpr *n = (AssignOrExpr *)nc.node;
    n->lvalue = parse_expr(c);
    if (looking_at(c, T_EQ)) {
        n->assign_token = consume(c);
        n->rvalue.ptr = parse_expr(c);
    }
    return end_node(c, nc);
}

Cond *parse_cond(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_COND);
    Cond *n = (Cond *)nc.node;
    switch (at(c).t) {
        case T_LET:
            n->t = COND_UNION_TAG;
            n->union_tag = parse_union_tag_cond(c);
            break;
        default:
            n->t = COND_EXPR;
            n->expr = parse_expr(c);
            break;
    }
    return end_node(c, nc);
}

UnionTagCond *parse_union_tag_cond(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_UNION_TAG_COND);
    UnionTagCond *n = (UnionTagCond *)nc.node;

    assert(consume(c).t == T_LET);

    n->trigger = parse_unpack_union(c);

    if (!expect(c, &n->head, T_EQ)) {
        return end_node(c, nc);
    }

    n->assign_token = consume(c);
    n->expr = parse_expr(c);

    return end_node(c, nc);
}

Type *parse_type(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_TYPE);
    Type *n = (Type *)nc.node;
again:
    switch (at(c).t) {
        case T_S8:
        case T_U8:
        case T_S16:
        case T_U16:
        case T_S32:
        case T_U32:
        case T_S64:
        case T_U64:
        case T_F32:
        case T_F64:
        case T_BOOL:
        case T_STRING:
        case T_UNIT:
        case T_ANY: {
            {
                NodeCtx builtin_ctx = start_node(c, NODE_BUILTIN_TYPE);
                BuiltinType *builtin_type = (BuiltinType *)builtin_ctx.node;
                builtin_type->token = token_attr_anon(c, consume(c));
                n->t = TYPE_BUILTIN;
                n->builtin_type = end_node(c, builtin_ctx);
            }
            break;
        }
        case T_LBRK:
            n->t = TYPE_COLL;
            n->coll_type = parse_coll_type(c);
            break;
        case T_STRUCT:
            n->t = TYPE_STRUCT;
            n->struct_type = parse_struct_type(c);
            break;
        case T_UNION:
            n->t = TYPE_UNION;
            n->union_type = parse_union_type(c);
            break;
        case T_ENUM:
            n->t = TYPE_ENUM;
            n->enum_type = parse_enum_type(c);
            break;
        case T_ERROR:
            n->t = TYPE_ERR;
            n->err_type = parse_err_type(c);
            break;
        case T_STAR:
            n->t = TYPE_PTR;
            n->ptr_type = parse_ptr_type(c);
            break;
        case T_FUN:
            n->t = TYPE_FN;
            n->fn_type = parse_fn_type(c);
            break;
        case T_SCOPE:
        case T_IDENT:
            n->t = TYPE_SCOPED_IDENT;
            n->scoped_ident = parse_scoped_ident(c);
            break;
        default:
            if (expect_one_of(
                    c, &n->head,
                    TOKS(T_S8, T_U8, T_S16, T_U16, T_S32, T_U32, T_S64, T_U64,
                         T_F32, T_F64, T_BOOL, T_STRING, T_UNIT, T_ANY, T_LBRK,
                         T_STRUCT, T_UNION, T_ENUM, T_ERROR, T_STAR, T_FUN,
                         T_SCOPE, T_IDENT),
                    "a type")) {
                goto again;
            }
            break;
    }
    return end_node(c, nc);
}

CollType *parse_coll_type(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_COLL_TYPE);
    CollType *n = (CollType *)nc.node;
    assert(consume(c).t == T_LBRK);
    // For empty index, e.g.: []u32
    if (looking_at(c, T_RBRK)) {
        next(c);
        n->element_type = parse_type(c);
        return end_node(c, nc);
    }
    n->index_expr.ptr = parse_expr(c);
    if (!skip_if(c, &n->head, T_RBRK)) {
        return end_node(c, nc);
    }
    n->element_type = parse_type(c);
    return end_node(c, nc);
}

StructType *parse_struct_type(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_STRUCT_TYPE);
    StructType *n = (StructType *)nc.node;
    assert(consume(c).t == T_STRUCT);
    n->body = parse_struct_body(c);
    return end_node(c, nc);
}

Fields *parse_fields(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_FIELDS);
    if (looking_at(c, T_RBRC)) {
        return end_node(c, nc);
    }
    (void)parse_field(c);
    while (looking_at(c, T_COMMA)) {
        next(c);
        // Ignore trailing comma
        if (looking_at(c, T_RBRC)) {
            break;
        }
        (void)parse_field(c);
    }
    return end_node(c, nc);
}

Field *parse_field(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_FIELD);
    Field *n = (Field *)nc.node;
    n->name = parse_ident(c);
    n->type = parse_type(c);
    return end_node(c, nc);
}

Types *parse_types(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_TYPES);
    if (looking_at(c, T_RPAR)) {
        return end_node(c, nc);
    }
    (void)parse_type(c);
    while (looking_at(c, T_COMMA)) {
        next(c);
        // Ignore trailing comma
        if (looking_at(c, T_RPAR)) {
            break;
        }
        (void)parse_type(c);
    }
    return end_node(c, nc);
}

UnionType *parse_union_type(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_UNION_TYPE);
    UnionType *n = (UnionType *)nc.node;
    assert(consume(c).t == T_UNION);
    if (!skip_if(c, &n->head, T_LBRC)) {
        return end_node(c, nc);
    }
    n->fields = parse_fields(c);
    if (!skip_if(c, &n->head, T_RBRC)) {
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

EnumType *parse_enum_type(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_ENUM_TYPE);
    EnumType *n = (EnumType *)nc.node;
    assert(consume(c).t == T_ENUM);
    if (!skip_if(c, &n->head, T_LBRC)) {
        return end_node(c, nc);
    }
    n->alts = parse_idents(c);
    if (!skip_if(c, &n->head, T_RBRC)) {
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

ErrType *parse_err_type(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_ERR_TYPE);
    ErrType *n = (ErrType *)nc.node;
    assert(consume(c).t == T_ERROR);
    if (!skip_if(c, &n->head, T_LBRC)) {
        return end_node(c, nc);
    }
    n->errs = parse_errs(c);
    if (!skip_if(c, &n->head, T_RBRC)) {
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

Idents *parse_idents(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_IDENTS);
    Idents *n = (Idents *)nc.node;
    bool start = true;
    while (!looking_at(c, T_RBRC)) {
        if (!start && !skip_if(c, &n->head, T_COMMA)) {
            return end_node(c, nc);
        }
        // Ignore trailing comma
        if (looking_at(c, T_RBRC)) {
            break;
        }
        (void)parse_ident(c);
        start = false;
    }
    return end_node(c, nc);
}

Errs *parse_errs(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_ERRS);
    Errs *n = (Errs *)nc.node;
    bool start = true;
    while (!looking_at(c, T_RBRC)) {
        if (!start && !skip_if(c, &n->head, T_COMMA)) {
            return end_node(c, nc);
        }
        // Ignore trailing comma
        if (looking_at(c, T_RBRC)) {
            break;
        }
        (void)parse_err(c);
        start = false;
    }
    return end_node(c, nc);
}

Err *parse_err(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_ERR);
    Err *n = (Err *)nc.node;
    if (looking_at(c, T_BANG)) {
        n->embedded = true;
        token_attr(c, "kind", consume(c));
    }
    n->scoped_ident = parse_scoped_ident(c);
    return end_node(c, nc);
}

PtrType *parse_ptr_type(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_PTR_TYPE);
    PtrType *n = (PtrType *)nc.node;
    assert(consume(c).t == T_STAR);
    if (looking_at(c, T_RO)) {
        n->ro.ok = true;
        n->ro.value = token_attr(c, "modifier", consume(c));
    }
    n->ref_type = parse_type(c);
    return end_node(c, nc);
}

FnType *parse_fn_type(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_FN_TYPE);
    FnType *n = (FnType *)nc.node;
    assert(consume(c).t == T_FUN);
    if (!skip_if(c, &n->head, T_LPAR)) {
        return end_node(c, nc);
    }
    n->params = parse_types(c);
    if (!skip_if(c, &n->head, T_RPAR)) {
        return end_node(c, nc);
    }
    if (looking_at(c, T_ARROW)) {
        next(c);
        n->return_type.ptr = parse_type(c);
    }
    return end_node(c, nc);
}

Ident *parse_ident(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_IDENT);
    Ident *n = (Ident *)nc.node;

    if (!expect(c, &n->head, T_IDENT)) {
        return end_node(c, nc);
    }

    n->token = token_attr_anon(c, consume(c));
    return end_node(c, nc);
}

// Some rules that are used in many contexts take a follow set from the
// caller. This improves the error recovery as it has more context of what
// it should be syncing on than just "whatever can follow is really common rule"
ScopedIdent *parse_scoped_ident(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_SCOPED_IDENT);
    if (looking_at(c, T_SCOPE)) {
        Tok empty_str = {
            .t = T_EMPTY_STRING,
            .text = ZTOS(""),
            .offset = at(c).offset,
        };
        NodeCtx ident_ctx = start_node(c, NODE_IDENT);
        Ident *ident = (Ident *)ident_ctx.node;
        ident->token = empty_str;
        (void)end_node(c, ident_ctx);
        next(c);  // skip the '::'
    }
    // TODO: add '::' to follow
    (void)parse_ident(c);
    while (looking_at(c, T_SCOPE)) {
        next(c);
        (void)parse_ident(c);
    }
    return end_node(c, nc);
}

CallArgs *parse_call_args(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_CALL_ARGS);
    CallArgs *n = (CallArgs *)nc.node;

    if (!skip_if(c, &n->head, T_LPAR)) {
        return end_node(c, nc);
    }

    // No arguments
    if (looking_at(c, T_RPAR)) {
        next(c);
        return end_node(c, nc);
    }

    (void)parse_call_arg(c);
    while (looking_at(c, T_COMMA)) {
        next(c);
        // Ignore trailing comma
        if (looking_at(c, T_RPAR)) {
            break;
        }
        (void)parse_call_arg(c);
    }

    if (!skip_if(c, &n->head, T_RPAR)) {
        return end_node(c, nc);
    }

    return end_node(c, nc);
}

CallArg *parse_call_arg(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_CALL_ARG);
    CallArg *n = (CallArg *)nc.node;
    ParseState start = set_marker(c);
    if (looking_at(c, T_IDENT)) {
        next(c);
        if (looking_at(c, T_EQ)) {
            backtrack(c, start);
            n->name.ptr = parse_ident(c);
            n->assign_token = consume(c);
        } else {
            backtrack(c, start);
        }
    }
    n->value = parse_expr(c);
    return end_node(c, nc);
}

// Helpful resource for pratt parser:
// https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html#Pratt-parsing-the-general-shape

static bool is_prefix_op(TokKind t) {
    switch (t) {
        case T_AMP:
        case T_INC:
        case T_DEC:
        case T_STAR:
        case T_MINUS:
            return true;
        default:
            return false;
    }
}

static bool is_infix_op(TokKind t) {
    switch (t) {
        case T_PLUS:
        case T_MINUS:
        case T_PERC:
        case T_STAR:
        case T_SLASH:
        case T_AND:
        case T_OR:
        case T_AMP:
        case T_PIPE:
        case T_NEQ:
        case T_EQEQ:
        case T_LT:
        case T_GT:
        case T_LTEQ:
        case T_GTEQ:
            return true;
        default:
            return false;
    }
}

static bool is_postfix_op(TokKind t) {
    switch (t) {
        case T_DOT:
        case T_LPAR:
        case T_LBRK:
        case T_INC:
        case T_DEC:
        case T_BANG:
        case T_QUEST:
            return true;
        default:
            return false;
    }
}

static bool is_op(TokKind t) {
    return is_prefix_op(t) || is_infix_op(t) || is_postfix_op(t);
}

typedef struct {
    u32 left;
    u32 right;
} Power;

typedef enum {
    PREC_POSTFIX,
    PREC_UNARY,
    PREC_MULTIPLICATIVE,
    PREC_ADDITIVE,
    PREC_SHIFT,
    PREC_RELATIONAL,
    PREC_EQUALITY,
    PREC_BAND,
    PREC_BXOR,
    PREC_BOR,
    PREC_AND,
    PREC_OR,
    // PREC_ASSIGNMENT,
    PREC_COUNT,
} Precendence_Group;

static const Power binding_power_of[PREC_COUNT] = {
    [PREC_POSTFIX] = {23, 24},
    [PREC_UNARY] = {22, 21},
    [PREC_MULTIPLICATIVE] = {19, 20},
    [PREC_ADDITIVE] = {17, 18},
    [PREC_SHIFT] = {15, 16},
    [PREC_RELATIONAL] = {13, 14},
    [PREC_EQUALITY] = {11, 12},
    [PREC_BAND] = {9, 10},
    [PREC_BOR] = {7, 8},
    [PREC_AND] = {5, 6},
    [PREC_OR] = {3, 4},
    // [PREC_ASSIGNMENT] = {2, 1},
};

static Index *parse_index(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_INDEX);
    Index *n = (Index *)nc.node;
    assert(consume(c).t == T_LBRK);

    n->t = INDEX_SINGLE;

    // e.g. [:10]
    if (looking_at(c, T_CLN)) {
        n->t = INDEX_RANGE;
        token_attr_anon(c, consume(c));
        if (!looking_at(c, T_RBRK)) {
            // Although it is not technically correct to have
            // ';' as a "valid" delimiter, it is used here to prevent
            // spurious errors caused by unmatched '['
            n->end.ptr = parse_expr(c);
        }
        goto delim;
    }

    n->start.ptr = parse_expr(c);

    if (looking_at(c, T_CLN)) {
        n->t = INDEX_RANGE;
        token_attr_anon(c, consume(c));
        if (!looking_at(c, T_RBRK)) {
            n->end.ptr = parse_expr(c);
        }
        goto delim;
    }

delim:
    (void)skip_if(c, &n->head, T_RBRK);
    return end_node(c, nc);
}

static NodeCtx parse_postfix(ParseCtx *c, Expr *lhs) {
    NodeCtx expr_ctx = start_node(c, NODE_EXPR);
    Expr *expr = (Expr *)expr_ctx.node;

    switch (at(c).t) {
        case T_LBRK: {
            NodeCtx access_ctx = start_node(c, NODE_COLL_ACCESS);
            CollAccess *access = (CollAccess *)access_ctx.node;

            NodeCtx lhs_ctx = begin_node(c, &lhs->head);
            access->lvalue = end_node(c, lhs_ctx);
            access->index = parse_index(c);

            expr->t = EXPR_COLL_ACCESS;
            expr->coll_access = end_node(c, access_ctx);
            break;
        }
        case T_LPAR: {
            NodeCtx call_ctx = start_node(c, NODE_CALL);
            Call *call = (Call *)call_ctx.node;

            NodeCtx lhs_ctx = begin_node(c, &lhs->head);
            call->callable = end_node(c, lhs_ctx);
            call->args = parse_call_args(c);

            expr->t = EXPR_CALL;
            expr->call = end_node(c, call_ctx);
            break;
        }
        case T_DOT: {
            NodeCtx access_ctx = start_node(c, NODE_FIELD_ACCESS);
            FieldAccess *access = (FieldAccess *)access_ctx.node;

            NodeCtx lhs_ctx = begin_node(c, &lhs->head);
            access->lvalue = attr(c, "lvalue", end_node(c, lhs_ctx));

            next(c);
            access->field = parse_ident(c);

            expr->t = EXPR_FIELD_ACCESS;
            expr->field_access = end_node(c, access_ctx);
            break;
        }
        default: {
            NodeCtx postfix_ctx = start_node(c, NODE_POSTFIX_EXPR);
            PostfixExpr *postfix = (PostfixExpr *)postfix_ctx.node;

            postfix->op = token_attr(c, "op", consume(c));
            NodeCtx lhs_ctx = begin_node(c, &lhs->head);
            postfix->sub_expr = end_node(c, lhs_ctx);

            expr->t = EXPR_POSTFIX;
            expr->postfix_expr = end_node(c, postfix_ctx);
        }
    }

    return expr_ctx;
}

typedef struct {
    Power pow;
    bool ok;
} Maybe_Power;

static Maybe_Power infix_bpow(TokKind op) {
    switch (op) {
        case T_PLUS:
        case T_MINUS:
            return (Maybe_Power){binding_power_of[PREC_ADDITIVE], true};
        case T_PERC:
        case T_STAR:
        case T_SLASH:
            return (Maybe_Power){binding_power_of[PREC_MULTIPLICATIVE], true};
        case T_AND:
            return (Maybe_Power){binding_power_of[PREC_AND], true};
        case T_OR:
            return (Maybe_Power){binding_power_of[PREC_OR], true};
        case T_AMP:
            return (Maybe_Power){binding_power_of[PREC_BAND], true};
        case T_PIPE:
            return (Maybe_Power){binding_power_of[PREC_BOR], true};
        case T_NEQ:
        case T_EQEQ:
            return (Maybe_Power){binding_power_of[PREC_EQUALITY], true};
        case T_LT:
        case T_GT:
        case T_LTEQ:
        case T_GTEQ:
            return (Maybe_Power){binding_power_of[PREC_RELATIONAL], true};
        default:
            return (Maybe_Power){.ok = false};
    }
}

static Maybe_Power prefix_bpow(TokKind op) {
    switch (op) {
        case T_AMP:
        case T_INC:
        case T_DEC:
        case T_STAR:
        case T_MINUS:
            return (Maybe_Power){binding_power_of[PREC_UNARY], true};
        default:
            return (Maybe_Power){.ok = false};
    }
}

static Maybe_Power postfix_bpow(TokKind op) {
    switch (op) {
        case T_DOT:
        case T_LPAR:
        case T_LBRK:
        case T_INC:
        case T_DEC:
        case T_BANG:
        case T_QUEST:
            return (Maybe_Power){binding_power_of[PREC_POSTFIX], true};
        default:
            return (Maybe_Power){.ok = false};
            assert(false && "TODO");
    }
}

static NodeCtx expr_with_bpow(ParseCtx *c, u32 min_pow) {
    NodeCtx lhs_ctx;
    Expr *lhs = BAD_PTR;

    Tok tok = at(c);

    if (tok.t == T_LPAR) {
        next(c);
        lhs_ctx = expr_with_bpow(c, 0);
        lhs = REIFY_AS(lhs_ctx.node, Expr);

        if (!skip_if(c, &lhs->head, T_RPAR)) {
            return lhs_ctx;
        }
        if (!is_op(at(c).t)) {
            return lhs_ctx;
        }
    } else if (is_prefix_op(tok.t)) {
        lhs_ctx = start_node(c, NODE_EXPR);
        lhs = REIFY_AS(lhs_ctx.node, Expr);

        NodeCtx unary_ctx = start_node(c, NODE_UNARY_EXPR);
        UnaryExpr *unary_expr = (UnaryExpr *)unary_ctx.node;

        Power pow;
        Maybe_Power maybe_pow = prefix_bpow(tok.t);
        if (!maybe_pow.ok) {
            expected(c, &unary_expr->head, "valid prefix operator");
            pow = (Power){0, 0};
        } else {
            pow = maybe_pow.pow;
        }

        unary_expr->op = token_attr(c, "op", consume(c));
        unary_expr->sub_expr = end_node(c, expr_with_bpow(c, pow.right));

        lhs->t = EXPR_UNARY;
        lhs->unary_expr = end_node(c, unary_ctx);
    } else {
        lhs_ctx = start_node(c, NODE_EXPR);
        lhs = REIFY_AS(lhs_ctx.node, Expr);

        lhs->t = EXPR_ATOM;
        lhs->atom = parse_atom(c);
    }

    while (true) {
        Tok op = at(c);
        if (!is_infix_op(op.t) && !is_postfix_op(op.t)) {
            break;
        }

        Maybe_Power maybe_pow = postfix_bpow(op.t);
        if (maybe_pow.ok) {
            Power pow = maybe_pow.pow;
            if (pow.left < min_pow) {
                break;
            }
            // Restore to the old parent, as the sub expression will now be its
            // child and lhs will be a child of the sub expression.
            set_current_node(c, lhs_ctx.parent);
            lhs_ctx = parse_postfix(c, lhs);
            lhs = REIFY_AS(lhs_ctx.node, Expr);
            continue;
        }

        maybe_pow = infix_bpow(op.t);
        if (!maybe_pow.ok) {
            break;
        }
        Power pow = maybe_pow.pow;
        if (pow.left < min_pow) {
            break;
        }

        // Really sorry if someone is coming here to understand this code.
        // There is a bit of gymnastics here to make sure the implicit
        // child-parent relationships maintained by the NodeCtx system are
        // correct.
        //
        // The below code needs to make sure that the current lhs is not added
        // as a child of the current parent (stored in lhs_ctx.parent). It
        // should instead be added as a child of a new node which should become
        // the new descendant of current parent.
        set_current_node(c, lhs_ctx.parent);

        NodeCtx new_expr_ctx = start_node(c, NODE_EXPR);
        Expr *new_expr = (Expr *)new_expr_ctx.node;

        NodeCtx bin_ctx = start_node(c, NODE_BIN_EXPR);
        BinExpr *bin_expr = (BinExpr *)bin_ctx.node;

        bin_expr->op = token_attr(c, "op", op);
        NodeCtx old_lhs_ctx = begin_node(c, &lhs->head);
        bin_expr->left = attr(c, "left", end_node(c, old_lhs_ctx));
        next(c);
        bin_expr->right =
            attr(c, "right", end_node(c, expr_with_bpow(c, pow.right)));

        new_expr->t = EXPR_BIN;
        new_expr->bin_expr = end_node(c, bin_ctx);

        lhs = new_expr;
        lhs_ctx = new_expr_ctx;
    }

    return lhs_ctx;
}

Expr *parse_expr(ParseCtx *c) {
    // HACK to allow calling parse_expr as a root node,
    // we force the root constraint (node_ctx.parent == parse_ctx.current), if
    // the parent before calling `expr_with_bpow` was root (id=0).
    //
    // This is needed as expressions are the exception in that node id 0 can
    // actually become a child of higher id node.
    AstNode *parent = c->current;
    NodeCtx ctx = expr_with_bpow(c, 0);
    if (parent == c->ast->root) {
        ctx.parent = c->current;
    }
    return end_node(c, ctx);
}

static Tok get_atom_token(ParseCtx *c) {
    const char *attr_ = NULL;
    switch (at(c).t) {
        case T_IDENT:
            attr_ = "ident";
            break;
        case T_CHAR:
            attr_ = "char";
            break;
#define TOKEN(...)
#define KEYWORD(NAME, ...) \
    case T_##NAME:         \
        attr_ = "keyword"; \
        break;
            EACH_TOKEN
#undef TOKEN
#undef KEYWORD
        default:
            break;
    }
    if (attr_) {
        return token_attr(c, attr_, consume(c));
    }
    return token_attr_anon(c, consume(c));
}

Atom *parse_atom(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_ATOM);
    Atom *n = (Atom *)nc.node;

again:
    switch (at(c).t) {
        case T_SCOPE:
        case T_IDENT: {
            n->t = ATOM_SCOPED_IDENT;
            n->scoped_ident = parse_scoped_ident(c);
            return end_node(c, nc);
        }
        case T_NUM:
        case T_CHAR:
        case T_STR:
        case T_TRUE:
        case T_FALSE:
            n->t = ATOM_TOKEN;
            n->token = get_atom_token(c);
            return end_node(c, nc);
        case T_U8:
        case T_S8:
        case T_U16:
        case T_S16:
        case T_U32:
        case T_S32:
        case T_U64:
        case T_S64:
        case T_F32:
        case T_F64:
        case T_BOOL:
        case T_UNIT:
        case T_STRING:
            n->t = ATOM_BUILTIN_TYPE;
            n->builtin_type = get_atom_token(c);
            return end_node(c, nc);
        default:
            break;
    }

    if (expect_one_of(
            c, &n->head,
            TOKS(T_SCOPE, T_IDENT, T_NUM, T_CHAR, T_STR, T_TRUE, T_FALSE, T_U8,
                 T_S8, T_U16, T_S16, T_U32, T_S32, T_U64, T_S64, T_F32, T_F64,
                 T_BOOL, T_UNIT, T_STRING),
            "an atom")) {
        goto again;
    }

    return end_node(c, nc);
}

static void *attr(ParseCtx *c, const char *name, void *node) {
    assert(da_length(c->current->children) != 0);
    Child *ch =
        children_at(c->current->children, da_length(c->current->children) - 1);
    ch->name.ptr = name;
    return node;
}

static Tok token_attr(ParseCtx *c, const char *name, Tok tok) {
    ast_node_child_add(c->current, child_token_named_create(name, tok));
    return tok;
}

static Tok token_attr_anon(ParseCtx *c, Tok tok) {
    ast_node_child_add(c->current, child_token_create(tok));
    return tok;
}

static NodeCtx begin_node(ParseCtx *c, AstNode *node) {
    node->offset = c->lex.cursor;
    NodeCtx ctx = {
        .node = node,
        .parent = c->current,
    };
    c->current = ctx.node;
    return ctx;
}

static NodeCtx start_node(ParseCtx *c, NodeKind kind) {
    AstNode *nn = ast_node_create(c->ast, kind);
    // Set root node as the first started node
    if (c->ast->root == NULL) {
        c->ast->root = nn;
    }
    return begin_node(c, nn);
}

static void set_current_node(ParseCtx *c, AstNode *node) { c->current = node; }

static void *end_node(ParseCtx *c, NodeCtx ctx) {
    assert(ctx.node == c->current);
    bool root = ctx.parent == c->current;
    if (!root) {
        // Make sure we can't accidentally add a cycle
        assert(ctx.parent != c->current);
        if (ctx.parent != NULL) {
            ast_node_child_add(ctx.parent, child_node_create(c->current));
            c->current->parent.ptr = ctx.parent;
            set_current_node(c, ctx.parent);
        }
    }
    return ctx.node;
}

static ParseState set_marker(ParseCtx *c) { return (ParseState){c->lex}; }
static void backtrack(ParseCtx *c, ParseState marker) { c->lex = marker.lex; }

static Tok tnext(ParseCtx *c) {
    Tok tok;
    do {
        lex_consume(&c->lex);
        tok = lex_peek(&c->lex);
    } while (tok.t == T_CMNT);
    return tok;
}

static void next(ParseCtx *c) { (void)tnext(c); }

static Tok consume(ParseCtx *c) {
    Tok tok = lex_peek(&c->lex);
    next(c);
    return tok;
}

static Tok at(ParseCtx *c) { return lex_peek(&c->lex); }

static bool looking_at(ParseCtx *c, TokKind t) { return at(c).t == t; }

static bool one_of(TokKind t, Toks toks) {
    for (u32 i = 0; i < toks.len; i++) {
        if (t == toks.items[i]) {
            return true;
        }
    }
    return false;
}

static void advance(ParseCtx *c, Toks toks) {
    for (Tok tok = lex_peek(&c->lex); tok.t != T_EOF; tok = tnext(c)) {
        if (one_of(tok.t, toks)) {
            return;
        }
    }
}

static void advance1(ParseCtx *c, TokKind t) {
    for (Tok tok = lex_peek(&c->lex); tok.t != T_EOF; tok = tnext(c)) {
        if (tok.t == t) {
            return;
        }
    }
}

static void expected(ParseCtx *c, AstNode *in, const char *msg) {
    Tok tok = lex_peek(&c->lex);
    raise_syntax_error(c->lex.source, (SyntaxError){
                                          .at = c->lex.cursor,
                                          .expected = msg,
                                          .got = tok_to_string[tok.t].data,
                                      });
    in->has_error = true;
}

static bool expect(ParseCtx *c, AstNode *in, TokKind t) {
    Tok tok = lex_peek(&c->lex);
    bool match = tok.t == t;
    if (!match) {
        // If we are in a panic state already, don't report an error.
        // Try to synchronise on the currently expected token:
        // * if we can't - just backtrack and stay in a panic state.
        // * if we can - leave panic state and return true.
        if (c->panic_mode) {
            ParseState m = set_marker(c);
            advance1(c, t);
            if (looking_at(c, t)) {
                c->panic_mode = false;
                return true;
            }
            backtrack(c, m);
            return false;
        }
        c->panic_mode = true;
        expected(c, in, tok_to_string[t].data);
    } else {
        c->panic_mode = false;
    }
    return match;
}

static bool skip_if(ParseCtx *c, AstNode *in, TokKind t) {
    if (expect(c, in, t)) {
        next(c);
        return true;
    }
    return false;
}

// static void sync_if_none_of(ParseCtx *c, Toks toks) {
//     ParseState m = set_marker(c);
//     advance(c, toks);
//     if (!one_of(at(c).t, toks)) {
//         backtrack(c, m);
//     }
// }

static bool expect_one_of(ParseCtx *c, AstNode *in, Toks toks,
                          const char *message) {
    Tok tok = lex_peek(&c->lex);
    bool match = one_of(tok.t, toks);
    if (!match) {
        // If we are in a panic state already, don't report an error.
        // Try to synchronise on the currently expected token:
        // * if we can't - just backtrack and stay in a panic state.
        // * if we can - leave panic state and return true.
        if (c->panic_mode) {
            ParseState m = set_marker(c);
            advance(c, toks);
            if (one_of(at(c).t, toks)) {
                c->panic_mode = false;
                return true;
            }
            in->has_error = true;
            backtrack(c, m);
            return false;
        }
        c->panic_mode = true;
        expected(c, in, message);
    } else {
        c->panic_mode = false;
    }
    return match;
}

static void ensure_progress(ParseCtx *c, ParseFn parse_fn) {
    u32 cursor = c->lex.cursor;
    (void)parse_fn(c);
    bool no_progress_made = cursor == c->lex.cursor;
    if (no_progress_made) {
        next(c);
    }
}
