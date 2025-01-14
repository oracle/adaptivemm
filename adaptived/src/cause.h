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
 * adaptived causes header file
 *
 * "Causes" are events that trigger "Effects".  adaptived
 * will periodically evaluate the causes, and if they fire
 * then the associated effects will be enforced.
 *
 */

#ifndef __ADAPTIVED_CAUSE_H
#define __ADAPTIVED_CAUSE_H

#include <json-c/json.h>
#include <stdio.h>

#include <adaptived.h>

#include "defines.h"

enum cause_op_enum {
	COP_GREATER_THAN = 0,
	COP_LESS_THAN,
	COP_EQUAL,

	COP_CNT
};

extern const char * const cause_op_names[];

enum cause_enum {
	TIME_OF_DAY = 0,
	DAYS_OF_THE_WEEK,
	PRESSURE,
	PRESSURE_RATE,
	ALWAYS,
	CGROUP_SETTING,
	SETTING,
	PERIODIC,
	MEMINFO,
	SLABINFO,
	MEMORYSTAT,
	TOP,
	CGROUP_MEMORY_SETTING,
	ADAPTIVEMMD_CAUSES,

	CAUSE_CNT
};

struct adaptived_cause {
	/* populated by adaptived */
	enum cause_enum idx;
	char *name;
	const struct adaptived_cause_functions *fns;
	struct json_object *json; /* only used when building a rule at runtime */
	struct adaptived_cause *next;

	/* private data store for each cause plugin */
	void *data;
};

extern const char * const cause_names[];
extern const struct adaptived_cause_functions cause_fns[];
extern struct adaptived_cause *registered_causes;


int time_of_day_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval);
int time_of_day_main(struct adaptived_cause * const cse, int time_since_last_run);
void time_of_day_exit(struct adaptived_cause * const cse);
void time_of_day_print(const struct adaptived_cause * const cse, FILE *file);

int days_of_the_week_init(struct adaptived_cause * const cse, struct json_object *args_obj,
			  int interval);
int days_of_the_week_main(struct adaptived_cause * const cse, int time_since_last_run);
void days_of_the_week_exit(struct adaptived_cause * const cse);
void days_of_the_week_print(const struct adaptived_cause * const cse, FILE *file);

int pressure_init(struct adaptived_cause * const cse, struct json_object *args_obj,
		  int interval);
int pressure_main(struct adaptived_cause * const cse, int time_since_last_run);
void pressure_exit(struct adaptived_cause * const cse);
void pressure_print(const struct adaptived_cause * const cse, FILE *file);

int pressure_rate_init(struct adaptived_cause * const cse, struct json_object *args_obj,
		       int interval);
int pressure_rate_main(struct adaptived_cause * const cse, int time_since_last_run);
void pressure_rate_exit(struct adaptived_cause * const cse);
void pressure_rate_print(const struct adaptived_cause * const cse, FILE *file);

int always_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval);
int always_main(struct adaptived_cause * const cse, int time_since_last_run);
void always_exit(struct adaptived_cause * const cse);

int cgset_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval);
int cgset_main(struct adaptived_cause * const cse, int time_since_last_run);
void cgset_exit(struct adaptived_cause * const cse);

int cgset_memory_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval);
int cgset_memory_main(struct adaptived_cause * const cse, int time_since_last_run);

int periodic_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval);
int periodic_main(struct adaptived_cause * const cse, int time_since_last_run);
void periodic_exit(struct adaptived_cause * const cse);

int meminfo_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval);
int meminfo_main(struct adaptived_cause * const cse, int time_since_last_run);
void meminfo_exit(struct adaptived_cause * const cse);

int slabinfo_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval);
int slabinfo_main(struct adaptived_cause * const cse, int time_since_last_run);
void slabinfo_exit(struct adaptived_cause * const cse);

int memorystat_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval);
int memorystat_main(struct adaptived_cause * const cse, int time_since_last_run);
void memorystat_exit(struct adaptived_cause * const cse);

int top_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval);
int top_main(struct adaptived_cause * const cse, int time_since_last_run);
void top_exit(struct adaptived_cause * const cse);

int adaptivemmd_causes_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval);
int adaptivemmd_causes_main(struct adaptived_cause * const cse, int time_since_last_run);
void adaptivemmd_causes_exit(struct adaptived_cause * const cse);

#endif /* __ADAPTIVED_CAUSE_H */
