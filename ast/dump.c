#include <assert.h>

#include "ast.h"

static void indent(TreeDumpCtx *ctx) { ctx->indent_level++; }

static void deindent(TreeDumpCtx *ctx) {
    assert(ctx->indent_level != 0);
    ctx->indent_level--;
}

void dump_tree(TreeDumpCtx *ctx, AstNode *n) {
    Child *children = n->children;

    if (n->has_error) {
        fprintf(ctx->fs, "%*s%s(error!) {",
                ctx->indent_level * ctx->indent_width, "",
                node_kind_to_string(n->kind));
    } else {
        fprintf(ctx->fs, "%*s%s {", ctx->indent_level * ctx->indent_width, "",
                node_kind_to_string(n->kind));
    }
    if (da_length(children) != 0) {
        fprintf(ctx->fs, "\n");
    }

    for (u32 i = 0; i < da_length(children); i++) {
        Child child = children[i];
        indent(ctx);
        switch (child.t) {
            case CHILD_NODE:
                if (child.name.ptr) {
                    fprintf(ctx->fs, "%*s%s:\n",
                            ctx->indent_level * ctx->indent_width, "",
                            child.name.ptr);
                    indent(ctx);
                }
                dump_tree(ctx, child.node);
                if (child.name.ptr) {
                    deindent(ctx);
                }
                break;
            case CHILD_TOKEN:
                fprintf(ctx->fs, "%*s", ctx->indent_level * ctx->indent_width,
                        "");
                if (child.name.ptr) {
                    fprintf(ctx->fs, "%s=", child.name.ptr);
                }
                fprintf(ctx->fs, "'%.*s'\n", child.token.text.len,
                        child.token.text.data);
                break;
        }
        deindent(ctx);
    }

    if (da_length(children) != 0) {
        fprintf(ctx->fs, "%*s", ctx->indent_level * ctx->indent_width, "");
    }
    fprintf(ctx->fs, "}\n");
}

void dump_symbols(Ast *ast, const SourceCode *code) {
    MapCursor it = map_cursor_create(ast->tree_data.scope);
    while (map_cursor_next(&it)) {
        Scope **item = it.current;
        // NodeID id = *(NodeID *)map_key_of(m->scope_allocs, item);
        AstNode *self = (*item)->self;

        printf("scope: [node_ptr = %p]\n", (void *)self);
        printf("  node_type: <%s>\n", node_kind_to_string(self->kind));

        Scope *scope = *item;
        Scope *enclosing_scope = scope->enclosing_scope.ptr;
        if (enclosing_scope) {
            printf("  enclosing_scope: [node_ptr = %p]\n",
                   (void *)enclosing_scope->self);
        }

        bool first = true;
        MapCursor subit = map_cursor_create(scope->table);
        while (map_cursor_next(&subit)) {
            if (first) {
                printf("  + Entries:\n");
                first = false;
            }

            ScopeEntry **item = subit.current;
            bytes key = *(bytes *)map_key_of(scope->table, item);

            printf("  | \"%.*s\":\n", key.length, key.data);
            ScopeEntry *scope_entry_it = *item;
            while (scope_entry_it) {
                AstNode *entry = scope_entry_it->node;
                u32 offset = entry->offset;
                Position pos = line_and_column(code->lines, offset);
                printf("  |   <%s> @ %d:%d [node_ptr = %p]\n",
                       node_kind_to_string(entry->kind), pos.line, pos.column,
                       (void *)entry);
                scope_entry_it = scope_entry_it->shadows;
            }
        }
    }
}
