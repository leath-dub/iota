TESTS=""
for test in snapshots/in/*; do
  case $test in
    *.py)
      TESTS="$TESTS ${test%.ta}"
  esac
done

redo-ifchange $TESTS ./snaps.py
./snaps.py test
