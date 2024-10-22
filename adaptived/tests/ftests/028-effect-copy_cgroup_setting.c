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
 * Test to copy a cgroup setting from one file to another.
 *
 */

#include <stdbool.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>

#include <adaptived.h>

#include "ftests.h"

#define EXPECTED_RET -ETIME

static const char * const from_cgroup_file = "./test028_from.cgroup";
static const char * const to_cgroup_file = "./test028_to.cgroup";
static const int expected_value = 1024000;
static const char * const expected_value_str = "This was written by the cgroup_setting effect";

int main(int argc, char *argv[])
{
	char config_path[FILENAME_MAX];
	struct adaptived_ctx *ctx = NULL;
	int ret;

	snprintf(config_path, FILENAME_MAX - 1, "%s/028-effect-copy_cgroup_setting.json", argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	/* test int */
	ctx = adaptived_init(config_path);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	write_file(from_cgroup_file, "1024000");
	write_file(to_cgroup_file, "999999");

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 2000);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_SKIP_SLEEP, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_WARNING);
	if (ret)
		goto err;

	ret = adaptived_loop(ctx, true);
	if (ret != EXPECTED_RET) {
		adaptived_err("Test 028 returned: %d, expected: %d\n", ret, EXPECTED_RET);
		goto err;
	}

	ret = verify_int_file(to_cgroup_file, expected_value);
	if (ret)
		goto err;

	delete_file(from_cgroup_file);
	delete_file(to_cgroup_file);
	adaptived_release(&ctx);

	/* test zero value */
	ctx = adaptived_init(config_path);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	ctx = adaptived_init(config_path);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	write_file(from_cgroup_file, "0");
	write_file(to_cgroup_file, "1024000");

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 2000);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_SKIP_SLEEP, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_WARNING);
	if (ret)
		goto err;

	ret = adaptived_loop(ctx, true);
	if (ret != EXPECTED_RET) {
		adaptived_err("Test 028 returned: %d, expected: %d\n", ret, EXPECTED_RET);
		goto err;
	}

	ret = verify_int_file(to_cgroup_file, expected_value);
	if (ret)
		goto err;

	delete_file(from_cgroup_file);
	delete_file(to_cgroup_file);
	adaptived_release(&ctx);

	/* test string value */
	ctx = adaptived_init(config_path);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	write_file(from_cgroup_file, expected_value_str);
	write_file(to_cgroup_file, "999999");

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 2000);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_SKIP_SLEEP, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_WARNING);
	if (ret)
		goto err;

	ret = adaptived_loop(ctx, true);
	if (ret != EXPECTED_RET) {
		adaptived_err("Test 028 returned: %d, expected: %d\n", ret, EXPECTED_RET);
		goto err;
	}

	ret = verify_char_file(to_cgroup_file, expected_value_str);
	if (ret)
		goto err;

	delete_file(from_cgroup_file);
	delete_file(to_cgroup_file);
	adaptived_release(&ctx);

	return AUTOMAKE_PASSED;

err:
	delete_file(from_cgroup_file);
	delete_file(to_cgroup_file);
	adaptived_release(&ctx);

	return AUTOMAKE_HARD_ERROR;
}
