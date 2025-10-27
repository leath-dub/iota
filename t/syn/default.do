redo-ifchange $2.py ../test.py ../../ffi/iotalib.py ../../ffi/libiota.so
cd ..
./test.py syn/$2.py
