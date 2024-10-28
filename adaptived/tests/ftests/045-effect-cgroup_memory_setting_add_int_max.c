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
 * Test to add to a cgroup integer setting
 *
 */

#include <stdbool.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <sys/sysinfo.h>

#include <adaptived.h>

#include "ftests.h"

#define EXPECTED_RET -ETIME
#define EXPECTED_RET_FAIL 1

static const char * const cgroup_file = "./test045.cgroup";
static const long long expected_value = 25;

int main(int argc, char *argv[])
{
	char config_path[FILENAME_MAX];
	struct adaptived_ctx *ctx = NULL;
	struct sysinfo info;
	sysinfo(&info);
	int ret;

	snprintf(config_path, FILENAME_MAX - 1, "%s/045-effect-cgroup_memory_setting_add_int_max.json", argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	ctx = adaptived_init(config_path);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	write_file(cgroup_file, "99999");

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 5);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 3000);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_SKIP_SLEEP, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_DEBUG);
	if (ret)
		goto err;

	ret = adaptived_loop(ctx, true);
	if (ret != EXPECTED_RET_FAIL) {
		adaptived_err("Test 045 returned: %d, expected: %d\n", ret, EXPECTED_RET_FAIL);
		goto err;
	}

	ret = verify_char_file(cgroup_file, "max");
	if (ret)
		goto err;

	adaptived_release(&ctx);

	adaptived_dbg("cgroup memory set & add to max test OK\n");


	snprintf(config_path, FILENAME_MAX - 1, "%s/045-effect-cgroup_memory_setting_set_add_int.json", argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	ctx = adaptived_init(config_path);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 3000);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_SKIP_SLEEP, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_DEBUG);
	if (ret)
		goto err;

	ret = adaptived_loop(ctx, true);
	if (ret != EXPECTED_RET) {
		adaptived_err("Test 045 returned: %d, expected: %d\n", ret, EXPECTED_RET);
		goto err;
	}

	ret = verify_int_file(cgroup_file, 25);
	if (ret)
		goto err;

	delete_file(cgroup_file);
	adaptived_release(&ctx);

	adaptived_dbg("cgroup memory set & add test OK\n");

	return AUTOMAKE_PASSED;

err:
	delete_file(cgroup_file);
	adaptived_release(&ctx);

	return AUTOMAKE_HARD_ERROR;
}
