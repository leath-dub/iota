#include "ast.h"

#include <assert.h>

NodeMetadata new_node_metadata(void) {
  return (NodeMetadata){
      .next_id = 0,
      .flags =
          {
              .len = 0,
              .cap = 0,
              .items = NULL,
          },
      .trees =
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
  };
}

void node_metadata_free(NodeMetadata *m) {
  if (m->flags.items != NULL) {
    free(m->flags.items);
  }
  if (m->trees.items != NULL) {
    for (u32 id = 0; id < m->trees.len; id++) {
      NodeChildren *children = get_node_children(m, id);
      free(children->items);
    }
    free(m->trees.items);
  }
  if (m->names.items != NULL) {
    free(m->names.items);
  }
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
  NodeChildren *children = &m->trees.items[id];
  APPEND(children, child);
}

NodeChild child_token(Tok token) {
  return (NodeChild){
      .t = CHILD_TOKEN,
      .token = token,
      .name.ok = false,
  };
}

NodeChild child_token_named(const char *name, Tok token) {
  return (NodeChild){
      .t = CHILD_TOKEN,
      .token = token,
      .name = {.value = name, .ok = true},
  };
}

NodeChild child_node(NodeID id) {
  return (NodeChild){
      .t = CHILD_NODE,
      .id = id,
      .name.ok = false,
  };
}

NodeChild child_node_named(const char *name, NodeID id) {
  return (NodeChild){
      .t = CHILD_NODE,
      .id = id,
      .name = {.value = name, .ok = true},
  };
}

typedef struct {
  usize size;
  usize align;
} Layout;

#define LAYOUT_OF(T) \
  (Layout) { .size = sizeof(T), .align = _Alignof(T) }

typedef struct {
  const char *name;
  Layout layout;
} NodeDescriptor;

static NodeDescriptor node_descriptors[];

const char *node_kind_name(NodeKind kind) {
  return node_descriptors[kind].name;
}

void *new_node(NodeMetadata *m, Arena *a, NodeKind kind) {
  Layout layout = node_descriptors[kind].layout;
  NodeID *r = arena_alloc(a, layout.size, layout.align);
  APPEND(&m->flags, 0);
  APPEND(&m->names, node_kind_name(kind));
  APPEND(&m->trees, (NodeChildren){.len = 0, .cap = 0, .items = NULL});
  *r = m->next_id++;
  return r;
}

const char *get_node_name(NodeMetadata *m, NodeID id) {
  return *AT(m->names, id);
}

NodeChildren *get_node_children(NodeMetadata *m, NodeID id) {
  return AT(m->trees, id);
}

NodeChild *last_child(NodeMetadata *m, NodeID id) {
  NodeChildren *children = AT(m->trees, id);
  return AT(*children, children->len - 1);
}

static NodeDescriptor node_descriptors[NODE_KIND_COUNT] = {
#define USE(TYPE, UPPER_NAME, REPR) \
  [NODE_##UPPER_NAME] = {REPR, LAYOUT_OF(TYPE)},
    EACH_NODE
#undef USE
};
