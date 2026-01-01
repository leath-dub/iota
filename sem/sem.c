#include "sem.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

DA_DEFINE(toks, char)

void sem_raisef(Ast *ast, SourceCode *code, size_t offset, const char *fmt,
                ...) {
    char *toks = toks_create();

    va_list args;
    va_start(args, fmt);

    bool in_brc = false;
    const char *it = fmt;
    while (*it != '\0') {
        switch (*it) {
            case '{':
                if (!in_brc) {
                    in_brc = true;
                    it++;
                    continue;
                }
                break;
            case '}':
                if (in_brc) {
                    in_brc = false;
                    it++;
                    continue;
                }
                break;
            default:
                if (in_brc) {
                    switch (*it) {
                        case 't': {
                            TypeId ty = va_arg(args, TypeId);
                            TypeRepr *tr = ast_type_repr(ast, ty);
                            assert(tr);
                            static char type_txt[256];
                            fmt_type(type_txt, 256, ast, *tr);
                            size_t len = strlen(type_txt);
                            for (size_t i = 0; i < len; i++) {
                                toks_append(&toks, type_txt[i]);
                            }
                            break;
                        }
                        case 's': {
                            string s = va_arg(args, string);
                            for (size_t i = 0; i < s.len; i++) {
                                toks_append(&toks, s.data[i]);
                            }
                            break;
                        }
                        case 'c': {
                            const char *s = va_arg(args, const char *);
                            size_t len = strlen(s);
                            for (size_t i = 0; i < len; i++) {
                                toks_append(&toks, s[i]);
                            }
                            break;
                        }
                        case 'i': {
                            static char buf[256];
                            snprintf(buf, 256, "%ld", va_arg(args, u64));
                            size_t len = strlen(buf);
                            for (size_t i = 0; i < len; i++) {
                                toks_append(&toks, buf[i]);
                            }
                            break;
                        }
                        default:
                            assert(false && "invalid format");
                            break;
                    }

                    it++;
                    continue;
                }
                break;
        }

        toks_append(&toks, *it);
        it++;
    }

    va_end(args);

    size_t len = da_length(toks);
    char *buf = arena_alloc(&code->error_arena, len, _Alignof(char));
    memcpy(buf, toks, len);

    raise_semantic_error(code, (SemanticError){
                                   .at = offset,
                                   .message = buf,
                               });

    toks_delete(toks);
}
