#ifndef SCANNER_H_
#define SCANNER_H_

#include "uc.h"

#include <stddef.h>

uc_gcat runecat(rune cp);
const char *gctoa(uc_gcat cat);

#endif

// vim: ft=c
