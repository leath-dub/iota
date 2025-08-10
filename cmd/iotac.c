#include <assert.h>
#include <stdio.h>

#include "../lex/lex.h"

int main(void) {
	assert(lex_id("foo9'%") == 5);
}
