#ifndef PREDICT_H
#define	PREDICT_H

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	MAX_ORDER	11
#define	COUNT		8
#define MEMPREDICT_RECLAIM 0x01
#define MEMPREDICT_COMPACT 0x02

long compaction_rate;
int reclaim_rate;

struct lsq_struct {
	int next;
	int ready;
	long long y[COUNT];
	long long x[COUNT];
	long long sum_xy;
	long long sum_y;
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

struct zone_hash_entry {
	char *z_zone;
	char *z_node;
	struct lsq_struct z_lsq[MAX_ORDER];
	int z_n;
	struct zone_hash_entry *z_next;
	FILE *z_output[MAX_OUTPUT];
	unsigned long min;
	unsigned long low;
	unsigned long high;
	unsigned long managed;
};

unsigned long predict(struct frag_info *, struct lsq_struct *, int,
    int, struct zone_hash_entry *);

#ifdef __cplusplus
}
#endif

#endif /* PREDICT_H */
