redo-ifchange $2.py ../runner.sh ../test.py
cd ..
. ./runner.sh
run_test test.py syn/$2.py
