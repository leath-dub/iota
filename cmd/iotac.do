redo-ifchange iotac.c ../lex/liblex.a ../config.env
. ../config.env
cc -o $3 iotac.c ../lex/liblex.a $CFLAGS
