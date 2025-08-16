TESTS="runecat_test lex_test"
redo-ifchange $TESTS
for test in $TESTS; do
  ./$test
done
