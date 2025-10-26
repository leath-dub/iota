redo-ifchange ucgen.c ../config.env
. ../config.env
$CC -o $3 ucgen.c $CFLAGS
