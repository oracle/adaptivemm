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
 * Test to exercise the kill cgroup by psi effect
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
	"./test024cgroup",
	"./test024cgroup/child1",
	"./test024cgroup/child1/grandchild11/",
	"./test024cgroup/child2",
	"./test024cgroup/child3"
};
static const int cgroup_dirs_cnt = ARRAY_SIZE(cgroup_dirs);

static const char * const psi_files[] = {
	"./test024cgroup/cpu.pressure",
	"./test024cgroup/child1/cpu.pressure",
	"./test024cgroup/child1/grandchild11/cpu.pressure",
	"./test024cgroup/child2/cpu.pressure",
	"./test024cgroup/child3/cpu.pressure",
};
static const int psi_files_cnt = ARRAY_SIZE(psi_files);
static_assert(ARRAY_SIZE(cgroup_dirs) == ARRAY_SIZE(psi_files),
	      "cgroup_dirs_cnt should be the same size as psi_files_cnt");

static const float psi_some10[] = {
	5.7,
	9.2,
	18.6,
	25.8,
	13.7,
};
static_assert(ARRAY_SIZE(psi_some10) == ARRAY_SIZE(psi_files),
	      "psi_some10[] should be the same size as psi_files_cnt");

static const char * const cgroup_files[] = {
	"./test024cgroup/cgroup.procs",
	"./test024cgroup/child1/cgroup.procs",
	"./test024cgroup/child1/grandchild11/cgroup.procs",
	"./test024cgroup/child2/cgroup.procs",
	"./test024cgroup/child3/cgroup.procs",
};
static const int cgroup_files_cnt = ARRAY_SIZE(cgroup_files);

#define GRANDCHILD11_PID_COUNT 2
static pid_t grandchild11_pids[GRANDCHILD11_PID_COUNT];

#define CHILD2_PID_COUNT 5
static pid_t child2_pids[CHILD2_PID_COUNT];

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
		if (i == 2) {
			pids = grandchild11_pids;
			pids_cnt = GRANDCHILD11_PID_COUNT;
		} else if (i == 3) {
			pids = child2_pids;
			pids_cnt = CHILD2_PID_COUNT;
		} else {
			pids = NULL;
			pids_cnt = 0;
		}

		_write_cgroup_procs(cgroup_files[i], pids, pids_cnt);
	}
}

static void write_psi_files(void)
{
	char val[1024];
	int i;

	for (i = 0; i < psi_files_cnt; i++) {
		memset(val, '\0', sizeof(val));
		sprintf(val, "some avg10=%.2f avg60=0.00 avg300=0.00 total=1234\n"
			"full avg10=0.00 avg60=0.00 avg300=0.00 total=5678", psi_some10[i]);

		write_file(psi_files[i], val);
	}
}

int main(int argc, char *argv[])
{
	char config_path[FILENAME_MAX];
	struct adaptived_ctx *ctx = NULL;
	int ret;

	snprintf(config_path, FILENAME_MAX - 1, "%s/024-effect-kill_cgroup_by_psi.json",
		 argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	ctx = adaptived_init(config_path);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	ret = create_dirs(cgroup_dirs, cgroup_dirs_cnt);
	if (ret)
		goto err;

	ret = create_pids(grandchild11_pids, GRANDCHILD11_PID_COUNT, 5);
	if (ret)
		goto err;

	ret = create_pids(child2_pids, CHILD2_PID_COUNT, DEFAULT_SLEEP);
	if (ret)
		goto err;

	write_cgroup_procs();
	write_psi_files();

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
		adaptived_err("Test 024 returned: %d, expected: %d\n", ret, EXPECTED_RET);
		goto err;
	}

	wait_for_pids(grandchild11_pids, GRANDCHILD11_PID_COUNT);
	wait_for_pids(child2_pids, CHILD2_PID_COUNT);

	ret = verify_pids_were_not_killed(grandchild11_pids, GRANDCHILD11_PID_COUNT);
	if (ret)
		goto err;
	ret = verify_pids_were_killed(child2_pids, CHILD2_PID_COUNT);
	if (ret)
		goto err;
	kill_pids(grandchild11_pids, GRANDCHILD11_PID_COUNT);

	delete_files(cgroup_files, cgroup_files_cnt);
	delete_files(psi_files, psi_files_cnt);
	delete_dirs(cgroup_dirs, cgroup_dirs_cnt);
	adaptived_release(&ctx);

	return AUTOMAKE_PASSED;

err:
	kill_pids(grandchild11_pids, GRANDCHILD11_PID_COUNT);
	delete_files(cgroup_files, cgroup_files_cnt);
	delete_files(psi_files, psi_files_cnt);
	delete_dirs(cgroup_dirs, cgroup_dirs_cnt);
	adaptived_release(&ctx);

	return AUTOMAKE_HARD_ERROR;
}
