redo-ifchange syn/dump_ast runner.sh test.py
. ./runner.sh

for test in syn/*; do
  case $test in
    *.py)
      redo-ifchange ${test%.py}
  esac
done
