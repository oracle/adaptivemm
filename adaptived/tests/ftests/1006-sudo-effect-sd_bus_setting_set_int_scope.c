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
static const char * const cgroup_slice_scope_name = "system.slice/sudo1006.scope";
static const int expected_value = 89997312; /* Must be multiple of 4K */
static const char * const old_conf_file = "/etc/systemd/system.control/sudo1006.slice.d/50-MemoryMax.conf";
static const char * const old_unit_file_dir = "/etc/systemd/system.control/sudo1006.slice.d";

int main(int argc, char *argv[])
{
	char *cgrp_path = NULL, *cgrp_file = NULL;
	char config_path[FILENAME_MAX];
	struct adaptived_ctx *ctx = NULL;
	int ret, version;

	/*
	 * systemd will read from old conf files rather than the cgroup sysfs.  Therefore
	 * delete them
	 */
	delete_file(old_conf_file);
	delete_dir(old_unit_file_dir);

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

	ret = get_cgroup_version(&version);
	if (ret < 0)
		goto err;

	if (version == 1)
		ret = build_cgroup_path("memory", cgroup_slice_scope_name, &cgrp_path);
	else if (version == 2)
		ret = build_cgroup_path(NULL, cgroup_slice_scope_name, &cgrp_path);
	if (ret < 0)
		goto err;

	ret = build_systemd_memory_max_file(cgrp_path, &cgrp_file);
	if (ret < 0)
		goto err;

	ret = verify_int_file(cgrp_file, expected_value);
	if (ret)
		goto err;

	adaptived_release(&ctx);
	stop_transient(cgroup_scope_name);
	if (cgrp_file)
		free(cgrp_file);
	if (cgrp_path)
		free(cgrp_path);

	return AUTOMAKE_PASSED;

err:
	adaptived_release(&ctx);
	stop_transient(cgroup_scope_name);
	if (cgrp_file)
		free(cgrp_file);
	if (cgrp_path)
		free(cgrp_path);

	return AUTOMAKE_HARD_ERROR;
}
