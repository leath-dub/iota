libs="../lex/liblex.a ../syn/libsyn.a ../common/common.o ../mod/mod.o"
redo-ifchange iotac.c $libs ../config.env
. ../config.env
cc -o $3 iotac.c $libs $CFLAGS
