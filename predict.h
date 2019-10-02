#ifndef PREDICT_H
#define	PREDICT_H

#include <stdio.h>
#include <stdlib.h>

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

unsigned long predict(struct frag_info *, struct lsq_struct *, unsigned long);

#ifdef __cplusplus
}
#endif

#endif /* PREDICT_H */
