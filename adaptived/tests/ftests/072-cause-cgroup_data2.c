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
	"./test072cgroup",
	"./test072cgroup/child1",
	"./test072cgroup/child1/grandchild11/",
	"./test072cgroup/child2",
	"./test072cgroup/child3"
};
static const int cgroup_dirs_cnt = ARRAY_SIZE(cgroup_dirs);

static const char * const cgroup_files[] = {
	"./test072cgroup/memory.current",
	"./test072cgroup/cpu.uclamp.min",
	"./test072cgroup/cpuset.cpus.effective",
	"./test072cgroup/cpu.max",

	"./test072cgroup/child1/memory.current",
	"./test072cgroup/child1/cpu.uclamp.min",
	/* Intentionally omit cpuset.cpus.effective from the child1 cgroup */
	"./test072cgroup/child1/cpu.max",

	"./test072cgroup/child1/grandchild11/memory.current",
	"./test072cgroup/child1/grandchild11/cpu.uclamp.min",
	"./test072cgroup/child1/grandchild11/cpuset.cpus.effective",
	/* Intentionally omit cpu.max from grandchild11 */

	"./test072cgroup/child2/memory.current",
	"./test072cgroup/child2/cpu.uclamp.min",
	"./test072cgroup/child2/cpuset.cpus.effective",
	"./test072cgroup/child2/cpu.max",

	/* Intentionally omit memory.current from child3 */
	"./test072cgroup/child3/cpu.uclamp.min",
	"./test072cgroup/child3/cpuset.cpus.effective",
	"./test072cgroup/child3/cpu.max",
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
	/* file omitted */
	"10000 100000",

	"1024000",
	"0.000",
	"1,3,5,7,9,11",
	/* file omitted */

	"0",
	"9.876",
	"5-8",
	"max 1000000",

	/* file omitted */
	"4.680",
	"8,10",
	"500000 1000000",
};
static_assert(ARRAY_SIZE(file_contents) == ARRAY_SIZE(cgroup_files),
	      "file_contents should be the same size as cgroup_files_cnt");

const char * const config_token_file = "072-cause-cgroup_data2.json.token";
const char * const config_file = "072-cause-cgroup_data2.json";
const char * const expected_token_file = "072-cause-cgroup_data2.expected.token";
const char * const expected_file = "072-cause-cgroup_data2.expected";
const char * const out_file = "072-cause-cgroup_data2.out";

static void write_cgroup_files(void)
{
	int i;

	for (i = 0; i < cgroup_files_cnt; i++)
		write_file(cgroup_files[i], file_contents[i]);
}

int main(int argc, char *argv[])
{
	char *ftests_path, *config_token_path = NULL, *config_path = NULL, *out_path = NULL,
	     *expected_token_path = NULL, *expected_path = NULL;
	struct adaptived_ctx *ctx = NULL;
	ssize_t ftests_path_len, cwd_len;
	char cwd[FILENAME_MAX];
	int ret;

	ftests_path = argv[1];
	ftests_path_len = strlen(ftests_path);

	getcwd(cwd, sizeof(cwd));
	cwd_len = strlen(cwd);

	config_token_path = calloc(sizeof(char),
				   (ftests_path_len + strlen(config_token_file) + 2));
	if (!config_token_path)
		goto err;
	sprintf(config_token_path, "%s/%s", ftests_path, config_token_file);

	config_path = calloc(sizeof(char),
			     (cwd_len + strlen(config_file) + 2));
	if (!config_path)
		goto err;
	sprintf(config_path, "%s/%s", cwd, config_file);

	out_path = calloc(sizeof(char), (cwd_len + strlen(out_file) + 2));
	if (!out_path)
		goto err;
	sprintf(out_path, "%s/%s", cwd, out_file);

	expected_token_path = calloc(sizeof(char),
				     (ftests_path_len + strlen(expected_token_file) + 2));
	if (!expected_token_path)
		goto err;
	sprintf(expected_token_path, "%s/%s", ftests_path, expected_token_file);

	expected_path = calloc(sizeof(char),
			       (cwd_len + strlen(expected_file) + 2));
	if (!expected_path)
		goto err;
	sprintf(expected_path, "%s/%s", cwd, expected_file);

	ret = parse_token_file(config_token_path, config_path);
	if (ret)
		goto err;

	ret = parse_token_file(expected_token_path, expected_path);
	if (ret)
		goto err;

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
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_SKIP_SLEEP, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_EMERG);
	if (ret)
		goto err;

	ret = adaptived_loop(ctx, true);
	if (ret != EXPECTED_RET) {
		adaptived_err("Test 072 returned: %d, expected: %d\n", ret, EXPECTED_RET);
		goto err;
	}

	adaptived_release(&ctx);
	ctx = NULL;

	ret = compare_files_unsorted(out_path, expected_path);
	if (ret)
		goto err;

	if (config_token_path)
		free(config_token_path);

	if (expected_token_path)
		free(expected_token_path);

	if (out_path) {
		delete_file(out_path);
		free(out_path);
	}

	if (config_path) {
		delete_file(config_path);
		free(config_path);
	}

	if (expected_path) {
		delete_file(expected_path);
		free(expected_path);
	}

	delete_files(cgroup_files, cgroup_files_cnt);
	delete_dirs(cgroup_dirs, cgroup_dirs_cnt);

	if (ctx)
		adaptived_release(&ctx);

	return AUTOMAKE_PASSED;

err:
	if (config_token_path)
		free(config_token_path);

	if (expected_token_path)
		free(expected_token_path);

	if (out_path) {
		delete_file(out_path);
		free(out_path);
	}

	if (config_path) {
		delete_file(config_path);
		free(config_path);
	}

	if (expected_path) {
		delete_file(expected_path);
		free(expected_path);
	}

	delete_files(cgroup_files, cgroup_files_cnt);
	delete_dirs(cgroup_dirs, cgroup_dirs_cnt);

	if (ctx)
		adaptived_release(&ctx);

	return AUTOMAKE_HARD_ERROR;
}
