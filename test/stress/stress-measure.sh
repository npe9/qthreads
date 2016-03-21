#!/bin/sh

for f in *; do
    if [[  -x $f ]]; then
        for i in `seq 1 31`; do
            for j in `seq 1 32`; do
                if [[ $i*$j > 32 ]]; then
                    break
                fi
                QTHREAD_NUM_SHEPHERDS=$i QTHREAD_NUM_WORKERS_PER_SHEPHERD=$j ./subteams_uts| grep exec-time | awk '{print '$f','$i','$j',$2 }'; done
        done
    fi
done
