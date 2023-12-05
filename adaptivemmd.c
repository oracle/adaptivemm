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
#include "predict.h"

#define VERSION		"2.1.0"

#define LOCKFILE	"/var/run/adaptivemmd.pid"
/*
 * System files that provide information
 */
#define	BUDDYINFO		"/proc/buddyinfo"
#define ZONEINFO		"/proc/zoneinfo"
#define VMSTAT			"/proc/vmstat"
#define MEMINFO			"/proc/meminfo"
#define KPAGECOUNT		"/proc/kpagecount"
#define KPAGEFLAGS		"/proc/kpageflags"
#define MODULES			"/proc/modules"
#define HUGEPAGESINFO		"/sys/kernel/mm/hugepages"

/*
 * System files to control reclamation and compaction
 */
#define RESCALE_WMARK		"/proc/sys/vm/watermark_scale_factor"
#define	COMPACT_PATH_FORMAT	"/sys/devices/system/node/node%d/compact"

/*
 * System files to control negative dentries
 */
#define NEG_DENTRY_LIMIT	"/proc/sys/fs/negative-dentry-limit"

/*
 * Possible locations for configuration files
 */
#define CONFIG_FILE1		"/etc/sysconfig/adaptivemmd"
#define CONFIG_FILE2		"/etc/default/adaptivemmd"

#define MAX_NUMANODES	1024

#define MAX_VERBOSE	5
#define MAX_AGGRESSIVE	3
#define MAX_NEGDENTRY	100

/*
 * Number of consecutive samples showing growth in unaccounted memory
 * that will trigger memory leak warning
 */
#define UNACCT_MEM_GRTH_MAX	10

/* Minimum % change in meminfo numbers to trigger warnings */
#define MEM_TRIGGER_DELTA	10

unsigned long min_wmark[MAX_NUMANODES], low_wmark[MAX_NUMANODES];
unsigned long high_wmark[MAX_NUMANODES], managed_pages[MAX_NUMANODES];
unsigned long total_free_pages, total_cache_pages, total_hugepages, base_psize;
long compaction_rate, reclaim_rate;
struct lsq_struct page_lsq[MAX_NUMANODES][MAX_ORDER];
int dry_run;
int debug_mode, verbose, del_lock = 0;
unsigned long maxgap;
int aggressiveness = 2;
int periodicity, skip_dmazone;
int neg_dentry_pct = 15;	/* default is 1.5% */

/* Flags to enable various modules */
bool memory_pressure_check_enabled = true;
bool neg_dentry_check_enabled = true;
bool memleak_check_enabled = true;

/*
 * Highest value to set watermark_scale_factor to. This value is tied
 * to aggressiveness level. Higher  level of aggressiveness will result
 * in higher value for this. Set the default to match default
 * aggressiveness value
 */
unsigned int maxwsf = 700;
unsigned int mywsf;

/*
 * Highest order pages to lok at for fragmentation. Ignore order 10
 * pages, they require moving around lots of pages to create and thus
 * are expensive. This value is tied into aggressiveness level. Higher
 * level of aggressiveness will result in going up on the order for
 * fragmentation check. Set the default to match default aggressiveness
 * value
 */
int max_compaction_order = MAX_ORDER - 4;


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

void
log_msg(int level, char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	if (debug_mode) {
		char stamp[32], prepend[16];
		time_t now;
		struct tm *timenow;

		now = time(NULL);
		timenow = localtime(&now);
		strftime(stamp, 32, "%b %d %T", timenow);
		printf("%s ", stamp);
		switch (level) {
			case LOG_ERR:
				strcpy(prepend, "ERROR:");
				break;
			case LOG_WARNING:
				strcpy(prepend, "Warning:");
				break;
			case LOG_INFO:
				strcpy(prepend, "Info:");
				break;
			case LOG_DEBUG:
				strcpy(prepend, "Debug:");
				break;
		}
		printf("%s ", prepend);
		vprintf(fmt, args);
		printf("\n");
	}
	else {
		vsyslog(level, fmt, args);
	}
	va_end(args);
}

/*
 * Initiate memory compactiomn in the kernel on a given node.
 */
void
compact(int node_id)
{
	char compactpath[PATH_MAX];
	int fd;
	char c = '1';

	if (snprintf(compactpath, sizeof(compactpath), COMPACT_PATH_FORMAT,
	    node_id) >= sizeof(compactpath)) {
		(void) log_err("compactpath is too long");
		bailout(1);
	}

	if ((fd = open(compactpath, O_WRONLY|O_NONBLOCK)) == -1) {
		log_err("opening compaction path (%s)", strerror(errno));
		bailout(1);
	}

	if (write(fd, &c, sizeof(c)) != sizeof(c)) {
		log_err("writing to compaction path (%s)", strerror(errno));
		bailout(1);
	}

	close(fd);
}

/* Parse a single input line;  return 1 if successful or 0 otherwise. */
int
scan_line(char *line, char *node, char *zone, unsigned long *nr_free)
{
	char copy[LINE_MAX];
	unsigned int order;
	char *t;

	(void) strncpy(copy, line, sizeof(copy));

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
get_next_node(FILE *ifile, int *nid, unsigned long *nr_free)
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
			log_err("ftell on buddyinfo failed (%s)", strerror(errno));
			return ERR;
		}
		if (fgets(line, sizeof(line), ifile) == NULL) {
			if (feof(ifile)) {
				rewind(ifile);
				return EOF_RET;
			} else {
				log_err("fgets(): %s",
						strerror(ferror(ifile)));
				return ERR;
			}
		}

		if (!scan_line(line, node, zone, free_pages)) {
			log_err("invalid input: %s", line);
			return ERR;
		}
		sscanf(node, "%d", nid);

		/*
		 * Accumulate free pages infor for just the current node
		 */
		if (current_node == -1)
			current_node = *nid;

		if (*nid != current_node) {
			if (fseek(ifile, cur_pos, SEEK_SET) < 0) {
				log_err("fseek on buddyinfo failed (%s)", strerror(errno));
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
update_hugepages()
{
	DIR *dp;
	struct dirent *ep;
	unsigned long newhpages = 0;
	int rc = -1;

	dp = opendir(HUGEPAGESINFO);
	if (dp != NULL) {
		while ((ep = readdir(dp)) != NULL) {
			if (ep->d_type == DT_DIR) {
				FILE *fp;
				long pages, psize;
				char tmpstr[312];

				/* Check if it is one of the hugepages dir */
				if (strstr(ep->d_name, "hugepages-") == NULL)
					continue;
				snprintf(tmpstr, sizeof(tmpstr), "%s/%s/nr_hugepages", HUGEPAGESINFO, ep->d_name);
				if ((fp = fopen(tmpstr, "r")) != NULL) {
					if (fgets(tmpstr, sizeof(tmpstr), fp) != NULL) {
						sscanf(tmpstr, "%ld", &pages);
						sscanf(ep->d_name, "hugepages-%ldkB", &psize);
						newhpages += pages * psize / base_psize;
					}
					fclose(fp);
				}
			}
		}
		if (newhpages) {
			unsigned long tmp;

			tmp = abs(newhpages - total_hugepages);
			/*
			 * If number of hugepages changes from 0 to a
			 * positive number, percentage calculation will
			 * fail. Set the percentage to a high number
			 */
			if (total_hugepages == 0)
				rc = INT_MAX;
			else
				rc = (tmp*100)/total_hugepages;
		}
		/*
		 * If number of hugepages changes to 0, that would be
		 * a 100% change
		 */
		else if (total_hugepages)
			rc = 100;
		else
			rc = 0;
		total_hugepages = newhpages;
		closedir(dp);
	}

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
update_zone_watermarks()
{
	FILE *fp = NULL;
	size_t len = 256;
	char *line = malloc(len);
	int current_node = -1;

	fp = fopen(ZONEINFO, "r");
	if (!fp)
		return 0;

	while ((fgets(line, len, fp) != NULL)) {
		if (strncmp(line, "Node", 4) == 0) {
			char node[FLDLEN], zone[FLDLEN], zone_name[FLDLEN];
			int nid;
			unsigned long min, low, high, managed;

			sscanf(line, "%s %d, %s %8s\n", node, &nid, zone, zone_name);
			if ((current_node == -1) || (current_node != nid)) {
				current_node = nid;
				min_wmark[nid] = low_wmark[nid] = 0;
				high_wmark[nid] = managed_pages[nid] = 0;
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
			if (!skip_dmazone || (strncmp("DMA", zone_name, FLDLEN) != 0)) {
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

					sscanf(line, "%s %lu\n", name, &val);
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

				min_wmark[nid] += min;
				low_wmark[nid] += low;
				high_wmark[nid] += high;
				managed_pages[nid] += managed;
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
rescale_maxwsf()
{
	unsigned long reclaimable_pages, total_managed = 0;
	unsigned long gap, new_wsf;
	int i;

	if (total_hugepages == 0)
		return;

	for (i = 0; i < MAX_NUMANODES; i++)
		total_managed += managed_pages[i];
	if (total_managed == 0) {
		log_info(1, "Number of managed pages is 0");
		return;
	}

	/*
	 * Compute what the gap would be if maxwsf is applied to
	 * just reclaimable memory
	 */
	reclaimable_pages = total_managed - total_hugepages;
	gap = (reclaimable_pages * maxwsf)/10000;

	/*
	 * Now compute WSF needed for same gap if all memory was taken
	 * into account
	 */
	new_wsf = (gap * 10000)/total_managed;

	if ((new_wsf > 9) && (new_wsf < 1000))
		mywsf = new_wsf;
	else
		log_warn("Failed to compute reasonable WSF, %ld, total pages %ld, reclaimable pages %ld", new_wsf, total_managed, reclaimable_pages);
}

/*
 * Get the number of pages stolen by kswapd from /proc/vmstat.
 */
unsigned long
no_pages_reclaimed()
{
	FILE *fp = NULL;
	size_t len = 100;
	char *line = malloc(len);
	unsigned long val, reclaimed;
	char desc[100];

	fp = fopen(VMSTAT, "r");
	if (!fp)
		return 0;

	total_cache_pages = reclaimed = 0;
	while ((fgets(line, len, fp) != NULL)) {
		sscanf(line, "%s %lu\n", desc, &val );
		if (strcmp(desc, "pgsteal_kswapd") == 0)
			reclaimed += val;
		if (strcmp(desc, "pgsteal_kswapd_normal") == 0)
			reclaimed += val;
		if (strcmp(desc, "pgsteal_kswapd_movable") == 0)
			reclaimed += val;
		if (strcmp(desc, "nr_inactive_file") == 0)
			total_cache_pages += val;
		if (strcmp(desc, "nr_inactive_anon") == 0)
			total_cache_pages += val;
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
rescale_watermarks(int scale_up)
{
	int fd, i, count;
	unsigned long scaled_watermark, frac_free;
	char scaled_wmark[20], *c;
	unsigned long total_managed = 0;
	unsigned long mmark, lmark, hmark;

	for (i = 0; i < MAX_NUMANODES; i++)
		total_managed += managed_pages[i];
	/*
	 * Hugepages should not be taken into account for watermark
	 * calculations since they are not reclaimable
	 */
	total_managed -= total_hugepages;
	if (total_managed == 0) {
		log_info(1, "Number of managed non-huge pages is 0");
		return;
	}

	/*
	 * Fraction of managed pages currently free
	 */
	frac_free = (total_free_pages*1000)/total_managed;

	/*
	 * Get the current watermark scale factor.
	 */
	if ((fd = open(RESCALE_WMARK, O_RDONLY)) == -1) {
		log_err("Failed to open "RESCALE_WMARK" (%s)", strerror(errno));
		return;
	}

	if (read(fd, scaled_wmark, sizeof(scaled_wmark)) < 0) {
		log_err("Failed to read from "RESCALE_WMARK" (%s)", strerror(errno));
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
		lmark += low_wmark[i];
		hmark += high_wmark[i];
		if (low_wmark[i] != 0 )
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
		if (total_free_pages < ((lmark+hmark)/2))
			scaled_watermark = (atoi(scaled_wmark) * 9)/10;
		else
			scaled_watermark = ((unsigned long)(1000 - frac_free)/10)*10;
		/*
		 * Make sure new watermark scale factor does not end up
		 * higher than current value accidentally
		 */
		if (scaled_watermark >= atoi(scaled_wmark))
			scaled_watermark = (atoi(scaled_wmark) * 9)/10;
	}
	else {
		/*
		 * Determine how urgent the situation is regarding
		 * remaining free pages. If free pages are already
		 * below high watermark, check if there are enough
		 * pages potentially available in buffer cache.
		 */
		if (total_free_pages < hmark) {
			if (total_cache_pages > (hmark - total_free_pages)) {
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
			}
			else {
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
		}
		else {
			/*
			 * System is not in dire situation at this time yet.
			 * Free pages are expected to approach high
			 * watermark soon though. If there are lots of
			 * reclaimable pages available, start semi-aggressive
			 * reclamation. If not, raise watermark scale factor
			 * but only by 10% if above 100 otherwise by 20%
			 */
			if (total_cache_pages > (total_free_pages - hmark)) {
				scaled_watermark = ((unsigned long)(1000 - frac_free)/20)*10;
				if (scaled_watermark == 0)
					return;
			}
			else {
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
	if (scaled_watermark > mywsf)
		scaled_watermark = mywsf;

	/*
	 * Before committing to the new higher value of wsf, make sure
	 * it will not cause low watermark to be raised above current
	 * number of free pages. If that were to happen OOM killer will
	 * step in immediately. Allow for at least 2% headroom over low
	 * watermark.
	 */
	if (scale_up) {
		unsigned long threshold, loose_pages = total_free_pages +
							total_cache_pages;
		unsigned long new_lmark;

		mmark = lmark = 0;
		for (i = 0; i < MAX_NUMANODES; i++) {
			mmark += min_wmark[i];
			lmark += low_wmark[i];
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
		 * already below this threshhold, setting this new wsf
		 * is very likely to kick OOM killer.
		 */
		threshold = new_lmark + total_free_pages * 1.02;

		if (loose_pages <= threshold) {
			/*
			 * See if we can raise wsf by 10%
			 */
			scaled_watermark = (atoi(scaled_wmark) * 11)/10;
			new_lmark = mmark + ((lmark-mmark)*scaled_watermark/atoi(scaled_wmark));
			threshold = new_lmark + total_free_pages * 1.02;
			if (loose_pages <= threshold) {
				log_info(2, "Not enough free pages to raise watermarks, free pages=%ld, reclaimable pages=%ld, new wsf=%ld, min=%ld, current low wmark=%ld, new projected low watermark=%ld", total_free_pages, total_cache_pages, scaled_watermark, mmark, lmark, new_lmark);
				return;
			}
		}
	}

	if (atoi(scaled_wmark) == scaled_watermark) {
		if (scaled_watermark == mywsf)
			log_info(2, "At max WSF already (max WSF = %u)", mywsf);
		return;
	}

	log_info(1, "Adjusting watermarks. Current watermark scale factor = %s", scaled_wmark);
	if (dry_run)
		goto out;

	log_info(1, "New watermark scale factor = %ld", scaled_watermark);
	sprintf(scaled_wmark, "%ld\n", scaled_watermark);
	if ((fd = open(RESCALE_WMARK, O_WRONLY)) == -1) {
		log_err("Failed to open "RESCALE_WMARK" (%s)", strerror(errno));
		return;
	}
	if (write(fd, scaled_wmark, strlen(scaled_wmark)) < 0)
		log_err("Failed to write to "RESCALE_WMARK" (%s)", strerror(errno));

out:
	close(fd);
	return;
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
 *			run succesfully
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
		log_err("Can not open "RESCALE_WMARK" (%s)", strerror(errno));
		return 0;
	}

	/* Can we write to this file */
	if (read(fd, tmpstr, sizeof(tmpstr)) < 0) {
		log_err("Can not read "RESCALE_WMARK" (%s)", strerror(errno));
		return 0;
	}
	close(fd);
	if ((fd = open(RESCALE_WMARK, O_WRONLY)) == -1) {
		log_err("Can not open "RESCALE_WMARK" (%s)", strerror(errno));
		return 0;
	}

	if (write(fd, tmpstr, strlen(tmpstr)) < 0) {
		log_err("Can not write to "RESCALE_WMARK" (%s)", strerror(errno));
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
void
update_neg_dentry(bool init)
{
	int fd;

	/*
	 * bail out if this module is not enabled
	 */
	if (!neg_dentry_check_enabled)
		return;

	/*
	 * Set up negative dentry limit if functionality is supported on
	 * this kernel.
	 */
	if (access(NEG_DENTRY_LIMIT, F_OK) == 0) {
		if ((fd = open(NEG_DENTRY_LIMIT, O_RDWR)) == -1) {
			log_err("Failed to open "NEG_DENTRY_LIMIT" (%s)", strerror(errno));
		} else {
			/*
			 * Compute the value for negative dentry limit
			 * based upon just the reclaimable pages
			 */
			int val, i;
			unsigned long reclaimable_pages, total_managed = 0;
			char neg_dentry[LINE_MAX];

			for (i = 0; i < MAX_NUMANODES; i++)
				total_managed += managed_pages[i];
			reclaimable_pages = total_managed - total_hugepages;
			val = (reclaimable_pages * neg_dentry_pct)/total_managed;
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
			if (write(fd, neg_dentry, strlen(neg_dentry)) < 0)
				log_err("Failed to write to "NEG_DENTRY_LIMIT" (%s)", strerror(errno));
			close(fd);
		}
	}
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
get_unmapped_pages()
{
#define BATCHSIZE	8192
	int fd1, fd2;
	long pagecnt[BATCHSIZE/sizeof(long)];
	unsigned long pageflg[BATCHSIZE/sizeof(long)];
	ssize_t inbytes1, inbytes2;
	unsigned long count, unmapped_pages = 0;

	if ((fd1 = open(KPAGECOUNT, O_RDONLY)) == -1) {
		log_err("Error opening kpagecount");
		return(-1);
	}

	if ((fd2 = open(KPAGEFLAGS, O_RDONLY)) == -1) {
		log_err("Error opening kpageflags");
		return(-1);
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
				!((pageflg[i]>>KPF_HUGE) & 1))
				unmapped_pages++;

nextloop:
			i++;
		}
		inbytes1 = read(fd2, pageflg, BATCHSIZE);
		inbytes2 = read(fd1, pagecnt, BATCHSIZE);
		count = inbytes2/sizeof(long);
	}

	if ((inbytes1 < 0) || (inbytes2 < 0)) {
		if (inbytes1 < 0)
			log_err("Error reading kpageflagst");
		else
			log_err("Error reading kpagecount");
		close(fd1);
		close(fd2);
		return(-1);
	}

	close(fd1);
	close(fd2);
	return(unmapped_pages);
}

/*
 * pr_meminfo() - log /proc/meminfo contents
 */
void
pr_meminfo(int level)
{
	FILE *fp = NULL;
	char line[LINE_MAX];

	fp = fopen(MEMINFO, "r");
	if (!fp)
		return;

	while ((fgets(line, sizeof(line), fp) != NULL)) {
		log_info(level, "%s", line);
	}

	fclose(fp);

}

enum memdata_items {
	MEMAVAIL,
	BUFFERS,
	CACHED,
	SWPCACHED,
	UNEVICTABLE,
	MLOCKED,
	ANONPAGES,
	MAPPED,
	SHMEM,
	KRECLAIMABLE,
	SLAB,
	SUNRECLAIM,
	KSTACK,
	PGTABLE,
	SECPGTABLE,
	VMALLOCUSED,
	CMA,
	NR_MEMDATA_ITEMS
};

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
cmp_meminfo(int level, unsigned long *memdata, unsigned long *pr_memdata)
{
	int i;
	unsigned long delta;

	for (i=0; i<NR_MEMDATA_ITEMS; i++) {
		delta = abs(pr_memdata[i] - memdata[i]);
		if (delta == 0)
			continue;

		/* Is the change greater than warning trigger level */
		if (delta > (pr_memdata[i] * (MEM_TRIGGER_DELTA/100)))
			log_info(level, "%s %s by more than %d (previous = %lu K, current = %lu K)\n", memdata_item_name[i], (pr_memdata[i] < memdata[i] ? "grew":"decreased"), MEM_TRIGGER_DELTA, pr_memdata[i], memdata[i]);
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
void
check_memory_leak(bool init)
{
	static unsigned long base_mem, mem_remain, gr_count, prv_free;
	static unsigned long pr_memdata[NR_MEMDATA_ITEMS];
	unsigned long memdata[NR_MEMDATA_ITEMS];
	unsigned long i, mem_acctd;
	unsigned long total_managed = 0;
	long unmapped_pages;
	FILE *fp = NULL;
	char line[LINE_MAX];
	unsigned long val, inuse_mem, freemem, unacct_mem, mem_total;
	char desc[LINE_MAX];

	/*
	 * bail out if this module is not enabled
	 */
	if (!memleak_check_enabled)
		return;

	for (i = 0; i < MAX_NUMANODES; i++)
		total_managed += managed_pages[i];
	for (i = 0; i < NR_MEMDATA_ITEMS; i++)
		memdata[i] = 0;

	/*
	 * Now read meminfo file to get current memory info
	 */
	fp = fopen(MEMINFO, "r");
	if (!fp)
		return;

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
		sscanf(line, "%s %lu\n", desc, &val );
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
	inuse_mem = inuse_mem/base_psize;
	freemem = freemem/base_psize;
	mem_total = mem_total/base_psize;
	memdata[MEMAVAIL] = memdata[MEMAVAIL]/base_psize;
	fclose(fp);

	/*
	 * Sum of memory in use, memory allocated to hugepages and free
	 * memory is the amount of memory we can account for from
	 * /proc/meminfo
	 */
	mem_acctd = freemem + total_hugepages + inuse_mem;
	if (init) {
		/*
		 * The difference between total memory and memory
		 * accounted for is likely to be memory in use by kernel
		 * and kernel modules plus memory reserved by kernel,
		 * especially if this daemon was started when machine
		 * rebooted. Record this difference as base memory usage
		 * by the system
		 */
		base_mem = total_managed - mem_acctd;
		prv_free = freemem;
		for (i = 0; i < NR_MEMDATA_ITEMS; i++)
			pr_memdata[i] = memdata[i];
		log_info(5, "Base memory consumption set to %lu K", (base_mem * base_psize));
		goto out;
	}

	unmapped_pages = get_unmapped_pages();
	if (unmapped_pages < 0) {
		log_err("Failed to read unmapped pages count");
		unmapped_pages = 0;
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
		log_info(2, "Issue with memory computation, total_managed = %lu K, mem_acctd = %lu K, unmapped = %lu K", (total_managed * base_psize), (mem_acctd * base_psize), (unmapped_pages * base_psize));
		pr_meminfo(2);
	}
	val = total_managed - mem_acctd;
	if (val < base_mem) {
		base_mem = val;
		log_info(5, "Base memory consumption updated to %lu K", (base_mem * base_psize));
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
	if (total_managed > (mem_acctd + base_mem))
		unacct_mem = total_managed - (mem_acctd + base_mem);
	else
		unacct_mem = 0;

	log_info(5, "Unaccounted memory = %lu K, freemem = %lu K, memavail = %lu K", (unacct_mem * base_psize), (freemem * base_psize), (memdata[MEMAVAIL] * base_psize));

	/*
	 * If unaccounted for memory grew by more than warning trigger
	 * level compared to last sample, record it. If this unaccounted
	 * for memory continues to grow, we may have a slow steady leak.
	 */
	if (unacct_mem > (mem_remain * ((100+MEM_TRIGGER_DELTA)/100))) {
		if (mem_remain == 0) {
			mem_remain = unacct_mem;
			goto out;
		}

		gr_count++;

		/*
		 * Instead of slow steady leak, it is possible for large
		 * chunk of memory to be leaked all at once. If unaccounted
		 * memory doubled or more and we have seen growth in
		 * unaccounted memory for the last three consecutive samples,
		 * this may be a memory leak
		 */
		if (mem_remain && (unacct_mem > (mem_remain * 2)) &&
			(gr_count > 3)) {
			log_info(1, "Possible sudden memory leak - background memory use more than doubled (%lu K -> %lu K), unmapped memory = %lu K, freemem = %lu K, freemem previously = %lu K",
				(mem_remain * base_psize),
				(unacct_mem * base_psize),
				(unmapped_pages * base_psize),
				(freemem * base_psize),
				(prv_free * base_psize));
			pr_meminfo(1);
			cmp_meminfo(1, memdata, pr_memdata);
		}
		else {
			log_info(5, "Background memory use grew by more than %d (%lu -> %lu) K, unmapped memory = %lu K, freemem = %lu K, freemem previously = %lu K, MemAvail = %lu K", MEM_TRIGGER_DELTA,
				(mem_remain * base_psize),
				(unacct_mem * base_psize),
				(unmapped_pages * base_psize),
				(freemem * base_psize),
				(prv_free * base_psize),
				(memdata[MEMAVAIL] * base_psize));
			cmp_meminfo(1, memdata, pr_memdata);
		}
		mem_remain = unacct_mem;
	}
	else if (unacct_mem < (mem_remain * ((100-MEM_TRIGGER_DELTA)/100))){
		/*
		 * unaccounted memory is starting to shrink. Reset
		 * the counter
		 */
		gr_count = 0;
	}

	/*
	 * If unaccounted memory grew over 10 samples without shrinking
	 * in between, it may point to a slow leak
	 */
	if (gr_count > UNACCT_MEM_GRTH_MAX) {
		log_info(1, "Possible slow memory leak - background memory use has been growing steadily (currently %lu) K, unmapped memory = %lu K, freemem = %lu K, MemAvail = %lu K",
			(mem_remain * base_psize),
			(unmapped_pages * base_psize),
			(freemem * base_psize),
			(memdata[MEMAVAIL] * base_psize));
		pr_meminfo(1);
		cmp_meminfo(1, memdata, pr_memdata);
		gr_count = 0;
	}

out:
	/*
	 * Update persistent metrics for next invocation
	 */
	prv_free = freemem;
	for (i = 0; i < NR_MEMDATA_ITEMS; i++)
		pr_memdata[i] = memdata[i];
}

/*
 * updates_for_hugepages() - Update any values that need to be updated
 *	whenever number of hugepages on system changes significantly
 */
void
updates_for_hugepages(int delta)
{
	/*
	 * Don't do anything for delta less than 5%
	 */
	if (delta < 5)
		return;

	update_neg_dentry(false);
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
void
check_memory_pressure(bool init)
{
	static int compaction_requested[MAX_NUMANODES];
	static unsigned long last_bigpages[MAX_NUMANODES], last_reclaimed;
	static FILE *ifile;
	unsigned long nr_free[MAX_ORDER];
	struct frag_info free[MAX_ORDER];
	unsigned long time_elapsed, reclaimed_pages;
	unsigned long result = 0;
	int i, order, nid, retval;
	struct timespec spec, spec_after;
	static struct timespec spec_before;
	char *ipath;

	/*
	 * bail out if this module is not enabled
	 */
	if (!memory_pressure_check_enabled)
		return;

	if (init) {
		/*
		 * Initialize number of higher order pages seen in last scan
		 * and compaction requested per node
		 */
		for (i = 0; i < MAX_NUMANODES; i++) {
			last_bigpages[i] = 0;
			compaction_requested[i] = 0;
		}
		last_reclaimed = 0;

		ipath = BUDDYINFO;
		if ((ifile = fopen(ipath, "r")) == NULL) {
			log_err("fopen(input file)");
			bailout(1);
		}

		return;
	}

	/*
	 * Keep track of time to calculate the compaction and
	 * reclaim rates
	 */
	total_free_pages = 0;

	while ((retval = get_next_node(ifile, &nid, nr_free)) != 0) {
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
		result |= predict(free, page_lsq[nid],
				high_wmark[nid], low_wmark[nid], nid);

		if (last_bigpages[nid] != 0) {
			clock_gettime(CLOCK_MONOTONIC_RAW, &spec_after);
			time_elapsed = get_msecs(&spec_after) -
					get_msecs(&spec_before);
			if (free[MAX_ORDER-1].free_pages > last_bigpages[nid]) {
				compaction_rate =
					(free[MAX_ORDER-1].free_pages -
					last_bigpages[nid]) / time_elapsed;
				if (compaction_rate)
					log_info(5, "** compaction rate on node %d is %ld pages/msec",
					nid, compaction_rate);
			}
		}
		last_bigpages[nid] = free[MAX_ORDER-1].free_pages;

		/*
		 * Start compaction if requested. There is a cost
		 * to compaction in the kernel. Avoid issuing
		 * compaction request twice in a row.
		 */
		if (result & MEMPREDICT_COMPACT) {
			if (!compaction_requested[nid]) {
				log_info(2, "Triggering compaction on node %d", nid);
				if (!dry_run) {
					compact(nid);
					compaction_requested[nid] = 1;
					result &= ~MEMPREDICT_COMPACT;
				}
			}
		}
		/*
		 * Clear compaction requested flag for the next
		 * sampling interval
		 */
		compaction_requested[nid] = 0;
		total_free_pages += free[0].free_pages;

		/*
		 * If all of /proc/buddyinfo has been processed,
		 * terminate this loop and start the next one
		 */
		if (retval == -1)
			break;
	}

	if (retval == 0) {
		log_err("error reading buddyinfo (%s)", strerror(errno));
		bailout(1);
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
	if (result & (MEMPREDICT_RECLAIM | MEMPREDICT_LOWER_WMARKS)) {
		if (result & MEMPREDICT_RECLAIM)
			rescale_watermarks(1);
		else
			rescale_watermarks(0);
	}

	reclaimed_pages = no_pages_reclaimed();
	if (last_reclaimed) {
		clock_gettime(CLOCK_MONOTONIC_RAW, &spec_after);
		time_elapsed = get_msecs(&spec_after) - get_msecs(&spec_before);

		reclaim_rate = (reclaimed_pages - last_reclaimed) / time_elapsed;
		if (reclaim_rate)
			log_info(5, "** reclamation rate is %ld pages/msec", reclaim_rate);
	}
	last_reclaimed = reclaimed_pages;

	/*
	 * Get time from CLOCK_MONOTONIC_RAW which is not subject
	 * to perturbations caused by sysadmin or ntp adjustments.
	 * This ensures reliable calculations for the least square
	 * fit algorithm.
	 */
	clock_gettime(CLOCK_MONOTONIC_RAW, &spec_before);
}

/*
 * one_time_initializations() - Initialize settings that are set once at
 *	adaptivemmd startup
 *
 */
void
one_time_initializations()
{
	/*
	 * Update free page and hugepage counts before initialization
	 */
	update_zone_watermarks();
	update_hugepages();

	update_neg_dentry(true);
	check_memory_pressure(true);
	check_memory_leak(true);
}

/*
 * parse_config() - Parse the configuration file CONFIG_FILE1 or CONFIG_FILE2
 *
 * Read the configuration files skipping comment and blank lines. Each
 * valid line will have a parameter followed by '=' and a value.
 *
 * Returns:
 *	1	Parsing successful
 *	0	Parsing failed
 */
#define MAXTOKEN	32
#define OPT_V		"VERBOSE"
#define OPT_ENB_PAGE	"ENABLE_FREE_PAGE_MGMT"
#define OPT_GAP		"MAXGAP"
#define OPT_AGGR	"AGGRESSIVENESS"
#define OPT_ENB_DENTRY	"ENABLE_NEG_DENTRY_MGMT"
#define OPT_NEG_DENTRY1	"NEG-DENTRY-CAP"
#define OPT_NEG_DENTRY2	"NEG_DENTRY_CAP"
#define OPT_ENB_MEMLEAK	"ENABLE_MEMLEAK_CHECK"

int
parse_config()
{
	FILE *fstream;
	char *buf = NULL;
	size_t n = 0;

	if ((fstream = fopen(CONFIG_FILE1, "r")) == NULL) {
		if (errno == ENOENT) {
			/*
			 * Either of CONFIG_FILE1 and CONFIG_FILE2 may exist
			 */
			if ((fstream = fopen(CONFIG_FILE2, "r")) == NULL) {
				/* Config file may not exist and that is ok */
				if (errno == ENOENT) {
					return 1;
				}
				else {
					log_err("Can not open "CONFIG_FILE2" (%s)", strerror(errno));
					return 0;
				}
			}
		}
		else {
			log_err("Can not open "CONFIG_FILE1" (%s)", strerror(errno));
			return 0;
		}
	}

	while (getline(&buf, &n, fstream) > 0) {
		int i = 0;
		int j = 0;
		unsigned long val;
		char token[MAXTOKEN];

		/* Skip comments */
		if (buf[0] == '#')
			goto nextline;

		n = strlen(buf);
		/* Skip any whitespace at the beginning of a line */
		while ((i < n) && (isspace(buf[i])))
			i++;
		if (i >= n)
			goto nextline;

		/*
		 * Now look for '='. A valid line will  have a parameter
		 * name followed by '=' and a value with whitespace possible
		 * in between
		 */
		while ((buf[i] != '=') && (i < n) && (j < MAXTOKEN))
			if (!isspace(buf[i]))
				token[j++] = buf[i++];
			else
				i++;
		if (i >= n)
			goto nextline;
		token[j] = 0;
		val = strtoul(&buf[i+1], NULL, 0);

		if (strncmp(token, OPT_V, sizeof(OPT_V)) == 0) {
			if(val <= MAX_VERBOSE)
				verbose = val;
			else
				log_err("Verbosity value is greater than %d. Proceeding with defaults", MAX_VERBOSE);
		}
		else if (strncmp(token, OPT_ENB_PAGE, sizeof(OPT_ENB_PAGE)) == 0)
			memory_pressure_check_enabled = ((val==0)?false:true);
		else if (strncmp(token, OPT_GAP, sizeof(OPT_GAP)) == 0)
			maxgap = val;
		else if (strncmp(token, OPT_AGGR, sizeof(OPT_AGGR)) == 0) {
			if (val <= MAX_AGGRESSIVE)
				aggressiveness = val;
			else
				log_err("Aggressiveness value is greater than %d. Proceeding with defaults", MAX_AGGRESSIVE);
		}
		else if (strncmp(token, OPT_ENB_DENTRY, sizeof(OPT_ENB_DENTRY)) == 0)
			neg_dentry_check_enabled = ((val==0)?false:true);
		else if ((strncmp(token, OPT_NEG_DENTRY1, sizeof(OPT_NEG_DENTRY1)) == 0) ||
			(strncmp(token, OPT_NEG_DENTRY2, sizeof(OPT_NEG_DENTRY2)) == 0)) {
			/*
			 * negative dentry memory usage cap is expressed as
			 * a percentage of total memory on the system. Any
			 * number above MAX_NEGDENTRY can bring the system
			 * to its knees. A value of 0 here will disable
			 * negative dentry cacheing, so don't allow that
			 * either. If a sysadmin wants to disable cacheing,
			 * they can do it through sysctl tunables separately
			 * instead to doing it through adaptivemmd
			 */
			if (val > MAX_NEGDENTRY)
				log_err("Bad value for negative dentry cap = %d (>%d). Proceeding with default of %d", val, MAX_NEGDENTRY, neg_dentry_pct);
			else if (val < 1)
				neg_dentry_pct = 1;
			else
				neg_dentry_pct = val;
		}
		else if (strncmp(token, OPT_ENB_MEMLEAK, sizeof(OPT_ENB_MEMLEAK)) == 0)
			memleak_check_enabled = ((val==0)?false:true);
		else {
			log_err("Error in configuration file at token \"%s\". Proceeding with defaults", token);
			break;
		}


nextline:
		free(buf);
		buf = NULL;
		n = 0;
	}

	free(buf);
	fclose(fstream);
	return 1;
}

void
help_msg(char *progname)
{
	(void) printf(
		    "usage: %s "
		    "[-d] "
		    "[-v] "
		    "[-h] "
		    "[-s] "
		    "[-m <max_gb>] "
		    "[-a <level>]\n"
		    "Version %s\n"
		    "Options:\n"
		    "\t-v\tVerbose mode (use multiple to increase verbosity)\n"
		    "\t-d\tDebug mode (do not run as daemon)\n"
		    "\t-h\tHelp message\n"
		    "\t-s\tSimulate a run (dry run, implies \"-v -v -d\")\n"
		    "\t-m\tMaximum allowed gap between high and low watermarks in GB\n"
		    "\t-a\tAggressiveness level (1=high, 2=normal (default), 3=low)\n"
		    "\nNOTE: config options read from configuration file can be overridden\n      with command line options. Configuration file can be\n      %s or %s\n",
		    progname, VERSION, CONFIG_FILE1, CONFIG_FILE2);
}

#define TMPCHARBUFSIZE	128

int
main(int argc, char **argv)
{
	int c, i, lockfd;
	int errflag = 0;
	char tmpbuf[TMPCHARBUFSIZE];
	struct utsname name;

	openlog("adaptivemmd", LOG_PID, LOG_DAEMON);
	if (parse_config() == 0)
		bailout(1);

	while ((c = getopt(argc, argv, "a:m:hsvd")) != -1) {
		switch (c) {
		case 'a':
			aggressiveness = atoi(optarg);
			if ((aggressiveness < 1) || (aggressiveness > 3))
				aggressiveness = 2;
			break;
		case 'm':
			maxgap = atoi(optarg);
			break;
		case 'd':
			debug_mode = 1;
			break;
		case 'v':
			/* Ignore in case of dry run */
			if (dry_run == 0)
				verbose++;
			break;
		case 's':
			dry_run = 1;
			verbose = 2;
			debug_mode = 1;
			break;
		case 'h':
			help_msg(argv[0]);
			bailout(0);
		default:
			errflag++;
			break;
		}
	}

	/* Handle signals to ensure proper cleanup */
	signal(SIGTERM, mysig);
	signal(SIGHUP, mysig);

	/* Check if an instance is running already */
	lockfd = open(LOCKFILE, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (lockfd < 0) {
		if (errno == EEXIST)
			log_err("Lockfile %s exists. Another daemon may be running. Exiting now", LOCKFILE);
		else
			log_err("Failed to open lockfile %s", LOCKFILE);
		bailout(1);
	} else {
		del_lock = 1;
		ftruncate(lockfd, 0);
	}

	/* Become a daemon unless -d was specified */
	if (!debug_mode)
		if (daemon(0, 0) != 0) {
			log_err("Failed to become daemon. %s", strerror(errno));
			bailout(1);
		}

	snprintf(tmpbuf, TMPCHARBUFSIZE, "%ld\n", (long)getpid());
	if (write(lockfd, (const void *)tmpbuf, strlen(tmpbuf)) < 0)
		log_err("Failed to write PID to lockfile %s", LOCKFILE);
	close(lockfd);

	if (errflag) {
		help_msg(argv[0]);
		bailout(1);
	}

	if (!check_permissions())
		bailout(1);

	/*
	 * Determine the architecture we are running on and decide if "DMA"
	 * zone should be skipped in total memory calculations
	 */
	uname(&name);
	if (strcmp(name.machine, "x86_64") == 0 ||
		strcmp(name.machine, "i686") == 0)
		skip_dmazone = 1;
	else
		skip_dmazone = 0;

	/*
	 * Set up values for parameters based upon aggressiveness
	 * level desired
	 */
	switch (aggressiveness) {
		case 1:
			maxwsf = 400;
			max_compaction_order = MAX_ORDER - 6;
			periodicity = LOW_PERIODICITY;
			break;
		case 2:
			maxwsf = 700;
			max_compaction_order = MAX_ORDER - 4;
			periodicity = NORM_PERIODICITY;
			break;
		case 3:
			maxwsf = 1000;
			max_compaction_order = MAX_ORDER - 2;
			periodicity = HIGH_PERIODICITY;
			break;
	}

	update_zone_watermarks();

	/*
	 * If user specifies a maximum gap value for gap between low
	 * and high watermarks, recompute maxwsf to account for that.
	 * Update zone page information first.
         */
	if (maxgap != 0) {
		unsigned long total_managed = 0;
		for (i = 0; i < MAX_NUMANODES; i++)
			total_managed += managed_pages[i];
		maxwsf = (maxgap * 10000UL * 1024UL * 1024UL * 1024UL)/(total_managed * getpagesize());
	}
	mywsf = maxwsf;

	/*
	 * get base page size for the system and convert it to kB
	 */
	base_psize = getpagesize()/1024;

	pr_info("adaptivemmd "VERSION" started (verbose=%d, aggressiveness=%d, maxgap=%d)", verbose, aggressiveness, maxgap);

	one_time_initializations();

	while (1) {
		int retval;

		/*
		 * Start with updated zone watermarks and number of hugepages
		 * allocated since these can be adjusted by user any time.
		 * Update maxwsf to account for hugepages just in case
		 * number of hugepages changed unless user already gave
		 * a maxgap value.
		 */
		update_zone_watermarks();
		retval = update_hugepages();
		if (retval > 0)
			updates_for_hugepages(retval);
		if (maxgap == 0)
			rescale_maxwsf();

		check_memory_pressure(false);
		check_memory_leak(false);

		sleep(periodicity);
	}

	closelog();
	return(0);
}
