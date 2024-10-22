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
 * Test for the PSI cause when full_total is equal to the specified duration/threshold
 *
 */

#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <adaptived.h>

#include "ftests.h"

#define EXPECTED_RET -43

static const char *pressure_file = "041-cause-pressure_equal.pressure";
static int ctr = 0;

typedef int (*adaptived_injection_function)(struct adaptived_ctx * const ctx);
extern int adaptived_register_injection_function(struct adaptived_ctx * const ctx,
					      adaptived_injection_function fn);

static const float some_avg10[]  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
static const float some_avg60[]  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
static const float some_avg300[]  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
static const long long some_total[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, };

static const float full_avg10[]  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
static const float full_avg60[]  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
static const float full_avg300[] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
/* The test only looks at full_total */
static const long long full_total[] = {100, 5678, 89278, 1234567890, 1234567890, 1234567890,
				       1234567890, 1234567900, 1234567950, 1234568000, };

static_assert(ARRAY_SIZE(some_avg10) == ARRAY_SIZE(some_avg60), "");
static_assert(ARRAY_SIZE(some_avg60) == ARRAY_SIZE(some_avg300), "");
static_assert(ARRAY_SIZE(some_avg300) == ARRAY_SIZE(some_total), "");
static_assert(ARRAY_SIZE(some_total) == ARRAY_SIZE(full_avg10), "");
static_assert(ARRAY_SIZE(full_avg10) == ARRAY_SIZE(full_avg60), "");
static_assert(ARRAY_SIZE(full_avg60) == ARRAY_SIZE(full_avg300), "");
static_assert(ARRAY_SIZE(full_avg300) == ARRAY_SIZE(full_total), "");

static int inject(struct adaptived_ctx * const ctx)
{
	int fd, ret = 0;
	char buf[1024];
	ssize_t w;

	if (ctr >= ARRAY_SIZE(some_avg10))
		return -E2BIG;

	fd = open(pressure_file, O_TRUNC | O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
	if (fd <= 0)
		return -EINVAL;

	memset(buf, 0, sizeof(buf));

	w = sprintf(buf, "some avg10=%.2f avg60=%.2f avg300=%.2f total=%lld\n"
		    "full avg10=%.2f avg60=%.2f avg300=%.2f total=%lld",
		    some_avg10[ctr], some_avg60[ctr], some_avg300[ctr], some_total[ctr],
		    full_avg10[ctr], full_avg60[ctr], full_avg300[ctr], full_total[ctr]);
	if (w <= 0 || w >= 1024) {
		ret = -errno;
		goto err;
	}

	w = write(fd, buf, strlen(buf));
	if (w <= 0)
		ret = -errno;

err:
	close(fd);
	ctr++;

	return ret;
}

int main(int argc, char *argv[])
{
	char config_path[FILENAME_MAX];
	struct adaptived_ctx *ctx;
	int ret;

	snprintf(config_path, FILENAME_MAX - 1, "%s/041-cause-pressure_equal.json", argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	ctx = adaptived_init(config_path);
	if (!ctx)
		goto err;

	ret = adaptived_register_injection_function(ctx, inject);
	if (ret)
		goto err;

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 10);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 1000);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_SKIP_SLEEP, 1);
	if (ret)
		goto err;

	ret = adaptived_loop(ctx, true);
	if (ret != EXPECTED_RET)
		goto err;

	adaptived_release(&ctx);
	(void)remove(pressure_file);
	return AUTOMAKE_PASSED;

err:
	if (ctx)
		adaptived_release(&ctx);

	(void)remove(pressure_file);
	return AUTOMAKE_HARD_ERROR;
}
