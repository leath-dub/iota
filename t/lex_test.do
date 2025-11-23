redo-ifchange $2.c test.o ../config.env ../lex/liblex.a ../common/libcommon.a ../mod/mod.o ../mod/mod.h runner.sh
. ../config.env
. ./runner.sh
$CC -o $3 $2.c test.o ../lex/liblex.a ../common/libcommon.a ../mod/mod.o $CFLAGS
run_test "$3"
