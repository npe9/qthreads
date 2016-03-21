#!/bin/sh

# get only the number of physical cores
case `uname` in
    Darwin)
        NCORE=`sysctl -n hw.physicalcpu`
        ;;
    Linux)
        NCORE=`lscpu -p | egrep -v '^#' | sort -u -t, -k 2,4 | wc -l`
        ;;
    *)
        echo unsupported system 1>&2
        exit 1
esac

# this part is not going to work.
# I'm going to need to get this working.
# so I'm going to have a ton of different things I'm measuring.
# only measure system time used.
#

run()
{
    /usr/bin/time -p ./$1 2>&1 >/dev/null  | awk '{print "'$1'","'$QTHREAD_AFFINITY'",'$QTHREAD_STACK_SIZE','$QTHREAD_NUM_SHEPARDS','$QTHREAD_NUM_WORKERS_PER_SHEPHERD','$MT_COUNT','$BUFFERSIZE','$NUMITEMS','$ITERATIONS','$PAIRS',$0}'
}

# I need to add all the other potential environment variables in here. This way I can do a pca too if I need to.
QTHREAD_AFFINITY=yes
QTHREAD_STACK_SIZE=4096
QTHREAD_NUM_SHEPARDS=1
QTHREAD_NUM_WORKERS_PER_SHEPHERD=1
MT_COUNT=0
BUFFERSIZE=0
NUMITEMS=0
ITERATIONS=0
PAIRS=0
echo program QTHREAD_AFFINITY QTHREAD_STACK_SIZE QTHREAD_NUM_SHEPARDS QTHREAD_NUM_WORKERS_PER_SHEPHERD MT_COUNT BUFFERSIZE NUMITEMS ITERATIONS PAIRS time-type time
for a in yes no; do
    QTHREAD_AFFINITY=$a
    for s in `awk 'BEGIN { for(i = 8; i <= 16; i++) print 2**i }'`; do
        QTHREAD_STACK_SIZE=$s
        for i in `seq 1 $NCORE`; do
            for j in `seq 1 $NCORE`; do
                if [[ $i*$j > $NCORE ]]; then
                    break
                fi
                QTHREAD_NUM_SHEPHERDS=$i
                QTHREAD_NUM_WORKERS_PER_SHEPHERD=$j
                MT_COUNT=0
                NUMITEMS=0
                BUFFERSIZE=0

                for m in `awk 'BEGIN { for(i = 1; i <= 16; i++) for(j=0; j<10; j++)print 2**i }'`; do
                    MT_COUNT=$m
                    run precond_spawn_simple
                    run task_spawn
                    run test_spawn_simple
                done
                MT_COUNT=0
                for b in `awk 'BEGIN { for(i = 8; i <= 16; i++) print 2**i }'`; do
                    BUFFERSIZE=$b
                    for n in `seq 1 256`; do
                        NUMITEMS=$n
                        run feb_stream
                        run syncvar_stream
                    done
                done
                NUMITEMS=0
                BUFFERSIZE=0
                for it in `awk 'BEGIN { for(i = 8; i <= 16; i++) print 2**i }'`; do
                    ITERATIONS=$it
                    for p in `seq 1 32`; do
                        PAIRS=$p
                        run feb_prodcons_contended
                        run syncvar_prodcons_contended
                    done
                done
                ITERATIONS=0
                PAIRS=0
            done
        done
    done
done
