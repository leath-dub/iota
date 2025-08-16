objs="lex.o uc.o uc_data.o ../common/common.o"
redo-ifchange $objs
ar rcs $3 $objs
