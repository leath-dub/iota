redo-ifchange $2.c
. $(redo-ifchange ../config.env)
cc -o $3 -c $2.c $CFLAGS
