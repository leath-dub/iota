#include "uc.h"

#include <assert.h>
#include <string.h>

uc_gcat runecat(rune cp) {
    // binary search for category
    int l = 0, h = uc_data_size - 1;
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
        case GC_Lu:
            return "Lu";
        case GC_Ll:
            return "Ll";
        case GC_Lt:
            return "Lt";
        case GC_LC:
            return "LC";
        case GC_Lm:
            return "Lm";
        case GC_Lo:
            return "Lo";
        case GC_Mn:
            return "Mn";
        case GC_Mc:
            return "Mc";
        case GC_Me:
            return "Me";
        case GC_Nd:
            return "Nd";
        case GC_Nl:
            return "Nl";
        case GC_No:
            return "No";
        case GC_Pc:
            return "Pc";
        case GC_Pd:
            return "Pd";
        case GC_Ps:
            return "Ps";
        case GC_Pe:
            return "Pe";
        case GC_Pi:
            return "Pi";
        case GC_Pf:
            return "Pf";
        case GC_Po:
            return "Po";
        case GC_Sm:
            return "Sm";
        case GC_Sc:
            return "Sc";
        case GC_Sk:
            return "Sk";
        case GC_So:
            return "So";
        case GC_Zs:
            return "Zs";
        case GC_Zl:
            return "Zl";
        case GC_Zp:
            return "Zp";
        case GC_Cc:
            return "Cc";
        case GC_Cf:
            return "Cf";
        case GC_Cs:
            return "Cs";
        case GC_Co:
            return "Co";
        case GC_Cn:
            return "Cn";
        default:
            break;
    }
    return NULL;
}

size_t chartorune(rune *r, const char *s) {
    assert(s != NULL);
    int len = strlen(s);
    rune c = (unsigned char)*s;
    if (c == '\0') return 0;

    if ((c >> 7) == 0) {
        *r = c;
        return 1;
    } else if ((c >> 5) == 0x6) {
        if (len < 2) return 0;
        unsigned char c0 = (unsigned char)s[0];
        unsigned char c1 = (unsigned char)s[1];
        if ((c1 >> 6) != 0x2) {
            return 0;
        }
        *r = ((c0 & 0x1f) << 6) | (c1 & 0x3f);
        return 2;
    } else if ((c >> 4) == 0xE) {
        if (len < 3) return 0;
        unsigned char c0 = (unsigned char)s[0];
        unsigned char c1 = (unsigned char)s[1];
        unsigned char c2 = (unsigned char)s[2];
        if ((c1 >> 6) != 0x2 || (c2 >> 6) != 0x2) {
            return 0;
        }
        *r = ((c0 & 0xf) << 12) | ((c1 & 0x3f) << 6) | (c2 & 0x3f);
        return 3;
    } else if ((c >> 3) == 0x1E) {
        if (len < 4) return 0;
        unsigned char c0 = (unsigned char)s[0];
        unsigned char c1 = (unsigned char)s[1];
        unsigned char c2 = (unsigned char)s[2];
        unsigned char c3 = (unsigned char)s[3];
        if ((c1 >> 6) != 0x2 || (c2 >> 6) != 0x2 || (c3 >> 6) != 0x2) {
            return 0;
        }
        *r = ((c0 & 0x7) << 18) | ((c1 & 0x3f) << 12) | ((c2 & 0x3f) << 6) |
             (c3 & 0x3f);
        return 4;
    }
    return 0;
}

bool id_start(rune r) {
    switch (runecat(r)) {
        case GC_Lu:
        case GC_Ll:
        case GC_Lt:
        case GC_Lm:
        case GC_Lo:
        case GC_Nl:
            return true;
        default:
            // Fallback allow underscore at the start
            return r == '_';
    }
}

bool id_continue(rune r) {
    switch (runecat(r)) {
        case GC_Mn:
        case GC_Mc:
        case GC_Nd:
        case GC_Pc:
            return true;
        default:
            return id_start(r);
    }
}
