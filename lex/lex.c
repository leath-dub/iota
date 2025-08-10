#include <assert.h>
#include <stdio.h>

#include "lex.h"


// 12
// [10, 11, 12, 44]
//   l   m       h
//       l   m   h
//       lm  h
//       

uc_gcat runecat(rune cp) {
	// binary search for category
	int l = 0, h = uc_data_size - 1, m;
	while (l <= h) {
		int m = l + (h - l) / 2;
		if (cp < uc_data[m].start) {
			h = m - 1;
		} else if (cp > uc_data[m].end) {
			l = m + 1;
		} else {
			return uc_data[m].gc;
		}
	}
	return GC_INVALID;
}

const char *gctoa(uc_gcat cat) {
	switch (cat) {
	case GC_Lu: return "Lu";
	case GC_Ll: return "Ll";
	case GC_Lt: return "Lt";
	case GC_LC: return "LC";
	case GC_Lm: return "Lm";
	case GC_Lo: return "Lo";
	case GC_Mn: return "Mn";
	case GC_Mc: return "Mc";
	case GC_Me: return "Me";
	case GC_Nd: return "Nd";
	case GC_Nl: return "Nl";
	case GC_No: return "No";
	case GC_Pc: return "Pc";
	case GC_Pd: return "Pd";
	case GC_Ps: return "Ps";
	case GC_Pe: return "Pe";
	case GC_Pi: return "Pi";
	case GC_Pf: return "Pf";
	case GC_Po: return "Po";
	case GC_Sm: return "Sm";
	case GC_Sc: return "Sc";
	case GC_Sk: return "Sk";
	case GC_So: return "So";
	case GC_Zs: return "Zs";
	case GC_Zl: return "Zl";
	case GC_Zp: return "Zp";
	case GC_Cc: return "Cc";
	case GC_Cf: return "Cf";
	case GC_Cs: return "Cs";
	case GC_Co: return "Co";
	case GC_Cn: return "Cn";
	default: break;
	}
	return NULL;
}
