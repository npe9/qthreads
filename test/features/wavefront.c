#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <qthread/qthread.h>
#include <qthread/wavefront.h>
#include "argparsing.h"

static size_t ASIZE = 2000;

double **bigR = NULL;
/* note that the bottom-left corner is item 0 of the below array */

static void avg(const void *restrict left, const void *restrict leftdown,
                const void *restrict down, void *restrict out)
{
		*(double *)out =
		        (*(double *)left + *(double *)leftdown + *(double *)down) / 3.0;
}

static void assignrand(const size_t startat, const size_t stopat, qarray * a,
                       void *arg)
{
		double *ptr = (double *)qarray_elem_nomigrate(a, startat);
		const size_t max = stopat - startat;

		assert(a->unit_size == sizeof(double));

		for (size_t i = 0; i < max; i++) {
				/*long r = random();
				 * memcpy(&(ptr[i]), &r, sizeof(long)); */
				ptr[i] = (double)(i + startat);
		}
}

int main(int argc, char *argv[])
{
		qarray *v, *h;
		qt_wavefront_lattice *L;

		assert(qthread_initialize() == QTHREAD_SUCCESS);
		CHECK_VERBOSE();
		NUMARG(ASIZE, "TEST_ASIZE");
		iprintf("ASIZE: %i\n", (int)ASIZE);
		iprintf("%i threads\n", qthread_num_shepherds());

		v = qarray_create_configured(ASIZE, sizeof(double), FIXED_HASH, 1, 1);
		h = qarray_create_configured(ASIZE + 1, sizeof(double), FIXED_HASH, 1, 1);

		qarray_iter_loop(h, 1, ASIZE + 1, assignrand, NULL);
		qarray_iter_loop(v, 0, ASIZE, assignrand, NULL);
		iprintf("v items per seg: %i\n", (int)v->segment_size);

		/* do stuff */
		L = qt_wavefront(v, h, avg);

		if (L) {
				// qt_wavefront_print_lattice(L);
		} else {
				fprintf(stderr, "wavefront returned NULL!\n");
		}
		qarray_destroy(v);
		qarray_destroy(h);
		qt_wavefront_destroy_lattice(L);
		return 0;
}

/* vim:set expandtab */