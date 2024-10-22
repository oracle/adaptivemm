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
 * An effect to change cgroup settings via sd_bus
 *
 */

#include <sys/param.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <time.h>

#include <adaptived.h>
#include <adaptived-utils.h>

#include "adaptived-internal.h"
#include "defines.h"

struct cg_opts {
	char *target;
	char *setting;
	char *pre_set_from;
	struct adaptived_cgroup_value value;

	enum effect_op_enum op;

	bool limit_provided;
	struct adaptived_cgroup_value limit; /* optional */
	bool validate;
};

int sd_bus_setting_init(struct adaptived_effect * const eff, struct json_object *args_obj,
			const struct adaptived_cause * const cse)
{
	const char *setting_str, *target_str, *op_str;
	struct cg_opts *opts;
	int i, ret = 0;
	bool found_op;

	opts = malloc(sizeof(struct cg_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}
	memset(opts, 0, sizeof(struct cg_opts));

	opts->value.type = ADAPTIVED_CGVAL_CNT;
	opts->limit.type = ADAPTIVED_CGVAL_CNT;
	opts->limit_provided = false;

	ret = adaptived_parse_string(args_obj, "target", &target_str);
	if (ret)
		goto error;

	opts->target = malloc(sizeof(char) * strlen(target_str) + 1);
	if (!opts->target) {
		ret = -ENOMEM;
		goto error;
	}

	strcpy(opts->target, target_str);
	opts->target[strlen(target_str)] = '\0';

	ret = adaptived_parse_string(args_obj, "setting", &setting_str);
	if (ret)
		goto error;

	opts->setting = malloc(sizeof(char) * strlen(setting_str) + 1);
	if (!opts->setting) {
		ret = -ENOMEM;
		goto error;
	}

	strcpy(opts->setting, setting_str);
	opts->setting[strlen(setting_str)] = '\0';

	opts->pre_set_from = NULL;
	ret = adaptived_parse_string(args_obj, "pre_set_from", &setting_str);
	if (ret) {
		if (ret != -ENOENT)
			goto error;
		ret = 0;
	} else {
		opts->pre_set_from = malloc(sizeof(char) * strlen(setting_str) + 1);
		if (!opts->pre_set_from) {
			ret = -ENOMEM;
			goto error;
		}

		strcpy(opts->pre_set_from, setting_str);
		opts->pre_set_from[strlen(setting_str)] = '\0';
		adaptived_dbg("sd_bus setting: pre_set_from = %s\n", opts->pre_set_from);
	}

	ret = adaptived_parse_cgroup_value(args_obj, "value", &opts->value);
	if (ret)
		goto error;

	ret = adaptived_parse_string(args_obj, "operator", &op_str);
	if (ret)
		goto error;

	found_op = false;
	for (i = 0; i < EOP_CNT; i++) {
		if (strncmp(op_str, effect_op_names[i], strlen(effect_op_names[i])) == 0) {
			found_op = true;
			opts->op = i;
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
		opts->validate = false;
		ret = 0;
	} else if (ret) {
		adaptived_err("Failed to parse the sd_bus_setting validate arg: %d\n", ret);
		goto error;
	} else {
		char *sp = opts->setting;

		/* Check for special property handling and validate. */
		if (((strncmp(sp, "CPUQuota", strlen("CPUQuota")) == 0) &&
		    (strlen(sp) == strlen("CPUQuota"))) ||
			((strncmp(sp, "CPUQuotaPeriodSec", strlen("CPUQuotaPeriodSec")) == 0) &&
			    (strlen(sp) == strlen("CPUQuotaPeriodSec")))) {
			adaptived_err("Cannot validate special property: %s\n", opts->setting);
			ret = -EINVAL;
			goto error;
		}
	}
	adaptived_dbg("sd_bus setting: validate = %d\n", opts->validate);

	eff->data = (void *)opts;

	return ret;

error:
	if (opts) {
		if (opts->target)
			free(opts->target);
		if (opts->setting)
			free(opts->setting);
		if (opts->pre_set_from)
			free(opts->pre_set_from);
		adaptived_free_cgroup_value(&opts->value);
		adaptived_free_cgroup_value(&opts->limit);

		free(opts);
	}

	return ret;
}

static int add(const struct cg_opts * const opts, struct adaptived_cgroup_value * const value)
{
	uint32_t cgflags = 0;
	long long sum;
	int ret = 0;

	switch (value->type) {
	case ADAPTIVED_CGVAL_LONG_LONG:

		sum = value->value.ll_value + opts->value.value.ll_value;

		if (opts->limit_provided)
			sum = MIN(sum, opts->limit.value.ll_value);

		if (opts->validate)
			cgflags |= ADAPTIVED_CGROUP_FLAGS_VALIDATE;

		ret = adaptived_sd_bus_set_ll(opts->target, opts->setting, sum, cgflags);
		if (ret)
			return ret;
		break;
	case ADAPTIVED_CGVAL_FLOAT:
		adaptived_err("Not yet supported\n");
		ret = -ENOTSUP;
		break;
	default:
		adaptived_err("Unsupported type: %d\n", value->type);
		break;
	}

	return ret;
}

static int subtract(const struct cg_opts * const opts, struct adaptived_cgroup_value * const value)
{
	uint32_t cgflags = 0;
	long long diff;
	int ret;

	switch (value->type) {
	case ADAPTIVED_CGVAL_LONG_LONG:
		diff = value->value.ll_value - opts->value.value.ll_value;

		if (opts->limit_provided)
			diff = MAX(diff, opts->limit.value.ll_value);

		if (opts->validate)
			cgflags |= ADAPTIVED_CGROUP_FLAGS_VALIDATE;

		ret = adaptived_sd_bus_set_ll(opts->target, opts->setting, diff, cgflags);
		if (ret)
			return ret;
		break;
	case ADAPTIVED_CGVAL_FLOAT:
		adaptived_err("Not yet supported\n");
		ret = -ENOTSUP;
		break;
	default:
		adaptived_err("Unsupported type: %d\n", value->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int _sd_bus_setting_main(struct adaptived_effect * const eff)
{
	struct cg_opts *opts = (struct cg_opts *)eff->data;
	struct adaptived_cgroup_value val;
	uint32_t cgflags = 0;
	int ret;

	val.type = opts->value.type;

	switch (opts->op) {
	case EOP_ADD:
		ret = adaptived_sd_bus_get_value(opts->target, opts->setting, &val);
		if (ret)
			return ret;

		ret = add(opts, &val);
		if (ret)
			return ret;
		break;
	case EOP_SUBTRACT:
		ret = adaptived_sd_bus_get_value(opts->target, opts->setting, &val);
		if (ret)
			return ret;

		ret = subtract(opts, &val);
		if (ret)
			return ret;
		break;
	case EOP_SET:
		if (opts->validate)
			cgflags |= ADAPTIVED_CGROUP_FLAGS_VALIDATE;

		ret = adaptived_sd_bus_set_value(opts->target, opts->setting, &opts->value, cgflags);
		if (ret)
			return ret;
		break;
	default:
		break;
	}

	return 0;
}

int sd_bus_setting_main(struct adaptived_effect * const eff)
{
	struct cg_opts *opts = (struct cg_opts *)eff->data;

	if (opts->op != EOP_SET) {
		long long ll_value;
		uint32_t cgflags = 0;
		int ret;

		if (opts->pre_set_from) {
			ret = adaptived_sd_bus_get_ll(opts->target, opts->pre_set_from, &ll_value);
			if (ret)
				return ret;

			ret = adaptived_sd_bus_set_ll(opts->target, opts->setting, ll_value, cgflags);
			if (ret)
				return ret;
		}

		if (adaptived_sd_bus_setting_is_max(opts->target, opts->setting)) {
			if (opts->op == EOP_ADD) {
				adaptived_err("%s: %s at max. Can't change for EOP_ADD.\n", __func__,
					   opts->setting);
				return 1;
			}
			ret = adaptived_get_meminfo_field(PROC_MEMINFO, "MemTotal", &ll_value);
			if (ret)
				return ret;
			ret = adaptived_sd_bus_set_ll(opts->target, opts->setting, ll_value,
						   ADAPTIVED_CGROUP_FLAGS_VALIDATE);
			if (ret)
				return ret;
			adaptived_dbg("%s: %s at max. Changed to %lld\n", __func__, opts->setting, ll_value);

		}
	}
	return _sd_bus_setting_main(eff);
}

void sd_bus_setting_exit(struct adaptived_effect * const eff)
{
	struct cg_opts *opts = (struct cg_opts *)eff->data;

	if (opts->target)
		free(opts->target);
	if (opts->setting)
		free(opts->setting);
	if (opts->pre_set_from)
		free(opts->pre_set_from);
	adaptived_free_cgroup_value(&opts->value);
	adaptived_free_cgroup_value(&opts->limit);
	free(opts);
}
