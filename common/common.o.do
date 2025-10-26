redo-ifchange common.c ../config.env
. ../config.env
$CC -o $3 -c common.c $CFLAGS
