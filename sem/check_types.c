#include <assert.h>
#include <stdarg.h>

#include "sem.h"

// Canonicalizes all types apart from scoped idents which need to be resolved
// later as they could be referenced after definition
static DfsCtrl type_resolution_enter(void *_ctx, AstNode *node);
static DfsCtrl type_resolution_exit(void *_ctx, AstNode *node);

// Canonicalize scoped ident (references to type aliases)

static DfsCtrl check_types_enter(void *_ctx, AstNode *node);
static DfsCtrl check_types_exit(void *_ctx, AstNode *node);

typedef struct {
    Ast *ast;
    TypeId *type_hint;
    SourceCode *code;
} TypeCheckCtx;

DA_DEFINE(type_hint, TypeId)

typedef struct {
    Ast *ast;
    Type **current_type;
    TypeId *normalized_type;
    TypeId *type_memo;
    SourceCode *code;
} TypeResCtx;

static void seed_builtin_type(TypeResCtx *ctx, TokKind kind);

typedef u64 TypeHash;

DA_DEFINE(current_type, Type *)
MAP_DEFINE(normalized_type, Type *, TypeId)
MAP_DEFINE(type_memo, TypeRepr, TypeId)

static TypeHash type_hash_generic(void *repr);
static bool type_is_identical_generic(void *ta, void *tb);

void do_check_types(Ast *ast, SourceCode *code) {
    TypeCheckCtx ctx = {
        .ast = ast,
        .code = code,
        .type_hint = type_hint_create(),
    };

    {
        TypeResCtx ctx = {
            .ast = ast,
            .current_type = current_type_create(),
            .normalized_type = normalized_type_create(128),
            .type_memo =
                type_memo_create_with_cfg(128,
                                          (MapConfig){
                                              .cmp = type_is_identical_generic,
                                              .hash = type_hash_generic,
                                          }),
            .code = code,
        };

        seed_builtin_type(&ctx, T_U8);
        seed_builtin_type(&ctx, T_S8);
        seed_builtin_type(&ctx, T_U16);
        seed_builtin_type(&ctx, T_S16);
        seed_builtin_type(&ctx, T_U32);
        seed_builtin_type(&ctx, T_S32);
        seed_builtin_type(&ctx, T_U64);
        seed_builtin_type(&ctx, T_S64);
        seed_builtin_type(&ctx, T_F32);
        seed_builtin_type(&ctx, T_F64);
        seed_builtin_type(&ctx, T_UNIT);
        seed_builtin_type(&ctx, T_STRING);
        seed_builtin_type(&ctx, T_BOOL);

        ast_traverse_dfs(&ctx, ast,
                         (EnterExitVTable){
                             .enter = type_resolution_enter,
                             .exit = type_resolution_exit,
                         });

        MapCursor it = map_cursor_create(ctx.normalized_type);
        while (map_cursor_next(&it)) {
            Type *type = *(Type **)map_key_of(ctx.normalized_type, it.current);
            TypeId id = *(TypeId *)it.current;
            type_source_put(&ast->tree_data.type_source, id, type);
        }

        current_type_delete(ctx.current_type);
        normalized_type_delete(ctx.normalized_type);
        type_memo_delete(ctx.type_memo);
    }

    ast_traverse_dfs(&ctx, ast,
                     (EnterExitVTable){
                         .enter = check_types_enter,
                         .exit = check_types_exit,
                     });

    type_hint_delete(ctx.type_hint);
}

#define FNV1A_64_OFFSET_BASIS (uint64_t)0xcbf29ce484222325
#define FNV1A_64_PRIME (uint64_t)0x100000001b3

static uint64_t fnv1a(const uint8_t *key_data, size_t key_size) {
    uint64_t hash = FNV1A_64_OFFSET_BASIS;
    for (uint32_t i = 0; i < key_size; i++) {
        hash ^= key_data[i];
        hash *= FNV1A_64_PRIME;
    }
    return hash;
}

#define GOLDEN_RATIO 0x9e3779b97f4a7c15

static inline TypeHash hash_mix(TypeHash a, TypeHash b) {
    return a ^ (b + GOLDEN_RATIO + (a << 6) + (a >> 2));
}

static TypeHash type_hash(TypeRepr repr) {
    uint64_t h = repr.t;
    switch (repr.t) {
        case STORAGE_U8:
        case STORAGE_S8:
        case STORAGE_U16:
        case STORAGE_S16:
        case STORAGE_U32:
        case STORAGE_S32:
        case STORAGE_U64:
        case STORAGE_S64:
        case STORAGE_F32:
        case STORAGE_F64:
        case STORAGE_UNIT:
        case STORAGE_STRING:
        case STORAGE_BOOL:
            return h;
        case STORAGE_PTR:
            return hash_mix(h, repr.ptr_type.points_to);
        case STORAGE_TUPLE: {
            TypeTuple *tup = &repr.tuple_type;
            for (size_t i = 0; i < da_length(tup->types); i++) {
                h = hash_mix(h, tup->types[i]);
            }
            return h;
        }
        case STORAGE_STRUCT: {
            TypeStruct *st = &repr.struct_type;
            for (size_t i = 0; i < da_length(st->fields); i++) {
                TypeField f = st->fields[i];
                h = hash_mix(h, fnv1a((uint8_t *)f.name.data, f.name.len));
                h = hash_mix(h, f.type);
            }
            return h;
        }
        case STORAGE_TAGGED_UNION: {
            TypeTaggedUnion *tu = &repr.tagged_union_type;
            for (size_t i = 0; i < da_length(tu->types); i++) {
                h = hash_mix(h, tu->types[i]);
            }
            return h;
        }
        case STORAGE_ENUM: {
            TypeEnum *en = &repr.enum_type;
            for (size_t i = 0; i < da_length(en->alts); i++) {
                string s = en->alts[i];
                h = hash_mix(h, fnv1a((uint8_t *)s.data, s.len));
            }
            return h;
        }
        case STORAGE_ALIAS: {
            TypeAlias *alias = &repr.alias_type;
            return (TypeHash)alias->type_decl;
        }
        case STORAGE_FN:
            TODO("function types");
            return 0;
    }
    assert(false && "unreachable");
}

static TypeHash type_hash_generic(void *repr) {
    return type_hash(*(TypeRepr *)repr);
}

static TypeId type_id_of(TypeResCtx *ctx, TypeRepr repr) {
    bool inserted = false;
    TypeId *res = type_memo_get_or_insert(&ctx->type_memo, repr, &inserted);
    if (!inserted) {
        return *res;
    }

    TypeId new_id = ast_type_create(ctx->ast);
    *res = new_id;
    *ast_type_repr(ctx->ast, new_id) = repr;

    return new_id;
}

static void type_res_enum_type(TypeResCtx *ctx, EnumType *enum_type);
static void type_res_struct_type(TypeResCtx *ctx, StructType *struct_type);
static void type_res_tuple_type(TypeResCtx *ctx, TupleType *tuple_type);
static void type_res_tagged_union_type(TypeResCtx *ctx,
                                       TaggedUnionType *tu_type);
static void type_res_fn_type(TypeResCtx *ctx, FnType *fn_type);
static void type_res_builtin_type(TypeResCtx *ctx, BuiltinType *builtin_type);
static void type_res_ptr_type(TypeResCtx *ctx, PtrType *ptr_type);
static void type_res_scoped_ident(TypeResCtx *ctx, ScopedIdent *scoped_ident);

static DfsCtrl type_resolution_enter(void *_ctx, AstNode *node) {
    TypeResCtx *ctx = (TypeResCtx *)_ctx;
    if (node->kind == NODE_TYPE) {
        current_type_append(&ctx->current_type, (Type *)node);
    }
    return DFS_CTRL_KEEP_GOING;
}

static DfsCtrl type_resolution_exit(void *_ctx, AstNode *node) {
    TypeResCtx *ctx = (TypeResCtx *)_ctx;
    switch (node->kind) {
        case NODE_TYPE_DECL: {
            TypeDecl *td = (TypeDecl *)node;

            TypeId *aliases =
                normalized_type_get(ctx->normalized_type, td->type);
            assert(aliases);

            // Create an alias which points to the right-hand side type
            TypeId alias = type_id_of(ctx, (TypeRepr){
                                               .t = STORAGE_ALIAS,
                                               .alias_type =
                                                   (TypeAlias){
                                                       .type_decl = td,
                                                       .aliases = *aliases,
                                                   },
                                           });

            TypeRepr *tr = ast_type_repr(ctx->ast, alias);
            if (tr->alias_type.aliases == INVALID_TYPE) {
                // A reference triggered canonicalisation before the type
                // declaration. Just set the actual value of the aliases field
                tr->alias_type.aliases = *aliases;
            }

            ast_type_set(ctx->ast, node, alias);
            break;
        }
        case NODE_VAR_DECL: {
            VarDecl *vd = (VarDecl *)node;
            if (vd->binding->type.ptr != NULL) {
                TypeId *vt = normalized_type_get(ctx->normalized_type,
                                                 vd->binding->type.ptr);
                assert(vt);
                ast_type_set(ctx->ast, &vd->head, *vt);
            }
            break;
        }
        case NODE_FN_PARAM: {
            FnParam *fp = (FnParam *)node;
            TypeId *ft =
                normalized_type_get(ctx->normalized_type, fp->binding->type);
            assert(ft);
            ast_type_set(ctx->ast, &fp->head, *ft);
            break;
        }
        // TODO function types themselves
        case NODE_ENUM_TYPE:
            type_res_enum_type(ctx, (EnumType *)node);
            break;
        case NODE_STRUCT_TYPE:
            type_res_struct_type(ctx, (StructType *)node);
            break;
        case NODE_TUPLE_TYPE:
            type_res_tuple_type(ctx, (TupleType *)node);
            break;
        case NODE_TAGGED_UNION_TYPE:
            type_res_tagged_union_type(ctx, (TaggedUnionType *)node);
            break;
        case NODE_FN_TYPE:
            type_res_fn_type(ctx, (FnType *)node);
            break;
        case NODE_BUILTIN_TYPE:
            type_res_builtin_type(ctx, (BuiltinType *)node);
            break;
        case NODE_PTR_TYPE:
            type_res_ptr_type(ctx, (PtrType *)node);
            break;
        case NODE_TYPE: {
            Type *type = (Type *)node;
            if (type->t == TYPE_SCOPED_IDENT) {
                type_res_scoped_ident(ctx, type->scoped_ident);
            }
            current_type_pop(ctx->current_type);
            break;
        }
        default:
            break;
    }
    return DFS_CTRL_KEEP_GOING;
}

static void res_type(TypeResCtx *ctx, TypeId ty, AstNode *res_from) {
    Type **top =
        current_type_at(ctx->current_type, da_length(ctx->current_type) - 1);
    normalized_type_put(&ctx->normalized_type, *top, ty);
    ast_type_set(ctx->ast, res_from, ty);
}

static void type_res_enum_type(TypeResCtx *ctx, EnumType *enum_type) {
    AstNode *hd = &enum_type->alts->head;
    TypeRepr ty = {0};

    ty.t = STORAGE_ENUM;
    TypeEnum *te = &ty.enum_type;

    string **names = &te->alts;
    *names = type_enum_alts_create();

    for (u32 i = 0; i < da_length(hd->children); i++) {
        Ident *alt = child_ident_at(hd, i);
        type_enum_alts_append(names, alt->token.text);
    }

    type_enum_alts_shrink(names);
    arena_own(ctx->ast->arena, da_header_get(*names),
              da_length(*names) * sizeof(**names));

    res_type(ctx, type_id_of(ctx, ty), &enum_type->head);
}

static void type_res_struct_type(TypeResCtx *ctx, StructType *struct_type) {
    AstNode *hd = &struct_type->fields->head;
    TypeRepr ty = {0};

    ty.t = STORAGE_STRUCT;
    TypeStruct *ts = &ty.struct_type;

    TypeField **fields = &ts->fields;
    *fields = type_fields_create();

    for (u32 i = 0; i < da_length(hd->children); i++) {
        StructField *f = child_struct_field_at(hd, i);
        TypeId *ft =
            normalized_type_get(ctx->normalized_type, f->binding->type);
        assert(ft && "subtree type should be resolved");
        type_fields_append(fields, (TypeField){
                                       .name = f->binding->name->token.text,
                                       .type = *ft,
                                   });
    }

    type_fields_shrink(fields);
    arena_own(ctx->ast->arena, da_header_get(*fields),
              da_length(*fields) * sizeof(**fields));

    res_type(ctx, type_id_of(ctx, ty), &struct_type->head);
}

static void type_res_tuple_type(TypeResCtx *ctx, TupleType *tuple_type) {
    AstNode *hd = &tuple_type->types->head;
    TypeRepr ty = {0};

    ty.t = STORAGE_TUPLE;
    TypeTuple *tt = &ty.tuple_type;

    TypeId **types = &tt->types;
    *types = types_create();

    for (u32 i = 0; i < da_length(hd->children); i++) {
        Type *f = child_type_at(hd, i);
        TypeId *ft = normalized_type_get(ctx->normalized_type, f);
        assert(ft && "subtree type should be resolved");
        types_append(types, *ft);
    }

    types_shrink(types);
    arena_own(ctx->ast->arena, da_header_get(*types),
              da_length(*types) * sizeof(**types));

    res_type(ctx, type_id_of(ctx, ty), &tuple_type->head);
}

static bool types_contains(TypeId *types, TypeId ty) {
    for (size_t i = 0; i < da_length(types); i++) {
        if (types[i] == ty) {
            return true;
        }
    }
    return false;
}

static void type_res_tagged_union_type(TypeResCtx *ctx,
                                       TaggedUnionType *tu_type) {
    AstNode *hd = &tu_type->alts->head;
    TypeRepr ty = {0};

    ty.t = STORAGE_TAGGED_UNION;
    TypeTaggedUnion *tu = &ty.tagged_union_type;

    TypeId **types = &tu->types;
    *types = types_create();

    for (u32 i = 0; i < da_length(hd->children); i++) {
        UnionAlt *alt = child_union_alt_at(hd, i);
        switch (alt->t) {
            case UNION_ALT_TYPE: {
                // For now disallow nested tagged union types as the semantics
                // should be equivalent to not nesting, so no reason to allow.
                // (wait until a specific case is found where it could covey
                //  intent better than just writing the flat list of
                //  alternatives)
                if (alt->type->t == TYPE_TAGGED_UNION) {
                    sem_raisef(ctx->ast, ctx->code, alt->head.offset,
                               "declaration error: cannot nest anonymous "
                               "tagged union types");
                }
                TypeId *ft =
                    normalized_type_get(ctx->normalized_type, alt->type);
                assert(ft && "subtree type should be resolved");
                if (types_contains(*types, *ft)) {
                    sem_raisef(
                        ctx->ast, ctx->code, alt->type->head.offset,
                        "declaration error: duplicate type in tagged union");
                }
                types_append(types, *ft);
                break;
            }
            case UNION_ALT_INLINE_DECL: {
                // Handle inline type decl (e.g. (u32 | string | type
                // I_am_inline = u32))
                types_append(types,
                             ast_type_get(ctx->ast, &alt->inline_decl->head));
                break;
            }
        }
    }

    types_shrink(types);
    arena_own(ctx->ast->arena, da_header_get(*types),
              da_length(*types) * sizeof(**types));

    res_type(ctx, type_id_of(ctx, ty), &tu_type->head);
}

static void type_res_fn_type(TypeResCtx *ctx, FnType *fn_type) {
    (void)ctx;
    (void)fn_type;
    TODO("function types");
}

static TypeRepr get_builtin_type_repr(TokKind kind) {
    TypeRepr ty = {0};
    switch (kind) {
        case T_U8:
            ty.t = STORAGE_U8;
            break;
        case T_S8:
            ty.t = STORAGE_S8;
            break;
        case T_U16:
            ty.t = STORAGE_U16;
            break;
        case T_S16:
            ty.t = STORAGE_S16;
            break;
        case T_U32:
            ty.t = STORAGE_U32;
            break;
        case T_S32:
            ty.t = STORAGE_S32;
            break;
        case T_U64:
            ty.t = STORAGE_U64;
            break;
        case T_S64:
            ty.t = STORAGE_S64;
            break;
        case T_F32:
            ty.t = STORAGE_F32;
            break;
        case T_F64:
            ty.t = STORAGE_F64;
            break;
        case T_UNIT:
            ty.t = STORAGE_UNIT;
            break;
        case T_STRING:
            ty.t = STORAGE_STRING;
            break;
        case T_BOOL:
            ty.t = STORAGE_BOOL;
            break;
        default:
            assert(false && "not a known builtin type");
    }
    return ty;
}

static void seed_builtin_type(TypeResCtx *ctx, TokKind kind) {
    ctx->ast->tree_data.builtin_type[kind] =
        type_id_of(ctx, get_builtin_type_repr(kind));
}

static void type_res_builtin_type(TypeResCtx *ctx, BuiltinType *builtin_type) {
    TokKind tk = builtin_type->token.t;
    res_type(ctx, type_id_of(ctx, get_builtin_type_repr(tk)),
             &builtin_type->head);
}

static void type_res_ptr_type(TypeResCtx *ctx, PtrType *ptr_type) {
    TypeId *points_to =
        normalized_type_get(ctx->normalized_type, ptr_type->points_to);
    assert(points_to && "subtree type should be resolved");

    TypeRepr ty = {0};

    ty.t = STORAGE_PTR;
    ty.ptr_type = (TypePtr){
        .points_to = *points_to,
    };

    res_type(ctx, type_id_of(ctx, ty), &ptr_type->head);
}

static void type_res_scoped_ident(TypeResCtx *ctx, ScopedIdent *scoped_ident) {
    // We cannot always assume the cannonical version of the type a reference
    // is pointing to has been constructed yet (consider references before
    // declaration or self referential types). To combat this, during this
    // pass we just set what it refers to as INVALID_TYPE.

    TypeRepr ty = {0};

    AstNode *res = ast_resolves_to_get_scoped(ctx->ast, scoped_ident);
    assert(res);
    if (res->kind != NODE_TYPE_DECL) {
        sem_raisef(ctx->ast, ctx->code, scoped_ident->head.offset,
                   "symbol mismatch: name must resolve to a type");
        return;
    }

    ty.t = STORAGE_ALIAS;
    ty.alias_type = (TypeAlias){
        .type_decl = (TypeDecl *)res,
        .aliases = INVALID_TYPE,
    };

    res_type(ctx, type_id_of(ctx, ty), &scoped_ident->head);
}

static bool type_is_identical(TypeRepr *ta, TypeRepr *tb) {
    if (ta->t != tb->t) {
        return false;
    }

    switch (ta->t) {
        case STORAGE_U8:
        case STORAGE_S8:
        case STORAGE_U16:
        case STORAGE_S16:
        case STORAGE_U32:
        case STORAGE_S32:
        case STORAGE_U64:
        case STORAGE_S64:
        case STORAGE_F32:
        case STORAGE_F64:
        case STORAGE_UNIT:
        case STORAGE_STRING:
        case STORAGE_BOOL:
            return true;
        case STORAGE_PTR: {
            TypePtr *pa = &ta->ptr_type;
            TypePtr *pb = &tb->ptr_type;
            return pa->points_to == pb->points_to;
        }
        case STORAGE_TUPLE: {
            TypeTuple *tup_a = &ta->tuple_type;
            TypeTuple *tup_b = &tb->tuple_type;
            if (da_length(tup_a->types) != da_length(tup_b->types)) {
                return false;
            }
            for (size_t i = 0; i < da_length(tup_a->types); i++) {
                if (tup_a->types[i] != tup_b->types[i]) {
                    return false;
                }
            }
            return true;
        }
        case STORAGE_STRUCT: {
            TypeStruct *st_a = &ta->struct_type;
            TypeStruct *st_b = &tb->struct_type;
            if (da_length(st_a->fields) != da_length(st_b->fields)) {
                return false;
            }
            for (size_t i = 0; i < da_length(st_a->fields); i++) {
                TypeField fa = st_a->fields[i];
                TypeField fb = st_b->fields[i];
                if (!streql(fa.name, fb.name)) {
                    return false;
                }
                if (fa.type != fb.type) {
                    return false;
                }
            }
            return true;
        }
        case STORAGE_TAGGED_UNION: {
            TypeTaggedUnion *tu_a = &ta->tagged_union_type;
            TypeTaggedUnion *tu_b = &tb->tagged_union_type;
            if (da_length(tu_a->types) != da_length(tu_b->types)) {
                return false;
            }
            for (size_t i = 0; i < da_length(tu_a->types); i++) {
                if (tu_a->types[i] != tu_b->types[i]) {
                    return false;
                }
            }
            return true;
        }
        case STORAGE_ENUM: {
            TypeEnum *en_a = &ta->enum_type;
            TypeEnum *en_b = &tb->enum_type;
            if (da_length(en_a->alts) != da_length(en_b->alts)) {
                return false;
            }
            for (size_t i = 0; i < da_length(en_a->alts); i++) {
                string a = en_a->alts[i];
                string b = en_b->alts[i];
                if (!streql(a, b)) {
                    return false;
                }
            }
            return true;
        }
        case STORAGE_ALIAS: {
            TypeAlias *al_a = &ta->alias_type;
            TypeAlias *al_b = &tb->alias_type;
            return al_a->type_decl == al_b->type_decl;
        }
        case STORAGE_FN: {
            TODO("function types");
            return false;
        }
    }
    assert(false && "unreachable");
}

static bool type_is_identical_generic(void *ta, void *tb) {
    return type_is_identical((TypeRepr *)ta, (TypeRepr *)tb);
}

static void var_decl_infer_enter(TypeCheckCtx *ctx, VarDecl *vd);
static void var_decl_infer_exit(TypeCheckCtx *ctx, VarDecl *vd);

static DfsCtrl check_types_enter(void *_ctx, AstNode *node) {
    (void)_ctx;
    (void)node;
    // TODO: add "inferred_scope" context on entry of certain nodes:
    //
    // e.g. - function call argument
    //      - variable declaration
    //      - return statement
    //      - case statement
    TypeCheckCtx *ctx = _ctx;
    switch (node->kind) {
        case NODE_VAR_DECL:
            var_decl_infer_enter(ctx, (VarDecl *)node);
            break;
        default:
            break;
    }
    return DFS_CTRL_KEEP_GOING;
}

static void push_type_hint(TypeCheckCtx *ctx, TypeId hint) {
    assert(hint != INVALID_TYPE);
    type_hint_append(&ctx->type_hint, hint);
}

static void pop_type_hint(TypeCheckCtx *ctx) {
    assert(da_length(ctx->type_hint) != 0);
    type_hint_remove(ctx->type_hint, da_length(ctx->type_hint) - 1);
}

static TypeId current_type_hint(TypeCheckCtx *ctx) {
    assert(da_length(ctx->type_hint) != 0);
    return ctx->type_hint[da_length(ctx->type_hint) - 1];
}

static void var_decl_infer_enter(TypeCheckCtx *ctx, VarDecl *vd) {
    if (vd->binding->type.ptr != NULL) {
        push_type_hint(ctx, ast_type_get(ctx->ast, &vd->head));
    }
}

static void var_decl_infer_exit(TypeCheckCtx *ctx, VarDecl *vd) {
    if (vd->binding->type.ptr != NULL) {
        pop_type_hint(ctx);
    }
}

static void check_atom(TypeCheckCtx *ctx, Atom *atom);
// static void check_postfix_expr(TypeCheckCtx *ctx, PostfixExpr *postfix_expr);
// static void check_call(TypeCheckCtx *ctx, Call *call);
static void check_field_access(TypeCheckCtx *ctx, FieldAccess *field_access);
// static void check_coll_access(TypeCheckCtx *ctx, CollAccess *coll_access);
static void check_unary_expr(TypeCheckCtx *ctx, UnaryExpr *unary_expr);
static void check_bin_expr(TypeCheckCtx *ctx, BinExpr *bin_expr);
static void check_var_decl(TypeCheckCtx *ctx, VarDecl *var_decl);
static void check_scoped_ident(TypeCheckCtx *ctx, ScopedIdent *scoped_ident);

static DfsCtrl check_types_exit(void *_ctx, AstNode *node) {
    TypeCheckCtx *ctx = _ctx;
    switch (node->kind) {
        case NODE_ATOM:
            check_atom(ctx, (Atom *)node);
            break;
        case NODE_BIN_EXPR:
            check_bin_expr(ctx, (BinExpr *)node);
            break;
        case NODE_UNARY_EXPR:
            check_unary_expr(ctx, (UnaryExpr *)node);
            break;
        case NODE_VAR_DECL:
            check_var_decl(ctx, (VarDecl *)node);
            var_decl_infer_exit(ctx, (VarDecl *)node);
            break;
        case NODE_FIELD_ACCESS:
            check_field_access(ctx, (FieldAccess *)node);
            break;
        case NODE_SCOPED_IDENT:
            check_scoped_ident(ctx, (ScopedIdent *)node);
            break;
        default:
            break;
    }
    return DFS_CTRL_KEEP_GOING;
}

static bool type_is_convertible(TypeCheckCtx *ctx, TypeId to_ty,
                                TypeId from_ty) {
    (void)ctx;
    // TODO: add other implicit conversion logic for tagged union types
    return to_ty == from_ty;
}

static bool type_is_unsigned_integral(const TypeRepr *repr) {
    switch (repr->t) {
        case STORAGE_U8:
        case STORAGE_U16:
        case STORAGE_U32:
        case STORAGE_U64:
            return true;
        default:
            break;
    }
    return false;
}

static bool type_is_signed_integral(const TypeRepr *repr) {
    switch (repr->t) {
        case STORAGE_S8:
        case STORAGE_S16:
        case STORAGE_S32:
        case STORAGE_S64:
            return true;
        default:
            break;
    }
    return false;
}

static bool type_is_integral(const TypeRepr *repr) {
    return type_is_signed_integral(repr) || type_is_unsigned_integral(repr);
}

static bool type_is_floating_point(const TypeRepr *repr) {
    switch (repr->t) {
        case STORAGE_F32:
        case STORAGE_F64:
            return true;
        default:
            break;
    }
    return false;
}

static bool type_is_arithmetic(const TypeRepr *repr) {
    return type_is_integral(repr) || type_is_floating_point(repr);
}

static TypeId type_of_expr(TypeCheckCtx *ctx, Expr *expr) {
    Child *children = expr->head.children;
    assert(da_length(children) != 0);
    Child *child = children_at(children, 0);
    assert(child->t == CHILD_NODE);
    return ast_type_get(ctx->ast, child->node);
}

static TypeId get_builtin_type(TypeCheckCtx *ctx, TokKind kind) {
    return ctx->ast->tree_data.builtin_type[kind];
}

static AstNode *match_parent_chain(AstNode *node, size_t count, ...) {
    va_list args;
    va_start(args, count);

    AstNode *it = node->parent.ptr;
    AstNode *match = it;

    for (size_t i = 0; i < count; i++) {
        NodeKind kind = va_arg(args, NodeKind);
        if (!it || it->kind != kind) {
            return NULL;
        }
        match = it;
        it = it->parent.ptr;
    }

    return match;
}

static TypeId type_of_imperative_ref(Ast *ast, AstNode *res) {
    if (res->kind == NODE_IDENT) {
        AstNode *enum_type =
            match_parent_chain(res, 2, NODE_IDENTS, NODE_ENUM_TYPE);
        if (enum_type) {
            AstNode *type_decl =
                match_parent_chain(enum_type, 3, NODE_TYPE, NODE_TYPE_DECL);
            if (type_decl) {
                return ast_type_get(ast, type_decl);
            }
            return ast_type_get(ast, enum_type);
        }
    }
    return ast_type_get(ast, res);
}

static void check_atom(TypeCheckCtx *ctx, Atom *atom) {
    switch (atom->t) {
        case ATOM_TOKEN:
            switch (atom->token.t) {
                case T_NUM:
                    ast_type_set(ctx->ast, &atom->head,
                                 get_builtin_type(ctx, T_S32));
                    break;
                case T_STRING:
                    ast_type_set(ctx->ast, &atom->head,
                                 get_builtin_type(ctx, T_STRING));
                    break;
                default:
                    assert(false && "unhandled literal token");
                    break;
            }
            break;
        case ATOM_SCOPED_IDENT: {
            AstNode *res =
                ast_resolves_to_get_scoped(ctx->ast, atom->scoped_ident);
            assert(res);
            TypeId ref_ty = type_of_imperative_ref(ctx->ast, res);
            assert(ref_ty != INVALID_TYPE);
            ast_type_set(ctx->ast, &atom->head, ref_ty);
            break;
        }
        case ATOM_BUILTIN_TYPE:
            if (atom->head.parent.ptr != NULL) {
                if (atom->head.parent.ptr->kind != NODE_CALL) {
                    sem_raisef(ctx->ast, ctx->code, atom->head.offset,
                               "expression error: builtin type can only appear "
                               "as call expression");
                }
            }
            break;
    }
}

static void check_unary_expr(TypeCheckCtx *ctx, UnaryExpr *unary_expr) {
    TypeId sub_ty = type_of_expr(ctx, unary_expr->sub_expr);
    ast_type_set(ctx->ast, &unary_expr->head, sub_ty);
}

static void check_bin_expr(TypeCheckCtx *ctx, BinExpr *bin_expr) {
    TypeId lhs_ty = type_of_expr(ctx, bin_expr->left);
    TypeId rhs_ty = type_of_expr(ctx, bin_expr->right);
    if (lhs_ty != rhs_ty) {
        sem_raisef(ctx->ast, ctx->code, bin_expr->op.offset,
                   "type mismatch: left-hand side is '{t}' and right-hand side "
                   "is '{t}'",
                   lhs_ty, rhs_ty);
        return;
    }

    TypeRepr *repr = ast_type_repr(ctx->ast, lhs_ty);
    assert(repr);

    switch (bin_expr->op.t) {
        case T_STAR:
        case T_SLASH:
        case T_PLUS:
        case T_MINUS:
        case T_PERC:
            if (!type_is_arithmetic(repr)) {
                sem_raisef(ctx->ast, ctx->code, bin_expr->op.offset,
                           "semantic error: cannot use operator '{s}' with "
                           "operand type {t}; arithmetic type expected",
                           bin_expr->op.text, lhs_ty);
            }
            break;
        case T_AND:
        case T_OR: {
            TypeId bool_id = ctx->ast->tree_data.builtin_type[T_BOOL];
            if (lhs_ty != bool_id) {
                sem_raisef(ctx->ast, ctx->code, bin_expr->op.offset,
                           "semantic error: cannot use operator '{s}' with "
                           "operand type {t}; boolean type expected",
                           bin_expr->op.text, lhs_ty);
            }
        }
        case T_AMP:
        case T_PIPE:
        case T_NEQ:
        case T_EQEQ:
        case T_LT:
        case T_GT:
        case T_LTEQ:
        case T_GTEQ:
            break;
        default:
            assert(false && "unhandled operator");
            break;
    }

    ast_type_set(ctx->ast, &bin_expr->head, lhs_ty);
}

static void check_var_decl(TypeCheckCtx *ctx, VarDecl *var_decl) {
    TypeId var_ty = ast_type_get(ctx->ast, &var_decl->head);
    if (var_decl->init.ptr == NULL && var_ty == INVALID_TYPE) {
        assert(var_decl->binding->type.ptr == NULL);
        sem_raisef(ctx->ast, ctx->code, var_decl->head.offset,
                   "declaration error: variable has no type or initial value; "
                   "initial value allows for type inference");
        return;
    }

    TypeId rhs_ty = INVALID_TYPE;
    if (var_decl->init.ptr != NULL) {
        rhs_ty = type_of_expr(ctx, var_decl->init.ptr);
    }

    if (var_decl->init.ptr != NULL && var_ty != INVALID_TYPE) {
        assert(rhs_ty != INVALID_TYPE);

        if (!type_is_convertible(ctx, var_ty, rhs_ty)) {
            sem_raisef(ctx->ast, ctx->code,
                       var_decl->binding->type.ptr->head.offset,
                       "type mismatch: type of right-hand side '{t}' does not "
                       "match declared type '{t}'",
                       rhs_ty, var_ty);
            return;
        }
    }

    if (var_decl->init.ptr != NULL && var_ty == INVALID_TYPE) {
        assert(rhs_ty != INVALID_TYPE);
        ast_type_set(ctx->ast, &var_decl->head, rhs_ty);
    }
}

typedef struct {
    TypeId id;
    TypeRepr repr;
} TypeDesc;

static TypeDesc dealias(Ast *ast, TypeId id) {
    TypeRepr repr = *ast_type_repr(ast, id);
    while (repr.t == STORAGE_ALIAS) {
        id = repr.alias_type.aliases;
        repr = *ast_type_repr(ast, id);
    }
    return (TypeDesc){.id = id, .repr = repr};
}

static TypeDesc deref(Ast *ast, TypeId id) {
    TypeRepr repr = *ast_type_repr(ast, id);
    while (repr.t == STORAGE_PTR) {
        id = repr.ptr_type.points_to;
        repr = *ast_type_repr(ast, id);
    }
    return (TypeDesc){.id = id, .repr = repr};
}

static TypeDesc auto_deref(Ast *ast, TypeId id, bool *did_deref) {
    TypeRepr repr = *ast_type_repr(ast, id);
    if (repr.t == STORAGE_PTR) {
        id = repr.ptr_type.points_to;
        repr = *ast_type_repr(ast, id);
        if (did_deref != NULL) {
            *did_deref = true;
        }
    }
    return (TypeDesc){.id = id, .repr = repr};
}

static void check_field_access(TypeCheckCtx *ctx, FieldAccess *field_access) {
    Expr *lval = field_access->lvalue;
    TypeId lval_ty = type_of_expr(ctx, lval);
    assert(lval_ty != INVALID_TYPE);

    bool did_auto_deref = false;
    TypeDesc base =
        dealias(ctx->ast, auto_deref(ctx->ast, lval_ty, &did_auto_deref).id);

    if (base.repr.t != STORAGE_STRUCT) {
        if (did_auto_deref && base.repr.t == STORAGE_PTR) {
            TypeDesc storage = dealias(ctx->ast, deref(ctx->ast, base.id).id);
            if (storage.repr.t == STORAGE_STRUCT) {
                sem_raisef(ctx->ast, ctx->code, lval->head.offset,
                           "field access on struct type with too much pointer "
                           "indirection '{t}'",
                           lval_ty);
                return;
            }
        }
        sem_raisef(ctx->ast, ctx->code, lval->head.offset,
                   "invalid access: cannot access field of value with "
                   "non-struct type '{t}'",
                   lval_ty);
        return;
    }

    TypeStruct st = base.repr.struct_type;
    for (size_t i = 0; i < da_length(st.fields); i++) {
        TypeField f = st.fields[i];
        if (streql(f.name, field_access->field->token.text)) {
            ast_type_set(ctx->ast, &field_access->head, f.type);
            return;
        }
    }

    sem_raisef(ctx->ast, ctx->code, field_access->field->head.offset,
               "invalid access: field '{s}' not found in type '{t}'",
               field_access->field->token.text, lval_ty);
}

static Scope *scope_hint(TypeCheckCtx *ctx) {
    if (da_length(ctx->type_hint) == 0) {
        return NULL;
    }
    TypeId hint = dealias(ctx->ast, current_type_hint(ctx)).id;
    Type **tr = type_source_get(ctx->ast->tree_data.type_source, hint);
    if (!tr) {
        return NULL;
    }
    Type *ty = *tr;
    Child *child = children_at(ty->head.children, 0);
    if (child->t == CHILD_NODE) {
        return ast_scope_get(ctx->ast, child->node);
    }
    return NULL;
}

static void check_scoped_ident(TypeCheckCtx *ctx, ScopedIdent *scoped_ident) {
    assert(da_length(scoped_ident->head.children) != 0);

    Tok first = child_ident_at(&scoped_ident->head, 0)->token;
    if (first.t != T_EMPTY_STRING) {
        // Skip non inferred reference
        return;
    }

    AstNode *h = &scoped_ident->head;
    if (da_length(ctx->type_hint) == 0) {
        sem_raisef(ctx->ast, ctx->code, scoped_ident->head.offset,
                   "inferred name used in context without a hint");
        return;
    }

    Scope *scope = scope_hint(ctx);
    if (!scope) {
        TypeId hint = current_type_hint(ctx);
        sem_raisef(
            ctx->ast, ctx->code, scoped_ident->head.offset,
            "inferred name cannot be resolved in the current context as the "
            "type hint '{t}' does not have a scope",
            hint);
        return;
    }

    size_t start_i = 1;
    for (size_t i = start_i; i < da_length(h->children); i++) {
        Ident *ident = child_ident_at(&scoped_ident->head, i);
        u32 defined_at = ident->token.offset;
        string ident_text = ident->token.text;

        if (!scope) {
            string supposed_scope = child_ident_at(h, i - 1)->token.text;
            sem_raisef(ctx->ast, ctx->code, defined_at,
                       "inferred lookup error: cannot resolve '{s}' in '{s}' "
                       "as '{s}' does "
                       "not define a scope",
                       ident_text, supposed_scope, supposed_scope);
            break;
        }

        ScopeLookup lookup =
            scope_lookup(scope, ident_text,
                         i == 0 ? LOOKUP_MODE_LEXICAL : LOOKUP_MODE_DIRECT);
        if (!lookup.entry) {
            if (i != start_i) {
                string parent = child_ident_at(h, i - 1)->token.text;
                sem_raisef(
                    ctx->ast, ctx->code, defined_at,
                    "inferred lookup error: '{s}' not found inside scope '{s}'",
                    ident_text, parent);
            } else {
                sem_raisef(
                    ctx->ast, ctx->code, defined_at,
                    "inferred lookup error: '{s}' not found in type hint scope",
                    ident_text);
            }
            break;
        }

        assert(lookup.entry->shadows == NULL);
        ast_resolves_to_set(ctx->ast, ident, lookup.entry->node);

        scope = lookup.entry->sub_scope.ptr;
    }
}
