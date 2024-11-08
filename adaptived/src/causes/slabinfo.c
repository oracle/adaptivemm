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
 * Cause to detect when a change in a /proc/slabinfo field.
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

struct slabinfo_opts {
	enum cause_op_enum op;
	char *slabinfo_file;
	char *field;
	char *column;
	struct adaptived_cgroup_value threshold;
};

static void free_opts(struct slabinfo_opts * const opts)
{
	if (!opts)
		return;

	if (opts->slabinfo_file)
		free(opts->slabinfo_file);
	if (opts->field)
		free(opts->field);
	if (opts->column)
		free(opts->column);

	free(opts);
}

int slabinfo_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval)
{
	const char *slabinfo_file_str, *field_str, *column_str;
	struct slabinfo_opts *opts;
	int ret = 0;

	opts = malloc(sizeof(struct slabinfo_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}

	memset(opts, 0, sizeof( struct slabinfo_opts));

	ret = adaptived_parse_string(args_obj, "slabinfo_file", &slabinfo_file_str);
	if (ret == -ENOENT) {
		slabinfo_file_str = PROC_SLABINFO;
		ret = 0;
	} else if (ret) {
		adaptived_err("Failed to parse the slabinfo_file\n");
		goto error;
	}

	opts->slabinfo_file = malloc(sizeof(char) * strlen(slabinfo_file_str) + 1);
	if (!opts->slabinfo_file) {
		ret = -ENOMEM;
		goto error;
	}

	strcpy(opts->slabinfo_file, slabinfo_file_str);
	opts->slabinfo_file[strlen(slabinfo_file_str)] = '\0';
	adaptived_dbg("slabinfo_init: opts->slabinfo_file = %s\n", opts->slabinfo_file);

	ret = adaptived_parse_string(args_obj, "field", &field_str);
	if (ret) {
		adaptived_err("Failed to parse the field\n");
		goto error;
	}

	opts->field = malloc(sizeof(char) * strlen(field_str) + 1);
	if (!opts->field) {
		ret = -ENOMEM;
		goto error;
	}

	strcpy(opts->field, field_str);
	opts->field[strlen(field_str)] = '\0';
	adaptived_dbg("slabinfo_init: opts->field = %s\n", opts->field);

	ret = adaptived_parse_string(args_obj, "column", &column_str);
	if (ret == -ENOENT) {
		column_str = ACTIVE_OBJS;
		ret = 0;
	} else if (ret) {
		adaptived_err("Failed to parse the column\n");
		goto error;
	}
	opts->column = malloc(sizeof(char) * strlen(column_str) + 1);
	if (!opts->column) {
		ret = -ENOMEM;
		goto error;
	}

	strcpy(opts->column, column_str);
	opts->column[strlen(column_str)] = '\0';
	adaptived_dbg("slabinfo_init: opts->column = %s\n", opts->column);

	ret = parse_cause_operation(args_obj, NULL, &opts->op);
	if (ret)
		goto error;

	ret = adaptived_parse_cgroup_value(args_obj, "threshold", &opts->threshold);
	if (ret)
		goto error;

	opts->threshold.type = ADAPTIVED_CGVAL_LONG_LONG;

	ret = adaptived_cause_set_data(cse, (void *)opts);
	if (ret)
		goto error;

	return ret;

error:
	free_opts(opts);
	return ret;
}

int slabinfo_main(struct adaptived_cause * const cse, int time_since_last_run)
{
	struct slabinfo_opts *opts = (struct slabinfo_opts *)adaptived_cause_get_data(cse);
	long long ll_value = 0;
	int ret;

	ret = adaptived_file_exists(opts->slabinfo_file);
        if (ret)
                return ret;

	ret = adaptived_get_slabinfo_field(opts->slabinfo_file, opts->field, opts->column, &ll_value);
	if (ret)
		return ret;

	switch (opts->op) {
	case COP_GREATER_THAN:
		if (ll_value > opts->threshold.value.ll_value)
			return 1;
		break;
	case COP_LESS_THAN:
		if (ll_value < opts->threshold.value.ll_value)
			return 1;
		break;
	case COP_EQUAL:
		if (ll_value == opts->threshold.value.ll_value)
			return 1;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

void slabinfo_exit(struct adaptived_cause * const cse)
{
	struct slabinfo_opts *opts = (struct slabinfo_opts *)adaptived_cause_get_data(cse);

	free_opts(opts);
}
