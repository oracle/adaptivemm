#define	_GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
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

enum output_type {
	OUTPUT_OBSERVATIONS,
	OUTPUT_PREDICTIONS,
	MAX_OUTPUT
};

struct zone_hash_entry {
	char *z_zone;
	char *z_node;
	struct lsq_struct z_lsq[MAX_ORDER];
	int z_n;
	struct zone_hash_entry *z_next;
	FILE *z_output[MAX_OUTPUT];
};

struct node_hash_entry {
	char *n_node_id;
	pthread_t n_thread;
	pthread_mutex_t n_lock;
	pthread_cond_t n_cv;
	int n_do_compact;
	struct hsearch_data n_zone_hash;
};

/*
 * plot() appends entries to gnuplot input files that describe the memory usage
 * and the nature of any predictions.  For example, if the program is invoked
 * with
 *
 *	-o output -z 1,Normal
 *
 * then it will create output.1,Normal.observations and
 * output.1,Normal.predictions;  these can be read by starting gnuplot (v5) and
 * issuing
 *
 *	basename='output.1,Normal'
 *	load 'predictord.gnuplot'
 *
 * where the file predictord.gnuplot contains
 *
 *	o=basename.'.observations'
 *	p=basename.'.predictions'
 *
 *	set xl 'Time/s'
 *	set yl 'Free memory/pages'
 *	set tit basename
 *
 *	plot \
 *	o u 1:2 w l lc 1 lw 1 t 'Total', \
 *	for[c=12:3:-1] o u 1:c w l lc (c-2) lw 1 t 'Order '.(c-2), \
 *	\
 *	p u 2:3:4:5 w vec nohead lc 1 dt 1 lw 2 noti, \
 *	p u 2:6:7:8 w vec nohead lc 1 dt 2 lw 2 noti, \
 *	p u 2:9:10:11:1 w vec nohead lc var dt 1 lw 2 noti, \
 *	p u 12:13:14:15:1 w vec nohead lc var dt 2 lw 2 noti
 */
void
plot(struct zone_hash_entry *zhe, unsigned long *free,
    struct prediction_struct *p)
{
	int n = zhe->z_n++;
	float x, y, dx, dy;
	int order;
	FILE *observations = zhe->z_output[OUTPUT_OBSERVATIONS];
	FILE *predictions = zhe->z_output[OUTPUT_PREDICTIONS];

	/* the time */
	(void) fprintf(observations, "%d ", n);

	/* observed total free memory */
	(void) fprintf(observations, "% lld", (long long)free[0]);

	/* fragmented free memory for orders 1 to MAX_ORDER - 1 */
	for (order = 1; order < MAX_ORDER; order++)
		(void) fprintf(observations, " %lld", (long long)free[order]);
	(void) fprintf(observations, "\n");

	fflush(observations);

	if (p == NULL)
		return;

	/* the order, used to choose a colour */
	(void) fprintf(predictions, "%d ", p->order);

	/* the time */
	(void) fprintf(predictions, "%d ", n);

	/* total free memory historical trend given by a vector */
	(void) fprintf(predictions, "%lld %lld %lld ",
	    (long long)p->f_T_zero, (long long)-COUNT,
	    (long long)(-COUNT * p->R_T));

	/* total free memory projection */
	(void) fprintf(predictions, "%lld %lld %lld ",
	    (long long)p->f_T_zero, (long long)p->t_e,
	    (long long)(p->f_e - p->f_T_zero));

	/* fragmented free memory trend */
	(void) fprintf(predictions, "%lld %lld %lld ",
	    (long long)p->f_f_zero, (long long)-COUNT,
	    (long long)(-COUNT * p->R_f));

	/* fragmented free memory prediction */
	if (p->type == TYPE_ONE) {
		/* intersection of F_T and f_f */
		x = n;
		y = p->f_f_zero;
		dx = p->t_e;
		dy = p->f_e - y;
	} else {
		/* intersection of F_T and f_f(t >= 1) with compaction */
		x = n + 1;
		y = p->f_f_zero + p->R_f;
		dx = p->t_e - 1;
		dy = p->f_e - y;
	}
	(void) fprintf(predictions, "%lld %lld %lld %lld\n",
	    (long long)x, (long long)y, (long long)dx, (long long)dy);

	fflush(predictions);
}

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
		unsigned long nr_free[MAX_ORDER];
		unsigned long total_free;
		unsigned long free[MAX_ORDER];
		int order;
		struct prediction_struct prediction;
		struct prediction_struct *result;

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
		total_free = free[0] = 0;
		for (order = 0; order < MAX_ORDER; order++) {
			unsigned long free_pages;

			free_pages = nr_free[order] << order;
			total_free += free_pages;
			if (order < MAX_ORDER - 1) {
				free[order + 1] =
				    free[order] + free_pages;
			}
		}
		free[0] = total_free;

		/*
		 * Offer the predictor the fragmented free memory vector but
		 * do nothing else unless it issues a prediction.
		 */
		result = predict(free, zhe->z_lsq, threshold, rate,
		    &prediction);
		plot(zhe, free, result);
		if (result == NULL)
			continue;

		/* Wake the compactor if requested. */
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

	return (0);
}
