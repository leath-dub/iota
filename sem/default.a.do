if expr "$3" : "lib.*_pic.a" > /dev/null; then
  pic="_pic"
fi
objs="post_parse$pic.o symbol_table$pic.o"
redo-ifchange $objs
ar rcs $3 $objs
