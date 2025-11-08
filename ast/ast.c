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
      .scopes = hm_scope_new(128),
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
  HashMapCursorScope it = hm_cursor_scope_new(&m->scopes);
  Scope *scope = NULL;
  while ((scope = hm_cursor_scope_next(&it))) {
    hm_scope_entry_free(&scope->table);
  }
  hm_scope_free(&m->scopes);
}

void add_node_flags(NodeMetadata *m, NodeID id, NodeFlag flags) {
  m->flags.items[id] |= flags;
}

bool has_error(NodeMetadata *m, void *node) {
  assert(node != NULL);
  NodeID *id = node;
  return m->flags.items[*id] & NFLAG_ERROR;
}

void add_child(NodeMetadata *m, NodeID id, NodeChild child) {
  NodeChildren *children = &m->tree.items[id];
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
  return AT(m->tree, id);
}

NodeChild *last_child(NodeMetadata *m, NodeID id) {
  NodeChildren *children = AT(m->tree, id);
  return AT(*children, children->len - 1);
}

void remove_child(NodeMetadata *m, NodeID from, NodeID child) {
  NodeChildren *children = get_node_children(m, from);
  int child_index = -1;
  for (u32 i = 0; i < children->len; i++) {
    NodeChild *check = &children->items[i];
    bool child_to_remove = check->t == CHILD_NODE && *check->node.data == child;
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

void ast_traverse_dfs(TraversalOrder order, void *ctx, AnyNode root,
                      NodeMetadata *m,
                      void (*visit_fun)(void *ctx, NodeMetadata *m,
                                        AnyNode node)) {
  const NodeChildren *children = get_node_children(m, *root.data);
  for (u32 i = 0; i < children->len; i++) {
    NodeChild child = children->items[i];
    switch (child.t) {
      case CHILD_NODE:
        if (order == PRE_ORDER) {
          visit_fun(ctx, m, child.node);
        }
        ast_traverse_dfs(order, ctx, child.node, m, visit_fun);
        if (order == POST_ORDER) {
          visit_fun(ctx, m, child.node);
        }
        break;
      default:
        break;
    }
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
  APPEND(&m->tree, (NodeChildren){.len = 0, .cap = 0, .items = NULL});
  *r = m->next_id++;
  return r;
}
