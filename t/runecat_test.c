#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test.h"
#include "../lex/lex.h"

int main(void) {
	FILE *fs = fopen("../lex/UnicodeData.txt", "r");
	ASSERT(fs != NULL);

	int ec = 0, lno = 0;
	char *ln = NULL;
	size_t sz = 0;
	ssize_t nrd = 0;

	while ((nrd = getline(&ln, &sz, fs)) != -1) {
		char *cps = strtok(ln, ";");
		ASSERT(cps != NULL && strlen(cps) >= 4);

		// parse the code point field
		char *ep = NULL;
		long cp = strtol(cps, &ep, 16);
		ASSERT(*ep == '\0');

		// extract "General Category" as the third field
		char *gcn = NULL;
		ASSERT(strtok(NULL, ";") != NULL && (gcn = strtok(NULL, ";")) != NULL);

		ASSERT(strcmp(gcn, gctoa(runecat(cp))) == 0);

		lno++;
	}

	if (ln != NULL) free(ln);
	fclose(fs);
	exit(ec);
}
