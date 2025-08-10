redo-ifchange $2.c test.o ../lex/liblex.a ../config.env
. ../config.env
cc -o $3 $2.c test.o ../lex/liblex.a $CFLAGS
