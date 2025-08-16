redo-ifchange common.c ../config.env
. ../config.env
cc -o $3 -c common.c $CFLAGS
