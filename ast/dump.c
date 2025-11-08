#include <assert.h>

#include "ast.h"

static void indent(TreeDumpCtx *ctx) { ctx->indent_level++; }

static void deindent(TreeDumpCtx *ctx) {
  assert(ctx->indent_level != 0);
  ctx->indent_level--;
}

void tree_dump(TreeDumpCtx *ctx, NodeID id) {
  const NodeChildren *children = get_node_children(ctx->meta, id);

  if (ctx->meta->flags.items[id] & NFLAG_ERROR) {
    fprintf(ctx->fs, "%*s%s(error!) {", ctx->indent_level * ctx->indent_width,
            "", get_node_name(ctx->meta, id));
  } else {
    fprintf(ctx->fs, "%*s%s {", ctx->indent_level * ctx->indent_width, "",
            get_node_name(ctx->meta, id));
  }
  if (children->len != 0) {
    fprintf(ctx->fs, "\n");
  }

  for (u32 i = 0; i < children->len; i++) {
    NodeChild child = children->items[i];
    indent(ctx);
    switch (child.t) {
      case CHILD_NODE:
        if (child.name.ptr) {
          fprintf(ctx->fs, "%*s%s:\n", ctx->indent_level * ctx->indent_width,
                  "", child.name.ptr);
          indent(ctx);
        }
        tree_dump(ctx, child.id);
        if (child.name.ptr) {
          deindent(ctx);
        }
        break;
      case CHILD_TOKEN:
        fprintf(ctx->fs, "%*s", ctx->indent_level * ctx->indent_width, "");
        if (child.name.ptr) {
          fprintf(ctx->fs, "%s=", child.name.ptr);
        }
        fprintf(ctx->fs, "'%.*s'\n", child.token.text.len,
                child.token.text.data);
        break;
    }
    deindent(ctx);
  }

  if (children->len != 0) {
    fprintf(ctx->fs, "%*s", ctx->indent_level * ctx->indent_width, "");
  }
  fprintf(ctx->fs, "}\n");
}
