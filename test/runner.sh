#!/bin/sh

for i in benchmarks/* stress/*; do
    if !test -x $i; then
        continue;
    fi
    echo '1 32
    2 16
    32 1' | awk '{ print "QTHREAD_NUM_SHEPHERDS="$1" QTHREAD_NUM_WORKERS_PER_SHEPHERD="$2" ./'$i' >/dev/null" }' | sh
done


