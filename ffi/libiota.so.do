arcs="../lex/liblex_pic.a ../syn/libsyn_pic.a ../ast/libast_pic.a ../common/libcommon_pic.a"
objs="../mod/mod_pic.o shim_pic.o"
redo-ifchange $arcs $objs ../config.env
. ../config.env
$CC -shared -o $3 $objs -Wl,--whole-archive $arcs -Wl,--no-whole-archive
