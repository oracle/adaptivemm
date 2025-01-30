/*
 * Copyright (c) 2025, Oracle and/or its affiliates.
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
 * Test to verify all rules are properly loaded
 *
 */

#include <pthread.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>

#include <adaptived.h>

#include "ftests.h"

static void *adaptived_wrapper(void *arg)
{
	struct adaptived_ctx *ctx = arg;
	uintptr_t ret;

	ret = adaptived_loop(ctx, true);

	return (void *)ret;
}

int main(int argc, char *argv[])
{
	char config_path[FILENAME_MAX];
	pthread_t adaptived_thread;
	struct adaptived_ctx *ctx;
	uint32_t rule_cnt = 0;
	void *tret;
	int ret;

	snprintf(config_path, FILENAME_MAX - 1, "%s/070-rule-multiple_rules.json", argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	ctx = adaptived_init(config_path);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_DEBUG);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 1500);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 2);
	if (ret)
		goto err;

	ret = pthread_create(&adaptived_thread, NULL, &adaptived_wrapper, ctx);
	if (ret)
		goto err;

	/* wait for the adaptived loop to get up and running */
	sleep(1);

	ret = adaptived_get_attr(ctx, ADAPTIVED_ATTR_RULE_CNT, &rule_cnt);
	if (ret)
		goto err;
	if (rule_cnt != 5)
		goto err;

	pthread_join(adaptived_thread, &tret);

	if (tret != (void *)-ETIME)
		goto err;

	adaptived_release(&ctx);

	return AUTOMAKE_PASSED;

err:
	adaptived_release(&ctx);

	return AUTOMAKE_HARD_ERROR;
}
