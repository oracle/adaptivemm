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
#define	_GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#include <pthread.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <stdbool.h>
#include <signal.h>
#include <linux/kernel-page-flags.h>
#include <stddef.h>
#include <time.h>
#include <systemd/sd-journal.h>

#include <adaptived-utils.h>
#include <adaptived.h>

#include "adaptived-internal.h"
#include "defines.h"

#include "adaptivemmd.h"

#define VERSION		"9.9.9"

static int debug_mode, verbose;

#if 0
/*
 * Clean up before exiting
 */
void
bailout(int retval)
{
	if (del_lock)
		unlink(LOCKFILE);
	closelog();
	exit(retval);
}

/*
 * Signal handler to ensure cleanup before exiting
 */
void
mysig(int signo)
{
	bailout(0);
}
#endif

extern enum log_location log_loc;

/*
 * If "debug_mode" and messages are going to stderr or stdout,
 * precede the message with a timestamp.
 */
void prepend_ts()
{
	if (!debug_mode)
		return;
	if ((log_loc == LOG_LOC_JOURNAL) || (log_loc == LOG_LOC_SYSLOG))
		return;
	char stamp[32];
	time_t now;
	struct tm *timenow;

	now = time(NULL);
	timenow = localtime(&now);
	strftime(stamp, 32, "%b %d %T", timenow);
	if (log_loc == LOG_LOC_STDERR)
		fprintf(stderr, "%s ", stamp);
	else
		fprintf(stdout, "%s ", stamp);
}

void
log_msg(int level, char *fmt, ...)
{
	va_list args;
	va_start (args, fmt);
	char buf[FILENAME_MAX];

	prepend_ts();

	memset(buf, 0, FILENAME_MAX);
	vsnprintf(&buf[strlen(buf)], FILENAME_MAX - strlen(buf) - 1, fmt, args);
	switch (level) {
		case LOG_ERR:
			adaptived_err(buf);
			break;
		case LOG_WARNING:
			adaptived_wrn(buf);
			break;
		case LOG_INFO:
			adaptived_info(buf);
			break;
		case LOG_DEBUG:
			adaptived_dbg(buf);
			break;
	}
	va_end (args);
}

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
predict(struct adaptivemmd_opts * const opts, struct frag_info *frag_vec,
	struct lsq_struct *lsq, unsigned long high_wmark, unsigned long low_wmark, int nid)
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
		if (opts->reclaim_rate == 0)
			return 0;

		/*
		 * If number of free pages is already below high watermark,
		 * it is time to kick off reclamation. If not, compute
		 * time it will take to go below high_wmark.
		 */
		if (frag_vec[0].free_pages <= high_wmark) {
			retval |= MEMPREDICT_RECLAIM;
			log_info(2, "Reclamation recommended due to free pages being below high watermark\n");
			log_info(2, "Consumption rate on node %d=%ld pages/msec, reclaim rate is %ld pages/msec, Free pages=%ld, low wmark=%ld, high wmark=%ld\n", nid, abs(m[0]), opts->reclaim_rate, frag_vec[0].free_pages, low_wmark, high_wmark);
		}
		else {
			time_taken = (frag_vec[0].free_pages - high_wmark)
					/ abs(m[0]);

			/*
			 * Time to reclaim frag_vec[0].free_pages - high_wmark
			 */
			time_to_catchup = (frag_vec[0].free_pages -
						high_wmark) / opts->reclaim_rate;

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
				log_info(3, "Reclamation recommended due to high memory consumption rate\n");
				log_info(3, "Consumption rate on node %d=%ld pages/msec, reclaim rate is %ld pages/msec, Free pages=%ld, low wmark=%ld, high wmark=%ld\n", nid, abs(m[0]), opts->reclaim_rate, frag_vec[0].free_pages, low_wmark, high_wmark);
				log_info(3, "Time to below high watermark= %ld msec, time to catch up=%ld msec\n", time_taken, time_to_catchup);
				retval |= MEMPREDICT_RECLAIM;
			}
		}
	}

	/*
	 * Check if system is running low on higher order pages and needs
	 * compaction.
	 */
	for (order = opts->max_compaction_order; order > 0; order--) {
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
		if (opts->compaction_rate == 0)
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
				log_info(2, "Compaction recommended on node %d. Running out of order %d pages\n", nid, order);
				if (order < (MAX_ORDER -1))
					log_info(3, "No. of free order %d pages = %ld base pages, consumption rate=%ld pages/msec\n", order, (frag_vec[order+1].free_pages - frag_vec[order].free_pages), m[order]);
				 log_info(3, "Current compaction rate=%ld pages/msec\n", opts->compaction_rate);
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

			largest_window = 5 * LSQ_LOOKBACK * opts->periodicity * 1000;
			time_taken = x_cross - current_time;
			if (time_taken > largest_window)
				continue;

			/*
			 * How long will it take to compact as many pages as
			 * available current order pages
			 */
			time_to_catchup = (frag_vec[order+1].free_pages - frag_vec[order].free_pages) / opts->compaction_rate;
			if (time_taken >= time_to_catchup) {
				log_info(3, "Compaction recommended on node %d. Order %d pages consumption rate is high\n", nid, order);
				if (order < (MAX_ORDER -1))
					log_info(3, "No. of free order %d pages = %ld base pages, consumption rate=%ld pages/msec\n", order, (frag_vec[order+1].free_pages - frag_vec[order].free_pages), m[order]);
				 log_info(3, "Current compaction rate=%ld pages/msec, Exhaustion in %ld msec\n", opts->compaction_rate, time_taken);
				retval |= MEMPREDICT_COMPACT;
				break;
			}
		}
	}

	return retval;
}

/*
 * Initiate memory compactiomn in the kernel on a given node.
 */
int
compact(int node_id)
{
	char compactpath[PATH_MAX];
	int fd = 0, ret = 0;
	char c = '1';

	if (snprintf(compactpath, sizeof(compactpath), COMPACT_PATH_FORMAT,
	    node_id) >= sizeof(compactpath)) {
		log_err("compactpath is too long");
		ret = -EINVAL;
		goto error;
	}

	if ((fd = open(compactpath, O_WRONLY|O_NONBLOCK)) == -1) {
		log_err("opening compaction path (%s)\n", strerror(errno));
		ret = -errno;
		goto error;
	}

	if (write(fd, &c, sizeof(c)) != sizeof(c)) {
		log_err("writing to compaction path (%s)\n", strerror(errno));
		ret = -errno;
		goto error;
	}

error:
	if (fd)
		close(fd);
	return ret;
}
/*
 * Parse a single input line for buddyinfo;  return 1 if successful
 * or 0 otherwise.
 */
int
scan_buddyinfo(char *line, char *node, char *zone, unsigned long *nr_free)
{
        char copy[LINE_MAX + 1];
        unsigned int order;
        char *t;

        (void) strncpy(copy, line, LINE_MAX);

        if ((t = strtok(copy, " ")) == NULL || strcmp(t, "Node") ||
            (t = strtok(NULL, ",")) == NULL || sscanf(t, "%s", node) != 1 ||
            (t = strtok(NULL, " ")) == NULL || strcmp(t, "zone") ||
            (t = strtok(NULL, " ")) == NULL || sscanf(t, "%s", zone) != 1)
                return 0;

        for (order = 0; order < MAX_ORDER; order++) {
                if ((t = strtok(NULL, " ")) == NULL ||
                    sscanf(t, " %ld", &nr_free[order]) != 1)
                        return 0;
        }

        return 1;
}

/*
 * Compile free page info for the next node and update free pages
 * vector passed by the caller.
 *
 * RETURNS:
 *	1	No error
 *	0	Error
 *	-1	EOF
 *
 */
#define FLDLEN	20
#define NO_ERR	1
#define ERR	0
#define EOF_RET	-1
int
get_next_node(FILE *ifile, int *nid, unsigned long *nr_free, int skip_dmazone)
{
	char line[LINE_MAX];
	char node[FLDLEN], zone[FLDLEN];
	unsigned long free_pages[MAX_ORDER];
	int order, current_node = -1;

	for (order = 0; order < MAX_ORDER; order++)
		nr_free[order] = 0;
	/*
	 * Read the file one line at a time until we find the next
	 * "Normal" zone or reach EOF
	 */
	while (1) {
		long cur_pos;

		if ((cur_pos = ftell(ifile)) < 0) {
			log_err("ftell on buddyinfo failed (%s)\n", strerror(errno));
			return ERR;
		}
		if (fgets(line, sizeof(line), ifile) == NULL) {
			if (feof(ifile)) {
				rewind(ifile);
				return EOF_RET;
			}
			log_err("fgets(): %s\n", strerror(ferror(ifile)));
			return ERR;
		}

		if (!scan_buddyinfo(line, node, zone, free_pages)) {
			log_err("invalid input: %s\n", line);
			return ERR;
		}
		if (sscanf(node, "%d", nid) <= 0) {
			log_err("invalid input: %s\n", node);
			return ERR;
		}

		/*
		 * Accumulate free pages infor for just the current node
		 */
		if (current_node == -1)
			current_node = *nid;

		if (*nid != current_node) {
			if (fseek(ifile, cur_pos, SEEK_SET) < 0) {
				log_err("fseek on buddyinfo failed (%s)\n", strerror(errno));
				return ERR;
			}
			break;
		}

		/* Skip DMA zone if needed  */
		if (skip_dmazone && strncmp(zone, "DMA", FLDLEN) == 0)
			continue;

		/* Add up free page info for each order */
		for (order = 0; order < MAX_ORDER; order++)
			nr_free[order] += free_pages[order];
	}

	*nid = current_node;
	return NO_ERR;
}

/*
 * Compute the number of base pages tied up in hugepages.
 *
 * Return values:
 *	-1	Failed to read hugepages info
 *	>=0	Percentage change in number of hugepages since last update
 */
int
update_hugepages(struct adaptivemmd_opts * const opts)
{
	DIR *dp;
	struct dirent *ep;
	unsigned long newhpages = 0;
	int rc = -1;

	dp = opendir(MM_HUGEPAGESINFO);

	if (dp == NULL) {
		log_err("%s: opendir() failed for %s\n", __func__, MM_HUGEPAGESINFO);
		return -EINVAL;
	}

	while (((ep = readdir(dp)) != NULL) && (ep->d_type == DT_DIR)) {
		FILE *fp;
		long pages, psize;
		char tmpstr[312];

		/* Check if it is one of the hugepages dir */
		if (strstr(ep->d_name, "hugepages-") == NULL)
			continue;
		snprintf(tmpstr, sizeof(tmpstr), "%s/%s/nr_hugepages",
				MM_HUGEPAGESINFO, ep->d_name);
		fp = fopen(tmpstr, "r");
		if (fp == NULL)
			continue;
		if (fgets(tmpstr, sizeof(tmpstr), fp) != NULL) {
			sscanf(tmpstr, "%ld", &pages);
			sscanf(ep->d_name, "hugepages-%ldkB", &psize);
			newhpages += pages * psize / opts->base_psize;
		}
		fclose(fp);
	}
	if (newhpages) {
		unsigned long tmp;

		tmp = abs(newhpages - opts->total_hugepages);
		/*
		 * If number of hugepages changes from 0 to a
		 * positive number, percentage calculation will
		 * fail. Set the percentage to a high number
		 */
		if (opts->total_hugepages == 0)
			rc = INT_MAX;
		else
			rc = (tmp*100)/opts->total_hugepages;
	}
	/*
	 * If number of hugepages changes to 0, that would be
	 * a 100% change
	 */
	else if (opts->total_hugepages)
		rc = 100;
	else
		rc = 0;
	opts->total_hugepages = newhpages;
	closedir(dp);

	return rc;
}

/*
 * Parse watermarks and zone_managed_pages values from /proc/zoneinfo
 */
#define ZONE_MIN	"min"
#define ZONE_LOW	"low"
#define ZONE_HIGH	"high"
#define ZONE_MNGD	"managed"
#define ZONE_PGST	"pagesets"
int
update_zone_watermarks(struct adaptivemmd_opts * const opts)
{
	FILE *fp = NULL;
	size_t len = 256;
	char *line = malloc(len);
	int current_node = -1;

	fp = fopen(PROC_ZONEINFO, "r");
	if (!fp)
		return 0;

	while ((fgets(line, len, fp) != NULL)) {
		if (strncmp(line, "Node", 4) == 0) {
			char node[FLDLEN], zone[FLDLEN], zone_name[FLDLEN];
			int nid;
			unsigned long min = 0;
			unsigned long low = 0;
			unsigned long high = 0;
			unsigned long managed = 0;

			if (sscanf(line, "%s %d, %s %8s\n", node, &nid, zone, zone_name) <= 0)
				goto out;
			if ((current_node == -1) || (current_node != nid)) {
				current_node = nid;
				opts->min_wmark[nid] = opts->low_wmark[nid] = 0;
				opts->high_wmark[nid] = opts->managed_pages[nid] = 0;
			}

			/*
			 * Add up watermarks and managed pages for all
			 * zones. x86 and x86-64 reserve memory in DMA
			 * zone for use by I/O primarily. This memory should
			 * not be taken into account for free memory
			 * management. ARM on the other hand maps the
			 * first 1G of memory into DMA zone and then
			 * continues mapping into DMA32 and Normal zones.
			 * Ignore pages in DMA zone for x86 and x86-64.
			 */
			if (!opts->skip_dmazone || (strncmp("DMA", zone_name, FLDLEN) != 0)) {
				/*
				 * We found the normal zone. Now look for
				 * line "pages free"
				 */
				if (fgets(line, len, fp) == NULL)
					goto out;

				while (1) {
					unsigned long val;
					char name[256];

					if (fgets(line, len, fp) == NULL)
						goto out;

					if (sscanf(line, "%s %lu\n", name, &val) <= 0)
						goto out;
					if (strncmp(name, ZONE_MIN, sizeof(ZONE_MIN)) == 0)
						min = val;
					if (strncmp(name, ZONE_LOW, sizeof(ZONE_LOW)) == 0)
						low = val;
					if (strncmp(name, ZONE_HIGH, sizeof(ZONE_HIGH)) == 0)
						high = val;
					if (strncmp(name, ZONE_MNGD, sizeof(ZONE_MNGD)) == 0)
						managed = val;
					if (strncmp(name, ZONE_PGST, sizeof(ZONE_PGST)) == 0)
						break;
				}

				opts->min_wmark[nid] += min;
				opts->low_wmark[nid] += low;
				opts->high_wmark[nid] += high;
				opts->managed_pages[nid] += managed;
			}
		}
	}

out:
	free(line);
	fclose(fp);
	return 0;
}

/*
 * When kernel computes the gap between low and high watermarks
 * using watermark scale factor, it computes a percentage of
 * total memory on the system. This works when most of the
 * moemory is reclaimable and subject to watermarks. On systems
 * that allocate very large percentage of memory to hugepages,
 * this results in kernel computing watermarks that are just
 * too high for the remaining memory that is truly subject to
 * reclamation. Rescale max wsf down so kernel will end up
 * computing watermarks that are more in line with real
 * available memory.
 */
void
rescale_maxwsf(struct adaptivemmd_opts * const opts)
{
	unsigned long reclaimable_pages, total_managed = 0;
	unsigned long gap, new_wsf;
	int i;

	if (opts->total_hugepages == 0)
		return;

	for (i = 0; i < MAX_NUMANODES; i++)
		total_managed += opts->managed_pages[i];
	if (total_managed == 0) {
		log_info(1, "Number of managed pages is 0\n");
		return;
	}

	/*
	 * Compute what the gap would be if maxwsf is applied to
	 * just reclaimable memory
	 */
	reclaimable_pages = total_managed - opts->total_hugepages;
	gap = (reclaimable_pages * opts->maxwsf)/10000;

	/*
	 * Now compute WSF needed for same gap if all memory was taken
	 * into account
	 */
	new_wsf = (gap * 10000)/total_managed;

	if ((new_wsf > 9) && (new_wsf < 1000))
		opts->mywsf = new_wsf;
	else
		log_warn("Failed to compute reasonable WSF, %ld, total pages %ld, reclaimable pages %ld\n", new_wsf, total_managed, reclaimable_pages);
}

/*
 * Get the number of pages stolen by kswapd from /proc/vmstat.
 */
unsigned long
no_pages_reclaimed(struct adaptivemmd_opts * const opts)
{
	FILE *fp = NULL;
	size_t len = 100;
	char *line = malloc(len);
	unsigned long val, reclaimed;
	char desc[100];

	fp = fopen(PROC_VMSTAT, "r");
	if (!fp)
		return 0;

	opts->total_cache_pages = reclaimed = 0;
	while ((fgets(line, len, fp) != NULL)) {
		if (sscanf(line, "%s %lu\n", desc, &val) <= 0)
			break;
		if (strcmp(desc, "pgsteal_kswapd") == 0)
			reclaimed += val;
		if (strcmp(desc, "pgsteal_kswapd_normal") == 0)
			reclaimed += val;
		if (strcmp(desc, "pgsteal_kswapd_movable") == 0)
			reclaimed += val;
		if (strcmp(desc, "nr_inactive_file") == 0)
			opts->total_cache_pages += val;
		if (strcmp(desc, "nr_inactive_anon") == 0)
			opts->total_cache_pages += val;
	}

	free(line);
	fclose(fp);
	return reclaimed;
}

/*
 * Dynamically rescale the watermark_scale_factor to make kswapd
 * more aggressive
 */
void
rescale_watermarks(struct adaptivemmd_opts * const opts, int scale_up)
{
	int fd, i, count;
	unsigned long scaled_watermark, frac_free;
	char scaled_wmark[20], *c;
	unsigned long total_managed = 0;
	unsigned long mmark, lmark, hmark;

	log_dbg("rescale_watermarks: scale_up=%d\n", scale_up);

	for (i = 0; i < MAX_NUMANODES; i++)
		total_managed += opts->managed_pages[i];
	/*
	 * Hugepages should not be taken into account for watermark
	 * calculations since they are not reclaimable
	 */
	total_managed -= opts->total_hugepages;
	if (total_managed == 0) {
		log_info(1, "Number of managed non-huge pages is 0\n");
		return;
	}

	/*
	 * Fraction of managed pages currently free
	 */
	frac_free = (opts->total_free_pages*1000)/total_managed;

	/*
	 * Get the current watermark scale factor.
	 */
	if ((fd = open(RESCALE_WMARK, O_RDONLY)) == -1) {
		log_err("Failed to open "RESCALE_WMARK" (%s)\n", strerror(errno));
		return;
	}

	if (read(fd, scaled_wmark, sizeof(scaled_wmark)) < 0) {
		log_err("Failed to read from "RESCALE_WMARK" (%s)\n", strerror(errno));
		goto out;
	}
	close(fd);
	/* strip off trailing CR from current watermark scale factor */
	c = index(scaled_wmark, '\n');
	*c = 0;

	/*
	 * Compute average high and low watermarks across nodes
	 */
	lmark = hmark = count = 0;
	for (i = 0; i < MAX_NUMANODES; i++) {
		lmark += opts->low_wmark[i];
		hmark += opts->high_wmark[i];
		if (opts->low_wmark[i] != 0)
			count++;
	}
	lmark = lmark/count;
	hmark = hmark/count;

	/*
	 * If memory pressure is easing, scale watermarks back and let
	 * kernel reclaim pages at its natural rate. If number of free
	 * pages is below halfway between low and high watermarks, back
	 * off watermark scale factor 10% at a time
	 */
	if (!scale_up) {
		if (opts->total_free_pages < ((lmark+hmark)/2))
			scaled_watermark = (atoi(scaled_wmark) * 9)/10;
		else
			scaled_watermark = ((unsigned long)(1000 - frac_free)/10)*10;
		/*
		 * Make sure new watermark scale factor does not end up
		 * higher than current value accidentally
		 */
		if (scaled_watermark >= atoi(scaled_wmark))
			scaled_watermark = (atoi(scaled_wmark) * 9)/10;
	} else {
		/*
		 * Determine how urgent the situation is regarding
		 * remaining free pages. If free pages are already
		 * below high watermark, check if there are enough
		 * pages potentially available in buffer cache.
		 */
		if (opts->total_free_pages < hmark) {
			if (opts->total_cache_pages > (hmark - opts->total_free_pages)) {
				/*
				 * There are pages available to harvest.
				 * Aggressive reclaim is appropriate for
				 * this situation. Urgency is determined by
				 * the percentage of managed pages still
				 * available. As this number gets lower,
				 * watermark scale factor needs to go up
				 * to make kswapd reclaim pages more
				 * aggressively. Compute scale factor as
				 * multiples of 10.
				 */
				scaled_watermark = ((unsigned long)(1000 - frac_free)/10)*10;
				if (scaled_watermark == 0)
					return;
			} else {
				/*
				 * We are running low on free pages but
				 * there are not enough pages available to
				 * reclaim either, so no need to get
				 * overly aggressive. Be only half as
				 * aggressive as the case where pages are
				 * available to reclaim
				 */
				scaled_watermark = ((unsigned long)(1000 - frac_free)/20)*10;
				if (scaled_watermark == 0)
					return;
			}
		} else {
			/*
			 * System is not in dire situation at this time yet.
			 * Free pages are expected to approach high
			 * watermark soon though. If there are lots of
			 * reclaimable pages available, start semi-aggressive
			 * reclamation. If not, raise watermark scale factor
			 * but only by 10% if above 100 otherwise by 20%
			 */
			if (opts->total_cache_pages > (opts->total_free_pages - hmark)) {
				scaled_watermark = ((unsigned long)(1000 - frac_free)/20)*10;
				if (scaled_watermark == 0)
					return;
			} else {
				if (atoi(scaled_wmark) > 100)
					scaled_watermark = (atoi(scaled_wmark) * 11)/10;
				else
					scaled_watermark = (atoi(scaled_wmark) * 12)/10;
			}
		}

		/*
		 * Compare to current watermark scale factor. If it is
		 * the same as new scale factor, it means current setting
		 * is not working well enough already. Raise it by 10%
		 * instead.
		 */
		if (atoi(scaled_wmark) == scaled_watermark)
			scaled_watermark = (scaled_watermark * 11)/10;
	}

	/*
	 * Highest possible value for watermark scaling factor is
	 * 1000. If current scale is already set to 1000, no point
	 * in updating it. Also make sure new value is at least the
	 * minimum allowed value.
	 */
	if (scaled_watermark > 1000)
		scaled_watermark = 1000;
	if (scaled_watermark < 10)
		scaled_watermark = 10;
	if (scaled_watermark > opts->mywsf)
		scaled_watermark = opts->mywsf;

	/*
	 * Before committing to the new higher value of wsf, make sure
	 * it will not cause low watermark to be raised above current
	 * number of free pages. If that were to happen OOM killer will
	 * step in immediately. Allow for at least 2% headroom over low
	 * watermark.
	 */
	if (scale_up) {
		unsigned long threshold, loose_pages = opts->total_free_pages +
							opts->total_cache_pages;
		unsigned long new_lmark;

		mmark = lmark = 0;
		for (i = 0; i < MAX_NUMANODES; i++) {
			mmark += opts->min_wmark[i];
			lmark += opts->low_wmark[i];
		}

		/*
		 * Estimate the new low watermark if we were to increase
		 * WSF to this new value
		 */
		new_lmark = mmark +
			((lmark-mmark)*scaled_watermark/atoi(scaled_wmark));

		/*
		 * When low watermark gets raised, make sure there will
		 * still be enough free pages left for system to continue
		 * making progress. Use a headroom of 2% of currently
		 * available free pages. If the number of free pages is
		 * already below this threshold, setting this new wsf
		 * is very likely to kick OOM killer.
		 */
		threshold = new_lmark + opts->total_free_pages * 1.02;

 		if (loose_pages <= threshold) {
			/*
			 * See if we can raise wsf by 10%
			 */
			scaled_watermark = (atoi(scaled_wmark) * 11)/10;
			new_lmark = mmark + ((lmark-mmark)*scaled_watermark/atoi(scaled_wmark));
			threshold = new_lmark + opts->total_free_pages * 1.02;
			if (loose_pages <= threshold) {
				log_info(2, "Not enough free pages to raise watermarks, free pages=%ld, reclaimable pages=%ld, new wsf=%ld, min=%ld, current low wmark=%ld, new projected low watermark=%ld\n", opts->total_free_pages, opts->total_cache_pages, scaled_watermark, mmark, lmark, new_lmark);
				return;
			}
		}
	}

	if (atoi(scaled_wmark) == scaled_watermark) {
		if (scaled_watermark == opts->mywsf)
			log_info(2, "At max WSF already (max WSF = %u)\n", opts->mywsf);
		log_dbg("rescale_watermarks: Nothing to do. atoi(scaled_wmark) (%ld) == "
		    "scaled_watermark, opts->mywsf=%ld\n", atoi(scaled_wmark), opts->mywsf);
		return;
	}

	log_info(1, "Adjusting watermarks. Current watermark scale factor = %s\n", scaled_wmark);
	if (opts->dry_run)
		goto out;

	log_info(1, "New watermark scale factor = %ld\n", scaled_watermark);
	sprintf(scaled_wmark, "%ld\n", scaled_watermark);
	if ((fd = open(RESCALE_WMARK, O_WRONLY)) == -1) {
		log_err("Failed to open "RESCALE_WMARK" (%s)\n", strerror(errno));
		return;
	}
	if (write(fd, scaled_wmark, strlen(scaled_wmark)) < 0)
		log_err("Failed to write to "RESCALE_WMARK" (%s)\n", strerror(errno));

out:
	close(fd);
}

static inline unsigned long
get_msecs(struct timespec *spec)
{
	if (!spec)
		return -1;

	return (unsigned long)((spec->tv_sec * 1000) + (spec->tv_nsec / 1000));
}

/*
 * check_permissions() - Check all required permissions for this program to
 *			run successfully
 */
static int
check_permissions(void)
{
	int fd;
	char tmpstr[40];

	/*
	 * Make sure running kernel supports watermark_scale_factor file
	 */
	if ((fd = open(RESCALE_WMARK, O_RDONLY)) == -1) {
		adaptived_err("Can not open "RESCALE_WMARK" (%s)", strerror(errno));
		return 0;
	}

	/* Can we write to this file */
	if (read(fd, tmpstr, sizeof(tmpstr)) < 0) {
		adaptived_err("Can not read "RESCALE_WMARK" (%s)", strerror(errno));
		return 0;
	}
	close(fd);
	if ((fd = open(RESCALE_WMARK, O_WRONLY)) == -1) {
		adaptived_err("Can not open "RESCALE_WMARK" (%s)", strerror(errno));
		return 0;
	}

	if (write(fd, tmpstr, strlen(tmpstr)) < 0) {
		adaptived_err("Can not write to "RESCALE_WMARK" (%s)", strerror(errno));
		return 0;
	}
	close(fd);

	return 1;
}

/*
 * update_neg_dentry() - Update the limit for negative dentries based
 *	upon current system state
 *
 * Negative dentry limit is expressed as a percentage of total memory
 * on the system. If a significant amount of memory on the system is
 * allocated to hugepages, that percentage of total memory can be too
 * high a number of pages tobe allowed to be consumed by negative
 * dentries. To compensate for this, the percentage should be scaled
 * down so the number of maximum pages allowed for negative dentries
 * amounts to about the same percentage of non-hugepage memory pages.
 * As an example, a system with 512GB of memory:
 *
 *	512GB = 134217728 4k pages
 *
 * If negative dentry limit was set to 1%, that would amount to
 * 1342177 pages. Now let us allocate 448GB of this memory to
 * hugepages leaving 64GB of base pages:
 *
 *	64GB = 16777216 4k pages
 *
 * If we still allowed 1342177 pages to be consumed by negative
 * dentries, that would amount to 8% of reclaimable memory which
 * is significantly higher than original intent. To keep in line with
 * the original intent of allowing only 1% of reclaimable memory
 * this percentage should be scaled down to .1 which then yields up
 * to 134217 allowed to be consumed by negative dentries and
 *
 *	(134217/16777216)*100 = 0.08%
 *
 * NOTE: /proc/sys/fs/negative-dentry-limit is expressed as fraction
 *	of 1000, so above value will be translated to 1 which will be
 *	0.1%
 */
int
update_neg_dentry(struct adaptivemmd_opts * const opts, bool init)
{
	int fd;

        /*
         * bail out if this module is not enabled
         */
        if (!opts->neg_dentry_check_enabled) {
                return 0;
	}

	/*
	 * Set up negative dentry limit if functionality is supported on
	 * this kernel.
	 */
	if (access(NEG_DENTRY_LIMIT, F_OK) == 0) {
		if ((fd = open(NEG_DENTRY_LIMIT, O_RDWR)) == -1) {
			log_err("Failed to open "NEG_DENTRY_LIMIT" (%s)", strerror(errno));
			return -EINVAL;
		} else {
			/*
			 * Compute the value for negative dentry limit
			 * based upon just the reclaimable pages
			 */
			int val, i;
			unsigned long reclaimable_pages, total_managed = 0;
			char neg_dentry[LINE_MAX];

			for (i = 0; i < MAX_NUMANODES; i++)
				total_managed += opts->managed_pages[i];
			if (!total_managed) {
				log_err("%s: total_managed == 0\n", __func__);
				return -EINVAL;
			}
			reclaimable_pages = total_managed - opts->total_hugepages;
			val = (reclaimable_pages * opts->neg_dentry_pct) / total_managed;
			/*
			 * This value should not be 0 since that would
			 * disallow cacheing of negative dentries. It
			 * also not be higher than 75 since that would
			 * allow most of the free memory to be consumed
			 * by negative dentries.
			 */
			if (val < 1)
				val = 1;
			if (val > MAX_NEGDENTRY)
				val = MAX_NEGDENTRY;
			snprintf(neg_dentry, sizeof(neg_dentry), "%u\n", val);
			log_info(1, "Updating negative dentry limit to %s", neg_dentry);
			if (write(fd, neg_dentry, strlen(neg_dentry)) < 0) {
				log_err("Failed to write to "NEG_DENTRY_LIMIT" (%s)", strerror(errno));
				close(fd);
				return -errno;
			}
			close(fd);
		}
	}
	return 0;
}

/*
 * get_unmapped_pages()- count number of unmapped pages reported in
 *	/proc/kpagecount
 *
 * /proc/kpagecount reports a current mapcount as a 64-bit integer for
 * each PFN on the system. It includes PFN that may not have a physical
 * page backing them. /proc/kpageflags reports current flags for each
 * PFN including flag for whether a physical page is present at that
 * PFN. This flag can be used to identify PFNs that do not have a
 * backing pahysical page
 */
long
get_unmapped_pages(struct adaptivemmd_opts * const opts)
{
	int fd1, fd2;
	long pagecnt[BATCHSIZE/sizeof(long)];
	unsigned long pageflg[BATCHSIZE/sizeof(long)];
	ssize_t inbytes1, inbytes2;
	unsigned long count, unmapped_pages = 0;

	log_dbg("get_unmapped_pages()\n");

	if ((fd1 = open(PROC_KPAGECOUNT, O_RDONLY)) == -1) {
		log_err("Error opening kpagecount\n");
		return -1;
	}

	if ((fd2 = open(PROC_KPAGEFLAGS, O_RDONLY)) == -1) {
		log_err("Error opening kpageflags\n");
		return -1;
	}

	inbytes1 = read(fd1, pagecnt, BATCHSIZE);
	inbytes2 = read(fd2, pageflg, BATCHSIZE);
	count = inbytes1/sizeof(long);
	while ((inbytes1 > 0) && (inbytes2 > 0)) {
		unsigned long i;

		i = 0;
		while (i < count) {
			/*
			 * Skip PFN not backed by physical page
			 */
			if ((pageflg[i]>>KPF_NOPAGE) & 1)
				goto nextloop;
			if ((pageflg[i]>>KPF_HWPOISON) & 1)
				goto nextloop;
			if ((pageflg[i]>>KPF_OFFLINE) & 1)
				goto nextloop;
			if ((pageflg[i]>>KPF_SLAB) & 1)
				goto nextloop;
			if ((pageflg[i]>>KPF_BUDDY) & 1)
				goto nextloop;
			if ((pageflg[i]>>KPF_PGTABLE) & 1)
				goto nextloop;

			/*
			 * Pages not mapped in are free unless
			 * they are reserved like hugetlb pages
			 */
			if ((pagecnt[i] == 0) &&
				!((pageflg[i]>>KPF_HUGE) & 1)) {
				unmapped_pages++;
			}

nextloop:
			i++;
		}
		inbytes1 = read(fd2, pageflg, BATCHSIZE);
		inbytes2 = read(fd1, pagecnt, BATCHSIZE);
		count = inbytes2/sizeof(long);
	}

	if ((inbytes1 < 0) || (inbytes2 < 0)) {
		if (inbytes1 < 0)
			log_err("Error reading kpageflagst\n");
		else
			log_err("Error reading kpagecount\n");
		close(fd1);
		close(fd2);
		return -1;
	}

	close(fd1);
	close(fd2);

	log_dbg("get_unmapped_pages(), unmapped_pages=%ld\n", unmapped_pages);

	return unmapped_pages;
}

/*
 * pr_meminfo() - log /proc/meminfo contents
 */
void
pr_meminfo(int level)
{
        FILE *fp = NULL;
        char line[LINE_MAX];

        fp = fopen(PROC_MEMINFO, "r");
        if (!fp)
                return;

        while ((fgets(line, sizeof(line), fp) != NULL))
                log_info(level, "%s", line);

        fclose(fp);

}

char * const memdata_item_name[NR_MEMDATA_ITEMS] = {
	"MemAvailable",
	"Buffers",
	"Cached",
	"SwapCached",
	"Unevicatble",
	"Mlocked",
	"AnonPages",
	"Mapped",
	"Shmem",
	"Kreclaimable",
	"Slab",
	"Sunreclaim",
	"KernelStack",
	"PageTables",
	"SecPageTables",
	"VmallocUsed",
	"CmaTotal"
};

/*
 * cmp_meminfo() - Compare two instances of meminfo data and print the ones
 *		that have changed considerably
 */
void
cmp_meminfo(int level, unsigned long *memdata, unsigned long *pr_memdata, int mem_trigger_delta)
{
	int i;
	unsigned long delta;

	for (i=0; i<NR_MEMDATA_ITEMS; i++) {
		delta = abs(pr_memdata[i] - memdata[i]);
		if (delta == 0)
			continue;

		/* Is the change greater than warning trigger level */
		if (delta > (pr_memdata[i] * (mem_trigger_delta/100))) {
			log_info(level, "%s %s by more than %d (previous = %lu K, current = %lu K)\n", memdata_item_name[i], (pr_memdata[i] < memdata[i] ? "grew":"decreased"), MEM_TRIGGER_DELTA, pr_memdata[i], memdata[i]);
		}
	}
}

void set_for_effect(struct adaptivemmd_opts * const opts, unsigned long freemem)
{
	struct curr_mem_info *cmi = &opts->curr_mem_info;
	int i;

	/* save current memory info for effect */
	cmi->freemem = freemem;
	cmi->prv_free = opts->prv_free;
	cmi->mem_remain = opts->mem_remain;
	cmi->unacct_mem = opts->unacct_mem;
	cmi->unmapped_pages = opts->unmapped_pages;
	for (i = 0; i < NR_MEMDATA_ITEMS; i++) {
		cmi->pr_memdata[i] = opts->pr_memdata[i];
	}
}

/*
 * check_memory_leak() - Check for potential memory leak
 *
 * Compute the total memory in use currently by adding up various fields
 * in /proc/meminfo and current free memory. Any difference between this
 * number and total memory is a potential memory leak if the delta keeps
 * increasing.
 *
 * Tricky part here is accounting for all memory in use legitimately.
 * Use the following formula with values from /proc/meminfo to compute
 * memory accounted for:
 *
 * mem_good =	AnonPages +
 *		Buffers +
 *		Cached +
 *		CmaTotal +
 *		KReclaimable +
 *		KernelStack +
 *		PageTables +
 *		SwapCached +
 *		SUnreclaim +
 *		SecPageTables +
 *		Unevictable +
 *		MemFree +
 *		hugepages (computed by update_hugepages)
 *
 * Note: Active(file) and Inactive(file) memory is accounted for in Cached.
 *
 * Now monitor the delta (MemTotal - mem_good) and if the delta
 * keeps going up, there is a potential memory leak. To account for
 * memory used by kernel text, kernel modules and firmware, compute
 * this delta on first run and mark that as baseline. This baseline
 * can be updated if (MemTotal - mem_good) drops below previous
 * baseline.
 */
int
check_memory_leak(struct adaptivemmd_opts * const opts, bool init)
{
	unsigned long memdata[NR_MEMDATA_ITEMS];
	unsigned long i, mem_acctd;
	unsigned long total_managed = 0;
	FILE *fp = NULL;
	char line[LINE_MAX];
	unsigned long val, inuse_mem, freemem = 0, mem_total = 0;
	char desc[LINE_MAX];
	int possible_memory_leak = 0;

	log_dbg("%s: memleak_check_enabled=%d\n", __func__, opts->memleak_check_enabled);

        /*
         * bail out if this module is not enabled
         */
        if (!opts->memleak_check_enabled) {
                return 0;
	}

	freemem = 0;

	for (i = 0; i < MAX_NUMANODES; i++)
		total_managed += opts->managed_pages[i];
	for (i = 0; i < NR_MEMDATA_ITEMS; i++)
		memdata[i] = 0;

	/*
	 * Now read meminfo file to get current memory info
	 */
	fp = fopen(PROC_MEMINFO, "r");
	if (!fp)
		return -EINVAL;

	/*
	 * Compute amount of memory accounted for since it is clearly in
	 * use.
	 *
	 * in use memory = Buffers + Cached + Mlocked +
	 *			Shmem + KReclaimable + Slab + KernelStack +
	 *			PageTables + SecPageTables + CmaTotal
	 */
	inuse_mem = 0;
	while ((fgets(line, sizeof(line), fp) != NULL)) {
		if (sscanf(line, "%s %lu\n", desc, &val) <= 0)
			break;
		switch (desc[0]) {
		case 'A':
			if (strcmp(desc, "AnonPages:") == 0) {
				inuse_mem += val;
				memdata[ANONPAGES] = val;
			}
			break;

		case 'B':
			if (strcmp(desc, "Buffers:") == 0) {
				inuse_mem += val;
				memdata[BUFFERS] = val;
			}
			break;

		case 'C':
			if (strcmp(desc, "Cached:") == 0) {
				inuse_mem += val;
				memdata[CACHED] = val;
			} else if (strcmp(desc, "CmaTotal:") == 0) {
				inuse_mem += val;
				memdata[CMA] = val;
			}
			break;

		case 'K':
			if (strcmp(desc, "KReclaimable:") == 0) {
				inuse_mem += val;
				memdata[KRECLAIMABLE] = val;
			} else if (strcmp(desc, "KernelStack:") == 0) {
				inuse_mem += val;
				memdata[KSTACK] = val;
			}
			break;

		case 'M':
			if (strcmp(desc, "MemFree:") == 0)
				freemem = val;
			else if (strcmp(desc, "MemTotal:") == 0)
				mem_total = val;
			else if (strcmp(desc, "MemAvailable:") == 0)
				memdata[MEMAVAIL] = val;
			else if (strcmp(desc, "Mlocked:") == 0)
				memdata[MLOCKED] = val;
			else if (strcmp(desc, "Mapped:") == 0)
				memdata[MAPPED] = val;
			break;

		case 'P':
			if (strcmp(desc, "PageTables:") == 0) {
				inuse_mem += val;
				memdata[PGTABLE] = val;
			}
			break;

		case 'S':
			if (strcmp(desc, "SwapCached:") == 0) {
				inuse_mem += val;
				memdata[SWPCACHED] = val;
			} else if (strcmp(desc, "SUnreclaim:") == 0) {
				inuse_mem += val;
				memdata[SUNRECLAIM] = val;
			} else if (strcmp(desc, "SecPageTables:") == 0) {
				inuse_mem += val;
				memdata[SECPGTABLE] = val;
			} else if (strcmp(desc, "Shmem:") == 0) {
				memdata[SHMEM] = val;
			} else if (strcmp(desc, "Slab:") == 0) {
				memdata[SLAB] = val;
			}
			break;

		case 'U':
			if (strcmp(desc, "Unevictable:") == 0) {
				inuse_mem += val;
				memdata[UNEVICTABLE] = val;
			}
			break;

		case 'V':
			if (strcmp(desc, "VmallocUsed:") == 0)
				memdata[VMALLOCUSED] = val;
			break;

		default:
			break;
		}
	}

	/*
	 * Data in meminfo is in kB while we are tracking memory in pages.
	 * Convert data computed from meminfo to pages
	 */
	inuse_mem = inuse_mem/opts->base_psize;
	freemem = freemem/opts->base_psize;
	mem_total = mem_total/opts->base_psize;
	memdata[MEMAVAIL] = memdata[MEMAVAIL]/opts->base_psize;
	fclose(fp);

	/*
	 * Sum of memory in use, memory allocated to hugepages and free
	 * memory is the amount of memory we can account for from
	 * /proc/meminfo
	 */
	mem_acctd = freemem + opts->total_hugepages + inuse_mem;

	if (init) {
		/*
		 * The difference between total memory and memory
		 * accounted for is likely to be memory in use by kernel
		 * and kernel modules plus memory reserved by kernel,
		 * especially if this daemon was started when machine
		 * rebooted. Record this difference as base memory usage
		 * by the system
		 */
		opts->base_mem = total_managed - mem_acctd;
		opts->prv_free = freemem;
		for (i = 0; i < NR_MEMDATA_ITEMS; i++)
			opts->pr_memdata[i] = memdata[i];
		log_info(5, "Base memory consumption set to %lu K\n", (opts->base_mem * opts->base_psize));
		goto out;
	}

	opts->unmapped_pages = get_unmapped_pages(opts);
	if (opts->unmapped_pages < 0) {
		log_err("Failed to read unmapped pages count\n");
		opts->unmapped_pages = 0;
	}

	/*
	 * Base memory consumption is the minimum amount of memory system
	 * uses during normal operation. This memory is potentially
	 * consumed by kernel and any other basic services. This value is
	 * computed as part of adaptivemmd initialization. Update this value
	 * if base memory consumption seems to have gone down which would
	 * mean initial estimate was high
	 */
	if (total_managed < mem_acctd) {
		log_info(2, "Issue with memory computation, total_managed = %lu K, mem_acctd = %lu K, unmapped = %lu K\n", (total_managed * opts->base_psize), (mem_acctd * opts->base_psize), (opts->unmapped_pages * opts->base_psize));
		pr_meminfo(LOG_DEBUG);
	}
	val = total_managed - mem_acctd;
	if (val < opts->base_mem) {
		opts->base_mem = val;
		log_info(5, "Base memory consumption updated to %lu K\n", (opts->base_mem * opts->base_psize));
		/*
		 * When base memory consumption is updated, unaccounted
		 * memory will be 0. No point in continuing with leak
		 * detection. So leak detection in the next invocation.
		 */
		goto out;
	}

	/*
	 * All the memory we have accounted for and the base memory usage
	 * of the system should be close to total memory on the system.
	 * If not, we have some unaccounted for memory. This can be
	 * normal since drivers may allocate memory for their operation
	 * or user may have loaded new kernel modules. We will keep track of
	 * this memory.
	 */
	if (total_managed > (mem_acctd + opts->base_mem))
		opts->unacct_mem = total_managed - (mem_acctd + opts->base_mem);
	else
		opts->unacct_mem = 0;

	log_info(5, "Unaccounted memory = %lu K, freemem = %lu K, memavail = %lu K\n",
	    (opts->unacct_mem * opts->base_psize), (freemem * opts->base_psize),
		(memdata[MEMAVAIL] * opts->base_psize));

	/*
	 * If unaccounted for memory grew by more than warning trigger
	 * level compared to last sample, record it. If this unaccounted
	 * for memory continues to grow, we may have a slow steady leak.
	 */
	if (opts->unacct_mem > (opts->mem_remain * ((100 + opts->mem_trigger_delta)/100))) {
		if (opts->mem_remain == 0) {
			opts->mem_remain = opts->unacct_mem;
			goto out;
		}

		opts->gr_count++;

		/*
		 * Instead of slow steady leak, it is possible for large
		 * chunk of memory to be leaked all at once. If unaccounted
		 * memory doubled or more and we have seen growth in
		 * unaccounted memory for the last three consecutive samples,
		 * this may be a memory leak
		 */
		if (opts->mem_remain && (opts->unacct_mem > (opts->mem_remain * 2)) &&
			(opts->gr_count > 3)) {
			opts->mem_leak_trigger = SUDDEN_MEMORY_LEAK_TRIGGER;
		} else {
			opts->mem_leak_trigger = BACKGROUND_MEMORY_LEAK_TRIGGER;
		}
		set_for_effect(opts, freemem);
		opts->mem_remain = opts->unacct_mem;

		possible_memory_leak = 1;
	} else if (opts->unacct_mem < (opts->mem_remain * ((100 - opts->mem_trigger_delta)/100))){
		/*
		 * unaccounted memory is starting to shrink. Reset
		 * the counter
		 */
		opts->gr_count = 0;
	}

	/*
	 * If unaccounted memory grew over 10 samples without shrinking
	 * in between, it may point to a slow leak
	 */
	if (opts->gr_count > opts->unacct_mem_grth_max) {
		opts->gr_count = 0;
		opts->mem_leak_trigger = SLOW_MEMORY_LEAK_TRIGGER;
		set_for_effect(opts, freemem);
		possible_memory_leak = 1;
	}
out:
	/*
	 * Update persistent metrics for next invocation
	 */
	opts->prv_free = freemem;
	for (i = 0; i < NR_MEMDATA_ITEMS; i++)
		opts->pr_memdata[i] = memdata[i];

	log_dbg("%s: possible_memory_leak=%d\n", __func__, possible_memory_leak);

	return possible_memory_leak;
}

int
check_memory_leak_effect(struct adaptivemmd_opts * const opts)
{
	unsigned long *memdata, *pr_memdata;
	unsigned long freemem, prv_free, mem_remain, base_psize, unacct_mem;
	long unmapped_pages;
	int ret = 0;
	struct curr_mem_info *cmi;

	if (!opts) {
		return -EINVAL;
	}
        /*
         * bail out if this module is not enabled
         */
        if (!opts->memleak_check_enabled) {
                return 0;
	}
	if (opts->mem_leak_trigger == NO_TRIGGER) {
		return 0;
	}

	cmi = (struct curr_mem_info *)&opts->curr_mem_info;
	if (!cmi) {
		log_err("check_memory_leak_effect: curr_mem_info is NULL\n");
		return -EINVAL;
	}
	memdata = opts->pr_memdata;	/* current at trigger time */
	pr_memdata = cmi->pr_memdata;	/* saved previous at trigger */
	freemem = cmi->freemem;		/* current at trigger time */
	prv_free = cmi->prv_free;	/* saved previous at trigger */
	mem_remain = cmi->mem_remain;
	unacct_mem = cmi->unacct_mem;
	unmapped_pages = cmi->unmapped_pages;
	base_psize = opts->base_psize;

	switch (opts->mem_leak_trigger) {
	case SUDDEN_MEMORY_LEAK_TRIGGER:
		log_info(1, "Possible sudden memory leak - background memory use more than doubled (%lu K -> %lu K), unmapped memory = %lu K, freemem = %lu K, freemem previously = %lu K\n",
			(mem_remain * base_psize),
			(unacct_mem * base_psize),
			(unmapped_pages * base_psize),
			(freemem * base_psize),
			(prv_free * base_psize));
		pr_meminfo(1);
		cmp_meminfo(1, memdata, pr_memdata, opts->mem_trigger_delta);
		break;
	case BACKGROUND_MEMORY_LEAK_TRIGGER:
		log_info(5, "Background memory use grew by more than %d (%lu -> %lu) K, unmapped memory = %lu K, freemem = %lu K, freemem previously = %lu K, MemAvail = %lu K\n", MEM_TRIGGER_DELTA,
			(mem_remain * base_psize),
			(unacct_mem * base_psize),
			(unmapped_pages * base_psize),
			(freemem * base_psize),
			(prv_free * base_psize),
			(memdata[MEMAVAIL] * base_psize));
		cmp_meminfo(1, memdata, pr_memdata, opts->mem_trigger_delta);
		break;
	case SLOW_MEMORY_LEAK_TRIGGER:
		log_info(1, "Possible slow memory leak - background memory use has been growing steadily (currently %lu) K, unmapped memory = %lu K, freemem = %lu K, MemAvail = %lu K\n",
			(mem_remain * base_psize),
			(unmapped_pages * base_psize),
			(freemem * base_psize),
			(memdata[MEMAVAIL] * base_psize));
		pr_meminfo(1);
		cmp_meminfo(1, memdata, pr_memdata, opts->mem_trigger_delta);
		break;
	default:
		log_err("%s: Unknown trigger type %d\n", opts->mem_leak_trigger);
		return -EINVAL;
	}
	return ret;
}

/*
 * updates_for_hugepages() - Update any values that need to be updated
 *	whenever number of hugepages on system changes significantly
 */
int
updates_for_hugepages(struct adaptivemmd_opts * const opts, int delta)
{
        int ret = 0;
        /*
         * Don't do anything for delta less than 5%
         */
        if (delta < 5)
                return 0;

        ret = update_neg_dentry(opts, false);

        return ret;
}

/*
 * check_memory_pressure() - Evaluate and respond to memory consumption trend
 *
 * Update computation of memory cpnmsumption trend for overall memory
 * consumption and consumption of each order page. Take action by changing
 * watermarks or forcing memory compaction if an issues is anticipated in
 * near future
 *
 * Input:
 *	init - flag. When set, indicates this module should perform any
 *		initializations it needs for its operation. The module
 *		may choose to perform only initiazations when this flag
 *		is set
 */
int
check_memory_pressure(struct adaptivemmd_opts * const opts, bool init)
{
	unsigned long nr_free[MAX_ORDER];
	struct frag_info free[MAX_ORDER];
	unsigned long time_elapsed, reclaimed_pages;
	unsigned long result = 0;
	int i, order, nid, retval;
	struct timespec spec, spec_after;
	int triggered = 0;

	opts->mem_pressure_trigger = NO_TRIGGER;
	opts->final_result = 0;

        /*
         * bail out if this module is not enabled
         */
        if (!opts->memory_pressure_check_enabled) {
                return 0;
	}

	if (init) {
		/*
		 * Initialize number of higher order pages seen in last scan
		 * and compaction requested per node
		 */
		for (i = 0; i < MAX_NUMANODES; i++) {
			opts->last_bigpages[i] = 0;
			opts->compaction_requested[i] = 0;
		}
		opts->last_reclaimed = 0;

		if ((opts->ifile = fopen(PROC_BUDDYINFO, "r")) == NULL) {
			log_err("fopen() failed for %s\n", PROC_BUDDYINFO);
			return -EINVAL;
		}

		return 0;
	}

	for (i = 0; i < MAX_NUMANODES; i++) {
		opts->compaction_requested[i] = 0;
	}
	/*
	 * Keep track of time to calculate the compaction and
	 * reclaim rates
	 */
	opts->total_free_pages = 0;

	while ((retval = get_next_node(opts->ifile, &nid, nr_free, opts->skip_dmazone)) != 0) {
		unsigned long total_free;
		/*
		 * Assemble the fragmented free memory vector:
		 * the fragmented free memory at a given order is
		 * the sum of the occupancies at all lower orders.
		 * Memory is never fragmented at the lowest order
		 * and so free[0] is overloaded with the total free
		 * memory.
		 */
		total_free = free[0].free_pages = 0;
		clock_gettime(CLOCK_MONOTONIC_RAW, &spec);
		for (order = 0; order < MAX_ORDER; order++) {
			unsigned long free_pages;

			free_pages = nr_free[order] << order;
			total_free += free_pages;
			if (order < MAX_ORDER - 1) {
				free[order + 1].free_pages =
					free[order].free_pages + free_pages;
				free[order + 1].msecs = (long long)get_msecs(&spec);
			}
		}
		free[0].free_pages = total_free;
		free[0].msecs = (long long)get_msecs(&spec);

		/*
		 * Offer the predictor the fragmented free memory
		 * vector but do nothing else unless it issues a
		 * prediction. Accumulate recommendation from
		 * prediction algorithm across all nodes so we
		 * adjust watermarks only once per wake up.
		 */
		result |= predict(opts, free, opts->page_lsq[nid],
			opts->high_wmark[nid], opts->low_wmark[nid], nid);

		if (opts->last_bigpages[nid] != 0) {
			clock_gettime(CLOCK_MONOTONIC_RAW, &spec_after);
			time_elapsed = get_msecs(&spec_after) -
					get_msecs(&opts->spec_before);
			if (free[MAX_ORDER-1].free_pages > opts->last_bigpages[nid]) {
				opts->compaction_rate =
					(free[MAX_ORDER-1].free_pages -
					opts->last_bigpages[nid]) / time_elapsed;
				if (opts->compaction_rate)
					log_info(5, " compaction rate on node %d is %ld pages/msec\n",
					nid, opts->compaction_rate);
			}
		}
		opts->last_bigpages[nid] = free[MAX_ORDER-1].free_pages;

		/*
		 * Start compaction if requested. There is a cost
		 * to compaction in the kernel. Avoid issuing
		 * compaction request twice in a row.
		 */
		if (result & MEMPREDICT_COMPACT) {
			if (!opts->compaction_requested[nid]) {
				log_info(1, "Triggering compaction on node %d, result=0x%lx\n", nid, result);
				if (!opts->dry_run) {
					opts->compaction_requested[nid] = result;
					result &= ~MEMPREDICT_COMPACT;
					opts->mem_pressure_trigger = MEMORY_PRESSURE_TRIGGER;
					triggered = 1;
				}
			}
		}
		/*
		 * Clear compaction requested flag for the next
		 * sampling interval
		 */
		// opts->compaction_requested[nid] = 0;
		opts->total_free_pages += free[0].free_pages;

		/*
		 * If all of /proc/buddyinfo has been processed,
		 * terminate this loop and start the next one
		 */
		if (retval == EOF_RET)
			break;
	}

	if (retval != EOF_RET && retval < 0) {
		log_err("error reading buddyinfo (%s)\n", strerror(errno));
		return -errno;
	}


	/*
	 * Adjust watermarks if needed. Both MEMPREDICT_RECLAIM
	 * and MEMPREDICT_LOWER_WMARKS can be set even though it
	 * seems contradictory. That is because result accumulates
	 * all prediction results across all nodes. One node may
	 * have enough free pages or has increasing number of
	 * free pages while another node may be running low. In
	 * such cases, MEMPREDICT_RECLAIM takes precedence
	 */
	opts->final_result = result;
	if (result & (MEMPREDICT_RECLAIM | MEMPREDICT_LOWER_WMARKS)) {
		log_dbg("%s: Trigger, result=0x%lx\n", __func__, result);
		opts->mem_pressure_trigger = MEMORY_PRESSURE_TRIGGER;
		triggered = 1;
	}

	reclaimed_pages = no_pages_reclaimed(opts);
	if (opts->last_reclaimed) {
		clock_gettime(CLOCK_MONOTONIC_RAW, &spec_after);
		time_elapsed = get_msecs(&spec_after) - get_msecs(&opts->spec_before);

		opts->reclaim_rate = (reclaimed_pages - opts->last_reclaimed) / time_elapsed;
		if (opts->reclaim_rate)
			log_info(5, "** reclamation rate is %ld pages/msec\n", opts->reclaim_rate);
	}
	opts->last_reclaimed = reclaimed_pages;

	/*
	 * Get time from CLOCK_MONOTONIC_RAW which is not subject
	 * to perturbations caused by sysadmin or ntp adjustments.
	 * This ensures reliable calculations for the least square
	 * fit algorithm.
	 */
	clock_gettime(CLOCK_MONOTONIC_RAW, &opts->spec_before);

	return triggered;
}

int
check_memory_pressure_effect(struct adaptivemmd_opts * const opts)
{
	unsigned long result = 0;
	int nid;

        /*
         * bail out if this module is not enabled
         */
        if (!opts->memory_pressure_check_enabled) {
                return 0;
	}
	if (opts->mem_pressure_trigger == NO_TRIGGER) {
                return -EINVAL;
	}
	if (opts->mem_pressure_trigger != MEMORY_PRESSURE_TRIGGER) {
		log_err("%s: trigger type should be %d, but got %d\n",
		    MEMORY_PRESSURE_TRIGGER, opts->mem_pressure_trigger);
		return -EINVAL;
	}

	for (nid = 0; nid < MAX_NUMANODES; nid++) {
		result = opts->compaction_requested[nid];
		if (result & MEMPREDICT_COMPACT) {
			log_info(2, "%s: Doing compaction on node %d\n", __func__, nid);
			if (!opts->dry_run) {
				int ret = compact(nid);
				if (ret < 0) {
					log_err("%s: compact() failed, ret=%d\n", __func__, ret);
					return ret;
				}
			}
		}
	}

	result = opts->final_result;
	log_dbg("%s: result=0x%lx\n", __func__, result);
	/*
	 * Adjust watermarks if needed. Both MEMPREDICT_RECLAIM
	 * and MEMPREDICT_LOWER_WMARKS can be set even though it
	 * seems contradictory. That is because result accumulates
	 * all prediction results across all nodes. One node may
	 * have enough free pages or has increasing number of
	 * free pages while another node may be running low. In
	 * such cases, MEMPREDICT_RECLAIM takes precedence
	 */
	if (result & (MEMPREDICT_RECLAIM | MEMPREDICT_LOWER_WMARKS)) {
		if (result & MEMPREDICT_RECLAIM)
			rescale_watermarks(opts, 1);
		else
			rescale_watermarks(opts, 0);
	}
	return 0;
}

/*
 * run_adaptivemm_init() - Initialize settings that are set once at
 *	adaptivemmd startup
 *
 */
int run_adaptivemm_init(struct adaptivemmd_opts * const opts, int interval)
{
	struct utsname name;
	int ret = 0;

#if 0
        /* Handle signals to ensure proper cleanup */
        signal(SIGTERM, mysig);
        signal(SIGHUP, mysig);

        /* Check if an instance is running already */
        lockfd = open(LOCKFILE, O_RDWR|O_CREAT|O_EXCL, 0644);
        if (lockfd < 0) {
                if (errno == EEXIST)
                        adaptived_err("Lockfile %s exists. Another daemon may be running. Exiting now\n", LOCKFILE);
                else
                        adaptived_err("Failed to open lockfile %s\n", LOCKFILE);
                bailout(1);
        } else {
                del_lock = 1;
                ftruncate(lockfd, 0);
        }

        snprintf(tmpbuf, TMPCHARBUFSIZE, "%ld\n", (long)getpid());
        if (write(lockfd, (const void *)tmpbuf, strlen(tmpbuf)) < 0) {
                adaptived_err("Failed to write PID to lockfile %s\n", LOCKFILE);
                close(lockfd);
                bailout(1);
        }
#endif
	if (!check_permissions()) {
		log_err("ERROR: No permission to read/write required files. Are you running as root? Exiting\n");
		goto error;
	}

	/*
         * Determine the architecture we are running on and decide if "DMA"
         * zone should be skipped in total memory calculations
         */
        uname(&name);
        if (strcmp(name.machine, "x86_64") == 0 ||
                strcmp(name.machine, "i686") == 0)
                opts->skip_dmazone = 1;
        else
                opts->skip_dmazone = 0;

	debug_mode = opts->debug_mode;
	verbose = opts->verbose;
	/*
	 * Set up values for parameters based upon interval
	 */
	switch (interval / 1000) {
	case LOW_PERIODICITY:           /* 60 second interval */
		opts->aggressiveness = 1;
		opts->maxwsf = 400;
		opts->max_compaction_order = MAX_ORDER - 6;
		opts->periodicity = LOW_PERIODICITY;
		break;
	case NORM_PERIODICITY:          /* 30 second interval */
		opts->aggressiveness = 2;
		opts->maxwsf = 700;
		opts->max_compaction_order = MAX_ORDER - 4;
		opts->periodicity = NORM_PERIODICITY;
		break;
	case HIGH_PERIODICITY:          /* 15 second interval */
		opts->aggressiveness = 3;
		opts->maxwsf = 1000;
		opts->max_compaction_order = MAX_ORDER - 2;
		opts->periodicity = HIGH_PERIODICITY;
		break;
	default:
		if (opts->memleak_check_enabled && (!opts->neg_dentry_check_enabled
		    && !opts->memory_pressure_check_enabled)) {
			/* if only check_memory_leak(), it can be run at various intervals. */
			opts->aggressiveness = 3;
			opts->maxwsf = 1000;
			opts->max_compaction_order = MAX_ORDER - 2;
			opts->periodicity = HIGH_PERIODICITY;
		} else {
			log_err("Invalid interval: %dms, interval must be: %d, %d or %d\n",
			    interval, LOW_PERIODICITY * 1000, NORM_PERIODICITY * 1000,
				HIGH_PERIODICITY * 1000);
			ret = -EINVAL;
			goto error;
		}
	}
	update_zone_watermarks(opts);

        /*
         * If user specifies a maximum gap value for gap between low
         * and high watermarks, recompute maxwsf to account for that.
         * Update zone page information first.
        */
        if (opts->maxgap != 0) {
                unsigned long total_managed = 0;
		int i;

                for (i = 0; i < MAX_NUMANODES; i++)
                        total_managed += opts->managed_pages[i];
                opts->maxwsf = (opts->maxgap * 10000UL * 1024UL * 1024UL * 1024UL)/(total_managed * getpagesize());
        }
        opts->mywsf = opts->maxwsf;

        /*
         * get base page size for the system and convert it to kB
         */
        opts->base_psize = getpagesize()/1024;

        /*
         * Update free page and hugepage counts before initialization
         */
        update_zone_watermarks(opts);
        ret = update_hugepages(opts);
        if (ret < 0) {
                log_err("%s: update_hugepages() failed, ret=%d\n", __func__, ret);
                goto error;
        }
        ret = update_neg_dentry(opts, true);
        if (ret < 0) {
                log_err("%s: update_neg_dentry() failed, ret=%d\n", __func__, ret);
                goto error;
        }
        ret = check_memory_pressure(opts, true);
        if (ret < 0) {
                log_err("%s: check_memory_pressure() failed, ret=%d\n", __func__, ret);
                goto error;
        }
        ret = check_memory_leak(opts, true);
        if (ret < 0) {
                log_err("%s: check_memory_leak() failed, ret=%d\n", __func__, ret);
                goto error;
        }
	log_info(1, "adaptivemmd %s started (verbose=%d, debug_mode=%d, aggressiveness=%d, maxgap=%d)\n",
                VERSION, verbose, debug_mode, opts->aggressiveness, opts->maxgap);
	return ret;

error:
	adaptived_err("run_adaptivemm_init: FAIL, ret=%d\n", ret);
	return ret;
}

/*
 * Start with updated zone watermarks and number of hugepages
 * allocated since these can be adjusted by user any time.
 * Update maxwsf to account for hugepages just in case
 * number of hugepages changed unless user already gave
 * a maxgap value.
 */
int run_adaptivemm(struct adaptivemmd_opts * const opts)
{
	int ret;
	int triggered = 0;

        update_zone_watermarks(opts);
        ret = update_hugepages(opts);
        if (ret < 0) {
                log_err("%s: update_hugepages() failed, ret=%d\n", __func__, ret);
                goto error;
        }
	if (ret > 0) {
		int delta = ret;

		ret = updates_for_hugepages(opts, delta);
		if (ret < 0) {
			log_err("%s: updates_for_hugepages() failed, delta=%d, ret=%d\n", __func__, delta, ret);
			goto error;
		}
	}
	if (opts->maxgap == 0)
		rescale_maxwsf(opts);

	opts->mem_pressure_trigger = NO_TRIGGER;
	opts->mem_leak_trigger = NO_TRIGGER;

        ret = check_memory_pressure(opts, false);
        if (ret < 0) {
                log_err("%s: check_memory_pressure() failed, ret=%d\n", __func__, ret);
                goto error;
        }
	if (ret)
		triggered = 1; 

        ret = check_memory_leak(opts, false);
        if (ret < 0) {
                log_err("%s: check_memory_leak() failed, ret=%d\n", __func__, ret);
                goto error;
        }
	if (ret)
		triggered = 1; 
	return triggered;

error:
	return ret;
}

int run_adaptivemm_effects(struct adaptivemmd_opts * const opts)
{
	int ret;

        ret = check_memory_pressure_effect(opts);
        if (ret < 0) {
                log_err("%s: check_memory_pressure_effect() failed, ret=%d\n", __func__, ret);
                goto error;
        }
        ret = check_memory_leak_effect(opts);
        if (ret < 0) {
                log_err("%s: check_memory_leak_effect() failed, ret=%d\n", __func__, ret);
                goto error;
        }
	return ret;

error:
	return ret;
}
