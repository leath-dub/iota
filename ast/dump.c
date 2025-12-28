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
                printf("  |   <%s> @ %d:%d [node_ptr = %p]\n",
                       node_kind_to_string(entry->kind), pos.line, pos.column,
                       (void *)entry);
                scope_entry_it = scope_entry_it->shadows;
            }
        }
    }
}

static void dump_type(Ast *ast, FILE *fs, TypeRepr repr) {
    switch (repr.t) {
        case STORAGE_U8:
            fprintf(fs, "u8");
            break;
        case STORAGE_S8:
            fprintf(fs, "s8");
            break;
        case STORAGE_U16:
            fprintf(fs, "u16");
            break;
        case STORAGE_S16:
            fprintf(fs, "s16");
            break;
        case STORAGE_U32:
            fprintf(fs, "u32");
            break;
        case STORAGE_S32:
            fprintf(fs, "s32");
            break;
        case STORAGE_U64:
            fprintf(fs, "u64");
            break;
        case STORAGE_S64:
            fprintf(fs, "s64");
            break;
        case STORAGE_F32:
            fprintf(fs, "f32");
            break;
        case STORAGE_F64:
            fprintf(fs, "f64");
            break;
        case STORAGE_UNIT:
            fprintf(fs, "unit");
            break;
        case STORAGE_STRING:
            fprintf(fs, "string");
            break;
        case STORAGE_PTR:
            fprintf(fs, "*");
            dump_type(ast, fs, *ast_type_repr(ast, repr.ptr_type.points_to));
            break;
        case STORAGE_TUPLE:
            fprintf(fs, "(");
            for (size_t i = 0; i < da_length(repr.tuple_type.types); i++) {
                dump_type(ast, fs,
                          *ast_type_repr(ast, repr.tuple_type.types[i]));
                fprintf(fs, ",");
            }
            fprintf(fs, ")");
            break;
        case STORAGE_STRUCT:
            fprintf(fs, "struct { ");
            for (size_t i = 0; i < da_length(repr.struct_type.fields); i++) {
                if (i != 0) {
                    fprintf(fs, " ");
                }
                TypeField f = repr.struct_type.fields[i];
                fprintf(fs, "%.*s: ", SPLAT(f.name));
                dump_type(ast, fs, *ast_type_repr(ast, f.type));
                fprintf(fs, ",");
            }
            fprintf(fs, " }");
            break;
        case STORAGE_TAGGED_UNION:
            fprintf(fs, "(");
            for (size_t i = 0; i < da_length(repr.tagged_union_type.types);
                 i++) {
                dump_type(ast, fs,
                          *ast_type_repr(ast, repr.tagged_union_type.types[i]));
                fprintf(fs, "|");
            }
            fprintf(fs, ")");
            break;
        case STORAGE_ENUM:
            fprintf(fs, "enum { ");
            for (size_t i = 0; i < da_length(repr.enum_type.alts); i++) {
                if (i != 0) {
                    fprintf(fs, " ");
                }
                string alt = repr.enum_type.alts[i];
                fprintf(fs, "%.*s", SPLAT(alt));
                fprintf(fs, ",");
            }
            fprintf(fs, " }");
            break;
        case STORAGE_ALIAS: {
            string name = repr.alias_type.type_decl->name->token.text;
            fprintf(fs, "%.*s", SPLAT(name));
            break;
        }
        case STORAGE_FN:
            TODO("function types");
            break;
    }
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
