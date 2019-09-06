#include <stddef.h>
#include "predict_impl.h"

/*
 * This function inserts the given value into the list of most recently seen
 * data and returns the parameters, m and c, of a straight line of the form
 * y = mx + c that, according to the the method of least squares, fits them
 * best.  The formulation is for the special case in which x_i = i + 1 - N;
 * this reduces the need for storage and permits constant time updates.
 */
static int
lsq_fit(struct lsq_struct *lsq, long long new_y, long long *m, long long *c)
{
	unsigned long long oldest_y;

	oldest_y = lsq->y[lsq->slot];

	lsq->sum_xy -= lsq->sum_y;
	if (lsq->ready)
		lsq->sum_xy += COUNT * oldest_y;

	lsq->sum_y += new_y;
	if (lsq->ready)
		lsq->sum_y -= oldest_y;

	lsq->y[lsq->slot++] = new_y;

	if (lsq->slot == COUNT) {
		lsq->slot = 0;
		lsq->ready = 1;
	}

	if (!lsq->ready)
		return -1;

	*m = (COUNT * lsq->sum_xy - SUM_X * lsq->sum_y) /
	    (COUNT * SUM_XX - SUM_X * SUM_X);

	*c = (lsq->sum_y - *m * SUM_X) / COUNT;

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
struct prediction_struct *
predict(unsigned long *free, struct lsq_struct *lsq, int threshold, int R_c,
    struct prediction_struct *p)
{
	int order;
	long long m[MAX_ORDER];
	long long c[MAX_ORDER];
	int is_ready = 1;

	for (order = 0; order < MAX_ORDER; order++) {
		if (lsq_fit(&lsq[order], free[order], &m[order],
		    &c[order]) == -1)
			is_ready = 0;
	}

	if (!is_ready)
		return NULL;

	p->f_T_zero = c[0];
	p->R_T = m[0];

	for (order = 1; order < MAX_ORDER; order++) {

		p->order = order;
		p->f_f_zero = c[order];
		p->R_f = m[order];

		if (p->f_T_zero <= p->f_f_zero)
			continue;

		/*
		 * There are only two reasons to begin compaction immediately,
		 * i.e. at the beginning of this interval.  The first is that
		 * the alternative would be exhaustion before the beginning of
		 * the next interval.
		 */
		if (p->R_T < p->R_f) {
			p->t_e = (p->f_T_zero - p->f_f_zero) /
			    (p->R_f - p->R_T);
			if (p->t_e < 1) {
				/*
				 * Don't bother compacting if the expected
				 * fragmentation improves upon the given
				 * threshold.
				 */
				p->f_e = (p->f_T_zero - p->f_f_zero) *
				    p->R_T / (p->R_f - p->R_T) +
				    p->f_T_zero;
				if (p->f_e > threshold) {
					p->type = TYPE_ONE;
					return p;
				}
			}
		}

		/*
		 * The second reason is that deferring compaction until the
		 * start of the next interval would result, at the time of
		 * exhaustion, in a surfeit of free fragmented memory above the
		 * desired threshold.
		 */
		if (p->R_T < p->R_f + R_c) {
			p->t_e = (p->f_T_zero - p->f_f_zero + R_c) /
			    (p->R_f + R_c - p->R_T);
			if (p->t_e > 1) {
				p->f_e = (p->f_T_zero - p->f_f_zero + R_c) *
				    p->R_T / (p->R_f + R_c - p->R_T) +
				    p->f_T_zero;
				if (p->f_e > threshold) {
					p->type = TYPE_TWO;
					return p;
				}
			}
		}
	}

	return NULL;
}
