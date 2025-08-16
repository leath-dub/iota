redo-ifchange $2.c test.o ../config.env common.o runner.sh
. ../config.env
. ./runner.sh
cc -o $3 $2.c test.o common.o $CFLAGS
run_test "$3"
