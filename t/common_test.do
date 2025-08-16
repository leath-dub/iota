redo-ifchange $2.c test.o ../config.env ../common/common.o runner.sh
. ../config.env
. ./runner.sh
cc -o $3 $2.c test.o ../common/common.o $CFLAGS
run_test "$3"
