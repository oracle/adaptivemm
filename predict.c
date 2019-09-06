#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "predict_impl.h"

unsigned long mempredict_threshold = 1000;

#define MEMRECLAIM_THRESHOLD 20

/*
 * This function inserts the given value into the list of most recently seen
 * data and returns the parameters, m and c, of a straight line of the form
 * y = mx + c that, according to the the method of least squares, fits them
 * best.  The formulation is for the special case in which x_i = i + 1 - N;
 * this reduces the need for storage and permits constant time updates.
 */
static int
lsq_fit(struct lsq_struct *lsq, long long new_y, long long new_x,
	long long *m, long long *c)
{
	long long sigma_x, sigma_y;
	long sigma_xy, sigma_xx;
	long long slope_divisor;
	int i, next;
	long x_offset;

	next = lsq->next++;
	lsq->x[next] = new_x;
	lsq->y[next] = new_y;

	if (lsq->next == COUNT) {
		lsq->next = 0;
		lsq->ready = 1;
	}

	if (!lsq->ready)
		return -1;

	x_offset = lsq->x[lsq->next];
	for (i = 0; i < COUNT; i++)
		lsq->x[i] -= x_offset;

	sigma_x = sigma_y = sigma_xy = sigma_xx = 0;
	for (i = 0; i < COUNT; i++) {
		sigma_x += lsq->x[i];
		sigma_y += lsq->y[i];
		sigma_xy += (lsq->x[i] * lsq->y[i]);
		sigma_xx += (lsq->x[i] * lsq->x[i]);
	}

	slope_divisor = COUNT * sigma_xx - sigma_x * sigma_x;
	if (slope_divisor == 0)
		return -1;

	*m = ((COUNT * sigma_xy - sigma_x * sigma_y) * 100) / slope_divisor;
	*c = (sigma_y - *m * sigma_x) / COUNT;

	for (i = 0; i < COUNT; i++)
		lsq->x[i] += x_offset;

	return 0;
}

/*
 * This function determines whether it is necessary to begin compaction now in
 * order to avert exhaustion of any of the free lists.  Its basis is a simple
 * model in which the total free memory, f_T, is consumed at a constant rate,
 * R_T, i.e.
 *
 *	f_T(t) = R_T t + f_T(0)
 *
 * For any given order, o, members of subordinate lists constitute fragmented
 * free memory, f_f(o):  the blocks are notionally free but they are unavailable
 * for allocation.  The fragmented free memory is also assumed to behave
 * linearly and in the absence of compaction is given by
 *
 *	f_f(o, t) = R_f(o) t + f_f(o, 0)
 *
 * Compaction is assumed to proceed at a constant rate, R_c, that is independent
 * of order.
 *
 * It is assumed that all allocations will be made from contiguous memory
 * meaning that, under net memory pressure and with no change in fragmentation,
 * f_T will become equal to f_f and subsequent allocations will stall in either
 * direct compaction or reclaim.  Preemptive compaction will delay the onset of
 * exhaustion but, to be useful, must begin early enough and must proceed at a
 * sufficient rate.
 *
 * On each invocation, this function obtains estimates for the parameters
 * f_T(0), R_T, f_f(o, 0) and R_F(o).  It determines whether, if R_T and R_f(o)
 * remain constant and compaction begins at the next invocation, f_T(t) and
 * f_f(o, t) will intersect in the future at a point corresponding to a level of
 * free memory above some pre-defined limit that represents the lowest
 * achievable degree of fragmentation.  If this is the case then the function
 * indicates that compaction should begin now by returning a pointer to the same
 * structure passed in, now populated with parameters describing the anticipated
 * exhaustion.  The function returns NULL if no action is needed before the
 * start of the next time interval.
 *
 * Parameters f_T(0), R_T, f(o, 0), R_f(o) are determined by using the method of
 * least squares to fit a straight line to the most recently obtained data. R_c
 * is determined from [XXX: the vmstats already include a cumulative total for
 * the number of pages obtained;  augment them with the cumulative time spent in
 * compaction and recalculate a, presumably convergent, average each time here].
 * The threshold is [XXX: a very good starting point will be the value of f_f
 * after the completion of an entire compaction pass].
 */
unsigned long
predict(struct frag_info *frag_vec, struct lsq_struct *lsq, int threshold, int R_c,
	struct zone_hash_entry *zhe, unsigned long *scale_wmark)
{
	int order;
	long long m[MAX_ORDER];
	long long c[MAX_ORDER];
	int is_ready = 1;
	unsigned long retval = 0;

	for (order = 0; order < MAX_ORDER; order++) {
		if (lsq_fit(&lsq[order], frag_vec[order].free_pages,
		frag_vec[order].msecs, &m[order], &c[order]) == -1)
			is_ready = 0;
	}

	if (!is_ready)
		return retval;

	if (frag_vec[0].free_pages < zhe->high) {
		retval |= MEMPREDICT_RECLAIM;
	}

	if (m[0] >= 0) {
		for (order = 1; order < MAX_ORDER; order++) {
			if (m[0] == m[order])
				continue;

			long long x_cross = ((c[0] - c[order]) * 100) / (m[order] - m[0]);

			if ((x_cross < mempredict_threshold) && (x_cross > -mempredict_threshold)) {
				retval |= MEMPREDICT_COMPACT;
				return retval;
			}
		}
	}
	else {
		/*
		 * Time taken to go below high_wmark.
		 */
		unsigned long time_taken = (zhe->high - c[0]) / m[0];

		/*
		 * Time to reclaim frag_vec[0].free_pages - zhe->high
		 */
		unsigned long time_to_reclaim = (frag_vec[0].free_pages - zhe->high) / reclaim_rate;

		/*
		 * If time taken to go below high_wmark is greater than
		 * the time taken to reclaim the pages then we need to start kswapd
		 * now.
		 */
		if (time_taken > time_to_reclaim) {
			*scale_wmark = (frag_vec[0].free_pages - zhe->high);
			retval |= MEMPREDICT_RECLAIM;
		}

	}

	return retval;
}
