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
 * Test to exercise the cgroup setting by psi effect when the PSI is above
 * a certain threshold.  In this test, the cgroup setting is explicitly set.
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

typedef int (*adaptived_injection_function)(struct adaptived_ctx * const ctx);
extern int adaptived_register_injection_function(struct adaptived_ctx * const ctx,
					      adaptived_injection_function fn);

static int ctr = 0;

static const char * const cgroup_dirs[] = {
	"./test027cgroup",
	"./test027cgroup/child1",
	"./test027cgroup/child1/grandchild11/",
	"./test027cgroup/child1/grandchild12/",
	"./test027cgroup/child2",
	"./test027cgroup/child2/grandchild21/",
	"./test027cgroup/child3"
};
static const int cgroup_dirs_cnt = ARRAY_SIZE(cgroup_dirs);

static const char * const psi_files[] = {
	"./test027cgroup/cpu.pressure",
	"./test027cgroup/child1/cpu.pressure",
	"./test027cgroup/child1/grandchild11/cpu.pressure",
	"./test027cgroup/child1/grandchild12/cpu.pressure",
	"./test027cgroup/child2/cpu.pressure",
	"./test027cgroup/child2/grandchild21/cpu.pressure",
	"./test027cgroup/child3/cpu.pressure",
};
static const int psi_files_cnt = ARRAY_SIZE(psi_files);
static_assert(ARRAY_SIZE(cgroup_dirs) == ARRAY_SIZE(psi_files),
	      "cgroup_dirs_cnt should be the same size as psi_files_cnt");

static const float some_avg60[][ARRAY_SIZE(psi_files)] = {
	/*   ./, child1,  gc11,  gc12, child2,  gc21, child3 */
	{   1.0,    2.0,   3.0,   4.0,    5.0,   6.0,    7.0 }, /* child3 gets set */
	{   1.0,    2.0,   3.0,   4.0,    5.0,   6.0,    7.0 }, /* child3 gets set */
	{  10.0,    9.9,  11.1,   9.8,   10.0,  10.0,   10.0 }, /* gc11 gets set */
	{  25.0,   24.0,  23.0,  22.0,   31.0,  29.9,   29.9 }, /* child2 gets set */
};
static_assert(ARRAY_SIZE(some_avg60[0]) == ARRAY_SIZE(psi_files),
	      "some_avg60[] should be the same size as psi_files_cnt");

static const char * const cgroup_files[] = {
	"./test027cgroup/cpu.weight",
	"./test027cgroup/child1/cpu.weight",
	"./test027cgroup/child1/grandchild11/cpu.weight",
	"./test027cgroup/child1/grandchild12/cpu.weight",
	"./test027cgroup/child2/cpu.weight",
	"./test027cgroup/child2/grandchild21/cpu.weight",
	"./test027cgroup/child3/cpu.weight",
};
static const int cgroup_files_cnt = ARRAY_SIZE(cgroup_files);

static int cgroup_files_contents[] = {
	100,
	100,
	100,
	100,
	100,
	100,
	100
};
static_assert(ARRAY_SIZE(cgroup_files_contents) == ARRAY_SIZE(cgroup_files),
	      "cgroup file contents array must be same length as cgroup files array");

static int expected_cgroup_files_contents[] = {
	100,
	100,
	 75,
	100,
	 75,
	100,
	 75
};
static_assert(ARRAY_SIZE(expected_cgroup_files_contents) == ARRAY_SIZE(cgroup_files),
	      "expected cgroup file contents array must be same length as cgroup files array");

static void write_psi_files(int loop_cnt)
{
	char val[1024];
	int i;

	for (i = 0; i < psi_files_cnt; i++) {
		memset(val, '\0', sizeof(val));
		sprintf(val, "some avg10=0.00 avg60=%.2f avg300=0.00 total=1234\n"
			"full avg10=0.00 avg60=0.00 avg300=0.00 total=5678",
			some_avg60[loop_cnt][i]);

		write_file(psi_files[i], val);
	}
}

static void write_cgroup_files(void)
{
	char contents[1024];
	int i;

	for (i = 0; i < cgroup_files_cnt; i++) {
		memset(contents, '\0', sizeof(contents));
		sprintf(contents, "%d", cgroup_files_contents[i]);
		write_file(cgroup_files[i], contents);
	}
}

static int validate_files(void)
{
	int ret, i;

	for (i = 0; i < cgroup_files_cnt; i++) {
		ret = verify_int_file(cgroup_files[i], expected_cgroup_files_contents[i]);
		if (ret)
			return ret;
	}

	return 0;
}

static int inject(struct adaptived_ctx * const ctx)
{
	if (ctr >= ARRAY_SIZE(some_avg60))
		return -E2BIG;

	write_psi_files(ctr);
	ctr++;

	return 0;
}

int main(int argc, char *argv[])
{
	char config_path[FILENAME_MAX];
	struct adaptived_ctx *ctx = NULL;
	int ret;

	snprintf(config_path, FILENAME_MAX - 1, "%s/027-effect-cgroup_setting_by_psi_3.json",
		 argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	ctx = adaptived_init(config_path);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	ret = create_dirs(cgroup_dirs, cgroup_dirs_cnt);
	if (ret)
		goto err;

	write_cgroup_files();

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, ARRAY_SIZE(some_avg60));
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 10000);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_SKIP_SLEEP, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_INFO);
	if (ret)
		goto err;
	ret = adaptived_register_injection_function(ctx, inject);
	if (ret)
		goto err;

	ret = adaptived_loop(ctx, true);
	if (ret != EXPECTED_RET) {
		adaptived_err("Test 027 returned: %d, expected: %d\n", ret, EXPECTED_RET);
		goto err;
	}

	ret = validate_files();
	if (ret)
		goto err;

	delete_files(cgroup_files, cgroup_files_cnt);
	delete_files(psi_files, psi_files_cnt);
	delete_dirs(cgroup_dirs, cgroup_dirs_cnt);
	adaptived_release(&ctx);

	return AUTOMAKE_PASSED;

err:
	delete_files(cgroup_files, cgroup_files_cnt);
	delete_files(psi_files, psi_files_cnt);
	delete_dirs(cgroup_dirs, cgroup_dirs_cnt);
	adaptived_release(&ctx);

	return AUTOMAKE_HARD_ERROR;
}
