DEPS="../lex/lex.o"
redo-ifchange iotac.c $DEPS
cc -o $3 iotac.c $DEPS -Wall -Werror -Wextra -Wpedantic -O0 -g -I..
