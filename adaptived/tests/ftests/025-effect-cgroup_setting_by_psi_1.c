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
 * Test to exercise the cgroup setting by psi effect when the PSI is below
 * a certain threshold
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
	"./test025cgroup",
	"./test025cgroup/child1",
	"./test025cgroup/child1/grandchild11/",
	"./test025cgroup/child1/grandchild12/",
	"./test025cgroup/child2",
	"./test025cgroup/child2/grandchild21/",
	"./test025cgroup/child3"
};
static const int cgroup_dirs_cnt = ARRAY_SIZE(cgroup_dirs);

static const char * const psi_files[] = {
	"./test025cgroup/memory.pressure",
	"./test025cgroup/child1/memory.pressure",
	"./test025cgroup/child1/grandchild11/memory.pressure",
	"./test025cgroup/child1/grandchild12/memory.pressure",
	"./test025cgroup/child2/memory.pressure",
	"./test025cgroup/child2/grandchild21/memory.pressure",
	"./test025cgroup/child3/memory.pressure",
};
static const int psi_files_cnt = ARRAY_SIZE(psi_files);
static_assert(ARRAY_SIZE(cgroup_dirs) == ARRAY_SIZE(psi_files),
	      "cgroup_dirs_cnt should be the same size as psi_files_cnt");

static const float psi_full300[][ARRAY_SIZE(psi_files)] = {
	/*   ./, child1,  gc11,  gc12, child2,  gc21, child3 */
	{   1.0,    2.0,   3.0,   4.0,    5.0,   6.0,    7.0 }, /* root gets subtracted */
	{  10.0,    9.9,   9.9,   9.8,   10.0,  10.0,   10.0 }, /* gc12 gets subtracted */
	{  10.0,    9.9,   9.9,   9.8,   10.0,  10.0,   10.0 }, /* gc12 gets subtracted */
	{  25.0,   24.0,  23.0,  22.0,   21.0,  29.9,   29.9 }, /* child2 gets subtracted */
	{  35.0,   34.0,  33.0,   1.0,   35.0,   7.0,   39.9 }, /* gc21 gets subtracted: see note */

	/*
	 * note - in the last loop, gc21 should be subtracted (and not gc12) because gc12 has
	 * reached its limit
	 */
};
static_assert(ARRAY_SIZE(psi_full300[0]) == ARRAY_SIZE(psi_files),
	      "psi_full300[] should be the same size as psi_files_cnt");

static const char * const cgroup_files[] = {
	"./test025cgroup/memory.high",
	"./test025cgroup/child1/memory.high",
	"./test025cgroup/child1/grandchild11/memory.high",
	"./test025cgroup/child1/grandchild12/memory.high",
	"./test025cgroup/child2/memory.high",
	"./test025cgroup/child2/grandchild21/memory.high",
	"./test025cgroup/child3/memory.high",
};
static const int cgroup_files_cnt = ARRAY_SIZE(cgroup_files);

static int cgroup_files_contents[] = {
	1000,
	1000,
	1000,
	1000,
	1000,
	1000,
	1000
};
static const int cgroup_files_contents_cnt = ARRAY_SIZE(cgroup_files_contents);
static_assert(ARRAY_SIZE(cgroup_files_contents) == ARRAY_SIZE(cgroup_files),
	      "cgroup file contents array must be same length as cgroup files array");

static int expected_cgroup_files_contents[] = {
	999,
	1000,
	1000,
	998,
	999,
	999,
	1000
};
static const int expected_cgroup_files_contents_cnt = ARRAY_SIZE(expected_cgroup_files_contents);
static_assert(ARRAY_SIZE(expected_cgroup_files_contents) == ARRAY_SIZE(cgroup_files),
	      "expected cgroup file contents array must be same length as cgroup files array");

static void write_psi_files(int loop_cnt)
{
	char val[1024];
	int i;

	for (i = 0; i < psi_files_cnt; i++) {
		memset(val, '\0', sizeof(val));
		sprintf(val, "some avg10=0.00 avg60=0.00 avg300=0.00 total=1234\n"
			"full avg10=0.00 avg60=0.00 avg300=%.2f total=5678",
			psi_full300[loop_cnt][i]);

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
	if (ctr >= ARRAY_SIZE(psi_full300))
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

	snprintf(config_path, FILENAME_MAX - 1, "%s/025-effect-cgroup_setting_by_psi_1.json",
		 argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	ctx = adaptived_init(config_path);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	ret = create_dirs(cgroup_dirs, cgroup_dirs_cnt);
	if (ret)
		goto err;

	write_cgroup_files();

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, ARRAY_SIZE(psi_full300));
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 8000);
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
		adaptived_err("Test 025 returned: %d, expected: %d\n", ret, EXPECTED_RET);
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
