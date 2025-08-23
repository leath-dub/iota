#ifndef UC_H
#define UC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// see:
// https://www.unicode.org/versions/Unicode16.0.0/core-spec/chapter-4/#G134153
typedef enum {
  GC_Lu,
  GC_Ll,
  GC_Lt,
  GC_LC,
  GC_Lm,
  GC_Lo,
  GC_Mn,
  GC_Mc,
  GC_Me,
  GC_Nd,
  GC_Nl,
  GC_No,
  GC_Pc,
  GC_Pd,
  GC_Ps,
  GC_Pe,
  GC_Pi,
  GC_Pf,
  GC_Po,
  GC_Sm,
  GC_Sc,
  GC_Sk,
  GC_So,
  GC_Zs,
  GC_Zl,
  GC_Zp,
  GC_Cc,
  GC_Cf,
  GC_Cs,
  GC_Co,
  GC_Cn,
  GC_COUNT,
  GC_INVALID = -1,
} uc_gcat;

#define GCAT(gcn) GC_##gcn

typedef struct {
  uint32_t start;
  uint32_t end;
  uc_gcat gc;
} uc_map;

extern uc_map uc_data[];
extern size_t uc_data_size;

typedef uint32_t rune;

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

#endif
