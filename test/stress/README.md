# Stress Tests #

This directory contains stress tests which validate how qthreads performs under laod.

The tests are divided into:

* **feb_prodcons_contended** which tests the behavior of qthreads full/empty bit synchronization under contention
* **feb_stream** which mimics the stream benchmarks
* **precond_fib** which
* **precond_spawn_simple**
* **subteams_uts** unbalanced tree search see [this]() paper for the definition.
* **syncvar_prodcons_contended**
* **syncvar_stream**
* **task_spawn**
* **task_spawn_simple**

## generating experiments ##

## feb_prodcons_contended ##

tests the correctness of the behavior of threads under contention.

ITERATIONS
PAIRS


<arguments here>

so what are we measuring here?
what are we testing?

starts with an initial x value.

make pairs of consumers and producers.

so then you read nothing.
reading into null. 
What does that mean?

the stress testing isn't really performance for this.

so we are really just tests to see what happens when we try this.

they are using X to see what needs to get done.

## feb_stream ##

testing how streams of memory work
work on a circular buffer.

* BUFFERSIZE
* NUMITEMS

fork a producer and consumer.
then you fill
a pariticualr region of me meroy.

## precond_fib ##

Generates fibonacci nubmers to keep track of orderings (?)

* FIB_INPUT the size of fibonacci.

## precond_spawn_simple ##

* MT_COUNT number of threads to fork


## subteams_uts ##

does an unbalanced tree search which is a way of doing testing systems.

cite paper

* UTS_BF_0 bf_0? See paper
* UTS_ROOT_SEED
* UTS_TREE_DEPTH
* UTS_NON_LEAF_PROB
* UTS_NON_LEAF_NUM
* UTS_SHIFT_DEPTH
* UTS_SUBTEAM_PROB
* UTS_ROOT_CONTEXT

bf_0
root_seed
tree_depth
non_leaf_prob
non_leaf_bf
shift_depth
num_smaples
subteam_prob
root_context

## syncvar_prodcons_contended ##

Producer consumer stress test but this time using syncvars.


* ITERATIONS
* PAIRS

## syncvar_stream ##

More or less this code matches the prodcons

* BUFFERSIZE
* NUMITEMS

## task_spawn ##

test thread spawning.

* MT_COUNT

## task_spawn_simple ##

what is the difference between this and the others?

* MT_COUNT

