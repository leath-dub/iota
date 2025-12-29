#include <assert.h>
#include <stdio.h>

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

        if (!scope->table) {
            continue;
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
                printf("  |   <%s> @ %d:%d [node_ptr = %p]",
                       node_kind_to_string(entry->kind), pos.line, pos.column,
                       (void *)entry);
                Scope *sub_scope = scope_entry_it->sub_scope.ptr;
                if (sub_scope != NULL) {
                    printf(" [scope_ptr = %p]", (void *)sub_scope);
                }
                printf("\n");
                scope_entry_it = scope_entry_it->shadows;
            }
        }
    }
}

void fmt_type(char *buf, size_t size, Ast *ast, TypeRepr repr) {
    switch (repr.t) {
        case STORAGE_U8:
            snprintf(buf, size, "u8");
            break;
        case STORAGE_S8:
            snprintf(buf, size, "s8");
            break;
        case STORAGE_U16:
            snprintf(buf, size, "u16");
            break;
        case STORAGE_S16:
            snprintf(buf, size, "s16");
            break;
        case STORAGE_U32:
            snprintf(buf, size, "u32");
            break;
        case STORAGE_S32:
            snprintf(buf, size, "s32");
            break;
        case STORAGE_U64:
            snprintf(buf, size, "u64");
            break;
        case STORAGE_S64:
            snprintf(buf, size, "s64");
            break;
        case STORAGE_F32:
            snprintf(buf, size, "f32");
            break;
        case STORAGE_F64:
            snprintf(buf, size, "f64");
            break;
        case STORAGE_UNIT:
            snprintf(buf, size, "unit");
            break;
        case STORAGE_STRING:
            snprintf(buf, size, "string");
            break;
        case STORAGE_PTR:
            snprintf(buf, size, "*");
            fmt_type(buf, size, ast,
                     *ast_type_repr(ast, repr.ptr_type.points_to));
            break;
        case STORAGE_TUPLE:
            snprintf(buf, size, "(");
            for (size_t i = 0; i < da_length(repr.tuple_type.types); i++) {
                fmt_type(buf, size, ast,
                         *ast_type_repr(ast, repr.tuple_type.types[i]));
                snprintf(buf, size, ",");
            }
            snprintf(buf, size, ")");
            break;
        case STORAGE_STRUCT:
            snprintf(buf, size, "struct { ");
            for (size_t i = 0; i < da_length(repr.struct_type.fields); i++) {
                if (i != 0) {
                    snprintf(buf, size, " ");
                }
                TypeField f = repr.struct_type.fields[i];
                snprintf(buf, size, "%.*s: ", SPLAT(f.name));
                fmt_type(buf, size, ast, *ast_type_repr(ast, f.type));
                snprintf(buf, size, ",");
            }
            snprintf(buf, size, " }");
            break;
        case STORAGE_TAGGED_UNION:
            snprintf(buf, size, "(");
            for (size_t i = 0; i < da_length(repr.tagged_union_type.types);
                 i++) {
                fmt_type(buf, size, ast,
                         *ast_type_repr(ast, repr.tagged_union_type.types[i]));
                snprintf(buf, size, "|");
            }
            snprintf(buf, size, ")");
            break;
        case STORAGE_ENUM:
            snprintf(buf, size, "enum { ");
            for (size_t i = 0; i < da_length(repr.enum_type.alts); i++) {
                if (i != 0) {
                    snprintf(buf, size, " ");
                }
                string alt = repr.enum_type.alts[i];
                snprintf(buf, size, "%.*s", SPLAT(alt));
                snprintf(buf, size, ",");
            }
            snprintf(buf, size, " }");
            break;
        case STORAGE_ALIAS: {
            string name = repr.alias_type.type_decl->name->token.text;
            snprintf(buf, size, "%.*s", SPLAT(name));
            break;
        }
        case STORAGE_FN:
            TODO("function types");
            break;
    }
}

#define FMT_BUF_SZ 4096

static void dump_type(Ast *ast, FILE *fs, TypeRepr repr) {
    static char buf[FMT_BUF_SZ];
    fmt_type(buf, FMT_BUF_SZ, ast, repr);
    fprintf(fs, "%s", buf);
}

void dump_types(Ast *ast) {
    TypeRepr *types = ast->tree_data.type_data;
    for (size_t i = 0; i < da_length(types); i++) {
        TypeRepr repr = types[i];
        fprintf(stderr, "[%ld] = ", i);
        dump_type(ast, stderr, repr);
        fprintf(stderr, "\n");
    }
}
