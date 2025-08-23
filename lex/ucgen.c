#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"
#include "uc.h"

#define MAX(a, b) (a) > (b) ? (a) : (b)
#define ESC(code) #code

typedef struct {
  long start, end;
} rune_range;

typedef struct rune_ranges {
  uc_gcat gc;
  rune_range range;
  struct rune_ranges *next;
} rune_ranges;

uc_gcat stocat(char *s) {
  u32 len = strlen(s);
  assert(len == 2);
  switch (s[0]) {
    case 'L':
      switch (s[1]) {
        case 'u':
          return GC_Lu;
        case 'l':
          return GC_Ll;
        case 't':
          return GC_Lt;
        case 'C':
          return GC_LC;
        case 'm':
          return GC_Lm;
        case 'o':
          return GC_Lo;
      }
      break;
    case 'M':
      switch (s[1]) {
        case 'n':
          return GC_Mn;
        case 'c':
          return GC_Mc;
        case 'e':
          return GC_Me;
      }
      break;
    case 'N':
      switch (s[1]) {
        case 'd':
          return GC_Nd;
        case 'l':
          return GC_Nl;
        case 'o':
          return GC_No;
      }
      break;
    case 'P':
      switch (s[1]) {
        case 'c':
          return GC_Pc;
        case 'd':
          return GC_Pd;
        case 's':
          return GC_Ps;
        case 'e':
          return GC_Pe;
        case 'i':
          return GC_Pi;
        case 'f':
          return GC_Pf;
        case 'o':
          return GC_Po;
      }
      break;
    case 'S':
      switch (s[1]) {
        case 'm':
          return GC_Sm;
        case 'c':
          return GC_Sc;
        case 'k':
          return GC_Sk;
        case 'o':
          return GC_So;
      }
      break;
    case 'Z':
      switch (s[1]) {
        case 's':
          return GC_Zs;
        case 'l':
          return GC_Zl;
        case 'p':
          return GC_Zp;
      }
      break;
    case 'C':
      switch (s[1]) {
        case 'c':
          return GC_Cc;
        case 'f':
          return GC_Cf;
        case 's':
          return GC_Cs;
        case 'o':
          return GC_Co;
        case 'n':
          return GC_Cn;
      }
      break;
  }
  fprintf(stderr, "invalid general category %s", s);
  return GC_INVALID;
}

const char *cattos(uc_gcat cat) {
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
  fprintf(stderr, "invalid general category enum value %d\n", cat);
  return NULL;
}

void insert_codep(rune_ranges **rr, uc_gcat gc, long cp) {
  if (*rr == NULL) {
    // initial range to insert
    *rr = calloc(1, sizeof(rune_ranges));
    (*rr)->gc = gc;
    (*rr)->range.start = cp;
    (*rr)->range.end = cp;
    return;
  }

  rune_ranges *it = *rr, *prev = *rr;
  while (it != NULL) {
    assert(it->gc == gc);

    // check to see if `cp` can extend the current range
    // (i.e. it is only one more than .end or one less than .start)
    rune_range *r = &it->range;
    if (cp == r->start - 1) {
      r->start = cp;
      break;
    } else if (cp == r->end + 1) {
      r->end = cp;
      break;
    }
    prev = it;
    it = it->next;
  }

  // we reached the end without finding a place for the new codepoint
  // - create a new range for this code point
  if (it == NULL) {
    rune_range r = {.start = cp, .end = cp};
    prev->next = calloc(1, sizeof(rune_ranges));
    prev->next->gc = gc;
    prev->next->range = r;
  }
}

void rr_sort(rune_ranges *rr) {
  // Selection sort
  rune_ranges *it = rr;
  while (it != NULL) {
    rune_ranges *jt = it->next;
    while (jt != NULL) {
      rune_range ir = it->range;
      rune_range jr = jt->range;
      // assert they don't overlap - insertion should not allow this
      assert(jr.end < ir.start || ir.end < jr.start);
      if (jr.start < ir.start) {
        // Swap their values
        uc_gcat t = it->gc;
        it->gc = jt->gc;
        jt->gc = t;
        it->range = jr;
        jt->range = ir;
      }
      jt = jt->next;
    }
    it = it->next;
  }
}

int main(int argc, char *argv[]) {
  static rune_ranges *ranges[GC_COUNT] = {0};

  if (argc <= 1) {
    fprintf(stderr,
            "ucgen: missing argument file path (path to UnicodeData.txt)\n");
    exit(2);
  }

  char *dp = argv[1];
  u32 r = access(dp, F_OK);
  if (r != 0) {
    fprintf(stderr, "ucgen: failed to access %s: %s\n", dp, strerror(errno));
    exit(1);
  }

  rune_ranges *fst = NULL;

  u32 ec = 0, lno = 0;
  static char ln[BUFSIZ] = {0};
  size_t sz = BUFSIZ;
  FILE *fs = fopen(dp, "r");

  while (fgets(ln, sz, fs)) {
    char *cps = strtok(ln, ";");
    if (cps == NULL || strlen(cps) < 4) {
      fprintf(stderr, "%d: invalid format\n", lno);
      ec = 1;
      goto bail;
    }

    // parse the code point field
    char *ep = NULL;
    long cp = strtol(cps, &ep, 16);
    if (*ep != '\0') {
      fprintf(stderr, "%d: invalid format\n", lno);
      ec = 1;
      goto bail;
    }

    // extract "General Category" as the third field
    char *gcn = NULL;
    if (strtok(NULL, ";") == NULL || (gcn = strtok(NULL, ";")) == NULL) {
      fprintf(stderr, "%d: invalid format\n", lno);
      ec = 1;
      goto bail;
    }

    uc_gcat gc = stocat(gcn);
    if (gc == GC_INVALID) {
      ec = 1;
      goto bail;
    }

    insert_codep(&ranges[gc], gc, cp);
    lno++;
  }

  // sort each category and merge any ranges which
  for (uc_gcat gc = 0; gc < GC_COUNT; gc++) {
    rune_ranges *r = ranges[gc];
    if (r == NULL) {
      continue;
    }
    rr_sort(r);

    rune_ranges *it = r;
    while (it->next != NULL) {
      rune_ranges *n = it->next;
      // check if they overlap
      if (n->range.start <= it->range.end + 1) {
        it->range.end = MAX(it->range.end, n->range.end);
        it->next = n->next;
        free(n);
      } else {
        it = it->next;
      }
    }
  }

  // Join the ranges into a single list so they can be sorted
  // together.
  rune_ranges *prev = NULL;
  for (uc_gcat gc = 0; gc < GC_COUNT; gc++) {
    rune_ranges *r = ranges[gc];
    if (r == NULL) {
      continue;
    }

    rune_ranges *tail = r;
    while (tail->next != NULL) {
      tail = tail->next;
    }

    if (prev != NULL) {
      // link current to the previous
      prev->next = r;
    } else
      fst = r;
    prev = tail;
  }

  assert(fst != NULL);
  rr_sort(fst);

  // output range mapping data
  printf(ESC(#include "uc.h"\n\n));
  printf("uc_map uc_data[] = {\n");
  size_t c = 0;
  rune_ranges *it = fst;
  while (it != NULL) {
    const char *gcn = cattos(it->gc);
    printf("\t{ %ld, %ld, GC_%s },\n", it->range.start, it->range.end, gcn);
    it = it->next;
    c++;
  }
  printf("};\n\n");
  printf("size_t uc_data_size = %ld;\n", c);

bail:
  it = fst;
  while (it != NULL) {
    rune_ranges *n = it->next;
    free(it);  // initial pointer is not on heap
    it = n;
  }
  fclose(fs);

  exit(ec);
}
