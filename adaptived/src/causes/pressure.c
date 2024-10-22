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
 * PSI pressure cause and trigger implementation file
 *
 */

#include <json-c/json.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include <adaptived-utils.h>
#include <adaptived.h>

#include "defines.h"
#include "adaptived-internal.h"

#include "pressure.h"

const char * const pressure_name = "pressure";

const char * const pressure_type_names[] = {
	"cpu",
	"memory",
	"io",
};
static_assert(ARRAY_SIZE(pressure_type_names) == PRESSURE_TYPE_CNT,
	      "pressure_type_names[] must be same length as PRESSURE_TYPE_CNT");

const char * const meas_names[] = {
	"some-avg10",
	"some-avg60",
	"some-avg300",
	"some-total",
	"full-avg10",
	"full-avg60",
	"full-avg300",
	"full-total",
};
static_assert(ARRAY_SIZE(meas_names) == PRESSURE_MEAS_CNT,
	      "meas_names[] must be same length as PRESSURE_MEAS_CNT");

static void free_opts(struct pressure_opts * const opts)
{
	if (!opts)
		return;

	if (opts->common.pressure_file)
		free(opts->common.pressure_file);

	free(opts);
}

int pressure_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval)
{
	const char *meas_str, *press_str;
	struct pressure_opts *opts;
	int ret = 0;
	int i;

	opts = malloc(sizeof(struct pressure_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}

	memset(opts, 0, sizeof( struct pressure_opts));

	ret = adaptived_parse_string(args_obj, "pressure_file", &press_str);
	if (ret) {
		adaptived_err("Failed to parse the pressure_file setting\n");
		goto error;
	}

	opts->common.pressure_file = malloc(sizeof(char) * strlen(press_str) + 1);
	if (!opts->common.pressure_file) {
		ret = -ENOMEM;
		goto error;
	}

	strcpy(opts->common.pressure_file, press_str);
	opts->common.pressure_file[strlen(press_str)] = '\0';

	ret = adaptived_parse_string(args_obj, "measurement", &meas_str);
	if (ret)
		goto error;

	opts->common.meas = PRESSURE_MEAS_CNT;
	for (i = 0; i < PRESSURE_MEAS_CNT; i++) {
		if (strncmp(meas_names[i], meas_str, strlen(meas_names[i])) == 0) {
			opts->common.meas = i;
			break;
		}
	}
	if (opts->common.meas == PRESSURE_MEAS_CNT) {
		adaptived_err("Invalid measurement provided: %s\n", meas_str);
		ret = -EINVAL;
		goto error;
	}

	if (opts->common.meas == PRESSURE_FULL_TOTAL || opts->common.meas == PRESSURE_SOME_TOTAL) {
		ret = adaptived_parse_long_long(args_obj, "threshold", &opts->common.threshold.total);
		if (ret)
			goto error;
		if (opts->common.threshold.total <= 0) {
			ret = -EINVAL;
			goto error;
		}
	} else {
		ret = adaptived_parse_float(args_obj, "threshold", &opts->common.threshold.avg);
		if (ret)
			goto error;
		if (opts->common.threshold.avg < 0) {
			ret = -EINVAL;
			goto error;
		}
	}

	ret = adaptived_parse_int(args_obj, "duration", &opts->duration);
	if (ret == -ENOENT) {
		/* the user didn't provide a duration setting */
		adaptived_info("No duration was provided. "
			    "Trigger every time the threshold is exceeded\n");
		opts->duration = -1;
	} else if (ret) {
		goto error;
	}
	adaptived_info("%s: ret=%d, opts->duration=%d\n", __func__, ret, opts->duration);

	ret = parse_cause_operation(args_obj, NULL, &opts->op);
	if (ret)
		goto error;

	ret = adaptived_cause_set_data(cse, (void *)opts);
	if (ret)
		goto error;

	return ret;

error:
	free_opts(opts);
	return ret;
}

int pressure_main(struct adaptived_cause * const cse, int time_since_last_run)
{
	struct pressure_opts *opts = (struct pressure_opts *)adaptived_cause_get_data(cse);
	long long int_press;
	float float_press;
	int ret;

	if (opts->common.meas == PRESSURE_SOME_TOTAL || opts->common.meas == PRESSURE_FULL_TOTAL) {
		ret = adaptived_get_pressure_total(opts->common.pressure_file, opts->common.meas,
						&int_press);
		if (ret)
			return -EINVAL;

		switch (opts->op) {
		case COP_GREATER_THAN:
			if (int_press > opts->common.threshold.total)
				opts->current_duration += time_since_last_run;
			else
				opts->current_duration = 0;
			break;
		case COP_LESS_THAN:
			if (int_press < opts->common.threshold.total)
				opts->current_duration += time_since_last_run;
			else
				opts->current_duration = 0;
			break;
		case COP_EQUAL:
			if (int_press == opts->common.threshold.total)
				opts->current_duration += time_since_last_run;
			else
				opts->current_duration = 0;
			break;
		default:
			return -EINVAL;
		}
	} else {
		ret = adaptived_get_pressure_avg(opts->common.pressure_file, opts->common.meas,
					      &float_press);
		if (ret)
			return -EINVAL;

		switch (opts->op) {
		case COP_GREATER_THAN:
			if (float_press > opts->common.threshold.avg)
				opts->current_duration += time_since_last_run;
			else
				opts->current_duration = 0;
			break;
		case COP_LESS_THAN:
			if (float_press < opts->common.threshold.avg)
				opts->current_duration += time_since_last_run;
			else
				opts->current_duration = 0;
			break;
		case COP_EQUAL:
			if (float_press == opts->common.threshold.avg)
				opts->current_duration += time_since_last_run;
			else
				opts->current_duration = 0;
			break;
		default:
			return -EINVAL;
		}
	}

	/*
	 * There are two ways for this cause to trigger:
	 * 1. The user didn't specify a duration (opts->duration = -1) and the threshold
	 *    was exceeded on this invocation (opts->current_duration > 0)
	 * 2. The user specified a duration and we have exceeded it
	 *    (opts->current_duration >= opts->duration)
	 */
	if ((opts->duration < 0 && opts->current_duration > 0) ||
	    (opts->current_duration >= opts->duration)) {
		opts->current_duration = 0;
		return 1;
	}

	return 0;
}

void pressure_exit(struct adaptived_cause * const cse)
{
	struct pressure_opts *opts = (struct pressure_opts *)adaptived_cause_get_data(cse);

	free_opts(opts);
}
