#include <assert.h>

#include "ast.h"

static void indent(TreeDumpCtx *ctx) { ctx->indent_level++; }

static void deindent(TreeDumpCtx *ctx) {
    assert(ctx->indent_level != 0);
    ctx->indent_level--;
}

void dump_tree(TreeDumpCtx *ctx, NodeID id) {
    const NodeChildren *children = get_node_children(ctx->meta, id);

    if (ctx->meta->flags.items[id] & NFLAG_ERROR) {
        fprintf(ctx->fs, "%*s%s(error!) {",
                ctx->indent_level * ctx->indent_width, "",
                get_node_name(ctx->meta, id));
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
                    fprintf(ctx->fs, "%*s%s:\n",
                            ctx->indent_level * ctx->indent_width, "",
                            child.name.ptr);
                    indent(ctx);
                }
                dump_tree(ctx, *child.node.data);
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

    if (children->len != 0) {
        fprintf(ctx->fs, "%*s", ctx->indent_level * ctx->indent_width, "");
    }
    fprintf(ctx->fs, "}\n");
}

void dump_symbols(const SourceCode *code, NodeMetadata *m) {
    MapCursor it = map_cursor_create(m->scope_allocs);
    while (map_cursor_next(&it)) {
        Scope **item = it.current;
        NodeID id = *(NodeID *)map_key_of(m->scope_allocs, item);

        printf("scope: [node_id = %d]\n", id);
        printf("  node_type: <%s>\n", get_node_name(m, id));

        Scope *scope = *item;
        Scope *enclosing_scope = scope->enclosing_scope.ptr;
        if (enclosing_scope) {
            printf("  enclosing_scope: [node_id = %d]\n",
                   *(NodeID *)enclosing_scope->self.data);
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
                NodeID entry_id = *(NodeID *)scope_entry_it->node.data;
                u32 offset = get_node_offset(m, entry_id);
                Position pos = line_and_column(code->lines, offset);
                printf("  |   <%s> @ %d:%d [node_id = %d]\n",
                       node_kind_name(scope_entry_it->node.kind), pos.line,
                       pos.column, entry_id);
                scope_entry_it = scope_entry_it->shadows;
            }
        }
    }
}
