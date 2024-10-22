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
 * Copy a cgroup value from one cgroup setting file to another.
 *
 */

#include <assert.h>
#include <string.h>
#include <errno.h>

#include <adaptived-utils.h>
#include <adaptived.h>

#include "effect.h"

const char * const copy_cgroup_setting_name = "copy_cgroup_setting";

struct copy_cgroup_setting_opts {
	char *from_setting;
	char *to_setting;
	bool dont_copy_if_zero;
	bool validate;
};

int copy_cgroup_setting_init(struct adaptived_effect * const eff, struct json_object *args_obj,
	       const struct adaptived_cause * const cse)
{
	struct copy_cgroup_setting_opts *opts;
	const char *setting_str;
	int ret = 0;

	opts = malloc(sizeof(struct copy_cgroup_setting_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}
	memset(opts, 0, sizeof(struct copy_cgroup_setting_opts));

	ret = adaptived_parse_string(args_obj, "from_setting", &setting_str);
	if (ret)
		goto error;

	if (access(setting_str, F_OK) != 0) {
		adaptived_err("%s: can't find %s\n", __func__, setting_str);
		ret = -EEXIST;
		goto error;
	}

	opts->from_setting = malloc(sizeof(char) * strlen(setting_str) + 1);
	if (!opts->from_setting) {
		ret = -ENOMEM;
		goto error;
	}

	strcpy(opts->from_setting, setting_str);
	opts->from_setting[strlen(setting_str)] = '\0';

	ret = adaptived_parse_string(args_obj, "to_setting", &setting_str);
	if (ret)
		goto error;

	if (access(setting_str, F_OK) != 0) {
		adaptived_err("%s: can't find %s\n", __func__, setting_str);
		ret = -EEXIST;
		goto error;
	}

	opts->to_setting = malloc(sizeof(char) * strlen(setting_str) + 1);
	if (!opts->to_setting) {
		ret = -ENOMEM;
		goto error;
	}

	strcpy(opts->to_setting, setting_str);
	opts->to_setting[strlen(setting_str)] = '\0';

	ret = adaptived_parse_bool(args_obj, "dont_copy_if_zero", &opts->dont_copy_if_zero);
	if (ret == -ENOENT) {
		opts->dont_copy_if_zero = false;
		ret = 0;
	} else if (ret) {
		adaptived_err("%s: Failed to parse the dont_copy_if_zero arg: %d\n", __func__, ret);
		goto error;
	}
	adaptived_dbg("%s: dont_copy_if_zero = %d\n", __func__, opts->dont_copy_if_zero);

	ret = adaptived_parse_bool(args_obj, "validate", &opts->validate);
	if (ret == -ENOENT) {
		opts->validate = false;
		ret = 0;
	} else if (ret) {
		adaptived_err("%s: Failed to parse the validate arg: %d\n", __func__, ret);
		goto error;
	}
	adaptived_dbg("%s: validate = %d\n", __func__, opts->validate);

	ret = adaptived_effect_set_data(eff, (void *)opts);
	if (ret)
		goto error;

	return ret;

error:
	if (opts) {
		if (opts->from_setting)
			free(opts->from_setting);
		if (opts->to_setting)
			free(opts->to_setting);
		free(opts);
	}
	return ret;
}

int copy_cgroup_setting_main(struct adaptived_effect * const eff)
{
	struct copy_cgroup_setting_opts *opts = (struct copy_cgroup_setting_opts *)adaptived_effect_get_data(eff);
	struct adaptived_cgroup_value val;
	uint32_t cgflags = 0;
	int ret = 0;

	val.type = ADAPTIVED_CGVAL_DETECT;

	ret = adaptived_cgroup_get_value(opts->from_setting, &val);
	if (ret)
		return ret;

	if (val.type == ADAPTIVED_CGVAL_LONG_LONG && !val.value.ll_value && opts->dont_copy_if_zero) {
		adaptived_info("%s: from value is zero and dont_copy_if_zero is set.\n", __func__);
		return 0;
	}

	if (opts->validate)
		cgflags |= ADAPTIVED_CGROUP_FLAGS_VALIDATE;

	ret = adaptived_cgroup_set_value(opts->to_setting, &val, cgflags);
	if (ret)
		return ret;

	return 0;
}

void copy_cgroup_setting_exit(struct adaptived_effect * const eff)
{
	struct copy_cgroup_setting_opts *opts = (struct copy_cgroup_setting_opts *)eff->data;

	if (opts) {
		if (opts->from_setting)
			free(opts->from_setting);
		if (opts->to_setting)
			free(opts->to_setting);
		free(opts);
	}
}

const struct adaptived_effect_functions copy_cgroup_setting_fns = {
	copy_cgroup_setting_init,
	copy_cgroup_setting_main,
	copy_cgroup_setting_exit
};
