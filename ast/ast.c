#include "ast.h"

#include <assert.h>

Node_Metadata new_node_metadata(void) {
  return (Node_Metadata) {
    .flags = {
      .len = 0,
      .cap = 0,
      .items = NULL,
    },
    .next_id = 0,
  };
}

void node_metadata_free(Node_Metadata *m) {
  if (m->flags.items != NULL) {
    free(m->flags.items);
  }
}

void *new_node(Node_Metadata *m, Arena *a, usize size, usize align) {
  Node_ID *r = arena_alloc(a, size, align);
  APPEND(&m->flags, 0);
  *r = m->next_id++;
  return r;
}

void add_node_flags(Node_Metadata *m, Node_ID id, Node_Flags flags) {
  m->flags.items[id] |= flags;
}

bool has_error(Node_Metadata *m, void *node) {
  assert(node != NULL);
  Node_ID *id = node;
  return m->flags.items[*id] & NFLAG_ERROR;
}
