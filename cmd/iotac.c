#include <assert.h>
#include <stdio.h>

#include "../ast/ast.h"
#include "../syn/syn.h"

typedef struct {
  FILE *fs;
  u32 indent_level;
  u8 indent_width;
  Node_Metadata *meta;
} Tree_Dump_Ctx;

void indent(Tree_Dump_Ctx *ctx) { ctx->indent_level++; }

void deindent(Tree_Dump_Ctx *ctx) {
  assert(ctx->indent_level != 0);
  ctx->indent_level--;
}

void tree_dump(Tree_Dump_Ctx *ctx, Node_ID id) {
  const Node_Children *children = get_node_children(ctx->meta, id);

  printf("%*s%s {", ctx->indent_level * ctx->indent_width, "",
         get_node_name(ctx->meta, id));
  if (children->len != 0) {
    printf("\n");
  }

  for (u32 i = 0; i < children->len; i++) {
    Node_Child child = children->items[i];
    indent(ctx);
    switch (child.t) {
      case CHILD_NODE:
        if (child.name.ok) {
          printf("%*s%s:\n", ctx->indent_level * ctx->indent_width, "",
                 child.name.value);
          indent(ctx);
        }
        tree_dump(ctx, child.id);
        if (child.name.ok) {
          deindent(ctx);
        }
        break;
      case CHILD_TOKEN:
        printf("%*s", ctx->indent_level * ctx->indent_width, "");
        if (child.name.ok) {
          printf("%s=", child.name.value);
        }
        printf("'%.*s'\n", child.token.text.len, child.token.text.data);
        break;
    }
    deindent(ctx);
  }
  if (children->len != 0) {
    printf("%*s", ctx->indent_level * ctx->indent_width, "");
  }
  printf("}\n");
}

int main(void) {
  string source = ztos("let _ = ((x + ((foo)[10])) - 12);");
  Source_Code code = new_source_code(ztos("<string>"), source);

  Parse_Context pc = new_parse_context(code);

  Source_File *sf = source_file(&pc);
  (void)sf;

  // Tree_Dump_Ctx dump_ctx = {
  //     .fs = stdout, .indent_level = 0, .indent_width = 2, .meta = &pc.meta};
  // tree_dump(&dump_ctx, sf->id);
  //
  // printf("---\n");
  //
  // Dump_Out out = new_dump_out();
  // DUMP(&out, &pc, sf, source_file);

  source_code_free(&code);
  parse_context_free(&pc);
}
