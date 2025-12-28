#include <assert.h>

#include "sem.h"

// Canonicalizes all types apart from scoped idents which need to be resolved
// later as they could be referenced after definition
static DfsCtrl type_resolution_enter(void *_ctx, AstNode *node);
static DfsCtrl type_resolution_exit(void *_ctx, AstNode *node);

// Canonicalize scoped ident (references to type aliases)
static DfsCtrl type_alias_resolution_enter(void *_ctx, AstNode *node);

static DfsCtrl check_types_enter(void *_ctx, AstNode *node);
static DfsCtrl check_types_exit(void *_ctx, AstNode *node);

static void sem_raise(SourceCode *code, u32 at, const char *banner,
                      const char *msg);

typedef struct {
    Ast *ast;
    TypeRepr **type_hint;
    SourceCode *code;
} TypeCheckCtx;

DA_DEFINE(type_hint, TypeRepr *)

typedef struct {
    Ast *ast;
    Type **current_type;
    TypeId *normalized_type;
    TypeId *type_memo;
    SourceCode *code;
} TypeResCtx;

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
        ast_traverse_dfs(&ctx, ast,
                         (EnterExitVTable){
                             .enter = type_resolution_enter,
                             .exit = type_resolution_exit,
                         });
        ast_traverse_dfs(&ctx, ast,
                         (EnterExitVTable){
                             .enter = type_alias_resolution_enter,
                             .exit = NULL,
                         });

        current_type_delete(ctx.current_type);
        normalized_type_delete(ctx.normalized_type);
        type_memo_delete(ctx.type_memo);
    }

    ast_traverse_dfs(&ctx, ast,
                     (EnterExitVTable){
                         .enter = check_types_enter,
                         .exit = check_types_exit,
                     });
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

static void res_type(TypeResCtx *ctx, TypeId ty) {
    Type **top =
        current_type_at(ctx->current_type, da_length(ctx->current_type) - 1);
    normalized_type_put(&ctx->normalized_type, *top, ty);
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

    res_type(ctx, type_id_of(ctx, ty));
}

static void type_res_struct_type(TypeResCtx *ctx, StructType *struct_type) {
    AstNode *hd = &struct_type->fields->head;
    TypeRepr ty = {0};

    ty.t = STORAGE_STRUCT;
    TypeStruct *ts = &ty.struct_type;

    TypeField **fields = &ts->fields;
    *fields = type_fields_create();

    for (u32 i = 0; i < da_length(hd); i++) {
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

    res_type(ctx, type_id_of(ctx, ty));
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

    res_type(ctx, type_id_of(ctx, ty));
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
                TypeId *ft =
                    normalized_type_get(ctx->normalized_type, alt->type);
                assert(ft && "subtree type should be resolved");
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

    res_type(ctx, type_id_of(ctx, ty));
}

static void type_res_fn_type(TypeResCtx *ctx, FnType *fn_type) {
    (void)ctx;
    (void)fn_type;
    TODO("function types");
}

static void type_res_builtin_type(TypeResCtx *ctx, BuiltinType *builtin_type) {
    TokKind tk = builtin_type->token.t;
    TypeRepr ty = {0};
    switch (tk) {
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
        default:
            assert(false && "not a known builtin type");
    }
    res_type(ctx, type_id_of(ctx, ty));
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

    res_type(ctx, type_id_of(ctx, ty));
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
        sem_raise(ctx->code, scoped_ident->head.offset, "symbol mismatch",
                  "name must resolve to a type");
        return;
    }

    ty.t = STORAGE_ALIAS;
    ty.alias_type = (TypeAlias){
        .type_decl = (TypeDecl *)res,
        .aliases = INVALID_TYPE,
    };

    res_type(ctx, type_id_of(ctx, ty));
}

static DfsCtrl type_alias_resolution_enter(void *_ctx, AstNode *node) {
    TypeResCtx *ctx = (TypeResCtx *)_ctx;

    if (node->kind != NODE_TYPE) {
        goto ret;
    }

    Type *tp = (Type *)node;
    if (tp->t != TYPE_SCOPED_IDENT) {
        goto ret;
    }

    TypeId *tid = normalized_type_get(ctx->normalized_type, tp);
    assert(tid);
    TypeRepr *tr = ast_type_repr(ctx->ast, *tid);
    assert(tr->t == STORAGE_ALIAS);

    AstNode *res = ast_resolves_to_get_scoped(ctx->ast, tp->scoped_ident);
    assert(res);
    if (res->kind != NODE_TYPE_DECL) {
        sem_raise(ctx->code, node->offset, "symbol mismatch",
                  "name must resolve to a type");
        goto ret;
    }

    TypeDecl *td = (TypeDecl *)res;
    TypeId aliases = ast_type_get(ctx->ast, &td->head);
    assert(aliases != INVALID_TYPE);
    tr->alias_type.aliases = aliases;

ret:
    return DFS_CTRL_KEEP_GOING;
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

static DfsCtrl check_types_enter(void *_ctx, AstNode *node) {
    (void)_ctx;
    (void)node;
    // TODO: add "inferred_scope" context on entry of certain nodes:
    //
    // e.g. - function call argument
    //      - variable declaration
    //      - return statement
    //      - case statement
    return DFS_CTRL_KEEP_GOING;
}

// static void check_atom(TypeCheckCtx *ctx, Atom *atom);
// static void check_postfix_expr(TypeCheckCtx *ctx, PostfixExpr *postfix_expr);
// static void check_call(TypeCheckCtx *ctx, Call *call);
// static void check_field_access(TypeCheckCtx *ctx, FieldAccess *field_access);
// static void check_coll_access(TypeCheckCtx *ctx, CollAccess *coll_access);
// static void check_unary_expr(TypeCheckCtx *ctx, UnaryExpr *unary_expr);
// static void check_bin_expr(TypeCheckCtx *ctx, BinExpr *bin_expr);
// static void check_var_decl(TypeCheckCtx *ctx, VarDecl *var_decl);
// static void check_type_decl(TypeCheckCtx *ctx, TypeDecl *td);

static DfsCtrl check_types_exit(void *_ctx, AstNode *node) {
    TypeCheckCtx *ctx = _ctx;
    (void)ctx;
    switch (node->kind) {
        case NODE_VAR_DECL:
            // check_var_decl(ctx, (VarDecl *)node);
            break;
        default:
            break;
    }
    return DFS_CTRL_KEEP_GOING;
}

static void sem_raise(SourceCode *code, u32 at, const char *banner,
                      const char *msg) {
    raise_semantic_error(code, (SemanticError){
                                   .at = at,
                                   .banner = banner,
                                   .message = msg,
                               });
}

// static TypeRepr *type_from_symbol(TypeCheckCtx *ctx, AstNode *symbol);
// static AstNode *get_scoped_ident_symbol(TypeCheckCtx *ctx,
//                                         ScopedIdent *scoped_ident);
//
// static TypeRepr *type_from_token(TypeCheckCtx *ctx, Tok token) {
//     TypeRepr *type = type_create(ctx->ast->arena);
//     switch (token.t) {
//         case T_U8:
//             type->t = STORAGE_U8;
//             break;
//         case T_S8:
//             type->t = STORAGE_S8;
//             break;
//         case T_U16:
//             type->t = STORAGE_U16;
//             break;
//         case T_S16:
//             type->t = STORAGE_S16;
//             break;
//         case T_U32:
//             type->t = STORAGE_U32;
//             break;
//         case T_S32:
//             type->t = STORAGE_S32;
//             break;
//         case T_U64:
//             type->t = STORAGE_U64;
//             break;
//         case T_S64:
//             type->t = STORAGE_S64;
//             break;
//         case T_F32:
//             type->t = STORAGE_F32;
//             break;
//         case T_F64:
//             type->t = STORAGE_F64;
//             break;
//         case T_UNIT:
//             type->t = STORAGE_UNIT;
//             break;
//         default:
//             assert(false && "TODO");
//     }
//     return type;
// }

// static TypeRepr *type_from_lit(TypeCheckCtx *ctx, Tok lit) {
//     TypeRepr *type = type_create(ctx->ast->arena);
//     switch (lit.t) {
//         case T_NUM:
//             type->t = STORAGE_S32;
//             break;  // TODO validate number literal
//         default:
//             assert(false && "TODO");
//     }
//     return type;
// }
//
// static TypeRepr *type_from_tree(TypeCheckCtx *ctx, Type *tree) {
//     switch (tree->t) {
//         case TYPE_BUILTIN:
//             return type_from_token(ctx, tree->builtin_type->token);
//         case TYPE_PTR: {
//             TypeRepr *type = type_create(ctx->ast->arena);
//             type->t = STORAGE_PTR;
//             type->ptr_type.points_to = type_from_tree(ctx,
//             tree->ptr_type->ref_type); return type;
//         }
//         case TYPE_SCOPED_IDENT:
//             return type_from_symbol(
//                 ctx, get_scoped_ident_symbol(ctx, tree->scoped_ident));
//         case TYPE_FN:
//         case TYPE_COLL:
//         case TYPE_STRUCT:
//         case TYPE_TUPLE:
//         case TYPE_TAGGED_UNION:
//         case TYPE_ENUM:
//         case TYPE_ERR:
//             break;
//     }
//     assert(false && "TODO");
// }
//
// static TypeRepr *type_from_symbol(TypeCheckCtx *ctx, AstNode *symbol) {
//     switch (symbol->kind) {
//         case NODE_FN_DECL: {
//             FnDecl *fn = (FnDecl *)symbol;
//
//             TypeRepr *fn_type = type_create(ctx->ast->arena);
//
//             fn_type->t = STORAGE_FN;
//
//             AstNode *h = &fn->params->head;
//
//             for (u32 i = 0; i < da_length(h->children); i++) {
//                 FnParam *param = child_fn_param_at(h, i);
//                 TypeFnParam tparam = {
//                     .type = type_from_tree(ctx, param->binding->type),
//                     .variadic = param->variadic,
//                 };
//                 type_fn_params_append(&fn_type->fn_type.params, tparam);
//             }
//
//             type_fn_params_shrink(&fn_type->fn_type.params);
//             arena_own(ctx->ast->arena, fn_type->fn_type.params,
//             da_length(fn_type->fn_type.params));
//
//             TypeRepr *return_type = NULL;
//             if (fn->return_type.ptr != NULL) {
//                 return_type = type_from_tree(ctx, fn->return_type.ptr);
//             } else {
//                 return_type = type_create(ctx->ast->arena);
//                 return_type->t = STORAGE_UNIT;
//             }
//
//             fn_type->fn_type.return_type = return_type;
//
//             return fn_type;
//         }
//         case NODE_VAR_DECL: {
//             VarDecl *var = (VarDecl *)symbol;
//             TypeRepr *type = ast_type_get(ctx->ast, &var->head);
//             assert(type);
//             return type;
//         }
//         default:
//             break;
//     }
//     assert(false && "TODO");
// }
//
// static AstNode *get_scoped_ident_symbol(TypeCheckCtx *ctx,
//                                         ScopedIdent *scoped_ident) {
//     AstNode *h = &scoped_ident->head;
//     assert(child_ident_at(h, 0)->token.t != T_EMPTY_STRING && "TODO
//     inferred"); AstNode *node = ast_resolves_to_get(
//         ctx->ast, child_ident_at(h, da_length(h->children) - 1));
//     assert(node);
//     return node;
// }

// static void check_atom(TypeCheckCtx *ctx, Atom *atom) {
//     switch (atom->t) {
//         case ATOM_TOKEN:
//             ast_type_set(ctx->ast, &atom->head,
//                          type_from_lit(ctx, atom->builtin_type));
//             break;
//         case ATOM_SCOPED_IDENT:
//             ast_type_set(ctx->ast, &atom->head,
//                          type_from_symbol(ctx, get_scoped_ident_symbol(
//                                                    ctx,
//                                                    atom->scoped_ident)));
//             break;
//         default:
//             break;
//     }
// }
//
// static TypeRepr *type_of_expr(TypeCheckCtx *ctx, Expr *expr) {
//     Child *children = expr->head.children;
//     assert(da_length(children) != 0);
//     Child *child = children_at(children, 0);
//     assert(child->t == CHILD_NODE);
//     return ast_type_get(ctx->ast, child->node);
// }
//
//
// // static void check_postfix_expr(TypeCheckCtx *ctx, PostfixExpr
// *postfix_expr);
//
// static void check_call(TypeCheckCtx *ctx, Call *call) {
//     if (call->callable->t == EXPR_ATOM) {
//         Atom *atom = call->callable->atom;
//         if (atom->t == ATOM_BUILTIN_TYPE) {
//             // Cast to builtin type
//             TypeRepr *type = type_from_token(ctx, atom->builtin_type);
//             ast_type_set(ctx->ast, &call->head, type);
//         }
//     }
// }
//
// // static void check_field_access(TypeCheckCtx *ctx, FieldAccess
// *field_access);
// // static void check_coll_access(TypeCheckCtx *ctx, CollAccess *coll_access);
// // static void check_unary_expr(TypeCheckCtx *ctx, UnaryExpr *unary_expr);
//
// static void check_bin_expr(TypeCheckCtx *ctx, BinExpr *bin_expr) {
//     TypeRepr *lt = type_of_expr(ctx, bin_expr->left);
//     TypeRepr *rt = type_of_expr(ctx, bin_expr->right);
//     if (!lt || !rt) {
//         TODO("type not resolved");
//         return;
//     }
//
//     // Arena *erra = &ctx->code->error_arena;
//
//     if (lt->t != rt->t) {
//         sem_raise(ctx->code, bin_expr->op.offset, "type error", "mismatched
//         types");
//     }
//
//     ast_type_set(ctx->ast, &bin_expr->head, lt);
// }

// static void check_var_decl(TypeCheckCtx *ctx, VarDecl *var_decl) {
//     TypeRepr *vt = ast_type_get(ctx->ast, &var_decl->head);
//
//     if (var_decl->binding.type.ptr != NULL) {
//         assert(vt);
//         return;
//     }
//
//     type_of_expr(ctx, vt->)
//
//     TypeRepr *resolved_type = BAD_PTR;
//
//     Type *type = var_decl->binding->type.ptr;
//     if (type != NULL) {
//         TypeRepr *expected_type = type_from_tree(ctx, type);
//         if (resolved_type != BAD_PTR && expected_type->t != resolved_type->t)
//         {
//             sem_raise(ctx->code, var_decl->head.offset, "type error",
//                       "expression does not match declared type");
//         }
//         resolved_type = expected_type;
//     }
//
//     if (var_decl->init.ptr != NULL) {
//         resolved_type = type_of_expr(ctx, var_decl->init.ptr);
//     }
//
//     assert(resolved_type != BAD_PTR);
// }
//
// static void check_type_decl(TypeCheckCtx *ctx, TypeDecl *td) {
//     TypeRepr *ty = type_from_tree(ctx, td->type);
//     ast_type_set(ctx->ast, &td->head, ty);
// }
