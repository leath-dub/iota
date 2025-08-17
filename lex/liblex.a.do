objs="lex.o uc.o uc_data.o"
redo-ifchange $objs ../common/common.o
ar rcs $3 $objs
