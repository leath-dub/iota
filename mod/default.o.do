redo-ifchange $2.c ../common/common.h ../config.env
. ../config.env
cc -o $3 -c $2.c $CFLAGS
