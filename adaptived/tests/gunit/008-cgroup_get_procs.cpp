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
 * adaptived googletest for getting the processes in a cgroup
 */

#include <adaptived-utils.h>
#include <adaptived.h>
#include <ftw.h>

#include "gtest/gtest.h"
#include "defines.h"

static const char *cgroup_path_1 = "test008-1";
static const char *cgroup_path_2 = "test008-2";

static const int pids_1[] = {
	1, 23, 345, 6789, 12345, 678901, 2345678, 90123456
};
static const int pids_1_cnt = ARRAY_SIZE(pids_1);

static const int pids_2[] = {
	9, 87, 654, 3210, 98765, 432109, 8765432, 10987654, 3210, 98,
	7, 65, 432, 1098, 76543, 210987, 6543210, 98765432, 1098, 76
};
static const int pids_2_cnt = ARRAY_SIZE(pids_2);

class CgroupGetProcsTest : public ::testing::Test {
	protected:

	static void write_cgroup_procs(const char * const cgroup, const int *pids, int pids_cnt) {
		char procs_file[1024] = { '\0' };
		size_t written;
		FILE *f;
		int i;

		sprintf(procs_file, "%s/cgroup.procs", cgroup);

		f = fopen(procs_file, "w");
		ASSERT_NE(f, nullptr);

		for (i = 0; i < pids_cnt; i++) {
			written = fprintf(f, "%d\n", pids[i]);
			ASSERT_GE(written, 1);
		}

		fclose(f);
	}

	void SetUp() override {
		int ret, i;
		FILE *f;

		ret = mkdir(cgroup_path_1, S_IRWXU | S_IRWXG | S_IRWXO);
		ASSERT_EQ(ret, 0);
		ret = mkdir(cgroup_path_2, S_IRWXU | S_IRWXG | S_IRWXO);
		ASSERT_EQ(ret, 0);

		write_cgroup_procs(cgroup_path_1, pids_1, pids_1_cnt);
		write_cgroup_procs(cgroup_path_2, pids_2, pids_2_cnt);
	}

	/*
	 * https://stackoverflow.com/questions/5467725/how-to-delete-a-directory-and-its-contents-in-posix-c
	 */
	static int unlink_cb(const char *fpath, const struct stat *sb, int typeflag,
		      struct FTW *ftwbuf) {
		return remove(fpath);
	}

	int rmrf(const char * const path)
{
		return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
	}

	void TearDown() override {
		int ret;

		ret = rmrf(cgroup_path_1);
		ASSERT_EQ(ret, 0);
		ret = rmrf(cgroup_path_2);
		ASSERT_EQ(ret, 0);
	}
};

TEST_F(CgroupGetProcsTest, InvalidParams)
{
	int ret, pids_cnt;
	pid_t *pids;

	ret = adaptived_cgroup_get_procs(NULL, &pids, &pids_cnt);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_cgroup_get_procs(cgroup_path_1, NULL, &pids_cnt);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_cgroup_get_procs(cgroup_path_1, &pids, NULL);
	ASSERT_EQ(ret, -EINVAL);
}

TEST_F(CgroupGetProcsTest, FewerThan16Pids)
{
	int ret, pids_cnt, i;
	pid_t *pids;

	ret = adaptived_cgroup_get_procs(cgroup_path_1, &pids, &pids_cnt);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(pids_cnt, pids_1_cnt);

	for (i = 0; i < pids_cnt; i++)
		ASSERT_EQ(pids[i], pids_1[i]);

	free(pids);
}

TEST_F(CgroupGetProcsTest, MoreThan16Pids)
{
	int ret, pids_cnt, i;
	pid_t *pids;

	ret = adaptived_cgroup_get_procs(cgroup_path_2, &pids, &pids_cnt);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(pids_cnt, pids_2_cnt);

	for (i = 0; i < pids_cnt; i++)
		ASSERT_EQ(pids[i], pids_2[i]);

	free(pids);
}
