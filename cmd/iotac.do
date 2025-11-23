libs="../syn/libsyn.a ../lex/liblex.a ../sem/libsem.a ../ast/libast.a ../common/libcommon.a ../mod/mod.o"
redo-ifchange iotac.c $libs ../config.env
. ../config.env
$CC -o $3 iotac.c $libs $CFLAGS $LDFLAGS
