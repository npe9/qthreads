#include <qthread/allpairs.h>
#include <qthread/qthread.h>
#include <qthread/qdqueue.h>

#include <unistd.h>		       /* for getpagesize() */
#include <assert.h>
#include <stdlib.h>		       /* for malloc() */
#include <stdio.h>		       /* for printf */

struct qt_ap_wargs
{
    qdqueue_t *restrict work_queue;
    const dist_f distfunc;
    volatile aligned_t *restrict const no_more_work;
    volatile aligned_t *restrict const donecount;
    const qarray *a1;
    const qarray *a2;
    void *restrict * restrict output;
    const size_t outsize;
};

struct qt_ap_workunit
{
    size_t a1_start, a1_stop, a2_start, a2_stop;
};

aligned_t qt_ap_worker(qthread_t * restrict me,
		       struct qt_ap_wargs *restrict args)
{
    while (1) {
	struct qt_ap_workunit *restrict const wu =
	    qdqueue_dequeue(me, args->work_queue);
	if (wu == NULL) {
	    if (*(args->no_more_work)) {
		qthread_incr(args->donecount, 1);
		break;
	    }
	    qthread_yield(me);
	} else {
	    const char *a1_base =
		qarray_elem_nomigrate(args->a1, wu->a1_start);
	    const char *a2_base =
		qarray_elem_nomigrate(args->a2, wu->a2_start);
	    const size_t a1_usize = args->a1->unit_size;
	    const size_t a2_usize = args->a2->unit_size;
	    size_t a1_i;

	    for (a1_i = 0; a1_i < (wu->a1_stop - wu->a1_start); a1_i++) {
		const char *restrict const this_a1_base = a1_base + (a1_i * a1_usize);
		char *restrict const this_outbase =
		    args->output[a1_i + wu->a1_start];
		size_t a2_i;

		for (a2_i = 0; a2_i < (wu->a2_stop - wu->a2_start); a2_i++) {
		    args->distfunc(this_a1_base, a2_base + (a2_i * a2_usize),
				   this_outbase +
				   ((a2_i + wu->a2_start) * args->outsize));
		}
	    }
	    free(wu);
	}
    }
    return 0;
}

struct qt_ap_gargs
{
    qdqueue_t *restrict wq;
    const qarray *restrict array2;
};

struct qt_ap_gargs2
{
    qdqueue_t *restrict wq;
    const size_t start, stop;
};

void qt_ap_genwork2(qthread_t * restrict me, const size_t startat,
		    const size_t stopat, const qarray * restrict a,
		    struct qt_ap_gargs2 *restrict gargs)
{
    struct qt_ap_workunit *workunit = malloc(sizeof(struct qt_ap_workunit));

    workunit->a1_start = gargs->start;
    workunit->a1_stop = gargs->stop;
    workunit->a2_start = startat;
    workunit->a2_stop = stopat;

    qdqueue_enqueue(me, gargs->wq, workunit);
}

void qt_ap_genwork(qthread_t * restrict me, const size_t startat,
		   const size_t stopat, const qarray * restrict a,
		   struct qt_ap_gargs *restrict gargs)
{
    struct qt_ap_gargs2 garg2 = { gargs->wq, startat, stopat };

    qarray_iter_constloop(me, gargs->array2, 0, gargs->array2->count,
			  (qa_cloop_f) qt_ap_genwork2, &garg2);
}

/* The setup is this: we have two qarrays ([array1] and [array2]). We want to
 * call [distfunc] on each pair of elements. This function will produce a
 * result that is [outsize] bytes. Calling the function on each pair will
 * produce a result which is stored in the [output] 2-d array.
 *
 * To make this faster...
 * 1. We use qarray to chunk the work up into bite-size pieces
 * 2. We feed the work chunks into a qdqueue (currently, into locations defined by the array2 distribution)
 * 3. Per-shepherd worker threads pull out work and execute it, ideally near the data they work on.
 */
void qt_allpairs(const qarray * array1, const qarray * array2,
		 void *restrict * restrict output, const size_t outsize,
		 const dist_f distfunc)
{
    const qthread_shepherd_id_t max_i = qthread_num_shepherds();
    volatile aligned_t no_more_work = 0;
    volatile aligned_t donecount = 0;
    struct qt_ap_wargs wargs =
	{ NULL, distfunc, &no_more_work, &donecount, array1, array2, output,
	outsize
    };
    struct qt_ap_gargs gargs = { NULL, array2 };
    qthread_shepherd_id_t i;
    qthread_t *const me = qthread_self();

    assert(array1);
    assert(array2);

    /* step 1: set up work queue */
    gargs.wq = wargs.work_queue = qdqueue_create();
    /* step 2: spawn workers */
    for (i = 0; i < max_i; i++) {
	qthread_fork_to((qthread_f) qt_ap_worker, &wargs, NULL, i);
    }
    /* step 3: feed work into queue */
    qarray_iter_constloop(me, array1, 0, array1->count,
			  (qa_cloop_f) qt_ap_genwork, &gargs);
    /* step 4: wait for the workers to get done */
    no_more_work = 1;
    while (donecount < max_i) {
	qthread_yield(me);
    }
}
