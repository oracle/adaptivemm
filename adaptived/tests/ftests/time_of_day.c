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

#include <adaptived.h>

const char * const tod_name = "tod";

enum op_enum {
	OP_GREATER_THAN = 0,
	OP_CNT
};

static const char * const op_names[] = {
	"greaterthan",
};

struct time_of_day_opts {
	char *time_str;
	enum op_enum op;
	struct tm time;
};

int time_of_day_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval)
{
	struct time_of_day_opts *opts;
	const char *time_str, *op_str;
	struct tm time;
	bool found_op;
	int ret = 0;
	char *tret;
	int i;

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

	ret = adaptived_parse_string(args_obj, "operator", &op_str);
	if (ret)
		goto error;

	found_op = false;
	for (i = 0; i < OP_CNT; i++) {
		if (strncmp(op_str, op_names[i], strlen(op_names[i])) == 0) {
			found_op = true;
			opts->op = i;
			break;
		}
	}

	if (!found_op) {
		ret = -EINVAL;
		goto error;
	}

	ret = adaptived_cause_set_data(cse, (void *)opts);
	if (ret)
		goto error;

	return ret;

error:
	if (opts && opts->time_str)
		free(opts->time_str);

	if (opts)
		free(opts);

	return ret;
}

int time_of_day_main(struct adaptived_cause * const cse, int time_since_last_run)
{
	struct time_of_day_opts *opts;
	struct tm *cur_tm;
	time_t cur_time;
	int ret = 0;

	opts = (struct time_of_day_opts *)adaptived_cause_get_data(cse);

	time(&cur_time);
	cur_tm = localtime(&cur_time);

	switch (opts->op) {
		case OP_GREATER_THAN:
			if (cur_tm->tm_hour > opts->time.tm_hour)
				return 1;
			if (cur_tm->tm_hour == opts->time.tm_hour &&
			    cur_tm->tm_min > opts->time.tm_min)
				return 1;
			if (cur_tm->tm_hour == opts->time.tm_hour &&
			    cur_tm->tm_min == opts->time.tm_min &&
			    cur_tm->tm_sec > opts->time.tm_sec)
				return 1;
			break;
		default:
			ret = -EINVAL;
			break;
	}

	return ret;
}

void time_of_day_exit(struct adaptived_cause * const cse)
{
	struct time_of_day_opts *opts;

	opts = (struct time_of_day_opts *)adaptived_cause_get_data(cse);

	if (opts->time_str)
		free(opts->time_str);

	free(opts);
}

const struct adaptived_cause_functions tod_fns = {
	time_of_day_init,
	time_of_day_main,
	time_of_day_exit,
};
