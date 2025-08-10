#include <assert.h>
#include <stdio.h>

#include "../lex/lex.h"

int main(void) {
	assert(lex_id(u8"8foo_har") == 0);
}
