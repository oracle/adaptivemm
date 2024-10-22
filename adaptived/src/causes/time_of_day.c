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
 * time of day cause
 *
 * This file processes time of day causes
 *
 */

#define _XOPEN_SOURCE

#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "adaptived-internal.h"
#include "defines.h"

struct time_of_day_opts {
	char *time_str;
	enum cause_op_enum op;
	struct tm time;
};

static void free_opts(struct time_of_day_opts * const opts)
{
	if (!opts)
		return;

	if (opts->time_str)
		free(opts->time_str);

	free(opts);
}

int time_of_day_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval)
{
	struct time_of_day_opts *opts;
	const char *time_str;
	struct tm time;
	int ret = 0;
	char *tret;

	opts = malloc(sizeof(struct time_of_day_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}

	memset(opts, 0, sizeof(struct time_of_day_opts));

	ret = adaptived_parse_string(args_obj, "time", &time_str);
	if (ret)
		goto error;

	opts->time_str = malloc(sizeof(char) * strlen(time_str) + 1);
	if (!opts->time_str) {
		ret = -ENOMEM;
		goto error;
	}

	strcpy(opts->time_str, time_str);

	tret = strptime(time_str, "%H:%M:%S", &time);
	if (!tret) {
		/*
		 * We were unable to process all of the characters in the
		 * string.  Fail and notify the user
		 */
		ret = -EINVAL;
		goto error;
	}

	memcpy(&opts->time, &time, sizeof(struct tm));

	ret = parse_cause_operation(args_obj, NULL, &opts->op);
	if (ret)
		goto error;

	if (opts->op != COP_GREATER_THAN) {
		/* This cause currently only supports greaterthan */
		ret = -EINVAL;
		goto error;
	}

	/* we have successfully setup the time_of_day cause */
	cse->data = (void *)opts;

	return ret;

error:
	free_opts(opts);
	return ret;
}

int time_of_day_main(struct adaptived_cause * const cse, int time_since_last_run)
{
	struct time_of_day_opts *opts = (struct time_of_day_opts *)cse->data;
	struct tm *cur_tm;
	time_t cur_time;
	int ret = 0;

	time(&cur_time);
	cur_tm = localtime(&cur_time);

	switch (opts->op) {
		case COP_GREATER_THAN:
			if (cur_tm->tm_hour > opts->time.tm_hour) {
				adaptived_info("Cur hour %d > trigger hour %d\n",
					    cur_tm->tm_hour, opts->time.tm_hour);
				return 1;
			}
			if (cur_tm->tm_hour == opts->time.tm_hour &&
			    cur_tm->tm_min > opts->time.tm_min) {
				adaptived_info("Hours match.  Cur min %d > trigger min %d\n",
					    cur_tm->tm_min, opts->time.tm_min);
				return 1;
			}
			if (cur_tm->tm_hour == opts->time.tm_hour &&
			    cur_tm->tm_min == opts->time.tm_min &&
			    cur_tm->tm_sec > opts->time.tm_sec) {
				adaptived_info("Hrs and mins match.  Cur sec %d > trigger sec %d\n",
					    cur_tm->tm_sec, opts->time.tm_sec);
				return 1;
			}
			break;
		default:
			adaptived_err("Invalid ToD operation: %d\n", opts->op);
			ret = -EINVAL;
			break;
	}

	return ret;
}

void time_of_day_exit(struct adaptived_cause * const cse)
{
	struct time_of_day_opts *opts = (struct time_of_day_opts *)cse->data;

	free_opts(opts);
}
