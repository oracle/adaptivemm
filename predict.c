/*
 *  * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 *  * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *  *
 *  * This code is free software; you can redistribute it and/or modify it
 *  * under the terms of the GNU General Public License version 2 only, as
 *  * published by the Free Software Foundation.
 *  *
 *  * This code is distributed in the hope that it will be useful, but WITHOUT
 *  * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  * version 2 for more details (a copy is included in the LICENSE file that
 *  * accompanied this code).
 *  *
 *  * You should have received a copy of the GNU General Public License version
 *  * 2 along with this work; if not, write to the Free Software Foundation,
 *  * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *  *
 *  * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 *  * or visit www.oracle.com if you need additional information or have any
 *  * questions.
 *  */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "predict.h"

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

	if (lsq->next == LSQ_LOOKBACK) {
		lsq->next = 0;
		/*
		 * Lookback window is fill which means a reasonable
		 * best fit line can be computed. Flag enough data
		 * is available in lookback window now.
		 */
		lsq->ready = 1;
	}

	/*
	 * If lookback window is not full, do not continue with
	 * computing slope and intercept of best fit line.
	 */
	if (!lsq->ready)
		return -1;

	/*
	 * If lookback window is full, compute slope and intercept
	 * for the best fit line. In the process of computing those, we need
	 * to compute squares of values along x-axis. Sqaure values can be
	 * large enough to overflow 64-bits if they are large enough to
	 * begin with. To solve this problem, transform the line on
	 * x-axis so the first point falls at x=0. Since lsq->x is a
	 * circular buffer, lsq->next points to the oldest entry in this
	 * buffer.
	 */

	x_offset = lsq->x[lsq->next];
	for (i = 0; i < LSQ_LOOKBACK; i++)
		lsq->x[i] -= x_offset;

	/*
	 * Lookback window is full. Compute slope and intercept
	 * for the best fit line
	 */
	sigma_x = sigma_y = sigma_xy = sigma_xx = 0;
	for (i = 0; i < LSQ_LOOKBACK; i++) {
		sigma_x += lsq->x[i];
		sigma_y += lsq->y[i];
		sigma_xy += (lsq->x[i] * lsq->y[i]);
		sigma_xx += (lsq->x[i] * lsq->x[i]);
	}

	/*
	 * guard against divide-by-zero
	 */
	slope_divisor = LSQ_LOOKBACK * sigma_xx - sigma_x * sigma_x;
	if (slope_divisor == 0)
		return -1;

	*m = ((LSQ_LOOKBACK * sigma_xy - sigma_x * sigma_y) * 100) / slope_divisor;
	*c = (sigma_y - *m * sigma_x) / LSQ_LOOKBACK;

	/*
	 * Restore original values for x-axis
	 */
	for (i = 0; i < LSQ_LOOKBACK; i++)
		lsq->x[i] += x_offset;

	return 0;
}

/*
 * This function determines whether it is necessary to begin
 * reclamation/compaction now in order to avert exhaustion of any of the
 * free lists.
 *
 * Its basis is a simple model in which the total free memory, f_T, is
 * consumed at a constant rate, R_T, i.e.
 *
 *	f_T(t) = R_T * t + f_T(0)
 *
 * For any given order, o > 0, members of subordinate lists constitute
 * fragmented free memory, f_f(o): the blocks are notionally free but
 * they are unavailable for allocation. The fragmented free memory is
 * also assumed to behave linearly and in the absence of compaction is
 * given by
 *
 *	f_f(o, t) = R_f(o) t + f_f(o, 0)
 *
 * Order 0 function represents current trend line for total free pages
 * instead.
 *
 * It is assumed that all allocations will be made from contiguous
 * memory meaning that, under net memory pressure and with no change in
 * fragmentation, f_T will become equal to f_f and subsequent allocations
 * will stall in either direct compaction or reclaim. Preemptive compaction
 * will delay the onset of exhaustion but, to be useful, must begin early
 * enough and must proceed at a sufficient rate.
 *
 * On each invocation, this function obtains estimates for the
 * parameters f_T(0), R_T, f_f(o, 0) and R_f(o). Using the best fit
 * line, it then determines if reclamation or compaction should be started
 * now to avert free pages exhaustion or severe fragmentation. Return value
 * is a set of bits which represent which condition has been observed -
 * potential free memory exhaustion, and potential severe fragmentation.
 */
unsigned long
predict(struct frag_info *frag_vec, struct lsq_struct *lsq,
	unsigned long high_wmark, unsigned long low_wmark, int nid)
{
	int order;
	long long m[MAX_ORDER];
	long long c[MAX_ORDER];
	int is_ready = 1;
	unsigned long retval = 0;
	unsigned long time_taken, time_to_catchup;
	long long x_cross, current_time;
	struct timespec tspec;


	/*
	 * Compute the trend line for fragmentation on each order page.
	 * For order 0 pages, it will be a trend line showing rate
	 * of consumption of pages. For higher order pages, trend line
	 * shows loss/gain of pages of that order. When the trend line
	 * for example for order n pages intersects with trend line for
	 * total free pages, it means all available pages are of order
	 * (n-1) or lower and there is 100% fragmentation of order n
	 * pages. Kernel must compact pages at this point to gain
	 * new order n pages.
	 */
	for (order = 0; order < MAX_ORDER; order++) {
		if (lsq_fit(&lsq[order], frag_vec[order].free_pages,
				frag_vec[order].msecs, &m[order],
				&c[order]) == -1)
			is_ready = 0;
	}

	if (!is_ready)
		return retval;

#if 0
	if (frag_vec[0].free_pages < high_wmark) {
		retval |= MEMPREDICT_RECLAIM;
		return retval;
	}
#endif

	/*
	 * Trend line for each order page is available now. If the trend
	 * line for overall free pages is trending upwards (positive
	 * slope), there is no need to reclaim pages but there may be
	 * need to compact pages if system is running out of contiguous pages
	 * for higher orders.
	 */
	if (m[0] >= 0) {
		/*
		 * Since number of free pages is going up, it is
		 * time to adjust watermarks down.
		 */
		retval |= MEMPREDICT_LOWER_WMARKS;
	}
	else {
		/*
		 * Trend line for overall free pages is showing a
		 * negative trend. Check if we are approaching high
		 * watermark faster than pages are being reclaimed.
		 * If a reclaim rate has not been computed yet, do not
		 * compute if it is time to start reclamation
		 */
		if (reclaim_rate == 0)
			return 0;

		/*
		 * If number of free pages is already below high watermark,
		 * it is time to kick off reclamation. If not, compute
		 * time it will take to go below high_wmark.
		 */
		if (frag_vec[0].free_pages <= high_wmark) {
			retval |= MEMPREDICT_RECLAIM;
			log_info(2, "Reclamation recommended due to free pages being below high watermark");
			log_info(2, "Consumption rate on node %d=%ld pages/msec, reclaim rate is %ld pages/msec, Free pages=%ld, low wmark=%ld, high wmark=%ld", nid, abs(m[0]), reclaim_rate, frag_vec[0].free_pages, low_wmark, high_wmark);
		}
		else {
			time_taken = (frag_vec[0].free_pages - high_wmark)
					/ abs(m[0]);

			/*
			 * Time to reclaim frag_vec[0].free_pages - high_wmark
			 */
			time_to_catchup = (frag_vec[0].free_pages -
						high_wmark) / reclaim_rate;

			/*
			 * If time taken to go below high_wmark is fairly
			 * close to time it will take to reclaim enough
			 * then we need to start kswapd now. We will use
			 * 3 times of the time to catch up as threshold
			 * just to ensure reclamationprocess has enough time.
			 * This helps eliminate the possibility of forcing
			 * reclamation when time to go below high watermark
			 * is too far in the future.
			 */
			if (time_taken <= (3*time_to_catchup)) {
				log_info(3, "Reclamation recommended due to high memory consumption rate");
				log_info(3, "Consumption rate on node %d=%ld pages/msec, reclaim rate is %ld pages/msec, Free pages=%ld, low wmark=%ld, high wmark=%ld", nid, abs(m[0]), reclaim_rate, frag_vec[0].free_pages, low_wmark, high_wmark);
				log_info(3, "Time to below high watermark= %ld msec, time to catch up=%ld msec", time_taken, time_to_catchup);
				retval |= MEMPREDICT_RECLAIM;
			}
		}
	}

	/*
	 * Check if system is running low on higher order pages and needs
	 * compaction.
	 */
	for (order = max_compaction_order; order > 0; order--) {
		/*
		 * If lines are parallel, then they never intersect.
		 */
		if (m[0] == m[order])
			continue;

		/*
		 * If number of high order pages is increasing, no
		 * need to take any action
		 */
		if (m[order] < 0)
			continue;

		/*
		 * If system is unable to comapct pages, no point
		 * in forcing compaction
		 */
		if (compaction_rate == 0)
			return 0;

		/*
		 * Find the point of intersection of the two lines.
		 * The point of intersection represents 100%
		 * fragmentation for this order.
		 */
		x_cross = ((c[0] - c[order]) * 100) /
					(m[order] - m[0]);
#if 0
		y_cross = ((m[order] * c[0]) - (m[0] * c[order])) /
				(m[order] - m[0]);
#endif

		/*
		 * If they intersect anytime soon in the future
		 * or intersected recently in the past, then it
		 * is time for compaction and there is no need
		 * to continue evaluating remaining order pages.
		 * If intersection was in the past, it can be
		 * outside of current lookback window which means
		 * x_cross can be negative.
		 *
		 * To check if intersection is in near future,
		 * get the current time relative to x=0 on our
		 * graph.
		 */
		clock_gettime(CLOCK_MONOTONIC_RAW, &tspec);
		current_time = tspec.tv_sec*1000 + tspec.tv_nsec/1000 - lsq->x[lsq->next];;
		if (current_time < 0)
			current_time = 0;
		if ((x_cross < 0) ||
			(x_cross < current_time)) {
			unsigned long higher_order_pages =
				frag_vec[MAX_ORDER - 1].free_pages -
				frag_vec[order].free_pages;

			/*
			 * Check if there are enough higher order pages
			 * that could be broken into pages of current order
			 * given current rate of consumption and time
			 * remaining. If not, comapct now.
			 */
			if (higher_order_pages < (m[order] * x_cross)) {
				log_info(2, "Compaction recommended on node %d. Running out of order %d pages", nid, order);
				if (order < (MAX_ORDER -1))
					log_info(3, "No. of free order %d pages = %ld base pages, consumption rate=%ld pages/msec", order, (frag_vec[order+1].free_pages - frag_vec[order].free_pages), m[order]);
				 log_info(3, "Current compaction rate=%ld pages/msec", compaction_rate);
				retval |= MEMPREDICT_COMPACT;
				break;
			}
		}
		else {
			/*
			 * How long before we run out of current order
			 * pages.  We will constrain the size of window
			 * we look forward in since large window means
			 * the consumption trend can change in that time.
			 * It will be prudent to defer the decision to
			 * initiate compaction until the exhaustion period
			 * falls within this window.
			 */
			unsigned long largest_window;

			largest_window = 5 * LSQ_LOOKBACK * periodicity * 1000;
			time_taken = x_cross - current_time;
			if (time_taken > largest_window)
				continue;

			/*
			 * How long will it take to compact as many pages as
			 * available current order pages
			 */
			time_to_catchup = (frag_vec[order+1].free_pages - frag_vec[order].free_pages) / compaction_rate;
			if (time_taken >= time_to_catchup) {
				log_info(3, "Compaction recommended on node %d. Order %d pages consumption rate is high", nid, order);
				if (order < (MAX_ORDER -1))
					log_info(3, "No. of free order %d pages = %ld base pages, consumption rate=%ld pages/msec", order, (frag_vec[order+1].free_pages - frag_vec[order].free_pages), m[order]);
				 log_info(3, "Current compaction rate=%ld pages/msec, Exhaustion in %ld msec", compaction_rate, time_taken);
				retval |= MEMPREDICT_COMPACT;
				break;
			}
		}
	}

	return retval;
}
