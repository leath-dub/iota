#include <assert.h>

#include "ast.h"

static void indent(Tree_Dump_Ctx *ctx) { ctx->indent_level++; }

static void deindent(Tree_Dump_Ctx *ctx) {
  assert(ctx->indent_level != 0);
  ctx->indent_level--;
}

void tree_dump(Tree_Dump_Ctx *ctx, Node_ID id) {
  const Node_Children *children = get_node_children(ctx->meta, id);

  fprintf(ctx->fs, "%*s%s {", ctx->indent_level * ctx->indent_width, "",
          get_node_name(ctx->meta, id));
  if (children->len != 0) {
    fprintf(ctx->fs, "\n");
  }

  for (u32 i = 0; i < children->len; i++) {
    Node_Child child = children->items[i];
    indent(ctx);
    switch (child.t) {
      case CHILD_NODE:
        if (child.name.ok) {
          fprintf(ctx->fs, "%*s%s:\n", ctx->indent_level * ctx->indent_width,
                  "", child.name.value);
          indent(ctx);
        }
        tree_dump(ctx, child.id);
        if (child.name.ok) {
          deindent(ctx);
        }
        break;
      case CHILD_TOKEN:
        fprintf(ctx->fs, "%*s", ctx->indent_level * ctx->indent_width, "");
        if (child.name.ok) {
          fprintf(ctx->fs, "%s=", child.name.value);
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
