#!/bin/sh

run_test() {
  mkdir -p /tmp/iota-test-logs
  valgrind_log="/tmp/iota-test-logs/valgrind.$$.log"
  valgrind --leak-check=full \
           --show-leak-kinds=all \
           --track-origins=yes \
           --error-exitcode=42 \
           --exit-on-first-error=yes \
           --log-file="$valgrind_log" \
           ./"$1" || {
    # Output valgrind logs only if valgrind caused the non zero exit
    if test "$?" = "42"; then
      cat "$valgrind_log" 1>&2
    fi
    exit $?
  }
}
