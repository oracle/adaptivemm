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
 * snooze effect
 *
 * Some causes don't have a mechanism to ignore the cause for X seconds.
 * The snooze effect allows a user to ignore triggered cause(s) for the
 * specified duration.
 *
 */

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "adaptived-internal.h"
#include "defines.h"

struct snooze_opts {
	int duration;
	time_t prev_trigger;
};

int snooze_init(struct adaptived_effect * const eff, struct json_object *args_obj,
	        const struct adaptived_cause * const cse)
{
	struct snooze_opts *opts;
	int ret = 0;

	opts = malloc(sizeof(struct snooze_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}
	opts->prev_trigger = 0;

	ret = adaptived_parse_int(args_obj, "duration", &opts->duration);
	if (ret)
		goto error;

	/* we have successfully setup the snooze effect */
	eff->data = (void *)opts;

	return ret;

error:
	if (opts)
		free(opts);

	return ret;
}

int snooze_main(struct adaptived_effect * const eff)
{
	struct snooze_opts *opts = (struct snooze_opts *)eff->data;
	time_t cur_time;
	double diff;

	time(&cur_time);
	diff = difftime(cur_time, opts->prev_trigger);
	adaptived_dbg("Snooze duration: %d, Current diff: %.0lf\n", opts->duration, diff);

	if (diff < (double)(opts->duration / 1000.0f))
		/* inform adaptived to skip the remaining effects in this rule */
		return -EALREADY;

	opts->prev_trigger = cur_time;

	return 0;
}

void snooze_exit(struct adaptived_effect * const eff)
{
	struct snooze_opts *opts = (struct snooze_opts *)eff->data;

	free(opts);
}
