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

/* Size of sliding window for computing trend line */
#define	LSQ_LOOKBACK		8

/* How often should data be sampled and trend analyzed*/
#define LOW_PERIODICITY		60
#define NORM_PERIODICITY	30
#define HIGH_PERIODICITY	15

#define	MAX_ORDER		11
#define MEMPREDICT_RECLAIM	0x01
#define MEMPREDICT_COMPACT	0x02
#define MEMPREDICT_LOWER_WMARKS	0x04

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

#define BATCHSIZE 8192

#define EOF_RET -1

// FIXME
// #define MAX_VERBOSE     5
#define MAX_VERBOSE     6
#define MAX_NEGDENTRY   	100
#define MAX_NEGDENTRY_DEFAULT   15	/* default is 1.5% */

#define MAX_NUMANODES   1024

/*
 * Number of consecutive samples showing growth in unaccounted memory
 * that will trigger memory leak warning
 */
#define UNACCT_MEM_GRTH_MAX     10

/* Minimum % change in meminfo numbers to trigger warnings */
#define MEM_TRIGGER_DELTA       10

#define FLDLEN  20

/*
 * System files to control reclamation and compaction
 */
#define COMPACT_PATH_FORMAT "/sys/devices/system/node/node%d/compact"
#define RESCALE_WMARK           "/proc/sys/vm/watermark_scale_factor"

/*
 * System files to control negative dentries
 */
#define NEG_DENTRY_LIMIT        "/proc/sys/fs/negative-dentry-limit"

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

enum trigger_type {
	NO_TRIGGER,
	MEMORY_PRESSURE_TRIGGER,
	SUDDEN_MEMORY_LEAK_TRIGGER,
	BACKGROUND_MEMORY_LEAK_TRIGGER,
	SLOW_MEMORY_LEAK_TRIGGER
};

struct curr_mem_info {
	unsigned long pr_memdata[NR_MEMDATA_ITEMS];
	unsigned long freemem;
	unsigned long prv_free;
	unsigned long mem_remain;
	unsigned long unacct_mem;
	long unmapped_pages;
};

struct adaptivemmd_opts {
	int unacct_mem_grth_max;
	int mem_trigger_delta;
	int aggressiveness;
	unsigned int maxwsf;		/* Highest value to set watermark_scale_factor to. */
	unsigned int mywsf;
	int max_compaction_order;	/* Highest order pages to look at for fragmentation. */
	int compaction_requested[MAX_NUMANODES];
	unsigned long last_bigpages[MAX_NUMANODES];
	unsigned long last_reclaimed;
	unsigned long total_free_pages;
	unsigned long total_cache_pages;
	unsigned long total_hugepages;
	unsigned long base_psize;
	long compaction_rate;
	long reclaim_rate;
	struct lsq_struct page_lsq[MAX_NUMANODES][MAX_ORDER];
	FILE *ifile;
	struct timespec spec_before;
	int periodicity;
	int dry_run;

	bool memleak_check_enabled;
	bool memory_pressure_check_enabled;	/* aka ENABLE_FREE_PAGE_MGMT */
	int maxgap;				/* Maximum gap between low and high watermarks (in GB) */
	bool neg_dentry_check_enabled;		/* aka ENABLE_NEG_DENTRY_MGMT */
	int neg_dentry_pct;			/* aka NEG_DENTRY_CAP negative dentry memory usage cap is expressed as
						 * a percentage of total memory on the system. */
	unsigned long base_mem;
	unsigned long mem_remain;
	unsigned long gr_count;
	unsigned long prv_free;
	unsigned long memdata[NR_MEMDATA_ITEMS];
	unsigned long pr_memdata[NR_MEMDATA_ITEMS];
	unsigned long min_wmark[MAX_NUMANODES];
	unsigned long low_wmark[MAX_NUMANODES];
	unsigned long high_wmark[MAX_NUMANODES];
	unsigned long managed_pages[MAX_NUMANODES];
	int skip_dmazone;
	int debug_mode;
	int verbose;
	
	long unmapped_pages;
	unsigned long unacct_mem;

	struct curr_mem_info curr_mem_info;

	unsigned long final_result;

	enum trigger_type mem_pressure_trigger;
	enum trigger_type mem_leak_trigger;
};

struct adaptivemmd_effects_opts {
	const struct adaptived_cause *cse;
};

int run_adaptivemm_init(struct adaptivemmd_opts * const opts, int interval);

int run_adaptivemm(struct adaptivemmd_opts * const opts);

int run_adaptivemm_effects(struct adaptivemmd_opts * const opts);


#define log_err(...)    log_msg(LOG_ERR, __VA_ARGS__)
#define log_warn(...)   log_msg(LOG_WARNING, __VA_ARGS__)
#define log_dbg(...)    if (debug_mode) \
                                log_msg(LOG_DEBUG, __VA_ARGS__)
#define log_info(verb_level, ...)       if (verbose >= verb_level) \
                                                log_msg(LOG_INFO, __VA_ARGS__)

/* Use pr_info to log info irrespective of verbosity level */
#define pr_info(...)    log_msg(LOG_INFO, __VA_ARGS__)

extern void log_msg(int level, char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* PREDICT_H */
