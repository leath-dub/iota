redo-ifchange $2.c test.o ../lex/liblex.a
. $(redo-ifchange ../config.env)
cc -o $3 $2.c test.o ../lex/liblex.a $CFLAGS
