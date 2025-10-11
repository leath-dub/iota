redo-ifchange syn/dump_ast runner.sh test.py
for test in syn/*; do
  case $test in
    *.py) redo-ifchange $test
  esac
done
. ./runner.sh
run_test test.py
