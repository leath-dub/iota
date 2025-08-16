#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test.h"
#include "../lex/uc.h"

int main(void) {
	FILE *fs = fopen("../lex/UnicodeData.txt", "r");
	ASSERT(fs != NULL);

	u32 ec = 0, lno = 0;
	static char ln[BUFSIZ] = {0};
	size_t sz = BUFSIZ;

	while (fgets(ln, sz, fs)) {
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

	fclose(fs);
	exit(ec);
}
