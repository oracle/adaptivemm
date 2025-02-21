/*
 * Copyright (c) 2024-2025, Oracle and/or its affiliates.
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
 * An effect to modify a cgroup setting in a cgroup that has the largest/smallest PSI value
 *
 */

#include <sys/param.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>

#include <adaptived-utils.h>
#include <adaptived.h>

#include "adaptived-internal.h"
#include "pressure.h"
#include "defines.h"

struct cgroup_setting_psi_opts {
	char *cgroup_path;

	enum pressure_type_enum pressure_type;
	enum adaptived_pressure_meas_enum meas;
	enum cause_op_enum pressure_op;

	char *cgroup_setting;
	struct adaptived_cgroup_value value;

	enum effect_op_enum setting_op;

	struct adaptived_cgroup_value limit; /* optional */
	bool validate; /* optional */
	int max_depth; /* optional */

	/* internal variables that aren't passed in via JSON args */
	bool limit_provided;
};

int cgroup_setting_psi_init(struct adaptived_effect * const eff, struct json_object *args_obj,
			 const struct adaptived_cause * const cse)
{
	const char *cgroup_path_str, *meas_str, *type_str, *op_str, *setting_str;
	struct cgroup_setting_psi_opts *opts;
	int ret = 0, i;
	bool found_op;

	opts = malloc(sizeof(struct cgroup_setting_psi_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}
	opts->cgroup_path = NULL;
	opts->cgroup_setting = NULL;
	opts->value.type = ADAPTIVED_CGVAL_CNT;
	opts->limit.type = ADAPTIVED_CGVAL_CNT;
	opts->limit_provided = false;

	ret = adaptived_parse_string(args_obj, "cgroup", &cgroup_path_str);
	if (ret)
		goto error;

	opts->cgroup_path = malloc(sizeof(char) * strlen(cgroup_path_str) + 1);
	if (!opts->cgroup_path) {
		ret = -ENOMEM;
		goto error;
	}

	strcpy(opts->cgroup_path, cgroup_path_str);
	opts->cgroup_path[strlen(cgroup_path_str)] = '\0';

	ret = adaptived_parse_string(args_obj, "type", &type_str);
	if (ret)
		goto error;

	opts->pressure_type = PRESSURE_TYPE_CNT;
	for (i = 0; i < PRESSURE_TYPE_CNT; i++) {
		if (strcmp(pressure_type_names[i], type_str) == 0) {
			opts->pressure_type = i;
			break;
		}
	}
	if (opts->pressure_type == PRESSURE_TYPE_CNT) {
		adaptived_err("Invalid pressure type provided: %s\n", type_str);
		ret = -EINVAL;
		goto error;
	}

	ret = adaptived_parse_string(args_obj, "measurement", &meas_str);
	if (ret)
		goto error;

	opts->meas = PRESSURE_MEAS_CNT;
	for (i = 0; i < PRESSURE_MEAS_CNT; i++) {
		if (strcmp(meas_names[i], meas_str) == 0) {
			opts->meas = i;
			break;
		}
	}
	if (opts->meas == PRESSURE_MEAS_CNT || opts->meas == PRESSURE_FULL_TOTAL ||
	    opts->meas == PRESSURE_SOME_TOTAL) {
		adaptived_err("Invalid measurement provided: %s\n", meas_str);
		ret = -EINVAL;
		goto error;
	}

	ret = parse_cause_operation(args_obj, "pressure_operator", &opts->pressure_op);
	if (ret)
		goto error;

	ret = adaptived_parse_string(args_obj, "setting", &setting_str);
	if (ret)
		goto error;

	opts->cgroup_setting = malloc(sizeof(char) * strlen(setting_str) + 1);
	if (!opts->cgroup_setting) {
		ret = -ENOMEM;
		goto error;
	}

	strcpy(opts->cgroup_setting, setting_str);
	opts->cgroup_setting[strlen(setting_str)] = '\0';

	ret = adaptived_parse_cgroup_value(args_obj, "value", &opts->value);
	if (ret)
		goto error;

	ret = adaptived_parse_string(args_obj, "setting_operator", &op_str);
	if (ret)
		goto error;

	found_op = false;
	for (i = 0; i < EOP_CNT; i++) {
		if (strcmp(op_str, effect_op_names[i]) == 0) {
			found_op = true;
			opts->setting_op = i;
			break;
		}
	}

	if (!found_op) {
		ret = -EINVAL;
		goto error;
	}

	ret = adaptived_parse_cgroup_value(args_obj, "limit", &opts->limit);
	if (ret == -ENOENT) {
		opts->limit_provided = false;
		ret = 0;
	} else if (ret) {
		goto error;
	} else {
		opts->limit_provided = true;

		if (opts->limit.type != opts->value.type) {
			adaptived_err("limit must be same type as value\n");
			ret = -EINVAL;
			goto error;
		}
	}

	ret = adaptived_parse_bool(args_obj, "validate", &opts->validate);
	if (ret == -ENOENT) {
		opts->validate = false;;
		ret = 0;
	} else if (ret) {
		adaptived_err("Failed to parse the cgroup_setting validate arg: %d\n", ret);
		goto error;
	}
	adaptived_dbg("Cgroup setting: validate = %d\n", opts->validate);

	ret = adaptived_parse_int(args_obj, "max_depth", &opts->max_depth);
	if (ret == -ENOENT) {
		opts->max_depth = ADAPTIVED_PATH_WALK_UNLIMITED_DEPTH;
		ret = 0;
	} else if (ret) {
		goto error;
	}

	eff->data = (void *)opts;

	return ret;

error:
	if (opts) {
		if (opts->cgroup_path)
			free(opts->cgroup_path);
		if (opts->cgroup_setting)
			free(opts->cgroup_setting);
		adaptived_free_cgroup_value(&opts->value);
		adaptived_free_cgroup_value(&opts->limit);

		free(opts);
	}

	return ret;
}

static int get_psi_avg(const struct cgroup_setting_psi_opts * const opts,
		       const char * const cgroup_path, float * const avg)
{
	char pressure_path[FILENAME_MAX] = { '\0' };
	int ret;

	switch(opts->pressure_type) {
	case PRESSURE_TYPE_CPU:
		sprintf(pressure_path, "%s/cpu.pressure", cgroup_path);
		break;
	case PRESSURE_TYPE_MEMORY:
		sprintf(pressure_path, "%s/memory.pressure", cgroup_path);
		break;
	case PRESSURE_TYPE_IO:
		sprintf(pressure_path, "%s/io.pressure", cgroup_path);
		break;
	default:
		return -EINVAL;
	}

	ret = adaptived_get_pressure_avg(pressure_path, opts->meas, avg);
	if (ret)
		return ret;

	return 0;
}

static int add(const struct cgroup_setting_psi_opts * const opts,
	       struct adaptived_cgroup_value * const value)
{
	long long sum;

	switch (value->type) {
	case ADAPTIVED_CGVAL_LONG_LONG:
		sum = value->value.ll_value + opts->value.value.ll_value;

		if (opts->limit_provided)
			sum = MIN(sum, opts->limit.value.ll_value);

		if (value->value.ll_value == sum)
			return -EALREADY;

		value->value.ll_value = sum;
		break;
	case ADAPTIVED_CGVAL_FLOAT:
		adaptived_err("Not yet supported\n");
		return -ENOTSUP;
		break;
	default:
		adaptived_err("Unsupported type: %d\n", value->type);
		break;
	}

	return 0;
}

static int subtract(const struct cgroup_setting_psi_opts * const opts,
		    struct adaptived_cgroup_value * const value)
{
	long long diff;

	switch (value->type) {
	case ADAPTIVED_CGVAL_LONG_LONG:
		diff = value->value.ll_value - opts->value.value.ll_value;

		if (opts->limit_provided)
			diff = MAX(diff, opts->limit.value.ll_value);

		if (value->value.ll_value == diff)
			return -EALREADY;

		value->value.ll_value = diff;
		break;
	case ADAPTIVED_CGVAL_FLOAT:
		adaptived_err("Not yet supported\n");
		return -ENOTSUP;
		break;
	default:
		adaptived_err("Unsupported type: %d\n", value->type);
		break;
	}

	return 0;
}

static int calculate_value(struct cgroup_setting_psi_opts * const opts,
			   const char * const full_setting_path,
			   struct adaptived_cgroup_value * const value)
{
	int ret;

	switch (opts->setting_op) {
	case EOP_ADD:
		ret = adaptived_cgroup_get_value(full_setting_path, value);
		if (ret)
			goto error;

		ret = add(opts, value);
		break;
	case EOP_SUBTRACT:
		ret = adaptived_cgroup_get_value(full_setting_path, value);
		if (ret)
			goto error;

		ret = subtract(opts, value);
		break;
	case EOP_SET:
		memcpy(value, &opts->value, sizeof(struct adaptived_cgroup_value));
		ret = 0;
		break;
	default:
		adaptived_err("Unsupported type: %d\n", value->type);
		ret = -EINVAL;
		break;
	}

error:
	return ret;
}

static int compare_psi(struct cgroup_setting_psi_opts * const opts, const char * const cg_path,
		       float psi, float * const max_psi)
{
	char full_setting_path[FILENAME_MAX];
	struct adaptived_cgroup_value value;
	int ret;

	value.type = opts->value.type;

	switch (opts->pressure_op) {
	case COP_GREATER_THAN:
		if (psi < *max_psi)
			/* We have already found a better candidate */
			return -EALREADY;
		break;
	case COP_LESS_THAN:
		if (psi > *max_psi)
			/* We have already found a better candidate */
			return -EALREADY;
		break;
	default:
		return -EINVAL;
	}

	sprintf(full_setting_path, "%s/%s", cg_path, opts->cgroup_setting);
	full_setting_path[strlen(cg_path) + strlen(opts->cgroup_setting) + 1] = '\0';

	ret = calculate_value(opts, full_setting_path, &value);
	if (ret)
		return ret;

	/*
	 * This cgroup is the currently best candidate to be modified
	 */
	*max_psi = psi;

	return 0;
}

int cgroup_setting_psi_main(struct adaptived_effect * const eff)
{
	struct cgroup_setting_psi_opts *opts = (struct cgroup_setting_psi_opts *)eff->data;
	char *cgroup_path = NULL, *max_cgroup_path = NULL;
	struct adaptived_path_walk_handle *handle = NULL;
	char full_setting_path[FILENAME_MAX];
	struct adaptived_cgroup_value value;
	uint32_t cgflags = 0;
	float psi, max_psi;
	int ret;

	switch (opts->pressure_op) {
	case COP_GREATER_THAN:
		max_psi = -1.0f;
		break;
	case COP_LESS_THAN:
		max_psi = 101.0f;
		break;
	default:
		return -EINVAL;
	}

	value.type = opts->value.type;

	ret = adaptived_path_walk_start(opts->cgroup_path, &handle, ADAPTIVED_PATH_WALK_LIST_DIRS,
				     opts->max_depth);
	if (ret)
		goto error;

	do {
		ret = adaptived_path_walk_next(&handle, &cgroup_path);
		if (ret)
			goto error;
		if (!cgroup_path)
			/* We've reached the end of the tree */
			break;

		ret = get_psi_avg(opts, cgroup_path, &psi);
		if (ret)
			goto error;

		ret = compare_psi(opts, cgroup_path, psi, &max_psi);
		if (ret == -EALREADY) {
			/* A previous cgroup is a better candidate */
			free(cgroup_path);
			continue;
		} else if (ret) {
			goto error;
		}

		if (max_cgroup_path)
			free(max_cgroup_path);
		max_cgroup_path = cgroup_path;
	} while (true);

	if (max_cgroup_path) {
		sprintf(full_setting_path, "%s/%s", max_cgroup_path, opts->cgroup_setting);
		full_setting_path[strlen(max_cgroup_path) +
				  strlen(opts->cgroup_setting) + 1] = '\0';

		ret = calculate_value(opts, full_setting_path, &value);
		if (ret)
			goto error;

		if (opts->validate)
			cgflags |= ADAPTIVED_CGROUP_FLAGS_VALIDATE;

		ret = adaptived_cgroup_set_value(full_setting_path, &value, cgflags);
		if (ret)
			goto error;
	}

error:
	adaptived_path_walk_end(&handle);
	if (cgroup_path)
		free(cgroup_path);
	if (max_cgroup_path)
		free(max_cgroup_path);

	return ret;
}

void cgroup_setting_psi_exit(struct adaptived_effect * const eff)
{
	struct cgroup_setting_psi_opts *opts = (struct cgroup_setting_psi_opts *)eff->data;

	if (opts->cgroup_path)
		free(opts->cgroup_path);
	if (opts->cgroup_setting)
		free(opts->cgroup_setting);
	adaptived_free_cgroup_value(&opts->value);
	adaptived_free_cgroup_value(&opts->limit);
	free(opts);
}
