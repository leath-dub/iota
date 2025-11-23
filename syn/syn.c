#include "syn.h"

// TODO: add delimiter tokens as part of the parsing context

#include <assert.h>
#include <string.h>

#include "../ast/ast.h"
#include "../lex/lex.h"
#include "../mod/mod.h"

typedef struct {
    AnyNode node;
    AnyNode parent;
} NodeCtx;

static void *make_node(ParseCtx *c, NodeKind kind);
static void *attr(ParseCtx *c, const char *name, void *node);
static Tok token_attr(ParseCtx *c, const char *name, Tok tok);
static Tok token_attr_anon(ParseCtx *c, Tok tok);
static NodeCtx begin_node(ParseCtx *c, void *node, NodeKind kind);
static NodeCtx start_node(ParseCtx *c, NodeKind kind);
static void set_current_node(ParseCtx *c, AnyNode node);
static void *end_node(ParseCtx *c, NodeCtx ctx);
static Tok tnext(ParseCtx *c);
static void next(ParseCtx *c);
static Tok consume(ParseCtx *c);
static Tok at(ParseCtx *c);
static bool looking_at(ParseCtx *c, TokKind t);
static bool one_of(TokKind t, Toks toks);
static void advance(ParseCtx *c, Toks toks);
static void expected(ParseCtx *c, NodeID in, const char *msg);
static bool expect(ParseCtx *c, NodeID in, TokKind t);

ParseCtx new_parse_ctx(SourceCode *code) {
    static NodeID root = 0;
    return (ParseCtx){
        .lex = new_lexer(code),
        .arena = new_arena(),
        .current = {.data = &root, .kind = NODE_KIND_COUNT},
        .meta = new_node_metadata(),
    };
}

void parse_ctx_free(ParseCtx *c) {
    node_metadata_free(&c->meta);
    arena_free(&c->arena);
}

#define DECLARE_FOLLOW_SET(name, ...)                              \
    static const TokKind _FOLLOW_##name##_ITEMS[] = {__VA_ARGS__}; \
    static const Toks FOLLOW_##name = {                            \
        .items = (TokKind *)&_FOLLOW_##name##_ITEMS,               \
        .len = sizeof(_FOLLOW_##name##_ITEMS) / sizeof(TokKind),   \
    };

// TODO: maybe we need to be more dynamic with sync tokens

// These were manually calculated by looking at the tree-sitter grammar.
// I really fucking hope it was worth the effort.
DECLARE_FOLLOW_SET(IMPORT, T_IMPORT, T_LET, T_FUN, T_STRUCT, T_ENUM, T_ERROR,
                   T_UNION, T_TYPE)
DECLARE_FOLLOW_SET(DECL, T_LET, T_FUN, T_STRUCT, T_ENUM, T_ERROR, T_UNION)
DECLARE_FOLLOW_SET(VAR_BINDING, T_SCLN, T_EQ)
DECLARE_FOLLOW_SET(BINDING, T_COMMA, T_RPAR)
DECLARE_FOLLOW_SET(ALIAS_BINDING, T_COMMA, T_RBRC)
DECLARE_FOLLOW_SET(TYPE, T_COMMA, T_RPAR, T_SCLN, T_RBRC, T_LBRC, T_EQ)
DECLARE_FOLLOW_SET(IDENTS, T_RBRC)
DECLARE_FOLLOW_SET(ERRS, T_RBRC)
DECLARE_FOLLOW_SET(ERR, T_COMMA, T_RBRC)
DECLARE_FOLLOW_SET(FIELD, T_IDENT, T_RBRC)
DECLARE_FOLLOW_SET(STMTS, T_LBRC)
DECLARE_FOLLOW_SET(STMT, T_LET, T_FUN, T_STRUCT, T_ENUM, T_ERROR, T_UNION,
                   T_LBRC /*, TODO FIRST(expression) */)
DECLARE_FOLLOW_SET(ATOM, T_PLUS, T_MINUS, T_STAR, T_SLASH, T_EQ, T_EQEQ, T_NEQ,
                   T_AND, T_OR, T_RBRK, T_SCLN, T_RBRC)
DECLARE_FOLLOW_SET(UNION_TAG_COND, T_SCLN)
DECLARE_FOLLOW_SET(EXPR, T_SCLN, T_RBRK, T_COMMA, T_RPAR)
DECLARE_FOLLOW_SET(CASE_BRANCH, T_NUM, T_STR, T_CHAR, T_IDENT, T_SCOPE, T_RBRC)
DECLARE_FOLLOW_SET(CASE_PATT, T_ARROW)

SourceFile *parse_source_file(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_SOURCE_FILE);
    SourceFile *n = expect_node(NODE_SOURCE_FILE, nc.node);
    n->imports = parse_imports(c);
    n->decls = parse_decls(c);
    return end_node(c, nc);
}

Imports *parse_imports(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_IMPORTS);
    Imports *n = expect_node(NODE_IMPORTS, nc.node);
    while (looking_at(c, T_IMPORT)) {
        APPEND(n, parse_import(c));
    }
    arena_own(&c->arena, n->items, n->cap);
    return end_node(c, nc);
}

Import *parse_import(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_IMPORT);
    Import *n = expect_node(NODE_IMPORT, nc.node);
    assert(expect(c, n->id, T_IMPORT));
    n->module = parse_scoped_ident(c, TOKS(T_SCLN));
    if (!expect(c, n->id, T_SCLN)) {
        advance(c, FOLLOW_IMPORT);
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

Decls *parse_decls(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_DECLS);
    Decls *n = expect_node(NODE_DECLS, nc.node);
    while (!looking_at(c, T_EOF)) {
        APPEND(n, parse_decl(c));
    }
    arena_own(&c->arena, n->items, n->cap);
    return end_node(c, nc);
}

Decl *parse_decl(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_DECL);
    Decl *n = expect_node(NODE_DECL, nc.node);
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
            expected(c, n->id, "start of declaration");
            advance(c, FOLLOW_DECL);
            break;
    }
    return end_node(c, nc);
}

StructDecl *parse_struct_decl(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_STRUCT_DECL);
    StructDecl *n = expect_node(NODE_STRUCT_DECL, nc.node);
    assert(consume(c).t == T_STRUCT);
    Tok name = at(c);
    if (!expect(c, n->id, T_IDENT)) {
        advance(c, FOLLOW_DECL);
        return end_node(c, nc);
    }
    n->ident = token_attr(c, "name", name);
    n->body = parse_struct_body(c, FOLLOW_DECL);
    return end_node(c, nc);
}

StructBody *parse_struct_body(ParseCtx *c, Toks follow) {
    NodeCtx nc = start_node(c, NODE_STRUCT_BODY);
    StructBody *n = expect_node(NODE_STRUCT_BODY, nc.node);
    switch (at(c).t) {
        case T_LBRC:
            next(c);
            n->tuple_like = false;
            n->fields = parse_fields(c);
            if (!expect(c, n->id, T_RBRC)) {
                advance(c, follow);
                return end_node(c, nc);
            }
            break;
        case T_LPAR:
            next(c);
            n->tuple_like = true;
            n->types = parse_types(c);
            if (!expect(c, n->id, T_RPAR)) {
                advance(c, follow);
                return end_node(c, nc);
            }
            if (!expect(c, n->id, T_SCLN)) {
                advance(c, follow);
                return end_node(c, nc);
            }
            break;
        default:
            expected(c, n->id, "a struct or tuple body");
            advance(c, follow);
            break;
    }
    return end_node(c, nc);
}

EnumDecl *parse_enum_decl(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_ENUM_DECL);
    EnumDecl *n = expect_node(NODE_ENUM_DECL, nc.node);
    assert(consume(c).t == T_ENUM);
    Tok name = at(c);
    if (!expect(c, n->id, T_IDENT)) {
        advance(c, FOLLOW_DECL);
        return end_node(c, nc);
    }
    n->ident = token_attr(c, "name", name);
    if (!expect(c, n->id, T_LBRC)) {
        advance(c, FOLLOW_DECL);
        return end_node(c, nc);
    }
    n->alts = parse_idents(c);
    if (!expect(c, n->id, T_RBRC)) {
        advance(c, FOLLOW_DECL);
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

UnionDecl *parse_union_decl(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_UNION_DECL);
    UnionDecl *n = expect_node(NODE_UNION_DECL, nc.node);
    assert(consume(c).t == T_UNION);
    Tok name = at(c);
    if (!expect(c, n->id, T_IDENT)) {
        advance(c, FOLLOW_DECL);
        return end_node(c, nc);
    }
    n->ident = token_attr(c, "name", name);
    if (!expect(c, n->id, T_LBRC)) {
        advance(c, FOLLOW_DECL);
        return end_node(c, nc);
    }
    n->alts = parse_fields(c);
    if (!expect(c, n->id, T_RBRC)) {
        advance(c, FOLLOW_DECL);
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

ErrDecl *parse_err_decl(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_ERR_DECL);
    ErrDecl *n = expect_node(NODE_ERR_DECL, nc.node);
    assert(consume(c).t == T_ERROR);
    Tok name = at(c);
    if (!expect(c, n->id, T_IDENT)) {
        advance(c, FOLLOW_DECL);
        return end_node(c, nc);
    }
    n->ident = name;
    if (!expect(c, n->id, T_LBRC)) {
        advance(c, FOLLOW_DECL);
        return end_node(c, nc);
    }
    n->errs = parse_errs(c);
    if (!expect(c, n->id, T_RBRC)) {
        advance(c, FOLLOW_DECL);
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

VarDecl *parse_var_decl(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_VAR_DECL);
    VarDecl *n = expect_node(NODE_VAR_DECL, nc.node);
    assert(consume(c).t == T_LET);
    n->binding = attr(c, "binding", parse_var_binding(c));
    if (!looking_at(c, T_EQ) && !looking_at(c, T_SCLN)) {
        // Must have a type
        n->type = parse_type(c);
    }
    if (looking_at(c, T_EQ)) {
        n->init.assign_token = consume(c);
        n->init.expr = attr(c, "value", parse_expr(c));
    }
    if (!expect(c, n->id, T_SCLN)) {
        advance(c, FOLLOW_DECL);
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

VarBinding *parse_var_binding(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_VAR_BINDING);
    VarBinding *n = expect_node(NODE_VAR_BINDING, nc.node);
    switch (at(c).t) {
        case T_LPAR:
            n->t = VAR_BINDING_UNPACK_TUPLE;
            n->unpack_tuple = parse_unpack_tuple(c, FOLLOW_VAR_BINDING);
            break;
        case T_LBRC:
            n->t = VAR_BINDING_UNPACK_STRUCT;
            n->unpack_struct = parse_unpack_struct(c, FOLLOW_VAR_BINDING);
            break;
        case T_IDENT: {
            n->t = VAR_BINDING_BASIC;
            n->basic = token_attr(c, "name", consume(c));
            break;
        }
        default:
            expected(c, n->id, "a variable binding");
            advance(c, FOLLOW_VAR_BINDING);
            break;
    }
    return end_node(c, nc);
}

Binding *parse_binding(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_BINDING);
    Binding *n = expect_node(NODE_BINDING, nc.node);
    if (looking_at(c, T_STAR)) {
        n->ref.ok = true;
        n->ref.value = token_attr(c, "kind", consume(c));
        if (looking_at(c, T_RO)) {
            n->ref.ok = true;
            n->mod.value = token_attr(c, "modifier", consume(c));
        }
    }
    Tok name = at(c);
    if (!expect(c, n->id, T_IDENT)) {
        advance(c, FOLLOW_BINDING);
        return end_node(c, nc);
    }
    n->ident = token_attr(c, "name", name);
    return end_node(c, nc);
}

AliasBinding *parse_alias_binding(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_ALIAS_BINDING);
    AliasBinding *n = expect_node(NODE_ALIAS_BINDING, nc.node);
    n->binding = parse_binding(c);
    if (looking_at(c, T_EQ)) {
        next(c);
        Tok name = at(c);
        if (!expect(c, n->id, T_IDENT)) {
            advance(c, FOLLOW_ALIAS_BINDING);
            return end_node(c, nc);
        }
        n->alias.ok = true;
        n->alias.value = token_attr(c, "alias", name);
    }
    return end_node(c, nc);
}

UnpackStruct *parse_unpack_struct(ParseCtx *c, Toks follow) {
    NodeCtx nc = start_node(c, NODE_UNPACK_STRUCT);
    UnpackStruct *n = expect_node(NODE_UNPACK_STRUCT, nc.node);
    if (!expect(c, n->id, T_LBRC)) {
        advance(c, follow);
        return end_node(c, nc);
    }
    n->bindings = parse_alias_bindings(c, T_RBRC);
    if (!expect(c, n->id, T_RBRC)) {
        advance(c, follow);
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

UnpackTuple *parse_unpack_tuple(ParseCtx *c, Toks follow) {
    NodeCtx nc = start_node(c, NODE_UNPACK_TUPLE);
    UnpackTuple *n = expect_node(NODE_UNPACK_TUPLE, nc.node);
    if (!expect(c, n->id, T_LPAR)) {
        advance(c, follow);
        return end_node(c, nc);
    }
    n->bindings = parse_bindings(c, T_RPAR);
    if (!expect(c, n->id, T_RPAR)) {
        advance(c, follow);
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

UnpackUnion *parse_unpack_union(ParseCtx *c, Toks follow) {
    NodeCtx nc = start_node(c, NODE_UNPACK_UNION);
    UnpackUnion *n = expect_node(NODE_UNPACK_UNION, nc.node);

    Tok name = at(c);
    if (!expect(c, n->id, T_IDENT)) {
        advance(c, follow);
        return end_node(c, nc);
    }
    n->tag = token_attr(c, "tag", name);

    if (!expect(c, n->id, T_LPAR)) {
        advance(c, follow);
        return end_node(c, nc);
    }
    n->binding = parse_binding(c);
    if (!expect(c, n->id, T_RPAR)) {
        advance(c, follow);
        return end_node(c, nc);
    }

    return end_node(c, nc);
}

AliasBindings *parse_alias_bindings(ParseCtx *c, TokKind end) {
    NodeCtx nc = start_node(c, NODE_ALIAS_BINDINGS);
    AliasBindings *n = expect_node(NODE_ALIAS_BINDINGS, nc.node);
    APPEND(n, parse_alias_binding(c));
    while (looking_at(c, T_COMMA)) {
        next(c);
        // Ignore trailing comma
        if (looking_at(c, end)) {
            break;
        }
        APPEND(n, parse_alias_binding(c));
    }
    arena_own(&c->arena, n->items, n->cap);
    return end_node(c, nc);
}

Bindings *parse_bindings(ParseCtx *c, TokKind end) {
    NodeCtx nc = start_node(c, NODE_BINDINGS);
    Bindings *n = expect_node(NODE_BINDINGS, nc.node);
    APPEND(n, parse_binding(c));
    while (looking_at(c, T_COMMA)) {
        next(c);
        // Ignore trailing comma
        if (looking_at(c, end)) {
            break;
        }
        APPEND(n, parse_binding(c));
    }
    arena_own(&c->arena, n->items, n->cap);
    return end_node(c, nc);
}

FnDecl *parse_fn_decl(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_FN_DECL);
    FnDecl *n = expect_node(NODE_FN_DECL, nc.node);
    assert(consume(c).t == T_FUN);
    // TODO type parameters
    Tok name = at(c);
    if (!expect(c, n->id, T_IDENT)) {
        advance(c, FOLLOW_DECL);
        return end_node(c, nc);
    }
    n->ident = token_attr(c, "name", name);
    if (!expect(c, n->id, T_LPAR)) {
        advance(c, FOLLOW_DECL);
        return end_node(c, nc);
    }
    n->params = parse_fn_params(c);
    if (!expect(c, n->id, T_RPAR)) {
        advance(c, FOLLOW_DECL);
        return end_node(c, nc);
    }
    if (!looking_at(c, T_LBRC)) {
        n->return_type.ptr = parse_type(c);
    }
    n->body = parse_comp_stmt(c);
    return end_node(c, nc);
}

FnParams *parse_fn_params(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_FN_PARAMS);
    FnParams *n = expect_node(NODE_FN_PARAMS, nc.node);
    if (looking_at(c, T_RPAR)) {
        return end_node(c, nc);
    }
    APPEND(n, parse_fn_param(c));
    while (looking_at(c, T_COMMA)) {
        next(c);
        // Ignore trailing comma
        if (looking_at(c, T_RPAR)) {
            break;
        }
        APPEND(n, parse_fn_param(c));
    }
    arena_own(&c->arena, n->items, n->cap);
    return end_node(c, nc);
}

FnParam *parse_fn_param(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_FN_PARAM);
    FnParam *n = expect_node(NODE_FN_PARAM, nc.node);
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
    Stmt *n = expect_node(NODE_STMT, nc.node);
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
            n->t = STMT_RETURN;
            n->while_stmt = parse_while_stmt(c);
            break;
        case T_CASE:
            n->t = STMT_CASE;
            n->case_stmt = parse_case_stmt(c);
            break;
        default:
            n->t = STMT_EXPR;
            n->expr = parse_expr(c);
            if (!expect(c, n->id, T_SCLN)) {
                advance(c, FOLLOW_STMT);
                return end_node(c, nc);
            }
            break;
    }
    return end_node(c, nc);
}

CompStmt *parse_comp_stmt(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_COMP_STMT);
    CompStmt *n = expect_node(NODE_COMP_STMT, nc.node);
    if (!expect(c, n->id, T_LBRC)) {
        advance(c, FOLLOW_STMTS);
        return end_node(c, nc);
    }
    while (!looking_at(c, T_RBRC)) {
        if (looking_at(c, T_EOF)) {
            expected(c, n->id, "a statement");
            break;
        }
        APPEND(n, parse_stmt(c));
    }
    arena_own(&c->arena, n->items, n->cap);
    next(c);
    return end_node(c, nc);
}

IfStmt *parse_if_stmt(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_IF_STMT);
    IfStmt *n = expect_node(NODE_IF_STMT, nc.node);
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
    WhileStmt *n = expect_node(NODE_WHILE_STMT, nc.node);
    assert(consume(c).t == T_WHILE);
    n->cond = parse_cond(c);
    n->true_branch = parse_comp_stmt(c);
    return end_node(c, nc);
}

CasePatt *parse_case_patt(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_CASE_PATT);
    CasePatt *n = expect_node(NODE_CASE_PATT, nc.node);
    switch (at(c).t) {
        case T_LET:
            next(c);
            n->t = CASE_PATT_UNPACK_UNION;
            n->unpack_union = parse_unpack_union(c, FOLLOW_CASE_PATT);
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
    CaseBranch *n = expect_node(NODE_CASE_BRANCH, nc.node);
    n->patt = parse_case_patt(c);
    if (!expect(c, n->id, T_ARROW)) {
        advance(c, FOLLOW_CASE_BRANCH);
        return end_node(c, nc);
    }
    n->action = parse_stmt(c);
    return end_node(c, nc);
}

CaseBranches *parse_case_branches(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_CASE_BRANCHES);
    CaseBranches *n = expect_node(NODE_CASE_BRANCHES, nc.node);
    if (!expect(c, n->id, T_LBRC)) {
        advance(c, FOLLOW_STMT);
        return end_node(c, nc);
    }
    while (!looking_at(c, T_RBRC)) {
        if (looking_at(c, T_EOF)) {
            expected(c, n->id, "a case branch");
            break;
        }
        APPEND(n, parse_case_branch(c));
    }
    arena_own(&c->arena, n->items, n->cap);
    next(c);
    return end_node(c, nc);
}

CaseStmt *parse_case_stmt(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_CASE_STMT);
    CaseStmt *n = expect_node(NODE_CASE_STMT, nc.node);
    assert(consume(c).t == T_CASE);
    n->expr = parse_expr(c);
    n->branches = parse_case_branches(c);
    return end_node(c, nc);
}

ReturnStmt *parse_return_stmt(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_RETURN_STMT);
    ReturnStmt *n = expect_node(NODE_RETURN_STMT, nc.node);
    assert(consume(c).t == T_RETURN);
    if (looking_at(c, T_SCLN)) {
        return end_node(c, nc);
    }
    n->expr.ptr = parse_expr(c);
    if (!expect(c, n->id, T_SCLN)) {
        advance(c, FOLLOW_STMT);
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

Else *parse_else(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_ELSE);
    Else *n = expect_node(NODE_ELSE, nc.node);
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

Cond *parse_cond(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_COND);
    Cond *n = expect_node(NODE_COND, nc.node);
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
    UnionTagCond *n = expect_node(NODE_UNION_TAG_COND, nc.node);

    assert(consume(c).t == T_LET);

    n->trigger = parse_unpack_union(c, FOLLOW_UNION_TAG_COND);

    Tok assign_token = at(c);
    if (!expect(c, n->id, T_EQ)) {
        advance(c, FOLLOW_UNION_TAG_COND);
        return end_node(c, nc);
    }
    n->assign_token = assign_token;
    n->expr = parse_expr(c);

    return end_node(c, nc);
}

Type *parse_type(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_TYPE);
    Type *n = expect_node(NODE_TYPE, nc.node);
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
        case T_ANY: {
            {
                NodeCtx builtin_ctx = start_node(c, NODE_BUILTIN_TYPE);
                BuiltinType *builtin_type =
                    expect_node(NODE_BUILTIN_TYPE, builtin_ctx.node);
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
            n->scoped_ident = parse_scoped_ident(c, FOLLOW_TYPE);
            break;
        default:
            expected(c, n->id, "a type");
            advance(c, FOLLOW_TYPE);
            break;
    }
    return end_node(c, nc);
}

CollType *parse_coll_type(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_COLL_TYPE);
    CollType *n = expect_node(NODE_COLL_TYPE, nc.node);
    assert(consume(c).t == T_LBRK);
    // For empty index, e.g.: []u32
    if (looking_at(c, T_RBRK)) {
        next(c);
        n->element_type = parse_type(c);
        return end_node(c, nc);
    }
    n->index_expr.ptr = parse_expr(c);
    if (!expect(c, n->id, T_RBRK)) {
        advance(c, FOLLOW_TYPE);
        return end_node(c, nc);
    }
    n->element_type = parse_type(c);
    return end_node(c, nc);
}

StructType *parse_struct_type(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_STRUCT_TYPE);
    StructType *n = expect_node(NODE_STRUCT_TYPE, nc.node);
    assert(consume(c).t == T_STRUCT);
    n->body = parse_struct_body(c, FOLLOW_TYPE);
    return end_node(c, nc);
}

Fields *parse_fields(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_FIELDS);
    Fields *n = expect_node(NODE_FIELDS, nc.node);
    if (looking_at(c, T_RBRC)) {
        return end_node(c, nc);
    }
    APPEND(n, parse_field(c));
    while (looking_at(c, T_COMMA)) {
        next(c);
        // Ignore trailing comma
        if (looking_at(c, T_RBRC)) {
            break;
        }
        APPEND(n, parse_field(c));
    }
    arena_own(&c->arena, n->items, n->cap);
    return end_node(c, nc);
}

Field *parse_field(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_FIELD);
    Field *n = expect_node(NODE_FIELD, nc.node);
    Tok tok = at(c);
    if (!expect(c, n->id, T_IDENT)) {
        advance(c, FOLLOW_FIELD);
        return end_node(c, nc);
    }
    n->ident = token_attr(c, "name", tok);
    n->type = parse_type(c);
    return end_node(c, nc);
}

Types *parse_types(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_TYPES);
    Types *n = expect_node(NODE_TYPES, nc.node);
    if (looking_at(c, T_RPAR)) {
        return end_node(c, nc);
    }
    APPEND(n, parse_type(c));
    while (looking_at(c, T_COMMA)) {
        next(c);
        // Ignore trailing comma
        if (looking_at(c, T_RPAR)) {
            break;
        }
        APPEND(n, parse_type(c));
    }
    arena_own(&c->arena, n->items, n->cap);
    return end_node(c, nc);
}

UnionType *parse_union_type(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_UNION_TYPE);
    UnionType *n = expect_node(NODE_UNION_TYPE, nc.node);
    assert(consume(c).t == T_UNION);
    if (!expect(c, n->id, T_LBRC)) {
        advance(c, FOLLOW_TYPE);
        return end_node(c, nc);
    }
    n->fields = parse_fields(c);
    if (!expect(c, n->id, T_RBRC)) {
        advance(c, FOLLOW_TYPE);
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

EnumType *parse_enum_type(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_ENUM_TYPE);
    EnumType *n = expect_node(NODE_ENUM_TYPE, nc.node);
    assert(consume(c).t == T_ENUM);
    if (!expect(c, n->id, T_LBRC)) {
        advance(c, FOLLOW_TYPE);
        return end_node(c, nc);
    }
    n->alts = parse_idents(c);
    if (!expect(c, n->id, T_RBRC)) {
        advance(c, FOLLOW_TYPE);
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

ErrType *parse_err_type(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_ERR_TYPE);
    ErrType *n = expect_node(NODE_ERR_TYPE, nc.node);
    assert(consume(c).t == T_ERROR);
    if (!expect(c, n->id, T_LBRC)) {
        advance(c, FOLLOW_TYPE);
        return end_node(c, nc);
    }
    n->errs = parse_errs(c);
    if (!expect(c, n->id, T_RBRC)) {
        advance(c, FOLLOW_TYPE);
        return end_node(c, nc);
    }
    return end_node(c, nc);
}

Idents *parse_idents(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_IDENTS);
    Idents *n = expect_node(NODE_IDENTS, nc.node);
    bool start = true;
    while (!looking_at(c, T_RBRC)) {
        if (!start && !expect(c, n->id, T_COMMA)) {
            advance(c, FOLLOW_IDENTS);
            return end_node(c, nc);
        }
        // Ignore trailing comma
        if (looking_at(c, T_RBRC)) {
            break;
        }
        Tok identifier = at(c);
        if (!expect(c, n->id, T_IDENT)) {
            advance(c, FOLLOW_IDENTS);
            return end_node(c, nc);
        }
        APPEND(n, token_attr_anon(c, identifier));
        start = false;
    }
    if (n->len != 0) {
        arena_own(&c->arena, n->items, n->cap);
    }
    return end_node(c, nc);
}

Errs *parse_errs(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_ERRS);
    Errs *n = expect_node(NODE_ERRS, nc.node);
    bool start = true;
    while (!looking_at(c, T_RBRC)) {
        if (!start && !expect(c, n->id, T_COMMA)) {
            advance(c, FOLLOW_ERRS);
            return end_node(c, nc);
        }
        // Ignore trailing comma
        if (looking_at(c, T_RBRC)) {
            break;
        }
        APPEND(n, parse_err(c));
        start = false;
    }
    if (n->len != 0) {
        arena_own(&c->arena, n->items, n->cap);
    }
    return end_node(c, nc);
}

Err *parse_err(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_ERR);
    Err *n = expect_node(NODE_ERR, nc.node);
    if (looking_at(c, T_BANG)) {
        n->embedded = true;
        token_attr(c, "kind", consume(c));
    }
    n->scoped_ident = parse_scoped_ident(c, FOLLOW_ERR);
    return end_node(c, nc);
}

PtrType *parse_ptr_type(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_PTR_TYPE);
    PtrType *n = expect_node(NODE_PTR_TYPE, nc.node);
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
    FnType *n = expect_node(NODE_FN_TYPE, nc.node);
    assert(consume(c).t == T_FUN);
    if (!expect(c, n->id, T_LPAR)) {
        advance(c, FOLLOW_TYPE);
        return end_node(c, nc);
    }
    n->params = parse_types(c);
    if (!expect(c, n->id, T_RPAR)) {
        advance(c, FOLLOW_TYPE);
        return end_node(c, nc);
    }
    if (!one_of(at(c).t, FOLLOW_TYPE)) {
        n->return_type.ptr = parse_type(c);
    }
    return end_node(c, nc);
}

// Some rules that are used in many contexts take a follow set from the
// caller. This improves the error recovery as it has more context of what
// it should be syncing on than just "whatever can follow is really common rule"
ScopedIdent *parse_scoped_ident(ParseCtx *c, Toks follow) {
    NodeCtx nc = start_node(c, NODE_SCOPED_IDENT);
    ScopedIdent *n = expect_node(NODE_SCOPED_IDENT, nc.node);
    if (looking_at(c, T_SCOPE)) {
        Tok empty_str = {
            .t = T_EMPTY_STRING,
            .text = ZTOS(""),
            .offset = at(c).offset,
        };
        APPEND(n, token_attr_anon(c, empty_str));
        next(c);  // skip the '::'
    }
    Tok ident = at(c);
    if (!expect(c, n->id, T_IDENT)) {
        advance(c, follow);
        return end_node(c, nc);
    }
    APPEND(n, token_attr_anon(c, ident));
    while (looking_at(c, T_SCOPE)) {
        next(c);
        Tok ident = at(c);
        if (!expect(c, n->id, T_IDENT)) {
            advance(c, follow);
            return end_node(c, nc);
        }
        APPEND(n, token_attr_anon(c, ident));
    }
    arena_own(&c->arena, n->items, n->cap);
    return end_node(c, nc);
}

Init *parse_init(ParseCtx *c, TokKind delim) {
    NodeCtx nc = start_node(c, NODE_INIT);
    Init *n = expect_node(NODE_INIT, nc.node);
    // No items in list, just return
    if (looking_at(c, delim)) {
        return end_node(c, nc);
    }
    APPEND(n, parse_expr(c));
    while (looking_at(c, T_COMMA)) {
        next(c);
        // Ignore trailing comma
        if (looking_at(c, delim)) {
            break;
        }
        APPEND(n, parse_expr(c));
    }
    arena_own(&c->arena, n->items, n->cap);
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
        case T_EQ:
        case T_AND:
        case T_OR:
        case T_AMP:
        case T_PIPE:
        case T_NEQ:
        case T_EQEQ:
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
    PREC_ASSIGNMENT,
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
    [PREC_ASSIGNMENT] = {2, 1},
};

static Index *index(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_INDEX);
    Index *n = expect_node(NODE_INDEX, nc.node);
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
    if (!expect(c, n->id, T_RBRK)) {
        advance(c, FOLLOW_EXPR);
    }

    return end_node(c, nc);
}

static NodeCtx parse_postfix(ParseCtx *c, Expr *lhs) {
    NodeCtx expr_ctx = start_node(c, NODE_EXPR);
    Expr *expr = expect_node(NODE_EXPR, expr_ctx.node);

    switch (at(c).t) {
        case T_LBRK: {
            NodeCtx access_ctx = start_node(c, NODE_COLL_ACCESS);
            CollAccess *access = expect_node(NODE_COLL_ACCESS, access_ctx.node);

            NodeCtx lhs_ctx = begin_node(c, lhs, NODE_EXPR);
            access->lvalue = end_node(c, lhs_ctx);
            access->index = index(c);

            expr->t = EXPR_COLL_ACCESS;
            expr->coll_access = end_node(c, access_ctx);
            break;
        }
        case T_LPAR: {
            NodeCtx call_ctx = start_node(c, NODE_FN_CALL);
            FnCall *call = expect_node(NODE_FN_CALL, call_ctx.node);

            NodeCtx lhs_ctx = begin_node(c, lhs, NODE_EXPR);
            call->lvalue = end_node(c, lhs_ctx);

            if (!expect(c, call->id, T_LPAR)) {
                advance(c, FOLLOW_ATOM);
                goto yield_call_expr;
            }

            call->args = attr(c, "args", parse_init(c, T_RPAR));

            if (!expect(c, call->id, T_RPAR)) {
                advance(c, FOLLOW_ATOM);
            }

        yield_call_expr:
            expr->t = EXPR_FN_CALL;
            expr->fn_call = end_node(c, call_ctx);
            break;
        }
        case T_DOT: {
            NodeCtx access_ctx = start_node(c, NODE_FIELD_ACCESS);
            FieldAccess *access =
                expect_node(NODE_FIELD_ACCESS, access_ctx.node);

            NodeCtx lhs_ctx = begin_node(c, lhs, NODE_EXPR);
            access->lvalue = attr(c, "lvalue", end_node(c, lhs_ctx));

            next(c);
            Tok field = at(c);
            if (!expect(c, access->id, T_IDENT)) {
                advance(c, FOLLOW_ATOM);
                goto yield_access_expr;
            }
            access->field = token_attr(c, "field", field);

        yield_access_expr:
            expr->t = EXPR_FIELD_ACCESS;
            expr->field_access = end_node(c, access_ctx);
            break;
        }
        default: {
            NodeCtx postfix_ctx = start_node(c, NODE_POSTFIX_EXPR);
            PostfixExpr *postfix =
                expect_node(NODE_POSTFIX_EXPR, postfix_ctx.node);

            postfix->op = token_attr(c, "op", consume(c));
            NodeCtx lhs_ctx = begin_node(c, lhs, NODE_EXPR);
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
        case T_EQ:
            return (Maybe_Power){binding_power_of[PREC_ASSIGNMENT], true};
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
        lhs = expect_node(NODE_EXPR, lhs_ctx.node);

        if (!expect(c, lhs->id, T_RPAR)) {
            advance(c, FOLLOW_EXPR);
            return lhs_ctx;
        }
        if (!is_op(at(c).t)) {
            return lhs_ctx;
        }
    } else if (is_prefix_op(tok.t)) {
        lhs_ctx = start_node(c, NODE_EXPR);
        lhs = expect_node(NODE_EXPR, lhs_ctx.node);

        NodeCtx unary_ctx = start_node(c, NODE_UNARY_EXPR);
        UnaryExpr *unary_expr = expect_node(NODE_UNARY_EXPR, unary_ctx.node);

        Power pow;
        Maybe_Power maybe_pow = prefix_bpow(tok.t);
        if (!maybe_pow.ok) {
            expected(c, unary_expr->id, "valid prefix operator");
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
        lhs = expect_node(NODE_EXPR, lhs_ctx.node);

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
            lhs = expect_node(NODE_EXPR, lhs_ctx.node);
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
        Expr *new_expr = expect_node(NODE_EXPR, new_expr_ctx.node);

        NodeCtx bin_ctx = start_node(c, NODE_BIN_EXPR);
        BinExpr *bin_expr = expect_node(NODE_BIN_EXPR, bin_ctx.node);

        bin_expr->op = token_attr(c, "op", op);
        NodeCtx old_lhs_ctx = begin_node(c, lhs, NODE_EXPR);
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
    AnyNode parent = c->current;
    NodeCtx ctx = expr_with_bpow(c, 0);
    if (*parent.data == 0) {
        ctx.parent = c->current;
    }
    return end_node(c, ctx);
}

static void set_atom_token(ParseCtx *c, Atom *atom) {
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
        atom->token = token_attr(c, attr_, consume(c));
    } else {
        atom->token = token_attr_anon(c, consume(c));
    }
}

Atom *parse_atom(ParseCtx *c) {
    NodeCtx nc = start_node(c, NODE_ATOM);
    Atom *n = expect_node(NODE_ATOM, nc.node);
    switch (at(c).t) {
        case T_SCOPE:
        case T_IDENT: {
            n->t = ATOM_SCOPED_IDENT;
            n->scoped_ident = parse_scoped_ident(c, TOKS(T_SCLN));
            return end_node(c, nc);
        }
        case T_BTICK: {
            next(c);

            NodeCtx lit_ctx = start_node(c, NODE_BRACED_LIT);
            BracedLit *braced_lit = expect_node(NODE_BRACED_LIT, lit_ctx.node);

            braced_lit->type.ptr = parse_type(c);

            if (!expect(c, n->id, T_LBRC)) {
                advance(c, FOLLOW_ATOM);
                goto yield_braced_lit;
            }
            braced_lit->init = parse_init(c, T_LBRC);

            if (!expect(c, n->id, T_RBRC)) {
                advance(c, FOLLOW_ATOM);
                goto yield_braced_lit;
            }

        yield_braced_lit:
            n->t = ATOM_BRACED_LIT;
            n->braced_lit = end_node(c, lit_ctx);
            return end_node(c, nc);
        }
        case T_LBRC: {
            next(c);

            NodeCtx lit_ctx = start_node(c, NODE_BRACED_LIT);
            BracedLit *braced_lit = expect_node(NODE_BRACED_LIT, lit_ctx.node);

            braced_lit->init = parse_init(c, T_RBRC);
            n->t = ATOM_BRACED_LIT;
            n->braced_lit = end_node(c, lit_ctx);

            if (!expect(c, n->id, T_RBRC)) {
                advance(c, FOLLOW_ATOM);
            }

            return end_node(c, nc);
        }
        case T_NUM:
        case T_CHAR:
        case T_STR:
            n->t = ATOM_TOKEN;
            set_atom_token(c, n);
            return end_node(c, nc);
        default:
            break;
    }

    expected(c, n->id, "an atom");
    advance(c, FOLLOW_ATOM);
    return end_node(c, nc);
}

static void *make_node(ParseCtx *c, NodeKind kind) {
    return new_node(&c->meta, &c->arena, kind);
}

static void *attr(ParseCtx *c, const char *name, void *node) {
    NodeChild *child = last_child(&c->meta, *c->current.data);
    child->name.ptr = name;
    return node;
}

static Tok token_attr(ParseCtx *c, const char *name, Tok tok) {
    add_child(&c->meta, *c->current.data, child_token_named(name, tok));
    return tok;
}

static Tok token_attr_anon(ParseCtx *c, Tok tok) {
    add_child(&c->meta, *c->current.data, child_token(tok));
    return tok;
}

static NodeCtx begin_node(ParseCtx *c, void *node, NodeKind kind) {
    set_node_pos(&c->meta, *(NodeID *)node,
                 line_and_column(c->lex.source->lines, c->lex.cursor));
    NodeCtx ctx = {
        .node = {.data = node, .kind = kind},
        .parent = c->current,
    };
    c->current = ctx.node;
    return ctx;
}

static NodeCtx start_node(ParseCtx *c, NodeKind kind) {
    return begin_node(c, make_node(c, kind), kind);
}

static void set_current_node(ParseCtx *c, AnyNode node) { c->current = node; }

static void *end_node(ParseCtx *c, NodeCtx ctx) {
    assert(*ctx.node.data == *c->current.data);
    bool root = *ctx.parent.data == *c->current.data;
    if (!root) {
        assert(
            *ctx.parent.data !=
            *c->current.data);  // Make sure we can't accidentally add a cycle
        add_child(&c->meta, *ctx.parent.data, child_node(c->current));
        set_current_node(c, ctx.parent);
    }
    return ctx.node.data;
}

// static ParseState set_marker(ParseCtx *c) { return (ParseState){c->lex}; }
// static void backtrack(ParseCtx *c, ParseState marker) { c->lex = marker.lex;
// }

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

static void expected(ParseCtx *c, NodeID in, const char *msg) {
    Tok tok = lex_peek(&c->lex);
    raise_syntax_error(c->lex.source, (SyntaxError){
                                          .at = c->lex.cursor,
                                          .expected = msg,
                                          .got = tok_to_string[tok.t].data,
                                      });
    add_node_flags(&c->meta, in, NFLAG_ERROR);
}

static bool expect(ParseCtx *c, NodeID in, TokKind t) {
    Tok tok = lex_peek(&c->lex);
    bool match = tok.t == t;
    if (!match) {
        expected(c, in, tok_to_string[t].data);
    } else {
        next(c);
    }
    return match;
}
