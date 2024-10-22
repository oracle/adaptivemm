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
 * day of the week cause
 *
 * This file processes day of the week causes
 *
 */

#define _XOPEN_SOURCE

#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "adaptived-internal.h"

struct days {
	unsigned int sun : 1;
	unsigned int mon : 1;
	unsigned int tues : 1;
	unsigned int wed : 1;
	unsigned int thurs : 1;
	unsigned int fri : 1;
	unsigned int sat : 1;
};

struct days_of_the_week_opts {
	struct days d;
};

int consume_day(struct days_of_the_week_opts * const opts, const char * const day)
{
	if (strncmp(day, "sunday", strlen("sunday")) == 0 ||
	    strncmp(day, "Sunday", strlen("Sunday")) == 0 ||
	    strncmp(day, "sun", strlen("sun")) == 0 ||
	    strncmp(day, "Sun", strlen("Sun")) == 0) {
		opts->d.sun = 1;
	} else if (strncmp(day, "monday", strlen("monday")) == 0 ||
		   strncmp(day, "Monday", strlen("Monday")) == 0 ||
		   strncmp(day, "mon", strlen("mon")) == 0 ||
		   strncmp(day, "Mon", strlen("Mon")) == 0) {
		opts->d.mon = 1;
	} else if (strncmp(day, "tuesday", strlen("tuesday")) == 0 ||
		   strncmp(day, "Tuesday", strlen("Tuesday")) == 0 ||
		   strncmp(day, "tues", strlen("Tues")) == 0 ||
		   strncmp(day, "Tues", strlen("Tues")) == 0) {
		opts->d.tues = 1;
	} else if (strncmp(day, "wednesday", strlen("wednesday")) == 0 ||
		   strncmp(day, "Wednesday", strlen("Wednesday")) == 0 ||
		   strncmp(day, "wed", strlen("wed")) == 0 ||
		   strncmp(day, "Wed", strlen("Wed")) == 0) {
		opts->d.wed = 1;
	} else if (strncmp(day, "thursday", strlen("thursday")) == 0 ||
		   strncmp(day, "Thursday", strlen("Thursdayday")) == 0 ||
		   strncmp(day, "thurs", strlen("thurs")) == 0 ||
		   strncmp(day, "Thurs", strlen("Thurs")) == 0) {
		opts->d.thurs = 1;
	} else if (strncmp(day, "friday", strlen("friday")) == 0 ||
		   strncmp(day, "Friday", strlen("Friday")) == 0 ||
		   strncmp(day, "fri", strlen("fri")) == 0 ||
		   strncmp(day, "Fri", strlen("Fri")) == 0) {
		opts->d.fri = 1;
	} else if (strncmp(day, "saturday", strlen("saturday")) == 0 ||
		   strncmp(day, "Saturday", strlen("Saturday")) == 0 ||
		   strncmp(day, "sat", strlen("sat")) == 0 ||
		   strncmp(day, "Sat", strlen("Sat")) == 0) {
		opts->d.sat = 1;
	}

	return 0;
}

int days_of_the_week_init(struct adaptived_cause * const cse, struct json_object *args_obj,
			  int interval)
{
	struct json_object *days_obj, *day_obj;
	struct days_of_the_week_opts *opts;
	const char *day_str;
	json_bool exists;
	int i, day_cnt;
	int ret = 0;

	opts = malloc(sizeof(struct days_of_the_week_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}

	memset(opts, 0, sizeof(struct days_of_the_week_opts));

	exists = json_object_object_get_ex(args_obj, "days", &days_obj);
	if (!exists || !days_obj) {
		ret = -EINVAL;
		goto error;
	}

	day_cnt = json_object_array_length(days_obj);

	for (i = 0; i < day_cnt; i++) {
		day_obj = json_object_array_get_idx(days_obj, i);
		if (!day_obj) {
			ret = -EINVAL;
			goto error;
		}

		ret = adaptived_parse_string(day_obj, "day", &day_str);
		if (ret)
			goto error;

		ret = consume_day(opts, day_str);
		if (ret)
			goto error;
	}

	/* we have successfully setup the time_of_day cause */
	cse->data = (void *)opts;

	return ret;

error:
	if (opts)
		free(opts);

	return ret;
}

int days_of_the_week_main(struct adaptived_cause * const cse, int time_since_last_run)
{
	struct days_of_the_week_opts *opts = (struct days_of_the_week_opts *)cse->data;
	struct tm *cur_tm;
	time_t cur_time;

	time(&cur_time);
	cur_tm = localtime(&cur_time);

	switch (cur_tm->tm_wday) {
		case 0: /* Sunday */
			if (opts->d.sun) {
				adaptived_info("Sunday trigger\n");
				return 1;
			}
			break;
		case 1: /* Monday */
			if (opts->d.mon) {
				adaptived_info("Monday trigger\n");
				return 1;
			}
			break;
		case 2: /* Tuesday */
			if (opts->d.tues) {
				adaptived_info("Tuesday trigger\n");
				return 1;
			}
			break;
		case 3:
			if (opts->d.wed) {
				adaptived_info("Wednesday trigger\n");
				return 1;
			}
			break;
		case 4:
			if (opts->d.thurs) {
				adaptived_info("Thursday trigger\n");
				return 1;
			}
			break;
		case 5:
			if (opts->d.fri) {
				adaptived_info("Friday trigger\n");
				return 1;
			}
			break;
		case 6:
			if (opts->d.sat) {
				adaptived_info("Saturday trigger\n");
				return 1;
			}
			break;
		default:
			adaptived_err("Unexpected day: %d\n", cur_tm->tm_wday);
			return -1;
	}

	return 0;
}

void days_of_the_week_exit(struct adaptived_cause * const cse)
{
	struct days_of_the_week_opts *opts = (struct days_of_the_week_opts *)cse->data;

	free(opts);
}
