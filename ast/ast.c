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
        .scope_allocs = hm_scope_alloc_new(128),
        .positions =
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
    if (m->positions.items != NULL) {
        free(m->positions.items);
    }
    HashMapCursorScopeAlloc it = hm_cursor_scope_alloc_new(&m->scope_allocs);
    ScopeAlloc *alloc = NULL;
    while ((alloc = hm_cursor_scope_alloc_next(&it))) {
        hm_scope_entry_free(&alloc->scope_ref->table);
    }
    hm_scope_alloc_free(&m->scope_allocs);
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

void set_node_pos(NodeMetadata *m, NodeID id, Position pos) {
    *AT(m->positions, id) = pos;
}

Position get_node_pos(NodeMetadata *m, NodeID id) {
    return *AT(m->positions, id);
}

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
    string id_ref = {.data = (char *)node.data, .len = sizeof(*node.data)};
    ScopeAllocResult res = hm_scope_alloc_ensure(&m->scope_allocs, id_ref);
    assert(res.inserted && "probably tried to insert a scope twice");

    Scope *scope = arena_alloc(&m->arena, sizeof(Scope), _Alignof(Scope));
    scope->self = node;
    scope->table = hm_scope_entry_new(128);
    scope->enclosing_scope.ptr = NULL;
    res.entry->scope_ref = scope;

    return scope;
}

Scope *scope_get(NodeMetadata *m, NodeID id) {
    string id_ref = {.data = (char *)&id, .len = sizeof(id)};
    if (hm_scope_alloc_contains(&m->scope_allocs, id_ref)) {
        ScopeAllocResult res = hm_scope_alloc_ensure(&m->scope_allocs, id_ref);
        assert(!res.inserted);
        return res.entry->scope_ref;
    }
    return NULL;
}

void scope_insert(NodeMetadata *m, Scope *scope, string symbol, AnyNode node) {
    ScopeEntryResult res = hm_scope_entry_ensure(&scope->table, symbol);
    if (res.inserted) {
        res.entry->node = node;
        res.entry->shadows = NULL;
        return;
    }

    // Allocate a new entry and copy the old head into it.
    ScopeEntry *old_entry =
        arena_alloc(&m->arena, sizeof(ScopeEntry), _Alignof(ScopeEntry));
    *old_entry = *res.entry;

    res.entry->node = node;
    res.entry->shadows = old_entry;
}

void ast_traverse_dfs(void *ctx, AnyNode root, NodeMetadata *m,
                      EnterExitVTable vtable) {
    if (vtable.enter && !has_error(m, root.data)) {
        vtable.enter(ctx, m, root);
    }
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
    if (vtable.exit && !has_error(m, root.data)) {
        vtable.exit(ctx, m, root);
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
    APPEND(&m->positions, (Position){0});
    *r = m->next_id++;
    return r;
}
