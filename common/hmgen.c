#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Put any types you need to specialize the hash map with in here
static const char *types[] = {
    "Point",
};
static const int types_len = sizeof(types) / sizeof(*types);

static void safe_insert(char *buf, int size, int at, char ch) {
  if (at >= size) {
    fprintf(stderr, "index out of bounds");
    exit(1);
  }
  buf[at] = ch;
}

static const char *snake_case(const char *type, char *buf, int size) {
  int oi = 0;
  for (int i = 0; type[i]; i++, oi++) {
    char ch = type[i];
    if ('A' <= ch && ch <= 'Z') {
      if (i != 0) {
        safe_insert(buf, size, oi++, '_');
      }
      ch = tolower(ch);
    }
    safe_insert(buf, size, oi, ch);
  }
  safe_insert(buf, size, oi, '\0');
  return buf;
}

static void gen_start_hdr(FILE *hdr) {
  fprintf(hdr, "#ifndef HMTYPES_H\n");
  fprintf(hdr, "#define HMTYPES_H\n\n");
  fprintf(hdr, "#include \"hm.h\"\n\n");
  fprintf(hdr, "#include <assert.h>\n\n");
}

static void gen_end_hdr(FILE *hdr) { fprintf(hdr, "#endif\n"); }

static void gen_type_hdr(FILE *hdr, const char *type) {
  static char buf[BUFSIZ] = {0};
  const char *lowtype = snake_case(type, buf, BUFSIZ);

  fprintf(hdr, "struct %s;\n\n", type);
  fprintf(hdr, "typedef struct {\n");
  fprintf(hdr, "  HashMap base;\n");
  fprintf(hdr, "} HashMap%s;\n\n", type);

  fprintf(hdr, "HashMap%s hm_%s_new(usize slots);\n", type, lowtype);
  fprintf(
      hdr,
      "struct %s *hm_%s_ensure_sz(HashMap%s *hm, string key, usize size);\n",
      type, lowtype, type);
  fprintf(hdr, "bool hm_%s_contains(HashMap%s *hm, string key);\n", lowtype,
          type);
  fprintf(hdr, "void hm_%s_free(HashMap%s *hm);\n\n", lowtype, type);
  fprintf(hdr, "#define hm_%s_ensure(hm, key) \\\n", lowtype);
  fprintf(hdr, "  hm_%s_ensure_sz(hm, key, sizeof(struct %s))\n", lowtype,
          type);
  fprintf(hdr, "#define hm_%s_put(hm, key, value) \\\n", lowtype);
  fprintf(hdr, "  (void)(*hm_%s_ensure(hm, key) = value)\n", lowtype);
  fprintf(hdr, "#define hm_%s_get(hm, key) \\\n", lowtype);
  fprintf(hdr, "  (assert(hm_%s_contains(hm, key)), \\\n", lowtype);
  fprintf(hdr, "     hm_%s_ensure(hm, key))\n\n", lowtype);
}

static void gen_start_imp(FILE *hdr) {
  fprintf(hdr, "#include \"hmtypes.h\"\n\n");
}

static void gen_end_imp(FILE *hdr) { (void)hdr; }

static void gen_type_imp(FILE *hdr, const char *type) {
  static char buf[BUFSIZ] = {0};
  const char *lowtype = snake_case(type, buf, BUFSIZ);

  fprintf(hdr, "HashMap%s hm_%s_new(usize slots) {\n", type, lowtype);
  fprintf(hdr, "  return (HashMap%s) { .base = hm_unsafe_new(slots) };\n",
          type);
  fprintf(hdr, "}\n\n");

  fprintf(
      hdr,
      "struct %s *hm_%s_ensure_sz(HashMap%s *hm, string key, usize size) {\n",
      type, lowtype, type);
  fprintf(hdr, "  Entry *entry = hm_unsafe_ensure(&hm->base, key, size);\n");
  fprintf(hdr, "  return (struct %s *)entry->data;\n", type);
  fprintf(hdr, "}\n\n");

  fprintf(hdr, "bool hm_%s_contains(HashMap%s *hm, string key) {\n", lowtype,
          type);
  fprintf(hdr, "  return hm_unsafe_contains(&hm->base, key);\n");
  fprintf(hdr, "}\n\n");

  fprintf(hdr, "void hm_%s_free(HashMap%s *hm) {\n", lowtype, type);
  fprintf(hdr, "  hm_unsafe_free(&hm->base);\n");
  fprintf(hdr, "}\n");
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "usage: hmgen <h|c> <file path>\n");
    exit(2);
  }

  bool imp = false;
  const char *h_or_c = argv[1];
  switch (*h_or_c) {
    case 'h':
      imp = false;
      break;
    case 'c':
      imp = true;
      break;
  }

  const char *path = argv[2];

  FILE *file = fopen(path, "w");
  if (file == NULL) {
    int code = errno;
    fprintf(stderr, "error: opening file %s: errno %d\n", path, code);
    exit(2);
  }

  if (!imp) {
    gen_start_hdr(file);
    for (int i = 0; i < types_len; i++) {
      gen_type_hdr(file, types[i]);
    }
    gen_end_hdr(file);
  } else {
    gen_start_imp(file);
    for (int i = 0; i < types_len; i++) {
      gen_type_imp(file, types[i]);
    }
    gen_end_imp(file);
  }

  fclose(file);
}
