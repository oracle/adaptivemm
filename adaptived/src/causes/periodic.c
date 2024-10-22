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
 * Cause that will periodically trigger.
 *
 */

#include <json-c/json.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#include <adaptived-utils.h>
#include <adaptived.h>

#include "defines.h"
#include "adaptived-internal.h"

struct periodic_opts {
	/* period is in milliseconds */
	int period;

	int current_duration;
};

int periodic_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval)
{
	struct periodic_opts *opts;
	int ret = 0;

	opts = malloc(sizeof(struct periodic_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}

	memset(opts, 0, sizeof( struct periodic_opts));

	ret = adaptived_parse_int(args_obj, "period", &opts->period);
	if (ret)
		goto error;

	ret = adaptived_cause_set_data(cse, (void *)opts);
	if (ret)
		goto error;

	return ret;

error:
	if (opts)
		free(opts);

	return ret;
}

int periodic_main(struct adaptived_cause * const cse, int time_since_last_run)
{
	struct periodic_opts *opts = (struct periodic_opts *)adaptived_cause_get_data(cse);

	opts->current_duration += time_since_last_run;

	if (opts->current_duration >= opts->period) {
		opts->current_duration = 0;
		/* periodic trigger */
		return 1;
	}

	return 0;
}

void periodic_exit(struct adaptived_cause * const cse)
{
	struct periodic_opts *opts = (struct periodic_opts *)adaptived_cause_get_data(cse);

	if (opts)
		free(opts);
}
