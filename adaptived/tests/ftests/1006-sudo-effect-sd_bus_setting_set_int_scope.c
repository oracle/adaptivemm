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
 * Test to set a cgroup integer setting
 *
 */

#include <stdbool.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include <adaptived.h>

#include "ftests.h"

#define EXPECTED_RET -ETIME

static const char * const cgroup_scope_name = "sudo1006.scope";
static const char * const cgroup_file =  "/sys/fs/cgroup/system.slice/sudo1006.scope/memory.low";
static const int expected_value = 89997312; /* Must be multiple of 4K */

int main(int argc, char *argv[])
{
	char config_path[FILENAME_MAX];
	struct adaptived_ctx *ctx = NULL;
	int ret;
	int i = 0;
	int max_retries = 10;

	snprintf(config_path, FILENAME_MAX - 1, "%s/1006-sudo-effect-sd_bus_setting_set_int_scope.json", argv[1]);
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
	ret = start_unit(cgroup_scope_name, NULL);
	if (ret) {
		adaptived_err("Failed to create scope: %s, ret=%d\n", cgroup_scope_name, ret);
		goto err;
	}
	ret = adaptived_loop(ctx, true);
	if (ret != EXPECTED_RET)
		goto err;

	ret = verify_int_file(cgroup_file, expected_value);
	if (ret)
		goto err;

	adaptived_release(&ctx);
	stop_transient(cgroup_scope_name);
	return AUTOMAKE_PASSED;

err:
	adaptived_release(&ctx);
	stop_transient(cgroup_scope_name);

	return AUTOMAKE_HARD_ERROR;
}
