redo-ifchange iotac.c ../lex/liblex.a
. $(redo-ifchange ../config.env)
cc -o $3 iotac.c ../lex/liblex.a $CFLAGS
