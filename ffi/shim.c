// For open_memstream
#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdio.h>

#include "../ast/ast.h"
#include "../syn/syn.h"

typedef NodeID *(*ParseFn)(ParseCtx *);
typedef NodeID *(*ParseFnDelim)(ParseCtx *, Toks toks);

char *ffi_parse(void *parse_fn, const char *srcz, bool delim) {
  string src = ztos((char *)srcz);
  SourceCode code = new_source_code(ztos("<string>"), src);
  ParseCtx ctx = new_parse_ctx(code);

  NodeID *root;
  if (delim) {
    ParseFnDelim parse_fn_delim = (ParseFnDelim)parse_fn;
    root = parse_fn_delim(&ctx, TOKS(T_EOF));
  } else {
    ParseFn parse_fn_nodelim = (ParseFn)parse_fn;
    root = parse_fn_nodelim(&ctx);
  }

  char *buf = NULL;
  usize len = 0;
  FILE *memfs = open_memstream(&buf, &len);

  TreeDumpCtx dump_ctx = {
      .fs = memfs, .indent_level = 0, .indent_width = 2, .meta = &ctx.meta};
  tree_dump(&dump_ctx, *root);

  fflush(memfs);
  fclose(memfs);

  source_code_free(&code);
  parse_ctx_free(&ctx);

  return buf;
}
