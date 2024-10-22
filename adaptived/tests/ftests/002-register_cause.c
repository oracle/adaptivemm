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
 * Test to register a cause at runtime and utilize it
 *
 */

#include <json-c/json.h>
#include <errno.h>

#include <adaptived.h>

#include "ftests.h"

#define EXPECTED_RET -123

struct wait_cause_opts {
	int wait; /* number of loops to wait before returning success */
	int wait_cnt; /* number of loops we have waited */
};

int wait_cause_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval)
{
	struct wait_cause_opts *opts;
	int ret = 0;

	opts = malloc(sizeof(struct wait_cause_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}

	ret = adaptived_parse_int(args_obj, "wait", &opts->wait);
	if (ret)
		goto error;

	opts->wait_cnt = 0;

	ret = adaptived_cause_set_data(cse, (void *)opts);
	if (ret)
		goto error;

	return ret;

error:
	if (opts)
		free(opts);

	return ret;
}

int wait_cause_main(struct adaptived_cause * const cse, int time_since_last_run)
{
	struct wait_cause_opts *opts;

	opts = (struct wait_cause_opts *)adaptived_cause_get_data(cse);

	if (opts->wait_cnt >= opts->wait)
		return 1;

	opts->wait_cnt++;
	return 0;
}

void wait_cause_exit(struct adaptived_cause * const cse)
{
	struct wait_cause_opts *opts;

	opts = (struct wait_cause_opts *)adaptived_cause_get_data(cse);

	free(opts);
}

const struct adaptived_cause_functions wait_cause_fns = {
	wait_cause_init,
	wait_cause_main,
	wait_cause_exit,
};

int main(int argc, char *argv[])
{
	char config_path[FILENAME_MAX];
	struct adaptived_ctx *ctx;
	int ret;

	snprintf(config_path, FILENAME_MAX - 1, "%s/002-register_cause.json", argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	ctx = adaptived_init(config_path);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 7);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 1000);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_SKIP_SLEEP, 1);
	if (ret)
		goto err;

	ret = adaptived_register_cause(ctx, "wait_cause", &wait_cause_fns);
	if (ret)
		goto err;

	ret = adaptived_loop(ctx, true);
	if (ret != EXPECTED_RET)
		goto err;

	adaptived_release(&ctx);
	return AUTOMAKE_PASSED;

err:
	adaptived_release(&ctx);
	return AUTOMAKE_HARD_ERROR;
}
