redo-ifchange $2.c test.o ../lex/liblex.a ../config.env runner.sh
. ../config.env
. ./runner.sh
cc -o $3 $2.c test.o ../lex/liblex.a $CFLAGS
run_test "$3"
