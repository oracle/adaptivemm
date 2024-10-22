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
 * Test to run the adaptived loop asynchronously
 *
 */

#include <pthread.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <time.h>

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
	struct timespec sleep, start, end;
	struct adaptived_rule_stats stats;
	char config_path[FILENAME_MAX];
	pthread_t adaptived_thread;
	struct adaptived_ctx *ctx;
	uint32_t loop_cnt;
	double time_diff;
	void *tret;
	int ret;

	snprintf(config_path, FILENAME_MAX - 1, "%s/013-adaptived-asynchronous_loop.json", argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	ctx = adaptived_init(config_path);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 3);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 1000);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_DEBUG);
	if (ret)
		goto err;

	clock_gettime(CLOCK_MONOTONIC, &start);
	ret = pthread_create(&adaptived_thread, NULL, &adaptived_wrapper, ctx);
	if (ret)
		goto err;

	/* 
	 * Wait a short amount of time.  This should either happen while
	 * the first pass through the adaptived_loop() is running, or (most
	 * likely), it will happen during the first sleep.
	 */
	sleep.tv_sec = 0;
	sleep.tv_nsec = 100000000LL;
	ret = nanosleep(&sleep, NULL);

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 3000);
	if (ret)
		goto err;

	pthread_join(adaptived_thread, &tret);
	clock_gettime(CLOCK_MONOTONIC, &end);

	time_diff = time_elapsed(&start, &end);

	/*
	 * This test should take approximately 4 seconds to run:
	 * 	adaptived main loop (loop cnt = 1)
	 * 	sleep 1 second
	 * 	while sleeping, increase interval to 3 seconds
	 * 	adaptived main loop (loop cnt = 2)
	 * 	sleep 3 seconds
	 * 	adaptived main loop (loop cnt = 3)
	 * 	loop exists with -ETIME
	 */
	if (time_diff > 4.1 || time_diff < 3.9) {
		adaptived_err("Expected test to take 4 seconds to run, but it took %f\n", time_diff);
		goto err;
	}

	if (tret != (void *)-ETIME) {
		adaptived_err("Expected the adaptived_loop() to return -ETIME, but it returned: %d\n",
			   tret);
		goto err;
	}

	ret = adaptived_get_rule_stats(ctx, "Empty set of rules.  Will never trigger", &stats);
	if (ret)
		goto err;
	if (stats.loops_run_cnt != 3) {
		adaptived_err("Expected 3 loops, but %d loops ran\n", loop_cnt);
		goto err;
	}

	adaptived_release(&ctx);
	return AUTOMAKE_PASSED;

err:
	adaptived_release(&ctx);
	return AUTOMAKE_HARD_ERROR;
}
