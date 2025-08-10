#include <assert.h>
#include "../lex/lex.h"

int main(void) {
	uc_gcat cat = runecat('0');
	assert(cat == GC_Nd);
}
