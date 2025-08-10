redo-ifchange test.c
. $(redo-ifchange ../config.env)
cc -o $3 -c test.c $CFLAGS
