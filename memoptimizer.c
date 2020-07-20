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
#include "predict.h"

#define VERSION		"0.8"

/* How often should data be sampled and trend analyzed*/
#define LOW_PERIODICITY		30
#define NORM_PERIODICITY	15
#define HIGH_PERIODICITY	3

#define	COMPACT_PATH_FORMAT	"/sys/devices/system/node/node%d/compact"
#define	BUDDYINFO		"/proc/buddyinfo"
#define ZONEINFO		"/proc/zoneinfo"
#define RESCALE_WMARK		"/proc/sys/vm/watermark_scale_factor"
#define VMSTAT			"/proc/vmstat"

#define CONFIG_FILE		"/etc/memoptimizer.cfg"

#define MAX_NUMANODES	1024

unsigned long min_wmark[MAX_NUMANODES], low_wmark[MAX_NUMANODES];
unsigned long high_wmark[MAX_NUMANODES], managed_pages[MAX_NUMANODES];
unsigned long total_free_pages, total_cache_pages;
struct lsq_struct page_lsq[MAX_NUMANODES][MAX_ORDER];
int dry_run;
unsigned long maxwsf = 1000;

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
		exit(1);
	}

	if ((fd = open(compactpath, O_WRONLY)) == -1) {
		log_err("opening compaction path (%s)", strerror(errno));
		exit(1);
	}

	if (write(fd, &c, sizeof(c)) != sizeof(c)) {
		log_err("writing to compaction path (%s)", strerror(errno));
		exit(1);
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

	if (verbose)
		log_info("Adjusting watermarks. Current watermark scale factor = %s", scaled_wmark);
	if (dry_run)
		goto out;

	if (verbose)
		log_info("New watermark scale factor = %ld", scaled_watermark);
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

void
parse_config()
{
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
		    "\nNOTE: config options read from %s can\n      be overridded with command line options\n",
		    progname, VERSION, CONFIG_FILE);
}

int
main(int argc, char **argv)
{
	int c, i, aggressiveness = 2;
	int errflag = 0;
	int compaction_requested[MAX_NUMANODES];
	char *ipath;
	FILE *ifile;
	unsigned long last_bigpages[MAX_NUMANODES], last_reclaimed = 0;
	unsigned long time_elapsed, reclaimed_pages, maxgap;

	maxgap = 0;
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
			exit(0);
		default:
			errflag++;
			break;
		}
	}

	openlog("memoptimizer", LOG_PID, LOG_DAEMON);
	/* Become a daemon unless -d was specified */
	if (!debug_mode)
		if (daemon(0, 0) != 0) {
			log_err("Failed to become daemon. %s", strerror(errno));
			exit(1);
		}

	if (errflag) {
		help_msg(argv[0]);
		exit(1);
	}

	if (!check_permissions())
		exit(1);

	if (maxgap != 0) {
		unsigned long total_managed = 0;
		for (i=0; i<MAX_NUMANODES; i++)
			total_managed += managed_pages[i];
		maxwsf = (maxgap * 10000 * 1024 * 1024 * 1024)/(total_managed * getpagesize());
	}
	else {
		switch (aggressiveness) {
			case 1:
				maxwsf = 1000;
				break;
			case 2:
				maxwsf = 700;
				break;
			case 3:
				maxwsf = 400;
				break;
		}
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
		exit(1);
	}

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
		clock_gettime(CLOCK_REALTIME, &spec_before);

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
			clock_gettime(CLOCK_REALTIME, &spec);
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
			 * prediction.
			 */
			result |= predict(free, page_lsq[nid],
					high_wmark[nid], nid);

			if (last_bigpages[nid] != 0) {
				clock_gettime(CLOCK_REALTIME, &spec_after);
				time_elapsed = get_msecs(&spec_after) -
						get_msecs(&spec_before);
				if (free[MAX_ORDER-1].free_pages >
						last_bigpages[nid]) {
					compaction_rate =
						(free[MAX_ORDER-1].free_pages -
						last_bigpages[nid]) /
						time_elapsed;
					if (compaction_rate && (verbose > 2))
						log_info("** compaction rate on node %d is %ld pages/msec",
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
					if (verbose > 1)
						log_info("Triggering compaction on node %d", nid);
					if (!dry_run) {
						compact(nid);
						compaction_requested[nid] = 1;
					}
				}
			}
			if (result & (MEMPREDICT_RECLAIM | MEMPREDICT_LOWER_WMARKS)) {
				if (result & MEMPREDICT_RECLAIM)
					rescale_watermarks(1);
				else
					rescale_watermarks(0);
			}

			/*
			 * Clear compaction requested flag for the next
			 * sampling interval
			 */
			compaction_requested[nid] = 0;
			total_free_pages += free[0].free_pages;
		}

		reclaimed_pages = no_pages_reclaimed();
		if (last_reclaimed) {
			clock_gettime(CLOCK_REALTIME, &spec_after);
			time_elapsed = get_msecs(&spec_after) - get_msecs(&spec_before);

			reclaim_rate = (reclaimed_pages - last_reclaimed) / time_elapsed;
			if (reclaim_rate && (verbose > 2))
				log_info("reclamation rate is %ld pages/msec", reclaim_rate);
		}
		last_reclaimed = reclaimed_pages;

		rewind(ifile);
		result = 0;
		switch (aggressiveness) {
			case 1:
				sleep(HIGH_PERIODICITY);
				break;
			case 2:
				sleep(NORM_PERIODICITY);
				break;
			case 3:
				sleep(LOW_PERIODICITY);
				break;
		}
	}

	closelog();
	return(0);
}
