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
 * Test to exercise the kill cgroup effect
 *
 */

#include <sys/stat.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include <adaptived.h>

#include "ftests.h"

#define EXPECTED_RET -ETIME

static const char * const cgroup_dir = "./test022cgroup";
static const char * const cgroup_file = "./test022cgroup/cgroup.procs";

#define PID_COUNT 6
static pid_t pids[PID_COUNT];

static void write_cgroup_procs(void)
{
	char buf[1024] = { '\0' };
	char pid[16];
	int i;

	for (i = 0; i < PID_COUNT; i++) {
		memset(pid, 0, sizeof(pid));
		sprintf(pid, "%d\n", pids[i]);
		strcat(buf, pid);
	}

	write_file(cgroup_file, buf);
}

int main(int argc, char *argv[])
{
	char config_path[FILENAME_MAX];
	struct adaptived_ctx *ctx = NULL;
	int ret;

	snprintf(config_path, FILENAME_MAX - 1, "%s/022-effect-kill_cgroup.json", argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	ctx = adaptived_init(config_path);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	ret = create_dir(cgroup_dir);
	if (ret)
		goto err;
	ret = create_pids(pids, PID_COUNT, DEFAULT_SLEEP);
	if (ret)
		goto err;
	write_cgroup_procs();

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 2);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 7000);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_SKIP_SLEEP, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_INFO);
	if (ret)
		goto err;

	ret = adaptived_loop(ctx, true);
	if (ret != EXPECTED_RET) {
		adaptived_err("Test 022 returned: %d, expected: %d\n", ret, EXPECTED_RET);
		goto err;
	}

	wait_for_pids(pids, PID_COUNT);

	ret = verify_pids_were_killed(pids, PID_COUNT);
	if (ret)
		goto err;

	delete_file(cgroup_file);
	rmdir(cgroup_dir);
	adaptived_release(&ctx);

	return AUTOMAKE_PASSED;

err:
	delete_file(cgroup_file);
	rmdir(cgroup_dir);
	adaptived_release(&ctx);

	return AUTOMAKE_HARD_ERROR;
}
