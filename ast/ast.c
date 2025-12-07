#include "ast.h"

#include <assert.h>
#include <string.h>

NodeMetadata new_node_metadata(void) {
    return (NodeMetadata){
        .next_id = 0,
        .flags =
            {
                .len = 0,
                .cap = 0,
                .items = NULL,
            },
        .tree =
            {
                .len = 0,
                .cap = 0,
                .items = NULL,
            },
        .names =
            {
                .len = 0,
                .cap = 0,
                .items = NULL,
            },
        .scope_allocs = scope_map_create(128),
        .resolved_nodes = any_node_map_create(128),
        .types = type_repr_map_create(128),
        .offsets =
            {
                .len = 0,
                .cap = 0,
                .items = NULL,
            },
        .arena = new_arena(),
    };
}

void node_metadata_free(NodeMetadata *m) {
    if (m->flags.items != NULL) {
        free(m->flags.items);
    }
    if (m->tree.items != NULL) {
        for (u32 id = 0; id < m->tree.len; id++) {
            NodeChildren *children = get_node_children(m, id);
            free(children->items);
        }
        free(m->tree.items);
    }
    if (m->names.items != NULL) {
        free(m->names.items);
    }
    if (m->offsets.items != NULL) {
        free(m->offsets.items);
    }

    MapCursor it = map_cursor_create(m->scope_allocs);
    while (map_cursor_next(&it)) {
        Scope *scope = *(Scope **)it.current;
        scope_entry_map_delete(scope->table);
    }
    scope_map_delete(m->scope_allocs);
    any_node_map_delete(m->resolved_nodes);
    type_repr_map_delete(m->types);
    arena_free(&m->arena);
}

void add_node_flags(NodeMetadata *m, NodeID id, NodeFlag flags) {
    m->flags.items[id] |= flags;
}

bool has_error(NodeMetadata *m, void *node) {
    assert(node != NULL);
    NodeID *id = node;
    return m->flags.items[*id] & NFLAG_ERROR;
}

void set_node_offset(NodeMetadata *m, NodeID id, u32 pos) {
    *AT(m->offsets, id) = pos;
}

u32 get_node_offset(NodeMetadata *m, NodeID id) { return *AT(m->offsets, id); }

void add_child(NodeMetadata *m, NodeID id, NodeChild child) {
    NodeChildren *children = &m->tree.items[id].children;
    APPEND(children, child);
}

NodeChild child_token(Tok token) {
    return (NodeChild){
        .t = CHILD_TOKEN,
        .token = token,
        .name.ptr = NULL,
    };
}

NodeChild child_token_named(const char *name, Tok token) {
    return (NodeChild){
        .t = CHILD_TOKEN,
        .token = token,
        .name.ptr = name,
    };
}

NodeChild child_node(AnyNode node) {
    return (NodeChild){
        .t = CHILD_NODE,
        .node = node,
        .name.ptr = NULL,
    };
}

NodeChild child_node_named(const char *name, AnyNode node) {
    return (NodeChild){
        .t = CHILD_NODE,
        .node = node,
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

const char *get_node_name(NodeMetadata *m, NodeID id) {
    return *AT(m->names, id);
}

NodeChildren *get_node_children(NodeMetadata *m, NodeID id) {
    return &AT(m->tree, id)->children;
}

void set_node_parent(NodeMetadata *m, NodeID id, AnyNode parent) {
    NodeTreeItem *item = AT(m->tree, id);
    assert(!item->parent.ok && "parent node already set");
    item->parent.ok = true;
    item->parent.value = parent;
}

AnyNode get_node_parent(NodeMetadata *m, NodeID id) {
    NodeTreeItem *item = AT(m->tree, id);
    assert(item->parent.ok);
    return item->parent.value;
}

NodeChild *last_child(NodeMetadata *m, NodeID id) {
    NodeChildren *children = get_node_children(m, id);
    return AT(*children, children->len - 1);
}

void remove_child(NodeMetadata *m, NodeID from, NodeID child) {
    NodeChildren *children = get_node_children(m, from);
    int child_index = -1;
    for (u32 i = 0; i < children->len; i++) {
        NodeChild *check = &children->items[i];
        bool child_to_remove =
            check->t == CHILD_NODE && *check->node.data == child;
        if (child_to_remove) {
            child_index = i;
            break;
        }
    }
    assert(child_index != -1 && "Child not found");

    NodeChild *child_ptr = &children->items[child_index];
    memmove(child_ptr, child_ptr + 1,
            (children->len - child_index - 1) * sizeof(NodeChild));
    children->len--;
}

void *expect_node(NodeKind kind, AnyNode node) {
    assert(kind == node.kind);
    return node.data;
}

Scope *scope_attach(NodeMetadata *m, AnyNode node) {
    bool inserted = false;
    Scope **scope_ref =
        scope_map_get_or_insert(&m->scope_allocs, *node.data, &inserted);
    assert(inserted && "probably tried to insert a scope twice");

    Scope *scope = arena_alloc(&m->arena, sizeof(Scope), _Alignof(Scope));
    scope->self = node;
    scope->table = scope_entry_map_create(128);
    scope->enclosing_scope.ptr = NULL;
    *scope_ref = scope;

    return scope;
}

Scope *scope_get(NodeMetadata *m, NodeID id) {
    Scope **res = scope_map_get(m->scope_allocs, id);
    return res ? *res : NULL;
}

void scope_insert(NodeMetadata *m, Scope *scope, string symbol, AnyNode node) {
    bool inserted = false;
    ScopeEntry **entry_ref = scope_entry_map_get_or_insert(
        &scope->table, (bytes){RSPLATU(symbol)}, &inserted);
    if (inserted) {
        *entry_ref =
            arena_alloc(&m->arena, sizeof(ScopeEntry), _Alignof(ScopeEntry));
        (*entry_ref)->node = node;
        (*entry_ref)->shadows = NULL;
        return;
    }

    // Allocate a new entry and copy the old head into it.
    ScopeEntry *old_entry =
        arena_alloc(&m->arena, sizeof(ScopeEntry), _Alignof(ScopeEntry));
    *old_entry = **entry_ref;

    (*entry_ref)->node = node;
    (*entry_ref)->shadows = old_entry;
}

ScopeLookup scope_lookup(Scope *scope, string symbol, ScopeLookupMode mode) {
    bytes symbol_bytes = {RSPLATU(symbol)};

    ScopeEntry **entry = scope_entry_map_get(scope->table, symbol_bytes);
    if (entry) {
        return (ScopeLookup){*entry, scope};
    }
    // If not doing lexical lookup, just fail here
    if (mode == LOOKUP_MODE_DIRECT) {
        return (ScopeLookup){NULL, NULL};
    }

    // Otherwise for lexical lookup check the enclosing scopes backwards
    Scope *it = scope;
    while (it->enclosing_scope.ptr) {
        entry =
            scope_entry_map_get(it->enclosing_scope.ptr->table, symbol_bytes);
        if (entry) {
            return (ScopeLookup){*entry, it->enclosing_scope.ptr};
        }
        it = it->enclosing_scope.ptr;
    }

    return (ScopeLookup){NULL, NULL};
}

void ast_traverse_dfs(void *ctx, AnyNode root, NodeMetadata *m,
                      EnterExitVTable vtable) {
    DfsCtrl ctrl = DFS_CTRL_KEEP_GOING;
    if (vtable.enter && !has_error(m, root.data)) {
        ctrl = vtable.enter(ctx, root);
    }
    if (ctrl == DFS_CTRL_KEEP_GOING) {
        const NodeChildren *children = get_node_children(m, *root.data);
        for (u32 i = 0; i < children->len; i++) {
            NodeChild child = children->items[i];
            switch (child.t) {
                case CHILD_NODE:
                    ast_traverse_dfs(ctx, child.node, m, vtable);
                    break;
                default:
                    break;
            }
        }
    }
    if (vtable.exit && !has_error(m, root.data)) {
        (void)vtable.exit(ctx, root);
    }
}

#define DESCRIBE_NODE(TYPE, UPPER_NAME, REPR) \
    [NODE_##UPPER_NAME] = {REPR, LAYOUT_OF(TYPE)},

static NodeDescriptor node_descriptors[NODE_KIND_COUNT] = {
    EACH_NODE(DESCRIBE_NODE)};

const char *node_kind_name(NodeKind kind) {
    return node_descriptors[kind].name;
}

void *new_node(NodeMetadata *m, Arena *a, NodeKind kind) {
    Layout layout = node_descriptors[kind].layout;
    NodeID *r = arena_alloc(a, layout.size, layout.align);
    APPEND(&m->flags, 0);
    APPEND(&m->names, node_kind_name(kind));
    APPEND(&m->tree, (NodeTreeItem){0});
    APPEND(&m->offsets, 0);
    *r = m->next_id++;
    return r;
}

void set_resolved_node(NodeMetadata *m, AnyNode node, AnyNode resolved_node) {
    assert(node.kind == NODE_IDENT);
    any_node_map_put(&m->resolved_nodes, *node.data, resolved_node);
}

AnyNode *try_get_resolved_node(NodeMetadata *m, NodeID id) {
    return any_node_map_get(m->resolved_nodes, id);
}

TypeRepr *type_alloc(NodeMetadata *m) {
    return arena_alloc(&m->arena, sizeof(TypeRepr), _Alignof(TypeRepr));
}

void type_set(NodeMetadata *m, AnyNode node, TypeRepr *type) {
    switch (node.kind) {
        case NODE_ATOM:
        case NODE_POSTFIX_EXPR:
        case NODE_CALL:
        case NODE_FIELD_ACCESS:
        case NODE_COLL_ACCESS:
        case NODE_UNARY_EXPR:
        case NODE_BIN_EXPR:
        case NODE_VAR_DECL:
            break;
        default:
            assert(false &&
                   "can only set type on concrete expression or variable "
                   "declaration");
    }
    type_repr_map_put(&m->types, *node.data, *type);
}

TypeRepr *type_try_get(NodeMetadata *m, NodeID id) {
    return type_repr_map_get(m->types, id);
}
