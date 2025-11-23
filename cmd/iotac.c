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

    assert(pc.meta.scopes.base.entries.len != 0);

    SourceFile *root = parse_source_file(&pc);

    AnyNode any_root = MAKE_ANY(root);

    do_build_symbol_table(&pc.meta, any_root);

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

    TreeDumpCtx dump_ctx = {
        .fs = stdout, .indent_level = 0, .indent_width = 2, .meta = &pc.meta};
    tree_dump(&dump_ctx, root->id);

    report_all_errors(code);

    // Debug symbol table
    HashMapCursorScopeAlloc it = hm_cursor_scope_alloc_new(&pc.meta.scopes);
    ScopeAlloc *alloc = NULL;
    while ((alloc = hm_cursor_scope_alloc_next(&it))) {
        Entry *entry = it.base.current_entry;
        assert(entry->key.len == sizeof(NodeID));

        NodeID id = *(NodeID *)it.base.current_entry->key.data;
        printf("scope [node_id = %d]:\n", id);
        printf("  node_type: %s\n", get_node_name(&pc.meta, id));

        Scope *enclosing_scope = alloc->scope_ref->enclosing_scope.ptr;
        if (enclosing_scope) {
            printf("  enclosing_scope: [node_id = %d]\n",
                   *(NodeID *)enclosing_scope->self.data);
        }

        HashMapCursorScopeEntry subit =
            hm_cursor_scope_entry_new(&alloc->scope_ref->table);
        ScopeEntry *scope_entry = NULL;
        while ((scope_entry = hm_cursor_scope_entry_next(&subit))) {
            string key = subit.base.current_entry->key;
            printf("    entry (%.*s):\n", key.len, key.data);
            ScopeEntry *scope_entry_it = scope_entry;
            while (scope_entry_it) {
                printf("      %s\n", node_kind_name(scope_entry_it->node.kind));
                scope_entry_it = scope_entry_it->shadows;
            }
        }
    }

    source_code_free(&code);
    parse_ctx_free(&pc);
    free(source.data);
}
