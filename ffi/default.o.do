src=${2%_pic}.c
if [ ! "$src" = "$2" ]; then
  EXTRA_CFLAGS="-fPIC"
fi
redo-ifchange $src ../config.env
. ../config.env
$CC -MD -MF $2.d -c -o $3 $src $CFLAGS $EXTRA_CFLAGS
read DEPS <$2.d
redo-ifchange ${DEPS#*:}
