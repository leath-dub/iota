redo-ifchange test.c ../config.env
. ../config.env
cc -o $3 -c test.c $CFLAGS
