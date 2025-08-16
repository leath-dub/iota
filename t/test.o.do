redo-ifchange test.c ../config.env ../common/common.o
. ../config.env
cc -o $3 -c test.c $CFLAGS
