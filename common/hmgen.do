redo-ifchange hmgen.c ../config.env
. ../config.env
$CC -o $3 hmgen.c $CFLAGS
