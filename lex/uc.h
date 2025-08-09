#ifndef UC_H
#define UC_H

#include <stddef.h>
#include <stdint.h>

// see: https://www.unicode.org/versions/Unicode16.0.0/core-spec/chapter-4/#G134153
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

extern uc_map *uc_data;
extern size_t uc_data_size;

#endif
