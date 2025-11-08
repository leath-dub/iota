#include <assert.h>
#include <stdio.h>

#include "../ast/ast.h"
#include "../syn/syn.h"

#define ERROR_IMMEDIATE

static string read_file(char *path) {
  FILE *f = fopen(path, "r");

  if (f == NULL) {
    fprintf(stderr, "iotac: failed to open file: %s\n", path);
    exit(70);
  }

  if (fseek(f, 0, SEEK_END)) {
    fclose(f);
    fprintf(stderr, "iotac: failed to seek file: %s\n", path);
    exit(70);
  }

  ssize file_size = ftell(f);
  if (file_size == -1) {
    fclose(f);
    fprintf(stderr, "iotac: failed query stream associated with file: %s\n",
            path);
    exit(70);
  }

  char *buf = malloc(file_size + 1);
  usize left = file_size;
  rewind(f);

  int bytes_read = fread(buf, 1, left, f);
  if (bytes_read != file_size && ferror(f)) {
    free(buf);
    fclose(f);
    fprintf(stderr, "iotac: error reading contents of file: %s\n", path);
    exit(70);
  }

  fclose(f);

  buf[file_size] = '\0';
  return ztos(buf);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: iotac <path to file to compile>\n");
    return 2;
  }

  char *path = argv[1];
  string source = read_file(path);

  SourceCode code = new_source_code(ztos(path), source);
  ParseCtx pc = new_parse_ctx(&code);

  assert(pc.meta.scopes.base.entries.len != 0);

  SourceFile *root = parse_source_file(&pc);

  TreeDumpCtx dump_ctx = {
      .fs = stdout, .indent_level = 0, .indent_width = 2, .meta = &pc.meta};
  tree_dump(&dump_ctx, root->id);

  for (u32 i = 0; i < code.errors.len; i++) {
    report_error(code, code.errors.items[i]);
  }
  //
  // printf("---\n");
  //
  // Dump_Out out = new_dump_out();
  // DUMP(&out, &pc, sf, source_file);

  source_code_free(&code);
  parse_ctx_free(&pc);
}
