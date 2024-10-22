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
 * Test to exercise the recursive kill cgroup effect
 *
 * Note that this test creates a fake cgroup hierarchy directly in the
 * tests/ftests directory and operates on it.
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

static const char * const cgroup_dirs[] = {
	"./test023cgroup",
	"./test023cgroup/child1",
	"./test023cgroup/child2",
	"./test023cgroup/child3"
};
static const int cgroup_dirs_cnt = ARRAY_SIZE(cgroup_dirs);

static const char * const cgroup_files[] = {
	"./test023cgroup/cgroup.procs",
	"./test023cgroup/child1/cgroup.procs",
	"./test023cgroup/child2/cgroup.procs",
	"./test023cgroup/child3/cgroup.procs",
};
static const int cgroup_files_cnt = ARRAY_SIZE(cgroup_files);

#define CHILD1_PID_COUNT 6
static pid_t child1_pids[CHILD1_PID_COUNT];

#define CHILD3_PID_COUNT 3
static pid_t child3_pids[CHILD3_PID_COUNT];

static void _write_cgroup_procs(const char * const file, const pid_t * const pids, int pids_cnt)
{
	char buf[1024] = { '\0' };
	char pid[16];
	int i;

	for (i = 0; i < pids_cnt; i++) {
		memset(pid, 0, sizeof(pid));
		sprintf(pid, "%d\n", pids[i]);
		strcat(buf, pid);
	}

	write_file(file, buf);
}

static void write_cgroup_procs(void)
{
	int i, pids_cnt;
	pid_t *pids;

	for (i = 0; i < cgroup_files_cnt; i++) {
		if (i == 1) {
			pids = child1_pids;
			pids_cnt = CHILD1_PID_COUNT;
		} else if (i == 3) {
			pids = child3_pids;
			pids_cnt = CHILD3_PID_COUNT;
		} else {
			pids = NULL;
			pids_cnt = 0;
		}

		_write_cgroup_procs(cgroup_files[i], pids, pids_cnt);
	}
}

int main(int argc, char *argv[])
{
	char config_path[FILENAME_MAX];
	struct adaptived_ctx *ctx = NULL;
	int ret;

	snprintf(config_path, FILENAME_MAX - 1, "%s/023-effect-kill_cgroup_recursive.json",
		 argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	ctx = adaptived_init(config_path);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	ret = create_dirs(cgroup_dirs, cgroup_dirs_cnt);
	if (ret)
		goto err;

	ret = create_pids(child1_pids, CHILD1_PID_COUNT, DEFAULT_SLEEP);
	if (ret)
		goto err;

	ret = create_pids(child3_pids, CHILD3_PID_COUNT, DEFAULT_SLEEP);
	if (ret)
		goto err;

	write_cgroup_procs();

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 2);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 8000);
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
		adaptived_err("Test 023 returned: %d, expected: %d\n", ret, EXPECTED_RET);
		goto err;
	}

	wait_for_pids(child1_pids, CHILD1_PID_COUNT);
	wait_for_pids(child3_pids, CHILD3_PID_COUNT);

	ret = verify_pids_were_killed(child1_pids, CHILD1_PID_COUNT);
	if (ret)
		goto err;
	ret = verify_pids_were_killed(child3_pids, CHILD3_PID_COUNT);
	if (ret)
		goto err;

	delete_files(cgroup_files, cgroup_files_cnt);
	delete_dirs(cgroup_dirs, cgroup_dirs_cnt);
	adaptived_release(&ctx);

	return AUTOMAKE_PASSED;

err:
	delete_files(cgroup_files, cgroup_files_cnt);
	delete_dirs(cgroup_dirs, cgroup_dirs_cnt);
	adaptived_release(&ctx);

	return AUTOMAKE_HARD_ERROR;
}
