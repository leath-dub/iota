#ifndef SCANNER_H_
#define SCANNER_H_

// NOTE: many of these functions are exposed through this header even if they
// are only internally used. This is just to facilitate testing.

#include "uc.h"

#include <stddef.h>
#include <stdbool.h>

uc_gcat runecat(rune cp);
const char *gctoa(uc_gcat cat);

// insert the next rune into "r" and return the number
// of bytes the rune represents (ever heard of plan 9?)
size_t chartorune(rune *r, const char *s);

// Not arsed to pull in a huge dependency for unicode so we have limited
// bespoke identifier detection which does not care about normalization or
// historical Other_ID_Start bullshit that I just don't want to deal with.
bool id_start(rune r);
bool id_continue(rune r);

size_t lex_id(const char *txt);

#endif

// vim: ft=c
