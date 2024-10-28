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
 * Cause to detect when a cgroup setting exceeds a specified value
 *
 */

#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include <adaptived-utils.h>
#include <adaptived.h>

#include "adaptived-internal.h"
#include "defines.h"

struct cgset_opts {
	enum cause_op_enum op;
	char *setting;
	struct adaptived_cgroup_value threshold;
	enum cg_setting_enum cg_setting_type;
};

static void free_opts(struct cgset_opts * const opts)
{
	if (!opts)
		return;

	if (opts->setting)
		free(opts->setting);

	free(opts);
}

int _cgset_init(struct adaptived_cause * const cse, struct json_object *args_obj,
		int interval, enum cg_setting_enum cg_setting)
{
	const char *setting_str;
	struct cgset_opts *opts;
	int ret = 0;

	opts = malloc(sizeof(struct cgset_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}

	memset(opts, 0, sizeof( struct cgset_opts));

	ret = adaptived_parse_string(args_obj, "setting", &setting_str);
	if (ret) {
		adaptived_err("Failed to parse the setting\n");
		goto error;
	}

	opts->setting = malloc(sizeof(char) * strlen(setting_str) + 1);
	if (!opts->setting) {
		ret = -ENOMEM;
		goto error;
	}

	strcpy(opts->setting, setting_str);
	opts->setting[strlen(setting_str)] = '\0';

	ret = parse_cause_operation(args_obj, NULL, &opts->op);
	if (ret)
		goto error;

	ret = adaptived_parse_cgroup_value(args_obj, "threshold", &opts->threshold);
	if (ret)
		goto error;

	ret = adaptived_cause_set_data(cse, (void *)opts);
	if (ret)
		goto error;

	opts->cg_setting_type = cg_setting;
	adaptived_dbg("_cgset_init: cg_setting_type = %d\n", opts->cg_setting_type);

	return ret;

error:
	free_opts(opts);
	return ret;
}

int cgset_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval)
{
	return _cgset_init(cse, args_obj, interval, CG_SETTING);
}

int cgset_memory_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval)
{
	return _cgset_init(cse, args_obj, interval, CG_MEMORY_SETTING);
}

int _cgset_main(struct adaptived_cause * const cse, int time_since_last_run)
{
	struct cgset_opts *opts = (struct cgset_opts *)adaptived_cause_get_data(cse);
	struct adaptived_cgroup_value val;
	int ret;

	val.type = opts->threshold.type;

	ret = adaptived_cgroup_get_value(opts->setting, &val);
	if (ret)
		return ret;

	switch (opts->op) {
	case COP_GREATER_THAN:
		switch (opts->threshold.type) {
		case ADAPTIVED_CGVAL_LONG_LONG:
			if (val.value.ll_value > opts->threshold.value.ll_value)
				return 1;
			break;
		case ADAPTIVED_CGVAL_FLOAT:
			if (val.value.float_value > opts->threshold.value.float_value)
				return 1;
			break;
		default:
			return -EINVAL;
		}
		break;
	case COP_LESS_THAN:
		switch (opts->threshold.type) {
		case ADAPTIVED_CGVAL_LONG_LONG:
			if (val.value.ll_value < opts->threshold.value.ll_value)
				return 1;
			break;
		case ADAPTIVED_CGVAL_FLOAT:
			if (val.value.float_value < opts->threshold.value.float_value)
				return 1;
			break;
		default:
			return -EINVAL;
		}
		break;
	case COP_EQUAL:
		switch (opts->threshold.type) {
		case ADAPTIVED_CGVAL_LONG_LONG:
			if (val.value.ll_value == opts->threshold.value.ll_value)
				return 1;
			break;
		case ADAPTIVED_CGVAL_FLOAT:
			if (val.value.float_value == opts->threshold.value.float_value)
				return 1;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int cgset_main(struct adaptived_cause * const cse, int time_since_last_run)
{
	return _cgset_main(cse, time_since_last_run);
}

int cgset_memory_main(struct adaptived_cause * const cse, int time_since_last_run)
{
	struct cgset_opts *opts = (struct cgset_opts *)adaptived_cause_get_data(cse);

	if (adaptived_cgroup_setting_is_max(opts->setting)) {
		long long ll_value;
		int ret;

		adaptived_dbg("cgset_memory_main: %s at max.\n", opts->setting);

		ret = adaptived_get_meminfo_field(PROC_MEMINFO, "MemTotal", &ll_value);
		if (ret)
			return ret;
		ret = adaptived_cgroup_set_ll(opts->setting, ll_value, ADAPTIVED_CGROUP_FLAGS_VALIDATE);
		if (ret)
			return ret;
		adaptived_dbg("cgset_memory_main: %s at max. Changed to %lld\n", opts->setting, ll_value);
	}

	return _cgset_main(cse, time_since_last_run);
}

void cgset_exit(struct adaptived_cause * const cse)
{
	struct cgset_opts *opts = (struct cgset_opts *)adaptived_cause_get_data(cse);

	free_opts(opts);
}
