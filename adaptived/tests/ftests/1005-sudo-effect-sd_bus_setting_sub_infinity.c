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
 * Test to add to a cgroup integer setting to property set to infinity via sd_bus
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

static const char * const cgroup_slice_name = "sudo1005.slice";
static long long expected_value;
static const char * const old_conf_file = "/etc/systemd/system.control/sudo1005.slice.d/50-MemoryMax.conf";
static const char * const old_unit_file_dir = "/etc/systemd/system.control/sudo1005.slice.d";

int main(int argc, char *argv[])
{
	char *cgrp_path = NULL, *cgrp_file = NULL;
	char config_path[FILENAME_MAX];
	struct adaptived_ctx *ctx = NULL;
	int ret, version;
        int br;
        char buf[FILENAME_MAX];
        long long memtotal;
        FILE *fp;
        char *line = NULL;
        size_t len = 0;

	/*
	 * systemd will read from old conf files rather than the cgroup sysfs.  Therefore
	 * delete them
	 */
	delete_file(old_conf_file);
	delete_dir(old_unit_file_dir);

	snprintf(config_path, FILENAME_MAX - 1, "%s/1005-sudo-effect-sd_bus_setting_sub_infinity.json", argv[1]);
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
	if (ret != EXPECTED_RET)
		goto err;
        fp = fopen("/proc/meminfo", "r");
        if (fp == NULL) {
                adaptived_err("Can't open top file %s\n", "/proc/meminfo");
                return -errno;
        }
        br = getline(&line, &len, fp);
        fclose(fp);
        if (br < 0 || !line) {
                adaptived_err("Read of %s failed.\n", "/proc/meminfo");
                return -errno;
        }
        line[strcspn(line, "\n")] = '\0';
        memset(buf, 0, FILENAME_MAX);
        strcpy(buf, line);
        /* MemTotal:       527700340 kB */
        sscanf(buf, "MemTotal:       %lld kB", &memtotal);
        memtotal *= 1024;

        expected_value = memtotal - 4096;

	ret = get_cgroup_version(&version);
	if (ret < 0)
		goto err;

	if (version == 1)
		ret = build_cgroup_path("memory", cgroup_slice_name, &cgrp_path);
	else if (version == 2)
		ret = build_cgroup_path(NULL, cgroup_slice_name, &cgrp_path);
	if (ret < 0)
		goto err;

	ret = build_systemd_memory_max_file(cgrp_path, &cgrp_file);
	if (ret < 0)
		goto err;

	ret = verify_ll_file(cgrp_file, expected_value);
	if (ret)
		goto err;

	adaptived_release(&ctx);
	stop_transient(cgroup_slice_name);
	if (cgrp_file)
		free(cgrp_file);
	if (cgrp_path)
		free(cgrp_path);

	return AUTOMAKE_PASSED;

err:
	adaptived_release(&ctx);
	stop_transient(cgroup_slice_name);
	if (cgrp_file)
		free(cgrp_file);
	if (cgrp_path)
		free(cgrp_path);

	return AUTOMAKE_HARD_ERROR;
}
