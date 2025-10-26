redo-ifchange $2.c test.o ../lex/liblex.a ../common/common.o ../config.env runner.sh
. ../config.env
. ./runner.sh
$CC -o $3 $2.c test.o ../lex/liblex.a ../common/common.o $CFLAGS
run_test "$3"
