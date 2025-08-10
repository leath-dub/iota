redo-ifchange ucgen.c ../config.env
. ../config.env
cc -o $3 ucgen.c $CFLAGS
