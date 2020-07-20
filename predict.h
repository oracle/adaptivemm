#ifndef PREDICT_H
#define	PREDICT_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	MAX_ORDER		11
#define	LSQ_LOOKBACK		8
#define MEMPREDICT_RECLAIM	0x01
#define MEMPREDICT_COMPACT	0x02
#define MEMPREDICT_LOWER_WMARKS	0x04

long compaction_rate;
long reclaim_rate;
int debug_mode, verbose;

struct lsq_struct {
	int next;
	int ready;
	long long y[LSQ_LOOKBACK];
	long long x[LSQ_LOOKBACK];
};

enum output_type {
	OUTPUT_OBSERVATIONS,
	OUTPUT_PREDICTIONS,
	MAX_OUTPUT
};

struct frag_info {
	long long free_pages;
	long long msecs;
};

unsigned long predict(struct frag_info *, struct lsq_struct *,
			unsigned long, int);

#define log_err(...)	log_msg(LOG_ERR, __VA_ARGS__)
#define log_warn(...)	log_msg(LOG_WARNING, __VA_ARGS__)
#define log_info(...)	log_msg(LOG_INFO, __VA_ARGS__)
#define log_dbg(...)	log_msg(LOG_DEBUG, __VA_ARGS__)

extern void log_msg(int level, char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* PREDICT_H */
