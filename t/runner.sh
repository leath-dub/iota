#!/bin/sh

run_test() {
  mkdir -p /tmp/iota-test-logs
  valgrind_log="/tmp/iota-test-logs/valgrind.$$.log"
  set +e
  valgrind --leak-check=full \
           --show-leak-kinds=all \
           --track-origins=yes \
           --error-exitcode=42 \
           --exit-on-first-error=yes \
           --log-file="$valgrind_log" \
           ./"$1"
  status="$?"
  if [ "$status" = "42" ]; then
    cat "$valgrind_log" 1>&2
  fi
  exit "$status"
}
