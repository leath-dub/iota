// For open_memstream
#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdio.h>

#include "../ast/ast.h"
#include "../syn/syn.h"

typedef AstNode *(*ParseFn)(ParseCtx *);
typedef AstNode *(*ParseFnDelim)(ParseCtx *, Toks toks);

char *ffi_parse(void (*parse_fn)(void), const char *srcz, bool delim) {
    string src = ztos((char *)srcz);
    SourceCode code = new_source_code(ztos("<string>"), src);
    Arena arena = new_arena();
    Ast ast = ast_create(&arena);
    ParseCtx ctx = parse_ctx_create(&ast, &code);

    AstNode *root;
    if (delim) {
        ParseFnDelim parse_fn_delim = (ParseFnDelim)parse_fn;
        root = parse_fn_delim(&ctx, TOKS(T_EOF));
    } else {
        ParseFn parse_fn_nodelim = (ParseFn)parse_fn;
        root = parse_fn_nodelim(&ctx);
    }

    if (code.errors.len != 0) {
        report_all_errors(code);
        fflush(code.error_stream);
        return NULL;
    }

    char *buf = NULL;
    usize len = 0;
    FILE *memfs = open_memstream(&buf, &len);

    TreeDumpCtx dump_ctx = {
        .fs = memfs, .indent_level = 0, .indent_width = 2, .ast = &ast};
    dump_tree(&dump_ctx, root);

    fflush(memfs);
    fclose(memfs);

    parse_ctx_delete(&ctx);
    ast_delete(ast);
    arena_free(&arena);
    source_code_free(&code);

    return buf;
}
