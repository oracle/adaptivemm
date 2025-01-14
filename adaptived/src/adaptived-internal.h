/*
 * Copyright (c) 2024, Oracle and/or its affiliates.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */
/**
 * Internal include header file for adaptived
 *
 */

#ifndef __ADAPTIVED_INTERNAL_H
#define __ADAPTIVED_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <pthread.h>
#include <syslog.h>
#include <stdio.h>

#include <adaptived.h>

#include "cause.h"
#include "effect.h"

#define API __attribute__((visibility("default")))

#ifndef __cplusplus
#define max(x,y) ( \
    { __auto_type __x = (x); __auto_type __y = (y); \
      __x > __y ? __x : __y; })
#define min(x,y) ( \
    { __auto_type __x = (x); __auto_type __y = (y); \
      __x < __y ? __x : __y; })
#endif

typedef int (*adaptived_injection_function)(struct adaptived_ctx * const ctx);

enum log_location {
	LOG_LOC_SYSLOG = 0,
	LOG_LOC_STDOUT,
	LOG_LOC_STDERR,
	LOG_LOC_JOURNAL,

	LOG_LOC_CNT
};

struct adaptived_rule {
	char *name;
	struct adaptived_cause *causes;
	struct adaptived_effect *effects;
	struct json_object *json; /* only used when building a rule at runtime */
	struct adaptived_rule_stats stats;

	struct adaptived_rule *next;
};

struct adaptived_ctx {
	/* options passed in on the command line */
	char config[FILENAME_MAX];
	int interval; /* in seconds */
	int max_loops;

	/* internal settings and structures */
	struct adaptived_rule *rules;
	adaptived_injection_function inject_fn;
	bool skip_sleep;
	pthread_mutex_t ctx_mutex;
	unsigned long loop_cnt;
	int daemon_nochdir;
	int daemon_noclose;
	bool daemon_mode;
};

/*
 * cause.c functions
 */

struct adaptived_cause *cause_init(const char * const name);
void cause_destroy(struct adaptived_cause ** cse);
void causes_init(void);
void causes_cleanup(void);

/*
 * effect.c functions
 */

struct adaptived_effect *effect_init(const char * const name);
void effect_destroy(struct adaptived_effect ** eff);
void effects_init(void);
void effects_cleanup(void);

/*
 * file_utils.c functions
 */

int get_ll_field_in_file(const char * const file, const char * const field,
			 const char * const separator, long long * const ll_valuep);

/*
 * log.c functions
 */

extern int log_level;
extern enum log_location log_loc;

/*
 * parse.c functions
 */

int parse_config(struct adaptived_ctx * const ctx);
int parse_rule(struct adaptived_ctx * const ctx, struct json_object * const rule_obj);
int insert_into_json_args_obj(struct json_object * const parent, const char * const key,
			      struct json_object * const arg);
long long adaptived_parse_human_readable(const char * const input);
int parse_cause_operation(struct json_object * const args_obj, const char * const name,
			  enum cause_op_enum * const op);

/*
 * proc_pid_stat_utils.c functions
 */

int _sort_pid_list(const void *p1, const void *p2);

/*
 * rule.c functions
 */

struct adaptived_rule *rule_init(const char * const name);
void rule_destroy(struct adaptived_rule ** rule);

/*
 * mem_utils defines
 */

#define PROC_BUDDYINFO		"/proc/buddyinfo"
#define PROC_ZONEINFO		"/proc/zoneinfo"
#define PROC_VMSTAT		"/proc/vmstat"
#define PROC_MEMINFO		"/proc/meminfo"
#define PROC_KPAGECOUNT		"/proc/kpagecount"
#define PROC_KPAGEFLAGS		"/proc/kpageflags"
#define PROC_MODULES		"/proc/modules"
#define PROC_SLABINFO		"/proc/slabinfo"
#define PROC_STAT   		"/proc/stat"
#define MM_HUGEPAGESINFO	"/sys/kernel/mm/hugepages"

/*
 * mem_utils slabinfo defines
 */
#define ACTIVE_OBJS	"<active_objs>"
#define NUM_OBJS	"<num_objs>"
#define OBJSIZE		"<objsize>"
#define OBJPERSLAB	"<objperslab>"
#define PAGESPERSLAB	"<pagesperslab>"
#define LIMIT		"<limit>"
#define BATCHCOUNT	"<batchcount>"
#define SHAREDFACTOR	"<sharedfactor>"
#define ACTIVE_SLABS	"<active_slabs>"
#define NUM_SLABS	"<num_slabs>"
#define SHAREDAVAIL	"<sharedavail>"

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __ADAPTIVED_INTERNAL_H */
