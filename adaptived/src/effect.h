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
 * adaptived effects header file
 *
 * "Causes" are events that trigger "Effects".  adaptived
 * will periodically evaluate the causes, and if they fire
 * then the associated effects will be enforced.
 *
 */

#ifndef __ADAPTIVED_EFFECT_H
#define __ADAPTIVED_EFFECT_H

#include <json-c/json.h>
#include <assert.h>

#include "defines.h"
#include "cause.h"

enum effect_op_enum {
	EOP_SET = 0,
	EOP_ADD,
	EOP_SUBTRACT,

	EOP_CNT
};

extern const char * const effect_op_names[];

enum effect_enum {
	EFFECT_PRINT = 0,
	EFFECT_VALIDATE,
	EFFECT_SNOOZE,
	EFFECT_CGROUP_SETTING,
	EFFECT_CGROUP_MEMORY_SETTING,
	EFFECT_KILL_CGROUP,
	EFFECT_KILL_CGROUP_BY_PSI,
	EFFECT_CGROUP_SETTING_BY_PSI,
	EFFECT_COPY_CGROUP_SETTING,
	EFFECT_LOGGER,
	EFFECT_PRINT_SCHEDSTAT,
	EFFECT_SETTING,
	EFFECT_SD_BUS_SETTING,
	EFFECT_KILL_PROCESSES,
	EFFECT_SIGNAL,
	EFFECT_ADAPTIVEMMD_EFFECTS,

	EFFECT_CNT
};

struct adaptived_effect {
	/* populated by adaptived */
	enum effect_enum idx;
	char *name;
	const struct adaptived_effect_functions *fns;
	struct json_object *json; /* only used when building a rule at runtime */
	struct adaptived_effect *next;

	/* private data store for each effect plugin */
	void *data;
};

extern const char * const effect_names[];
extern const struct adaptived_effect_functions effect_fns[];
extern struct adaptived_effect *registered_effects;

int print_init(struct adaptived_effect * const eff, struct json_object *args_obj,
	       const struct adaptived_cause * const cse);
int print_main(struct adaptived_effect * const eff);
void print_exit(struct adaptived_effect * const eff);

int validate_init(struct adaptived_effect * const eff, struct json_object *args_obj,
		  const struct adaptived_cause * const cse);
int validate_main(struct adaptived_effect * const eff);
void validate_exit(struct adaptived_effect * const eff);

int snooze_init(struct adaptived_effect * const eff, struct json_object *args_obj,
	       const struct adaptived_cause * const cse);
int snooze_main(struct adaptived_effect * const eff);
void snooze_exit(struct adaptived_effect * const eff);

int cgroup_setting_init(struct adaptived_effect * const eff, struct json_object *args_obj,
			const struct adaptived_cause * const cse);
int cgroup_setting_main(struct adaptived_effect * const eff);
void cgroup_setting_exit(struct adaptived_effect * const eff);

int cgroup_memory_setting_init(struct adaptived_effect * const eff, struct json_object *args_obj,
			const struct adaptived_cause * const cse);
int cgroup_memory_setting_main(struct adaptived_effect * const eff);

int kill_cgroup_init(struct adaptived_effect * const eff, struct json_object *args_obj,
		     const struct adaptived_cause * const cse);
int kill_cgroup_main(struct adaptived_effect * const eff);
void kill_cgroup_exit(struct adaptived_effect * const eff);

int kill_cgroup_psi_init(struct adaptived_effect * const eff, struct json_object *args_obj,
			 const struct adaptived_cause * const cse);
int kill_cgroup_psi_main(struct adaptived_effect * const eff);
void kill_cgroup_psi_exit(struct adaptived_effect * const eff);

int cgroup_setting_psi_init(struct adaptived_effect * const eff, struct json_object *args_obj,
			 const struct adaptived_cause * const cse);
int cgroup_setting_psi_main(struct adaptived_effect * const eff);
void cgroup_setting_psi_exit(struct adaptived_effect * const eff);

int copy_cgroup_setting_init(struct adaptived_effect * const eff, struct json_object *args_obj,
			const struct adaptived_cause * const cse);
int copy_cgroup_setting_main(struct adaptived_effect * const eff);
void copy_cgroup_setting_exit(struct adaptived_effect * const eff);

int logger_init(struct adaptived_effect * const eff, struct json_object *args_obj,
	       const struct adaptived_cause * const cse);
int logger_main(struct adaptived_effect * const eff);
void logger_exit(struct adaptived_effect * const eff);

int print_schedstat_init(struct adaptived_effect * const eff, struct json_object *args_obj,
	       const struct adaptived_cause * const cse);
int print_schedstat_main(struct adaptived_effect * const eff);
void print_schedstat_exit(struct adaptived_effect * const eff);

int sd_bus_setting_init(struct adaptived_effect * const eff, struct json_object *args_obj,
			const struct adaptived_cause * const cse);
int sd_bus_setting_main(struct adaptived_effect * const eff);
void sd_bus_setting_exit(struct adaptived_effect * const eff);

int kill_processes_init(struct adaptived_effect * const eff, struct json_object *args_obj,
		    const struct adaptived_cause * const cse);
int kill_processes_main(struct adaptived_effect * const eff);
void kill_processes_exit(struct adaptived_effect * const eff);

int signal_init(struct adaptived_effect * const eff, struct json_object *args_obj,
		const struct adaptived_cause * const cse);
int signal_main(struct adaptived_effect * const eff);
void signal_exit(struct adaptived_effect * const eff);

int adaptivemmd_effects_init(struct adaptived_effect * const eff, struct json_object *args_obj,
		const struct adaptived_cause * const cse);
int adaptivemmd_effects_main(struct adaptived_effect * const eff);
void adaptivemmd_effects_exit(struct adaptived_effect * const eff);

#endif /* __ADAPTIVED_EFFECT_H */
