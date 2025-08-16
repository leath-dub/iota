libs="../lex/liblex.a ../syn/libsyn.a"
redo-ifchange iotac.c $libs ../config.env
. ../config.env
cc -o $3 iotac.c $libs $CFLAGS
