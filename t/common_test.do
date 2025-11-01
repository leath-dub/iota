redo-ifchange $2.c test.o ../config.env ../common/libcommon.a runner.sh
. ../config.env
. ./runner.sh
$CC -o $3 $2.c test.o ../common/libcommon.a $CFLAGS
run_test "$3"
