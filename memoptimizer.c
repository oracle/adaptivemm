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
#include <unistd.h>
#include <ctype.h>
#include "predict.h"

#define VERSION		"1.3.0"

#define	COMPACT_PATH_FORMAT	"/sys/devices/system/node/node%d/compact"
#define	BUDDYINFO		"/proc/buddyinfo"
#define ZONEINFO		"/proc/zoneinfo"
#define RESCALE_WMARK		"/proc/sys/vm/watermark_scale_factor"
#define VMSTAT			"/proc/vmstat"

#define CONFIG_FILE1		"/etc/sysconfig/memoptimizer"
#define CONFIG_FILE2		"/etc/default/memoptimizer"

#define MAX_NUMANODES	1024

unsigned long min_wmark[MAX_NUMANODES], low_wmark[MAX_NUMANODES];
unsigned long high_wmark[MAX_NUMANODES], managed_pages[MAX_NUMANODES];
unsigned long total_free_pages, total_cache_pages;
long compaction_rate, reclaim_rate;
struct lsq_struct page_lsq[MAX_NUMANODES][MAX_ORDER];
int dry_run;
int debug_mode, verbose;
unsigned long maxgap;
int aggressiveness = 2;
int periodicity;

/*
 * Highest value to set watermark_scale_factor to. This value is tied
 * to aggressiveness level. Higher  level of aggressiveness will result
 * in higher value for this. Set the default to match default
 * aggressiveness value
 */
unsigned long maxwsf = 700;

/*
 * Highest order pages to lok at for fragmentation. Ignore order 10
 * pages, they require moving around lots of pages to create and thus
 * are expensive. This value is tied into aggressiveness level. Higher
 * level of aggressiveness will result in going up on the order for
 * fragmentation check. Set the default to match default aggressiveness
 * value
 */
int max_compaction_order = MAX_ORDER - 4;


void
bailout(int retval)
{
	closelog();
	exit(retval);
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
 * Find the next normal zone in buddyinfo file. When one is found,
 * update free pages vector and return 1.
 *
 * TODO: Should this consider "Movable" zones as well for reclamation
 *	and compaction?
 */
int
get_next_zone(FILE *ifile, int *nid, unsigned long *nr_free)
{
	char line[LINE_MAX];
	char node[10], zone[10];

	/*
	 * Read the file one line at a time until we find the next
	 * "Normal" zone or reach EOF
	 */
	while (1) {
		if (fgets(line, sizeof(line), ifile) == NULL) {
			if (feof(ifile)) {
				return (-1);
			} else {
				log_err("fgets(): %s",
						strerror(ferror(ifile)));
				return 0;
			}
		}

		if (!scan_line(line, node, zone, nr_free)) {
			log_err("invalid input: %s", line);
			return 0;
		}
		if (strcmp(zone, "Normal") == 0)
			break;
	}
	sscanf(node, "%d", nid);

	return 1;
}

/*
 * Parse watermarks and zone_managed_pages values from /proc/zoneinfo
 */
int
update_zone_watermarks()
{
	FILE *fp = NULL;
	size_t len = 100;
	char *line = malloc(len);

	fp = fopen(ZONEINFO, "r");
	if (!fp)
		return 0;

	while ((fgets(line, len, fp) != NULL)) {
		if (strncmp(line, "Node", 4) == 0) {
			char node[20];
			int nid;
			char zone[20];
			char zone_name[20];

			sscanf(line, "%s %d, %s %8s\n", node, &nid, zone, zone_name);

			if (strcmp("Normal", zone_name) == 0) {
				int i = 0;
				unsigned long min, low, high, managed;

				if (fgets(line, len, fp) == NULL)
					goto out;

				while (i < 7) {
					unsigned long wmark;
					char name[20];

					if (fgets(line, len, fp) == NULL)
						goto out;

					sscanf(line, "%s %lu\n", name, &wmark);
					switch (i) {
						case 0:
							min = wmark;
							break;
						case 1:
							low = wmark;
							break;
						case 2:
							high = wmark;
							break;
						case 6:
							managed = wmark;
							break;
					}
					i++;
				}

				min_wmark[nid] = min;
				low_wmark[nid] = low;
				high_wmark[nid] = high;
				managed_pages[nid] = managed;
			}
		}
	}

out:
	fclose(fp);
	return 0;
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
	unsigned long lmark, hmark;

	for (i=0; i<MAX_NUMANODES; i++)
		total_managed += managed_pages[i];
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
	for (i=0; i<MAX_NUMANODES; i++) {
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
	if (scaled_watermark > maxwsf)
		scaled_watermark = maxwsf;

	if (atoi(scaled_wmark) == scaled_watermark)
		return;

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

		if (strncmp(token, "VERBOSE", sizeof("VERBOSE")) == 0)
			verbose = val;
		else if (strncmp(token, "MAXGAP", sizeof("MAXGAP")) == 0)
			maxgap = val;
		else if (strncmp(token, "AGGRESSIVENESS", sizeof("AGGRESSIVENESS")) == 0)
			aggressiveness = val;
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

int
main(int argc, char **argv)
{
	int c, i;
	int errflag = 0;
	int compaction_requested[MAX_NUMANODES];
	char *ipath;
	FILE *ifile;
	unsigned long last_bigpages[MAX_NUMANODES], last_reclaimed = 0;
	unsigned long time_elapsed, reclaimed_pages;

	openlog("memoptimizer", LOG_PID, LOG_DAEMON);
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

	/* Become a daemon unless -d was specified */
	if (!debug_mode)
		if (daemon(0, 0) != 0) {
			log_err("Failed to become daemon. %s", strerror(errno));
			bailout(1);
		}

	if (errflag) {
		help_msg(argv[0]);
		bailout(1);
	}

	if (!check_permissions())
		bailout(1);

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

	/*
	 * If user specifies a maximum gap value for gap between low
	 * and high watermarks, recompute maxwsf to account for that.
         */
	if (maxgap != 0) {
		unsigned long total_managed = 0;
		for (i=0; i<MAX_NUMANODES; i++)
			total_managed += managed_pages[i];
		maxwsf = (maxgap * 10000UL * 1024UL * 1024UL * 1024UL)/(total_managed * getpagesize());
	}

	/*
	 * Initialize number of higher order pages seen in last scan
	 * and compaction requested per node 
	 */
	for (i=0; i<MAX_NUMANODES; i++) {
		last_bigpages[i] = 0;
		compaction_requested[i] = 0;
	}

	ipath = BUDDYINFO;
	if ((ifile = fopen(ipath, "r")) == NULL) {
		log_err("fopen(input file)");
		bailout(1);
	}

	pr_info("Memoptimizer "VERSION" started (verbose=%d, aggressiveness=%d)", verbose, aggressiveness);

	while (1) {
		unsigned long nr_free[MAX_ORDER];
		struct frag_info free[MAX_ORDER];
		int order, nid, retval;
		unsigned long result = 0;
		struct timespec spec, spec_after, spec_before;

		/*
		 * Start with updated zone watermarks since these can be
		 * adjusted by user any time
		 */
		update_zone_watermarks();

		/*
		 * Keep track of time to calculate the compaction and
		 * reclaim rates
		 */
		total_free_pages = 0;
		/*
		 * Get time from CLOCK_MONOTONIC_RAW which is not subject
		 * to perturbations caused by sysadmin or ntp adjustments.
		 * This ensures reliable calculations for the least square
		 * fit algorithm.
		 */
		clock_gettime(CLOCK_MONOTONIC_RAW, &spec_before);

		while ((retval = get_next_zone(ifile, &nid, nr_free)) != -1) {
			unsigned long total_free;

			if (retval == 0)
				break;

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
						free[order].free_pages +
						free_pages;
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
					high_wmark[nid], nid);

			if (last_bigpages[nid] != 0) {
				clock_gettime(CLOCK_MONOTONIC_RAW, &spec_after);
				time_elapsed = get_msecs(&spec_after) -
						get_msecs(&spec_before);
				if (free[MAX_ORDER-1].free_pages >
						last_bigpages[nid]) {
					compaction_rate =
						(free[MAX_ORDER-1].free_pages -
						last_bigpages[nid]) /
						time_elapsed;
					if (compaction_rate)
						log_info(3, "** compaction rate on node %d is %ld pages/msec",
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
				log_info(3, "reclamation rate is %ld pages/msec", reclaim_rate);
		}
		last_reclaimed = reclaimed_pages;

		rewind(ifile);
		result = 0;
		sleep(periodicity);
	}

	closelog();
	return(0);
}
