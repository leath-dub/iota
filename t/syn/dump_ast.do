DEPS="../test.o ../../syn/libsyn.a ../../lex/liblex.a ../../common/common.o ../../mod/mod.o"
redo-ifchange $2.c ../../config.env $DEPS
. ../../config.env
$CC -o $3 $2.c $DEPS $CFLAGS
