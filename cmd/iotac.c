#include <assert.h>
#include <stdio.h>

#include "../ast/ast.h"
#include "../sem/sem.h"
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

    Arena arena = new_arena();
    Ast ast = ast_create(&arena);
    ParseCtx parse_ctx = parse_ctx_create(&ast, &code);

    SourceFile *root = parse_source_file(&parse_ctx);
    (void)root;

    flush_errors(&code);

    do_build_symbol_table(&ast);
    do_resolve_names(&ast, &code);

    if (code.errors.len != 0) {
        flush_errors(&code);
        goto fini;
    }

    do_check_types(&ast, &code);

    // TreeDumpCtx dump_ctx = {
    //     .fs = stdout, .indent_level = 0, .indent_width = 2, .ast = &ast};
    // dump_tree(&dump_ctx, &root->head);

    flush_errors(&code);

    // dump_symbols(&ast, &code);
    dump_types(&ast);

fini:
    ast_delete(ast);
    arena_free(&arena);
    source_code_free(&code);
    free(source.data);
}
