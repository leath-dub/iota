redo-ifchange test.c ../config.env ../common/libcommon.a
. ../config.env
$CC -MD -MF $2.d -c -o $3 test.c $CFLAGS
read DEPS <$2.d
redo-ifchange ${DEPS#*:}
