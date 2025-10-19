redo-ifchange runner.sh

TESTS=""
for test in syn/*; do
  case $test in
    *.py)
      TESTS="$TESTS ${test%.py}"
  esac
done

redo-ifchange runner.sh $TESTS
