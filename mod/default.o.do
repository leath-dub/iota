redo-ifchange $2.c ../config.env
. ../config.env
$CC -MD -MF $2.d -c -o $3 $2.c $CFLAGS
read DEPS <$2.d
redo-ifchange ${DEPS#*:}
