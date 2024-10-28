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
 * Test for the setting cause
 *
 */

#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>

#include <adaptived.h>

#include "ftests.h"

#define EXPECTED_RET -65

static const char * const setting_file = "038-cause-setting.setting";
static int ctr = 0;

typedef int (*adaptived_injection_function)(struct adaptived_ctx * const ctx);
extern int adaptived_register_injection_function(struct adaptived_ctx * const ctx,
					      adaptived_injection_function fn);

static int value[] = { 1, 2, 3, 4, 5, 6, };

static int inject(struct adaptived_ctx * const ctx)
{
	int fd, ret = 0;
	char buf[16];
	ssize_t w;

	if (ctr >= ARRAY_SIZE(value))
		return -E2BIG;

	fd = open(setting_file, O_TRUNC | O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
	if (fd <= 0)
		return -EINVAL;

	memset(buf, 0, sizeof(buf));

	w = sprintf(buf, "%d", value[ctr]);
	if (w <= 0 || w >= 16) {
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

	snprintf(config_path, FILENAME_MAX - 1, "%s/038-cause-setting_ll_eq.json", argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	ctx = adaptived_init(config_path);
	if (!ctx)
		goto err;

	ret = adaptived_register_injection_function(ctx, inject);
	if (ret)
		goto err;

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 6);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 4000);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_SKIP_SLEEP, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_DEBUG);
	if (ret)
		goto err;

	ret = adaptived_loop(ctx, true);
	if (ret != EXPECTED_RET)
		goto err;

	adaptived_release(&ctx);
	(void)remove(setting_file);
	return AUTOMAKE_PASSED;

err:
	if (ctx)
		adaptived_release(&ctx);

	(void)remove(setting_file);
	return AUTOMAKE_HARD_ERROR;
}
