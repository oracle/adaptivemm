#define	_GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#include <pthread.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "predict.h"

/* How often should data be sampled and trend analyzed*/
#define PERIODICITY	3

#define	COMPACT_PATH_FORMAT	"/sys/devices/system/node/node%d/compact"
#define	BUDDYINFO		"/proc/buddyinfo"
#define ZONEINFO		"/proc/zoneinfo"
#define RESCALE_WMARK		"/proc/sys/vm/watermark_rescale_factor"
#define VMSTAT			"/proc/vmstat"

#define MAX_NUMANODES	1024

unsigned long min_wmark[MAX_NUMANODES], low_wmark[MAX_NUMANODES];
unsigned long high_wmark[MAX_NUMANODES], managed_pages[MAX_NUMANODES];
struct lsq_struct page_lsq[MAX_NUMANODES][MAX_ORDER];

void
compact(int node_id)
{
	char compactpath[PATH_MAX];
	int fd;
	char c = '1';

	if (snprintf(compactpath, sizeof (compactpath), COMPACT_PATH_FORMAT,
	    node_id) >= sizeof (compactpath)) {
		(void) fprintf(stderr, "compactpath is too long");
		exit(1);
	}

	if ((fd = open(compactpath, O_WRONLY)) == -1) {
		perror("opening compaction path");
		exit(1);
	}

	if (write(fd, &c, sizeof (c)) != sizeof (c)) {
		perror("writing to compaction path");
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

	(void) strncpy(copy, line, sizeof (copy));

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
 * Parse the next line of input, copying it to an optional output file; 
 * return 1 if successful, 0 on failure or -1 on EOF.
 */
int
get_next_zone(FILE *ifile, FILE *ofile, int *nid, unsigned long *nr_free)
{
	char line[LINE_MAX];
	char node[10], zone[10];

	/*
	 * Read the file one line at a time until we find the next
	 * "Normal" zone or reach EOF
	 */
	while (1) {
		if (fgets(line, sizeof (line), ifile) == NULL) {
			if (feof(ifile)) {
				return (-1);
			} else {
				(void) fprintf(stderr, "fgets(): %s",
						strerror(ferror(ifile)));
				return 0;
			}
		}

		if (ofile) {
			size_t size = strnlen(line, LINE_MAX);

			if (fwrite(line, size, 1, ofile) != 1) {
				(void) fprintf(stderr, "fwrite(): %s",
						strerror(ferror(ofile)));
				return 0;
			}
		}

		if (!scan_line(line, node, zone, nr_free)) {
			(void) fprintf(stderr, "invalid input: %s", line);
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
					return 0;

				while (i < 7) {
					unsigned long wmark;
					char name[20];

					if (fgets(line, len, fp) == NULL)
						return 0;

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
	unsigned long val;
	char desc[100];

	fp = fopen(VMSTAT, "r");
	if (!fp)
		return 0;

	while ((fgets(line, len, fp) != NULL)) {
		sscanf(line, "%s %lu\n", desc, &val );
		if (strcmp(desc, "pgsteal_kswapd") == 0)
			goto out;
	}

out:
	free(line);
	fclose(fp);
	return val;
}

/*
 * Dynamically rescale the watermark_scale_factor to make kswapd more aggresive
 */
int
rescale_watermarks(int nid, unsigned long rescale_watermark)
{
	int fd;
	long scaled_watermark;
	char scaled_wmark[20];;

{
	time_t curtime;

	time(&curtime);
	printf("DEBUG: %s\tAdjusting watermarks\n", ctime(&curtime));
}
	scaled_watermark = (long)(((rescale_watermark - min_wmark[nid]) / 2) * (10000 / managed_pages[nid]));
	sprintf(scaled_wmark, "%ld", scaled_watermark);

	if ((fd = open(RESCALE_WMARK, O_WRONLY)) == -1)
		return -1;

	if (write(fd, scaled_wmark, sizeof(scaled_wmark)) !=
		sizeof(scaled_wmark))
		return -1;
	return 0;
}

unsigned long
get_msecs(struct timespec *spec)
{
	if (!spec)
		return -1;

	return (unsigned long)((spec->tv_sec * 1000) + (spec->tv_nsec / 1000));
}

int
main(int argc, char **argv)
{
	int c, i;
	int oflag = 0;
	char *obase;
	int errflag = 0;
	int fd;
	char *ipath;
	char opath[PATH_MAX];
	FILE *ifile;
	FILE *ofile;
	unsigned long last_bigpages[MAX_NUMANODES];
	long time_elapsed;

	while ((c = getopt(argc, argv, "o:")) != -1) {
		switch (c) {
		case 'o':
			if (oflag++)
				errflag++;
			else
				obase = optarg;
			break;
		default:
			errflag++;
			break;
		}
	}

	if (errflag) {
		(void) fprintf(stderr,
		    "usage: %s "
		    "[-o output_basename]\n",
		    argv[0]);
		exit(1);
	}

	update_zone_watermarks();

	/*
	 * Initialize number of higher order pages seen in last scan
	 * per node
	 */
	for (i=0; i<MAX_NUMANODES; i++)
		last_bigpages[i] = 0;

	ipath = BUDDYINFO;
	if ((ifile = fopen(ipath, "r")) == NULL) {
		perror("fopen(input file)");
		exit(1);
	}
	if (oflag && snprintf(opath, sizeof (opath), "%s.input",
				obase) >= sizeof (opath)) {
		(void) fprintf(stderr, "output path is too long");
		exit(1);
	}
	if (!oflag) {
		ofile = NULL;
	} else {
		if ((fd = open(opath, O_EXCL|O_CREAT|O_WRONLY,
		    S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) == -1) {
			perror("open(output path)");
			exit(1);
		}
		if ((ofile = fdopen(fd, "w")) == NULL) {
			perror("fdopen");
			exit(1);
		}
	}

	while (1) {
		unsigned long nr_free[MAX_ORDER];
		unsigned long total_free;
		struct frag_info free[MAX_ORDER];
		int order, nid, retval;
		unsigned long result = 0;
		struct timespec spec, spec_after, spec_before;
		unsigned long scale_wmark = 0;

		/*
		 * Keep track of time to calculate the compaction and
		 * reclaim rates
		 */
		clock_gettime(CLOCK_REALTIME, &spec_before);
		unsigned long reclaim_before = no_pages_reclaimed();

		while ((retval = get_next_zone(ifile, ofile, &nid, nr_free)) != -1) {
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
			for (order = 0; order < MAX_ORDER; order++) {
				unsigned long free_pages;

				free_pages = nr_free[order] << order;
				total_free += free_pages;
				if (order < MAX_ORDER - 1) {
					free[order + 1].free_pages =
						free[order].free_pages +
						free_pages;
					clock_gettime(CLOCK_REALTIME, &spec);
					free[order + 1].msecs = (long long)get_msecs(&spec);
				}
			}
			free[0].free_pages = total_free;

			/*
			 * Offer the predictor the fragmented free memory
			 * vector but do nothing else unless it issues a
			 * prediction.
			 */
			result |= predict(free, page_lsq[nid],
					high_wmark[nid], &scale_wmark);
			//plot(zhe, free, result);

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
					printf("compaction rate is %ld\n",
						compaction_rate);
				}
			}
			last_bigpages[nid] = free[MAX_ORDER-1].free_pages;

			/* Wake the compactor if requested. */
			if (result & MEMPREDICT_COMPACT) {
				time_t curtime;

				time(&curtime);
				printf("DEBUG: %s\tTriggering compaction on node %d\n", ctime(&curtime), nid);
				compact(nid);
			}
		}

		clock_gettime(CLOCK_REALTIME, &spec_after);

		long time_elapsed = get_msecs(&spec_after) - get_msecs(&spec_before);
		unsigned long reclaim_after = no_pages_reclaimed();

		reclaim_rate = (reclaim_after - reclaim_before) / time_elapsed;
		printf("reclamation rate is %ld\n", reclaim_rate);

		if (result & MEMPREDICT_RECLAIM && scale_wmark > 0)
			rescale_watermarks(nid, scale_wmark);

		rewind(ifile);
		sleep(PERIODICITY);
	}

	return (0);
}
