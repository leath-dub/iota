libs="../syn/libsyn.a ../lex/liblex.a ../ast/ast.o ../common/common.o ../mod/mod.o"
redo-ifchange iotac.c $libs ../config.env
. ../config.env
$CC -o $3 iotac.c $libs $CFLAGS
