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

#define	COMPACT_PATH_FORMAT	"/sys/devices/system/node/node%s/compact"
#define	BUDDYINFO		"/proc/buddyinfo"
#define ZONEINFO		"/proc/zoneinfo"
#define RESCALE_WMARK		"/proc/sys/vm/watermark_rescale_factor"
#define VMSTAT			"/proc/vmstat"

struct node_hash_entry {
	char *n_node_id;
	pthread_t n_thread;
	pthread_mutex_t n_lock;
	pthread_cond_t n_cv;
	int n_do_compact;
	struct hsearch_data n_zone_hash;
};

void *
compact(void *arg)
{
	char compactpath[PATH_MAX];
	struct node_hash_entry *nhe = arg;
	int fd;
	int err;
	char c = '1';

	if (snprintf(compactpath, sizeof (compactpath), COMPACT_PATH_FORMAT,
	    nhe->n_node_id) >= sizeof (compactpath)) {
		(void) fprintf(stderr, "compactpath is too long");
		exit(1);
	}

	if ((fd = open(compactpath, O_WRONLY)) == -1) {
		perror("opening compaction path");
		exit(1);
	}

	if ((err = pthread_mutex_lock(&nhe->n_lock)) != 0) {
		(void) fprintf(stderr, "R: pthread_mutex_lock: %s",
		    strerror(err));
		exit(1);
	}

	while (1) {
		while (nhe->n_do_compact == 0) {
			if ((err = pthread_cond_wait(&nhe->n_cv,
				&nhe->n_lock)) != 0) {
				(void) fprintf(stderr,
				    "R: pthread_cond_wait: %s", strerror(err));
				exit(1);
			}
		}

		/* perform the actual compaction */
		if (write(fd, &c, sizeof (c)) != sizeof (c)) {
			perror("writing to compaction path");
			exit(1);
		}

		nhe->n_do_compact = 0;
	}
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
		return (0);

	for (order = 0; order < MAX_ORDER; order++) {
		if ((t = strtok(NULL, " ")) == NULL ||
		    sscanf(t, " %ld", &nr_free[order]) != 1)
			return (0);
	}

	return (1);
}

/*
 * Parse the next line of input, copying it to an optional output file;  return
 * 1 if successful or 0 on EOF.
 */
int
get_line(FILE *ifile, FILE *ofile, char *node, char *zone,
    unsigned long *nr_free)
{
	char line[LINE_MAX];

	if (fgets(line, sizeof (line), ifile) == NULL) {
		if (feof(ifile)) {
			return (0);
		} else {
			(void) fprintf(stderr, "fgets(): %s",
			    strerror(ferror(ifile)));
			exit(1);
		}
	}

	if (ofile) {
		size_t size = strnlen(line, LINE_MAX);

		if (fwrite(line, size, 1, ofile) != 1) {
			(void) fprintf(stderr, "fwrite(): %s",
			    strerror(ferror(ofile)));
			exit(1);
		}
	}

	if (!scan_line(line, node, zone, nr_free)) {
		(void) fprintf(stderr, "invalid input: %s", line);
		exit(1);
	}

	return (1);
}

/*
 * Parse watermarks and zone_managed_pages values from /proc/zoneinfo
 */
int
update_zone_watermarks(struct zone_hash_entry *zhe)
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

			if (strcmp(zhe->z_zone, zone_name) == 0) {
				if (fgets(line, len, fp) == NULL)
					return 0;

				int i = 0;
				unsigned long min, low, high, managed;

				while (i < 7) {
					if (fgets(line, len, fp) == NULL)
						return 0;

					unsigned long wmark;
					char name[20];
					sscanf(line, "%s %lu\n", name, &wmark);
					if (i == 0)
						min = wmark;
					if (i == 1)
						low = wmark;
					if (i == 2)
						high = wmark;
					if (i == 6)
						managed = wmark;
					i++;
				}

				zhe->min = min;
				zhe->low = low;
				zhe->high = high;
				zhe->managed = managed;

				return 0;
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
		int ret = sscanf(line, "%s %lu\n", desc, &val );


		if (strcmp(desc, "pgsteal_kswapd") == 0)
			goto out;
	}

out:
	free(line);
	return val;
}

/*
 * Dynamically rescale the watermark_scale_factor to make kswapd more aggresive
 */
int
rescale_watermarks(struct zone_hash_entry *zhe,
			unsigned long rescale_watermark)
{
	int fd;
	long scaled_watermark;
	char scaled_wmark[20];;

	/*
	 * zone->_watermark[HIGH] = zone->_watermark[MIN] + (zone_managed_pages(zone) * watermark_scale_factor) / 10000
	 */
	scaled_watermark = (long)(((rescale_watermark - zhe->min) / 2) * (10000 / zhe->managed));
	sprintf(scaled_wmark, "%ld", scaled_watermark);

	if ((fd = open(RESCALE_WMARK, O_WRONLY)) == -1)
		return -1;

	if (write(fd, scaled_wmark, sizeof(scaled_wmark)) != sizeof(scaled_wmark)) {
		return -1;
	}
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
	pthread_mutexattr_t mutexattr;
	struct hsearch_data node_hash = { 0 };
	ENTRY e;
	ENTRY *ep;
	struct node_hash_entry *nhe;
	struct zone_hash_entry *zhe;
	struct zone_hash_entry *zhe_list = NULL;
	int c;
	int rate = 0;
	int threshold = 0;
	int Cflag = 0;
	int cflag = 0;
	int tflag = 0;
	int iflag = 0;
	char *iarg;
	int oflag = 0;
	char *obase;
	int errflag = 0;
	int err;
	int fd;
	char *ipath;
	char opath[PATH_MAX];
	FILE *ifile;
	FILE *ofile;

	while ((c = getopt(argc, argv, "Cc:i:o:t:z:")) != -1) {
		switch (c) {
		case 'C':
			if (Cflag++ || iflag)
				errflag++;
			break;
		case 'c':
			if (cflag++)
				errflag++;
			else
				rate = -1 * atoi(optarg);
			break;
		case 'i':
			if (iflag++ || Cflag)
				errflag++;
			iarg = optarg;
			break;
		case 'o':
			if (oflag++)
				errflag++;
			else
				obase = optarg;
			break;
		case 't':
			if (tflag++)
				errflag++;
			else
				threshold = atoi(optarg);
			break;
		case 'z':
			zhe = calloc(1, sizeof (struct zone_hash_entry));
			if (zhe == NULL) {
				perror("calloc");
				exit(1);
			}
			zhe->z_node = strtok(optarg, ",");
			zhe->z_zone = strtok(NULL, ",");
			if (zhe->z_node == NULL ||
			    zhe->z_zone == NULL) {
				errflag++;
				break;
			}
			zhe->z_next = zhe_list;
			zhe_list = zhe;
			update_zone_watermarks(zhe);
			printf("%lu %lu %lu %lu\n", zhe->min, zhe->low, zhe->high, zhe->managed);
			break;
		default:
			errflag++;
			break;
		}
	}

	if (errflag || !cflag || !tflag || !oflag || !zhe_list) {
		(void) fprintf(stderr,
		    "usage: %s "
		    "[-C | -i input file] "
		    "-o output_basename "
		    "-c compaction_rate "
		    "-t fragmentation_threshold "
		    "-z node_id,zonename [-z node_id,zonename [-z ...]]\n",
		    argv[0]);
		exit(1);
	}

	if (!hcreate_r(1, &node_hash)) {
		perror("hcreate_r");
		exit(1);
	}

	if (Cflag) {
		if ((err = pthread_mutexattr_init(&mutexattr)) != 0) {
			(void) fprintf(stderr, "pthread_mutexattr_init: %s",
			    strerror(err));
			exit(1);
		}
		if ((err = pthread_mutexattr_settype(&mutexattr,
		    PTHREAD_MUTEX_ERRORCHECK)) != 0) {
			(void) fprintf(stderr, "pthread_mutexattr_settype: %s",
			    strerror(err));
			exit(1);
		}
	}

	/*
	 * Each node,zone specified on the command line is inserted into a
	 * double hash;  the first per node and the second per zone.  Entries on
	 * the per node hash act as cookies for corresponding threads
	 * responsible for triggering compaction.  Entries on the per zone
	 * hashes store memory statistics for the corresponding zone together
	 * with a unique file stream for printing the observability data.  The
	 * double hash is also used to detect duplicate specifications and to
	 * identify which lines of input to process.
	 */
	for (zhe = zhe_list; zhe; zhe = zhe->z_next) {
		int i;

		e.key = zhe->z_node;
		if (!hsearch_r(e, FIND, &ep, &node_hash)) {
			nhe = calloc(1, sizeof (struct node_hash_entry));
			if (nhe == NULL) {
				perror("calloc");
				exit(1);
			}
			nhe->n_node_id = zhe->z_node;

			if (Cflag) {
				if ((err = pthread_mutex_init(&nhe->n_lock,
				    &mutexattr)) != 0) {
					(void) fprintf(stderr,
					    "pthread_mutex_init: %s",
					    strerror(err));
					exit(1);
				}
				if ((err = pthread_cond_init(&nhe->n_cv,
				    NULL)) != 0) {
					(void) fprintf(stderr,
					    "pthread_cond_init: %s",
					    strerror(err));
					exit(1);
				}
				if ((err = pthread_create(&nhe->n_thread, NULL,
				    compact, nhe)) != 0) {
					(void) fprintf(stderr,
					    "pthread_create: %s",
					    strerror(err));
					exit(1);
				}
			}

			if (!hcreate_r(1, &nhe->n_zone_hash)) {
				perror("hcreate_r");
				exit(1);
			}
			e.data = nhe;
			if (!hsearch_r(e, ENTER, &ep, &node_hash)) {
				perror("hsearch_r");
				exit(1);
			}
		}

		nhe = ep->data;
		e.key = zhe->z_zone;
		if (hsearch_r(e, FIND, &ep, &nhe->n_zone_hash)) {
			(void) fprintf(stderr, "duplicate node,zone: %s,%s",
			    zhe->z_node, zhe->z_zone);
			exit(1);
		}
		for (i = 0; i < MAX_OUTPUT; i++) {
			char *output_names[MAX_OUTPUT] = {
				"observations",
				"predictions"
			};

			if (snprintf(opath, sizeof (opath), "%s.%s,%s.%s",
			    obase, zhe->z_node, zhe->z_zone,
			    output_names[i]) >= sizeof (opath)) {
				(void) fprintf(stderr,
				    "output path is too long");
				exit(1);
			}
			if ((fd = open(opath, O_EXCL|O_CREAT|O_WRONLY,
			    S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) == -1) {
				perror("open(output path)");
				exit(1);
			}
			if ((zhe->z_output[i] = fdopen(fd, "w")) == NULL) {
				perror("fdopen");
				exit(1);
			}
		}
		e.data = zhe;
		if (!hsearch_r(e, ENTER, &ep, &nhe->n_zone_hash)) {
			perror("hsearch_r(ENTER)");
			exit(1);
		}
	}

	ipath = iflag ? iarg : BUDDYINFO;
	if ((ifile = fopen(ipath, "r")) == NULL) {
		perror("fopen(input file)");
		exit(1);
	}
	if (snprintf(opath, sizeof (opath), "%s.input",
	    obase) >= sizeof (opath)) {
		(void) fprintf(stderr, "output path is too long");
		exit(1);
	}
	if (iflag) {
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
		char nodestr[16];
		char zonetype[16];
		unsigned long nr_free[MAX_ORDER], nr_free_after[MAX_ORDER];
		unsigned long total_free;
		struct frag_info free[MAX_ORDER];
		int order;
		unsigned long result;
		struct timespec spec, spec_after, spec_before;
		unsigned long scale_wmark = 0;
		/*
		 * Keep track of time to calculate the compaction and reclaim rates
		 */
		clock_gettime(CLOCK_REALTIME, &spec_before);
		unsigned long reclaim_before = no_pages_reclaimed();

		if (!get_line(ifile, ofile, nodestr, zonetype, nr_free)) {
			if (iflag) {
				break;
			} else {
				rewind(ifile);
				sleep(1);
				continue;
			}
		}

		/* Skip this zone if it wasn't specified on the command line. */
		e.key = nodestr;
		if (!hsearch_r(e, FIND, &ep, &node_hash))
			continue;
		nhe = ep->data;
		e.key = zonetype;
		if (!hsearch_r(e, FIND, &ep, &nhe->n_zone_hash))
			continue;
		zhe = ep->data;

		/*
		 * Assemble the fragmented free memory vector:  the fragmented
		 * free memory at a given order is the sum of the occupancies at
		 * all lower orders.  Memory is never fragmented at the lowest
		 * order and so free[0] is overloaded with the total free
		 * memory.
		 */
		total_free = free[0].free_pages = 0;
		for (order = 0; order < MAX_ORDER; order++) {
			unsigned long free_pages;

			free_pages = nr_free[order] << order;
			total_free += free_pages;
			if (order < MAX_ORDER - 1) {
				free[order + 1].free_pages =
				    free[order].free_pages + free_pages;
				clock_gettime(CLOCK_REALTIME, &spec);
				free[order + 1].msecs = (long long)get_msecs(&spec);
			}
		}
		free[0].free_pages = total_free;

		/*
		 * Offer the predictor the fragmented free memory vector but
		 * do nothing else unless it issues a prediction.
		 */
		result = predict(free, zhe->z_lsq, threshold, rate,
			zhe, &scale_wmark);
		//plot(zhe, free, result);

		if (!get_line(ifile, ofile, nodestr, zonetype, nr_free_after)) {
			if (iflag) {
				break;
			} else {
				rewind(ifile);
				sleep(1);
				continue;
			}
		}

		clock_gettime(CLOCK_REALTIME, &spec_after);

		long time_elapsed = (get_msecs(&spec_after) - get_msecs(&spec_before));
		long compacted_pages = 0;

		/*
		 * Get the number of higher order pages retrived in time_elapsed time.
		 */
		for (int i = 1; i < MAX_ORDER; i++) {
			unsigned long curr_compacted_pages = nr_free_after[i] -
				nr_free[i];

			if (curr_compacted_pages > 0)
				compacted_pages += curr_compacted_pages;
		}

		unsigned long reclaim_after = no_pages_reclaimed();
		compaction_rate = compacted_pages / time_elapsed;

		reclaim_rate = (reclaim_after - reclaim_before) / time_elapsed;
		printf("compaction rate is %ld\n", compaction_rate);

		if (result == 0)
			continue;

		if (result & MEMPREDICT_RECLAIM && scale_wmark > 0)
			rescale_watermarks(zhe, scale_wmark);

		/* Wake the compactor if requested. */
		if (result & MEMPREDICT_COMPACT) {
			if (!Cflag)
				continue;

			if ((err = pthread_mutex_trylock(&nhe->n_lock)) != 0) {
				if (err == EBUSY)
					continue;
				(void) fprintf(stderr, "pthread_mutex_lock: %s",
					strerror(err));
				exit(1);
			}
			nhe->n_do_compact = 1;
			if ((err = pthread_cond_signal(&nhe->n_cv)) != 0) {
				(void) fprintf(stderr, "pthread_cond_signal: %s",
				strerror(err));
				exit(1);
			}
			if ((err = pthread_mutex_unlock(&nhe->n_lock)) != 0) {
				(void) fprintf(stderr, "pthread_mutex_unlock: %s",
				strerror(err));
				exit(1);
			}
		}

	}

	return (0);
}
