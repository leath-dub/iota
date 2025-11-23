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
    ParseCtx pc = new_parse_ctx(&code);

    assert(pc.meta.scope_allocs.base.entries.len != 0);

    SourceFile *root = parse_source_file(&pc);

    AnyNode any_root = MAKE_ANY(root);

    do_build_symbol_table(&pc.meta, any_root);
    do_resolve_names(&code, &pc.meta, any_root);

    // Scope *global_scope = scope_get(&pc.meta, root->id);
    // ScopeEntry *entry = hm_scope_entry_get(&global_scope->table,
    // ztos("Point")); assert(!entry->shadows); assert(entry->node.kind ==
    // NODE_STRUCT_DECL); StructDecl *decl = (StructDecl *)entry->node.data;
    // printf("It was inserted: %.*s\n", decl->ident.text.len,
    //        decl->ident.text.data);
    //
    // Scope *struct_scope = scope_get(&pc.meta, decl->id);
    // entry = hm_scope_entry_get(&struct_scope->table, ztos("x"));
    // assert(!entry->shadows);
    // assert(entry->node.kind == NODE_FIELD);
    // Field *field = (Field *)entry->node.data;
    // printf("Has field: %.*s\n", field->ident.text.len,
    // field->ident.text.data);

    // TreeDumpCtx dump_ctx = {
    //     .fs = stdout, .indent_level = 0, .indent_width = 2, .meta =
    //     &pc.meta};
    // dump_tree(&dump_ctx, root->id);
    //
    // dump_symbols(&code, &pc.meta);

    report_all_errors(code);

    source_code_free(&code);
    parse_ctx_free(&pc);
    free(source.data);
}
