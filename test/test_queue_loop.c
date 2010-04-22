#ifdef HAVE_CONFIG_H
# include "config.h"		       /* for _GNU_SOURCE */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>		       /* for INT_MIN & friends (according to C89) */
#include <float.h>		       /* for DBL_MIN & friends (according to C89) */
#include <sys/time.h>
#include <time.h>
#include <qthread/qloop.h>
#include <qthread/qtimer.h>
#include "argparsing.h"

static unsigned int BIGLEN = 1000000U;
aligned_t *uia = NULL;

static void sum(qthread_t * me, const size_t startat, const size_t stopat,
		const size_t step, void *arg_)
{
    size_t i;
    aligned_t local_sum = 0;
    //printf("S%i: summing %i numbers, from %i to %i with a stride of %i\n", (int)qthread_shep(me), (int)(stopat-startat), (int)startat, (int)stopat, (int)step);
    for (i = startat; i < stopat; i += step) {
	local_sum += uia[i];
    }
    qthread_incr((aligned_t *) arg_, local_sum);
}

static const char * units[] = {
    "bytes",
    "kB",
    "MB",
    "GB",
    "TB",
    "PB",
    "EB",
    NULL
};
static char * human_readable_bytes(size_t bytes)
{
    static char str[1024];

    int unit = 0;
    double dbytes = bytes;
    while (dbytes > 1024.0 && units[unit] != 0) {
	dbytes /= 1024.0;
	unit++;
    }
    snprintf(str, 1024, "%.2f %s", dbytes, units[unit]);
    return str;
}

int main(int argc, char *argv[])
{
    size_t i;

    assert(qthread_initialize() == QTHREAD_SUCCESS);
    CHECK_VERBOSE();
    NUMARG(BIGLEN, "BIGLEN");
    future_init(128);

    {
	aligned_t uitmp = 0, uisum = 0;
	qqloop_handle_t *loophandle;
	qtimer_t t = qtimer_create();

	iprintf("allocating array of %u elements (%s)...\n", BIGLEN, human_readable_bytes(BIGLEN*sizeof(aligned_t)));
	uia = (aligned_t *) malloc(sizeof(aligned_t) * BIGLEN);
	assert(uia);
	iprintf("initializing array\n");
	for (i = 0; i < BIGLEN; i++) {
	    uia[i] = random();
	}
	qtimer_start(t);
	for (i = 0; i < BIGLEN; i++)
	    uisum += uia[i];
	qtimer_stop(t);
	iprintf("summing-serial   %u uints took %g seconds\n", BIGLEN,
		qtimer_secs(t));
	iprintf("\tsum was %lu\n", (unsigned long)uisum);
	loophandle = qt_loop_queue_create(TIMED, 0, BIGLEN, 1, sum, &uitmp);
	qtimer_start(t);
	qt_loop_queue_run(loophandle);
	qtimer_stop(t);
	iprintf("summing-parallel %u uints took %g seconds\n", BIGLEN,
		qtimer_secs(t));
	iprintf("\tsum was %lu\n", (unsigned long)uitmp);
	assert(uitmp == uisum);

	free(uia);
	qtimer_destroy(t);
    }

    return 0;
}
