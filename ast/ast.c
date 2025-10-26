#include "ast.h"

#include <assert.h>

Node_Metadata new_node_metadata(void) {
  return (Node_Metadata){
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

void node_metadata_free(Node_Metadata *m) {
  if (m->flags.items != NULL) {
    free(m->flags.items);
  }
  if (m->trees.items != NULL) {
    for (u32 id = 0; id < m->trees.len; id++) {
      Node_Children *children = get_node_children(m, id);
      free(children->items);
    }
    free(m->trees.items);
  }
  if (m->names.items != NULL) {
    free(m->names.items);
  }
}

void add_node_flags(Node_Metadata *m, Node_ID id, Node_Flag flags) {
  m->flags.items[id] |= flags;
}

bool has_error(Node_Metadata *m, void *node) {
  assert(node != NULL);
  Node_ID *id = node;
  return m->flags.items[*id] & NFLAG_ERROR;
}

void add_child(Node_Metadata *m, Node_ID id, Node_Child child) {
  Node_Children *children = &m->trees.items[id];
  APPEND(children, child);
}

Node_Child child_token(Tok token) {
  return (Node_Child){
      .t = CHILD_TOKEN,
      .token = token,
      .name.ok = false,
  };
}

Node_Child child_token_named(const char *name, Tok token) {
  return (Node_Child){
      .t = CHILD_TOKEN,
      .token = token,
      .name = {.value = name, .ok = true},
  };
}

Node_Child child_node(Node_ID id) {
  return (Node_Child){
      .t = CHILD_NODE,
      .id = id,
      .name.ok = false,
  };
}

Node_Child child_node_named(const char *name, Node_ID id) {
  return (Node_Child){
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
} Node_Descriptor;

static Node_Descriptor node_descriptors[];

const char *node_kind_name(Node_Kind kind) {
  return node_descriptors[kind].name;
}

void *new_node(Node_Metadata *m, Arena *a, Node_Kind kind) {
  Layout layout = node_descriptors[kind].layout;
  Node_ID *r = arena_alloc(a, layout.size, layout.align);
  APPEND(&m->flags, 0);
  APPEND(&m->names, node_kind_name(kind));
  APPEND(&m->trees, (Node_Children){.len = 0, .cap = 0, .items = NULL});
  *r = m->next_id++;
  return r;
}

const char *get_node_name(Node_Metadata *m, Node_ID id) {
  return *AT(m->names, id);
}

Node_Children *get_node_children(Node_Metadata *m, Node_ID id) {
  return AT(m->trees, id);
}

Node_Child *last_child(Node_Metadata *m, Node_ID id) {
  Node_Children *children = AT(m->trees, id);
  return AT(*children, children->len - 1);
}

static Node_Descriptor node_descriptors[NODE_KIND_COUNT] = {
#define USE(TYPE, UPPER_NAME, REPR) \
  [NODE_##UPPER_NAME] = {REPR, LAYOUT_OF(TYPE)},
    EACH_NODE
#undef USE
};
