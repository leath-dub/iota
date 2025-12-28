#include "ast.h"

#include <assert.h>
#include <string.h>

Ast ast_create(Arena *a) {
    return (Ast){
        .arena = a,
        .root = NULL,
        .tree_data = tree_data_create(a),
    };
}

static DfsCtrl ast_node_delete(void *ctx, AstNode *node) {
    (void)ctx;
    if (node->children) {
        children_delete(node->children);
    }
    return DFS_CTRL_KEEP_GOING;
}

void ast_delete(Ast ast) {
    ast_traverse_dfs(NULL, &ast,
                     (EnterExitVTable){
                         .exit = ast_node_delete,
                     });
    tree_data_delete(ast.tree_data);
}

TreeData tree_data_create(Arena *a) {
    return (TreeData){
        .arena = a,
        .scope = scope_map_create(128),
        .resolves_to = resolves_to_map_create(128),
        .type = type_map_create(128),
        .type_data = type_data_create(),
    };
}

void tree_data_delete(TreeData td) {
    MapCursor it = map_cursor_create(td.scope);
    while (map_cursor_next(&it)) {
        Scope **scope = it.current;
        if ((*scope)->table) {
            scope_entry_map_delete((*scope)->table);
        }
    }
    scope_map_delete(td.scope);
    resolves_to_map_delete(td.resolves_to);
    type_map_delete(td.type);
    type_data_delete(td.type_data);
}

#define IMPL_CHILD_ACCESS(NODE, UPPER_NAME, lower_name)       \
    NODE *child_##lower_name##_at(AstNode *n, size_t index) { \
        assert(da_length(n->children) != 0);                  \
        Child *ch = children_at(n->children, index);          \
        assert(ch->t == CHILD_NODE);                          \
        assert(ch->node->kind == NODE_##UPPER_NAME);          \
        return (NODE *)ch->node;                              \
    }

EACH_NODE(IMPL_CHILD_ACCESS)

Child child_token_create(Tok tok) {
    return (Child){
        .t = CHILD_TOKEN,
        .token = tok,
        .name.ptr = NULL,
    };
}

Child child_token_named_create(const char *name, Tok token) {
    return (Child){
        .t = CHILD_TOKEN,
        .token = token,
        .name.ptr = name,
    };
}

Child child_node_create(AstNode *n) {
    return (Child){
        .t = CHILD_NODE,
        .node = n,
        .name.ptr = NULL,
    };
}

Child child_node_named_create(const char *name, AstNode *n) {
    return (Child){
        .t = CHILD_NODE,
        .node = n,
        .name.ptr = name,
    };
}

typedef struct {
    usize size;
    usize align;
} Layout;

#define LAYOUT_OF(T) {.size = sizeof(T), .align = _Alignof(T)}

typedef struct {
    const char *name;
    Layout layout;
} NodeDescriptor;

Scope *ast_scope_create(Ast *ast) {
    Scope *res = arena_alloc(ast->arena, sizeof(Scope), _Alignof(Scope));
    res->self = NULL;
    res->table = NULL;
    res->enclosing_scope.ptr = NULL;
    return res;
}

void ast_scope_set(Ast *ast, AstNode *n, Scope *scope) {
    bool ins = false;
    Scope **sr = scope_map_get_or_insert(&ast->tree_data.scope, n, &ins);
    assert(ins && "probably tried to insert scope twice");
    scope->self = n;
    *sr = scope;
}

Scope *ast_scope_get(Ast *ast, AstNode *n) {
    Scope **res = scope_map_get(ast->tree_data.scope, n);
    return res ? *res : NULL;
}

void ast_scope_insert(Ast *ast, Scope *scope, string name, AstNode *n) {
    if (scope->table == NULL) {
        scope->table = scope_entry_map_create(128);
    }
    bool ins = false;
    ScopeEntry **er = scope_entry_map_get_or_insert(
        &scope->table, (bytes){RSPLATU(name)}, &ins);
    if (ins) {
        *er = arena_alloc(ast->arena, sizeof(ScopeEntry), _Alignof(ScopeEntry));
        (*er)->node = n;
        (*er)->shadows = NULL;
        return;
    }

    // Allocate a new entry and copy the old head into it.
    ScopeEntry *old_ent =
        arena_alloc(ast->arena, sizeof(ScopeEntry), _Alignof(ScopeEntry));
    *old_ent = **er;

    (*er)->node = n;
    (*er)->shadows = old_ent;
}

ScopeLookup scope_lookup(Scope *scope, string name, ScopeLookupMode mode) {
    bytes name_bytes = {RSPLATU(name)};

    ScopeEntry **entry = NULL;
    if (scope->table) {
        entry = scope_entry_map_get(scope->table, name_bytes);
        if (entry) {
            return (ScopeLookup){*entry, scope};
        }
    }

    // If not doing lexical lookup, just fail here
    if (mode == LOOKUP_MODE_DIRECT) {
        return (ScopeLookup){NULL, NULL};
    }

    // Otherwise for lexical lookup check the enclosing scopes backwards
    Scope *it = scope;
    while (it->enclosing_scope.ptr) {
        entry = scope_entry_map_get(it->enclosing_scope.ptr->table, name_bytes);
        if (entry) {
            return (ScopeLookup){*entry, it->enclosing_scope.ptr};
        }
        it = it->enclosing_scope.ptr;
    }

    return (ScopeLookup){NULL, NULL};
}

void ast_traverse_dfs(void *ctx, Ast *ast, EnterExitVTable vtable) {
    DfsCtrl ctrl = DFS_CTRL_KEEP_GOING;
    if (vtable.enter && !ast->root->has_error) {
        ctrl = vtable.enter(ctx, ast->root);
    }
    if (ctrl == DFS_CTRL_KEEP_GOING) {
        Child *children = ast->root->children;
        for (size_t i = 0; i < da_length(children); i++) {
            Child child = children[i];
            switch (child.t) {
                case CHILD_NODE: {
                    Ast sub_tree = *ast;
                    sub_tree.root = child.node;
                    ast_traverse_dfs(ctx, &sub_tree, vtable);
                    break;
                }
                default:
                    break;
            }
        }
    }
    if (vtable.exit && !ast->root->has_error) {
        (void)vtable.exit(ctx, ast->root);
    }
}

#define DESCRIBE_NODE(TYPE, UPPER_NAME, REPR) \
    [NODE_##UPPER_NAME] = {#REPR, LAYOUT_OF(TYPE)},

static NodeDescriptor node_descriptors[NODE_KIND_COUNT] = {
    EACH_NODE(DESCRIBE_NODE)};

const char *node_kind_to_string(NodeKind kind) {
    return node_descriptors[kind].name;
}

AstNode *ast_node_create(Ast *ast, NodeKind kind) {
    Layout layout = node_descriptors[kind].layout;
    AstNode *res = arena_alloc(ast->arena, layout.size, layout.align);
    res->kind = kind;
    return res;
}

void ast_node_child_add(AstNode *node, Child child) {
    if (!node->children) {
        node->children = children_create();
    }
    children_append(&node->children, child);
}

void ast_node_reparent(AstNode *node, AstNode *new_parent) {
    // If the node already has a parent, remove it from the
    // current parent's child list
    if (node->parent.ptr != NULL) {
        AstNode *p = node->parent.ptr;
        // Find the position of the node in the parent's child list
        size_t i = 0;
        assert(da_length(p->children) != 0);
        for (; i < da_length(p->children); i++) {
            Child child = p->children[i];
            if (child.t == CHILD_NODE && child.node == node) {
                break;
            }
        }

        Child found = p->children[i];
        assert(found.t == CHILD_NODE && found.node == node);

        children_remove(p->children, i);
    }

    ast_node_child_add(new_parent, child_node_create(node));
    node->parent.ptr = new_parent;
}

void ast_resolves_to_set(Ast *ast, Ident *ident, AstNode *to) {
    resolves_to_map_put(&ast->tree_data.resolves_to, ident, to);
}

AstNode *ast_resolves_to_get(Ast *ast, Ident *ident) {
    AstNode **res = resolves_to_map_get(ast->tree_data.resolves_to, ident);
    return res ? *res : NULL;
}

AstNode *ast_resolves_to_get_scoped(Ast *ast, ScopedIdent *scoped_ident) {
    Ident *last_ident = child_ident_at(
        &scoped_ident->head, da_length(scoped_ident->head.children) - 1);
    return ast_resolves_to_get(ast, last_ident);
}

TypeId ast_type_create(Ast *ast) {
    TypeRepr **type_data = &ast->tree_data.type_data;
    TypeId id = da_length(*type_data);
    type_data_append(type_data, (TypeRepr){0});
    return id + 1;
}

void ast_type_set(Ast *ast, AstNode *n, TypeId tid) {
    switch (n->kind) {
        case NODE_ATOM:
        case NODE_POSTFIX_EXPR:
        case NODE_CALL:
        case NODE_FIELD_ACCESS:
        case NODE_COLL_ACCESS:
        case NODE_UNARY_EXPR:
        case NODE_BIN_EXPR:
        case NODE_VAR_DECL:
        case NODE_TYPE_DECL:
            break;
        default:
            assert(false &&
                   "can only set type on concrete expression or variable "
                   "declaration");
    }
    type_map_put(&ast->tree_data.type, n, tid);
}

TypeId ast_type_get(Ast *ast, AstNode *n) {
    TypeId *res = type_map_get(ast->tree_data.type, n);
    return res ? *res : INVALID_TYPE;
}

TypeRepr *ast_type_repr(Ast *ast, TypeId id) {
    return type_data_at(ast->tree_data.type_data, id - 1);
}
