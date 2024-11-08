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
 * PSI pressure rate cause
 *
 * Uses linear regression to anticipate future pressures and will
 * trigger when the future PSI is expected to exceed the threshold 
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

const char * const pressure_rate_name = "pressure_rate";

static const int default_window_size = 30000;
static const int default_advanced_warning = 10000;

static const char * const action_names[] = {
	"falling",
	"rising",
};
static_assert(ARRAY_SIZE(action_names) == ACTION_CNT,
	      "action_names[] must be same length as ACTION_CNT");

static void free_opts(struct pressure_rate_opts * const opts)
{
	if (!opts)
		return;

	if (opts->data)
		free(opts->data);

	if (opts->common.pressure_file)
		free(opts->common.pressure_file);

	free(opts);
}

int pressure_rate_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval)
{
	const char *meas_str, *action_str, *press_str;
	struct pressure_rate_opts *opts;
	bool found_action;
	int ret = 0;
	int i;

	opts = malloc(sizeof(struct pressure_rate_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}

	memset(opts, 0, sizeof( struct pressure_rate_opts));

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
		adaptived_err("Total pressure is not supported by the pressure_rate cause\n");
		ret = -EINVAL;
		goto error;
	} else {
		ret = adaptived_parse_float(args_obj, "threshold", &opts->common.threshold.avg);
		if (ret)
			goto error;
		if (opts->common.threshold.avg < 0) {
			ret = -EINVAL;
			goto error;
		}
	}

	ret = adaptived_parse_string(args_obj, "action", &action_str);
	if (ret)
		goto error;

	found_action = false;
	for (i = 0; i < ACTION_CNT; i++) {
		if (strncmp(action_str, action_names[i], strlen(action_names[i])) == 0) {
			found_action = true;
			opts->action = i;
			break;
		}
	}

	if (!found_action) {
		adaptived_err("Invalid action provided: %s\n", action_str);
		ret = -EINVAL;
		goto error;
	}

	ret = adaptived_parse_int(args_obj, "window_size", &opts->window_size);
	if (ret == -ENOENT) {
		/* the user didn't provide a window size setting */
		adaptived_info("No window size was provided, using default: %d",
			    default_window_size);
		opts->window_size = default_window_size;
	} else if (ret) {
		goto error;
	}
	adaptived_info("%s: ret=%d, opts->window_size=%d\n", __func__, ret, opts->window_size);
	if (opts->window_size < 0)
		goto error;

	ret = adaptived_parse_int(args_obj, "advanced_warning", &opts->advanced_warning);
	if (ret == -ENOENT) {
		/* the user didn't provide the advanced warning setting */
		adaptived_info("No advanced warning was provided, using default: %d",
			    default_advanced_warning);
		opts->advanced_warning = default_advanced_warning;
	} else if (ret) {
		goto error;
	}
	adaptived_info("%s: ret=%d, opts->advanced_warning=%d\n", __func__, ret,
		    opts->advanced_warning);
	if (opts->advanced_warning < 0)
		goto error;

	opts->data_len = (int)((float)opts->window_size / (float)interval);

	/*
	 * The pressure_rate cause doesn't support Total PSI (and thus long long)
	 * arrays.  This has already been error checked earlier in this function,
	 * and therefore we don't need to check it here.
	 */
	opts->data = malloc(sizeof(float) * opts->data_len);
	if (!opts->data) {
		ret = -ENOMEM;
		goto error;
	}

	ret = adaptived_cause_set_data(cse, (void *)opts);
	if (ret)
		goto error;

	return ret;

error:
	free_opts(opts);
	return ret;
}

int pressure_rate_main(struct adaptived_cause * const cse, int time_since_last_run)
{
	struct pressure_rate_opts *opts = (struct pressure_rate_opts *)adaptived_cause_get_data(cse);
	float float_press;
	int ret;

	if (opts->common.meas == PRESSURE_SOME_TOTAL || opts->common.meas == PRESSURE_FULL_TOTAL) {
		/* Currently unsupported */
		return -EINVAL;
	} else {
		ret = adaptived_file_exists(opts->common.pressure_file);
		if (ret)
			return ret;

		ret = adaptived_get_pressure_avg(opts->common.pressure_file, opts->common.meas,
					      &float_press);
		if (ret)
			return ret;

		ret = adaptived_farray_append(opts->data, &float_press, opts->data_len,
					   &opts->data_sample_cnt);
		if (ret)
			return ret;

		adaptived_dbg("smplcnt = %d data_len = %d\n", opts->data_sample_cnt, opts->data_len);
		if (opts->data_sample_cnt < opts->data_len)
			/*
			 * Wait for the window to entirely fill before we run
			 * linear regression.  Otherwise we could get early false
			 * positives
			 */
			return 0;

		/* 
		 * We can re-use the float_press variable since it's already been added to the
		 * float array
		 */
		ret = adaptived_farray_linear_regression(opts->data, opts->data_sample_cnt,
						      time_since_last_run, opts->advanced_warning,
						      &float_press);
		if (ret)
			return ret;

		switch(opts->action) {
		case ACTION_RISING:
			/*
			 * Linear regression has predicted that the PSI value will exceed
			 * the threshold in advanced_warning seconds.  Trigger this cause
			 */
			if (float_press > opts->common.threshold.avg)
				return 1;
			break;
		case ACTION_FALLING:
			if (float_press < opts->common.threshold.avg)
				return 1;
			break;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

void pressure_rate_exit(struct adaptived_cause * const cse)
{
	struct pressure_rate_opts *opts = (struct pressure_rate_opts *)adaptived_cause_get_data(cse);

	free_opts(opts);
}
