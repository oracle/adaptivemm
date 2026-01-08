/*
 * Copyright (c) 2024-2025, Oracle and/or its affiliates.
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
 * Test to exercise the cgroup_data cause
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
	"./test071cgroup",
	"./test071cgroup/child1",
	"./test071cgroup/child1/grandchild11/",
	"./test071cgroup/child2",
	"./test071cgroup/child3"
};
static const int cgroup_dirs_cnt = ARRAY_SIZE(cgroup_dirs);

static const char * const cgroup_files[] = {
	"./test071cgroup/memory.current",
	"./test071cgroup/cpu.uclamp.min",
	"./test071cgroup/cpuset.cpus.effective",
	"./test071cgroup/cpu.max",

	"./test071cgroup/child1/memory.current",
	"./test071cgroup/child1/cpu.uclamp.min",
	"./test071cgroup/child1/cpuset.cpus.effective",
	"./test071cgroup/child1/cpu.max",

	"./test071cgroup/child1/grandchild11/memory.current",
	"./test071cgroup/child1/grandchild11/cpu.uclamp.min",
	"./test071cgroup/child1/grandchild11/cpuset.cpus.effective",
	"./test071cgroup/child1/grandchild11/cpu.max",

	"./test071cgroup/child2/memory.current",
	"./test071cgroup/child2/cpu.uclamp.min",
	"./test071cgroup/child2/cpuset.cpus.effective",
	"./test071cgroup/child2/cpu.max",

	"./test071cgroup/child3/memory.current",
	"./test071cgroup/child3/cpu.uclamp.min",
	"./test071cgroup/child3/cpuset.cpus.effective",
	"./test071cgroup/child3/cpu.max",
};
static const int cgroup_files_cnt = ARRAY_SIZE(cgroup_files);

/*
 * Write the file contents as strings since that's how adaptived will read out
 * the data.  adaptived will then try to determine the correct data type.
 */
static const char * const file_contents[] = {
	"4096000",
	"1.234",
	"1-2,4-7,10,12",
	"max 100000",

	"8192000",
	"3.456",
	"1-20",
	"10000 100000",

	"1024000",
	"0.000",
	"1,3,5,7,9,11",
	"250000 1000000",

	"0",
	"9.876",
	"5-8",
	"max 1000000",

	"12345678",
	"4.680",
	"8,10",
	"500000 1000000",
};
static_assert(ARRAY_SIZE(file_contents) == ARRAY_SIZE(cgroup_files),
	      "file_contents should be the same size as cgroup_files_cnt");


static void write_cgroup_files(void)
{
	int i;

	for (i = 0; i < cgroup_files_cnt; i++)
		write_file(cgroup_files[i], file_contents[i]);
}

int main(int argc, char *argv[])
{
	char config_path[FILENAME_MAX], expected_file[FILENAME_MAX];
	struct adaptived_ctx *ctx = NULL;
	int ret;

	snprintf(config_path, FILENAME_MAX - 1, "%s/071-cause-cgroup_data.json",
		 argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	ctx = adaptived_init(config_path);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	ret = create_dirs(cgroup_dirs, cgroup_dirs_cnt);
	if (ret)
		goto err;

	write_cgroup_files();

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 1234);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_SKIP_SLEEP, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_EMERG);
	if (ret)
		goto err;

	ret = adaptived_loop(ctx, true);
	if (ret != EXPECTED_RET) {
		adaptived_err("Test 071 returned: %d, expected: %d\n", ret, EXPECTED_RET);
		goto err;
	}

	adaptived_release(&ctx);
	ctx = NULL;

	snprintf(expected_file, FILENAME_MAX - 1, "%s/071-cause-cgroup_data.expected", argv[1]);
	expected_file[FILENAME_MAX - 1] = '\0';

	ret = compare_files_unsorted("071-cause-cgroup_data.out", expected_file);
	if (ret)
		goto err;

	delete_file("071-cause-cgroup_data.out");
	delete_files(cgroup_files, cgroup_files_cnt);
	delete_dirs(cgroup_dirs, cgroup_dirs_cnt);

	return AUTOMAKE_PASSED;

err:
	delete_file("071-cause-cgroup_data.out");
	delete_files(cgroup_files, cgroup_files_cnt);
	delete_dirs(cgroup_dirs, cgroup_dirs_cnt);
	if (ctx)
		adaptived_release(&ctx);

	return AUTOMAKE_HARD_ERROR;
}
