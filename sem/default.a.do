if expr "$3" : "lib.*_pic.a" > /dev/null; then
  pic="_pic"
fi
objs="sem$pic.o symbol_table$pic.o resolve_names$pic.o check_types$pic.o"
redo-ifchange $objs
ar rcs $3 $objs
