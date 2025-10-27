if expr "$3" : "lib.*_pic.a" > /dev/null; then
  pic="_pic"
fi
objs="ast$pic.o dump$pic.o"
redo-ifchange $objs
ar rcs $3 $objs
