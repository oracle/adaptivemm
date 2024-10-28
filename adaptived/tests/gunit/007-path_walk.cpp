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
 * adaptived googletest for walking a path
 */

#include <adaptived-utils.h>
#include <adaptived.h>
#include <ftw.h>

#include "gtest/gtest.h"
#include "defines.h"

static const char *dirs[] = {
	"test007",

	"test007/child1",
	"test007/child2",
	"test007/child3",

	"test007/child1/grandchild1-1",
	"test007/child1/grandchild1-2",
	"test007/child3/grandchild3-1",
	"test007/child3/grandchild3-2",
	"test007/child3/grandchild3-3",

	"test007/child1/grandchild1-2/greatgrandchild1-2-1",
	"test007/child3/grandchild3-3/greatgrandchild3-3-1",
};

static const char *files[] = {
	"test007/cgroup.procs",

	"test007/child1/cgroup.procs",

	"test007/child3/memory.max",
	"test007/child3/memory.min",
	"test007/child3/cgroup.procs",

	"test007/child1/grandchild1-2/cgroup.procs",
	"test007/child1/grandchild1-2/cpu.weight",
	"test007/child1/grandchild1-2/cpu.stat",

	"test007/child1/grandchild1-2/greatgrandchild1-2-1/cgroup.procs",
	"test007/child1/grandchild1-2/greatgrandchild1-2-1/memory.high",

	"test007/child3/grandchild3-3/greatgrandchild3-3-1/cgroup.procs",
};

class PathWalkTest : public ::testing::Test {
	protected:

	void SetUp() override {
		int ret, i;
		FILE *f;

		for (i = 0; i < ARRAY_SIZE(dirs); i++) {
			ret = mkdir(dirs[i], S_IRWXU | S_IRWXG | S_IRWXO);
			ASSERT_EQ(ret, 0);
		}

		for (i = 0; i < ARRAY_SIZE(files); i++) {
			f = fopen(files[i], "w");
			ASSERT_NE(f, nullptr);
			fclose(f);
		}
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

		ret = rmrf(dirs[0]);
		ASSERT_EQ(ret, 0);
	}
};

static int get_dir_index(const char * const path)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dirs); i++) {
		if (strcmp(path, dirs[i]) == 0)
			return i;
	}

	return -1;
}

static int get_file_index(const char * const path)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(files); i++) {
		if (strcmp(path, files[i]) == 0)
			return i;
	}

	return -1;
}

static int get_depth(const char * const path)
{
	const char * const needle = "/";
	const char *subpath;
	int slash_count = 0;

	subpath = strstr(path, needle);
	while (subpath) {
		slash_count++;

		subpath = strstr(&subpath[1], needle);
	}

	switch (slash_count) {
	case 0:
	case 1:
		return 0;
	default:
		return slash_count - 1;
	}
}

TEST_F(PathWalkTest, InvalidParams)
{
	struct adaptived_path_walk_handle *handle = NULL;
	const char *path = dirs[0];
	char *next;
	int ret;

	ret = adaptived_path_walk_start(NULL, &handle, ADAPTIVED_PATH_WALK_DEFAULT_FLAGS,
				     ADAPTIVED_PATH_WALK_UNLIMITED_DEPTH);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_path_walk_start(path, NULL, ADAPTIVED_PATH_WALK_LIST_DIRS,
				     ADAPTIVED_PATH_WALK_UNLIMITED_DEPTH);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_path_walk_next(NULL, &next);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_path_walk_next(&handle, NULL);
	ASSERT_EQ(ret, -EINVAL);

	adaptived_path_walk_end(NULL);
}

TEST_F(PathWalkTest, WalkDirectories)
{
	struct adaptived_path_walk_handle *handle = NULL;
	bool found[ARRAY_SIZE(dirs)] = { false };
	int ret, dir_index, i;
	char *next = NULL;

	/* 0 tells adaptived to use the default flags */
	ret = adaptived_path_walk_start(dirs[0], &handle, 0, ADAPTIVED_PATH_WALK_UNLIMITED_DEPTH);
	ASSERT_EQ(ret, 0);

	do {
		if (next)
			free(next);

		ret = adaptived_path_walk_next(&handle, &next);
		ASSERT_EQ(ret, 0);

		if (next) {
			dir_index = get_dir_index(next);
			ASSERT_GE(dir_index, 0);
			found[dir_index] = true;
		}
	} while (ret == 0 && next);

	adaptived_path_walk_end(&handle);
	ASSERT_EQ(handle, nullptr);

	for (i = 0; i < ARRAY_SIZE(dirs); i++)
		ASSERT_TRUE(found[i]);
}

TEST_F(PathWalkTest, WalkFiles)
{
	struct adaptived_path_walk_handle *handle = NULL;
	bool found[ARRAY_SIZE(files)] = { false };
	int ret, file_index, i;
	char *next = NULL;

	ret = adaptived_path_walk_start(dirs[0], &handle, ADAPTIVED_PATH_WALK_LIST_FILES,
				     ADAPTIVED_PATH_WALK_UNLIMITED_DEPTH);
	ASSERT_EQ(ret, 0);

	do {
		if (next)
			free(next);

		ret = adaptived_path_walk_next(&handle, &next);
		ASSERT_EQ(ret, 0);

		if (next) {
			file_index = get_file_index(next);
			ASSERT_GE(file_index, 0);
			found[file_index] = true;
		}
	} while (ret == 0 && next);

	adaptived_path_walk_end(&handle);
	ASSERT_EQ(handle, nullptr);

	for (i = 0; i < ARRAY_SIZE(files); i++)
		ASSERT_TRUE(found[i]);
}

TEST_F(PathWalkTest, WalkFilesAndDirs)
{
	struct adaptived_path_walk_handle *handle = NULL;
	bool found_f[ARRAY_SIZE(files)] = { false };
	bool found_d[ARRAY_SIZE(dirs)] = { false };
	int ret, index, i;
	char *next = NULL;

	ret = adaptived_path_walk_start(dirs[0], &handle,
				     ADAPTIVED_PATH_WALK_LIST_FILES |
				     ADAPTIVED_PATH_WALK_LIST_DIRS,
				     ADAPTIVED_PATH_WALK_UNLIMITED_DEPTH);
	ASSERT_EQ(ret, 0);

	do {
		if (next)
			free(next);

		ret = adaptived_path_walk_next(&handle, &next);
		ASSERT_EQ(ret, 0);

		if (next) {
			index = get_file_index(next);
			if (index >= 0)
				found_f[index] = true;
			index = get_dir_index(next);
			if (index >= 0)
				found_d[index] = true;
		}
	} while (ret == 0 && next);

	adaptived_path_walk_end(&handle);
	ASSERT_EQ(handle, nullptr);

	for (i = 0; i < ARRAY_SIZE(files); i++)
		ASSERT_TRUE(found_f[i]);
	for (i = 0; i < ARRAY_SIZE(dirs); i++)
		ASSERT_TRUE(found_d[i]);
}

TEST_F(PathWalkTest, WalkFilesAndDirsNoRecurse)
{
	struct adaptived_path_walk_handle *handle = NULL;
	int ret, index, i = 0;
	char *next = NULL;

	ret = adaptived_path_walk_start(dirs[0], &handle,
				     ADAPTIVED_PATH_WALK_LIST_FILES |
				     ADAPTIVED_PATH_WALK_LIST_DIRS, 0);
	ASSERT_EQ(ret, 0);

	do {
		if (next)
			free(next);

		ret = adaptived_path_walk_next(&handle, &next);
		ASSERT_EQ(ret, 0);

		if (next ) {
			i++;

			if (strcmp(next, "test007") != 0 &&
			    strcmp(next, "test007/cgroup.procs") != 0 &&
			    strcmp(next, "test007/child1") != 0 &&
			    strcmp(next, "test007/child2") != 0 &&
			    strcmp(next, "test007/child3") != 0)
				/* An unexpected string matched.  Always fail */
				ASSERT_STREQ(next, nullptr);
		}
	} while (ret == 0 && next);

	ASSERT_EQ(i, 5);

	adaptived_path_walk_end(&handle);
	ASSERT_EQ(handle, nullptr);
}

TEST_F(PathWalkTest, WalkDirsWithDots)
{
	struct adaptived_path_walk_handle *handle = NULL;
	bool found_dot = false, found_dotdot = false;
	const char * const dotdot = "test007/..";
	const char * const dot = "test007/.";
	int ret, found_cnt = 0;
	char *next = NULL;

	ret = adaptived_path_walk_start(dirs[0], &handle,
				     ADAPTIVED_PATH_WALK_LIST_DIRS |
				     ADAPTIVED_PATH_WALK_LIST_DOT_DIRS, 0);
	ASSERT_EQ(ret, 0);

	do {
		if (next)
			free(next);

		ret = adaptived_path_walk_next(&handle, &next);
		ASSERT_EQ(ret, 0);

		if (next) {
			found_cnt++;

			if (strcmp(next, dot) == 0)
				found_dot = true;
			else if (strcmp(next, dotdot) == 0)
				found_dotdot = true;
		}
	} while (ret == 0 && next);

	adaptived_path_walk_end(&handle);
	ASSERT_EQ(handle, nullptr);

	ASSERT_TRUE(found_dot);
	ASSERT_TRUE(found_dotdot);

	ASSERT_EQ(found_cnt, 6);
}

TEST_F(PathWalkTest, WalkFilesAndDirsWildcard)
{
	struct adaptived_path_walk_handle *handle = NULL;
	bool found_f[ARRAY_SIZE(files)] = { false };
	bool found_d[ARRAY_SIZE(dirs)] = { false };
	int ret, index, i;
	char *next = NULL;

	ret = adaptived_path_walk_start("test007/*", &handle,
				     ADAPTIVED_PATH_WALK_LIST_FILES |
				     ADAPTIVED_PATH_WALK_LIST_DIRS,
				     ADAPTIVED_PATH_WALK_UNLIMITED_DEPTH);
	ASSERT_EQ(ret, 0);

	do {
		if (next)
			free(next);

		ret = adaptived_path_walk_next(&handle, &next);
		ASSERT_EQ(ret, 0);

		if (next) {
			index = get_file_index(next);
			if (index >= 0)
				found_f[index] = true;
			index = get_dir_index(next);
			if (index >= 0)
				found_d[index] = true;
		}
	} while (ret == 0 && next);

	adaptived_path_walk_end(&handle);
	ASSERT_EQ(handle, nullptr);

	for (i = 0; i < ARRAY_SIZE(files); i++)
		ASSERT_TRUE(found_f[i]);

	ASSERT_FALSE(found_d[0]);
	for (i = 1; i < ARRAY_SIZE(dirs); i++)
		ASSERT_TRUE(found_d[i]);
}

TEST_F(PathWalkTest, WalkFilesAndDirsWildcard2)
{
	struct adaptived_path_walk_handle *handle = NULL;
	bool found_f[ARRAY_SIZE(files)] = { false };
	bool found_d[ARRAY_SIZE(dirs)] = { false };
	int ret, index, i;
	char *next = NULL;

	ret = adaptived_path_walk_start("test007*", &handle,
				     ADAPTIVED_PATH_WALK_LIST_FILES |
				     ADAPTIVED_PATH_WALK_LIST_DIRS,
				     ADAPTIVED_PATH_WALK_UNLIMITED_DEPTH);
	ASSERT_EQ(ret, 0);

	do {
		if (next)
			free(next);

		ret = adaptived_path_walk_next(&handle, &next);
		ASSERT_EQ(ret, 0);

		if (next) {
			index = get_file_index(next);
			if (index >= 0)
				found_f[index] = true;
			index = get_dir_index(next);
			if (index >= 0)
				found_d[index] = true;
		}
	} while (ret == 0 && next);

	adaptived_path_walk_end(&handle);
	ASSERT_EQ(handle, nullptr);

	for (i = 0; i < ARRAY_SIZE(files); i++)
		ASSERT_TRUE(found_f[i]);
	for (i = 0; i < ARRAY_SIZE(dirs); i++)
		ASSERT_TRUE(found_d[i]);
}

static void walk_directories_test(int max_depth)
{
	struct adaptived_path_walk_handle *handle = NULL;
	bool found[ARRAY_SIZE(dirs)] = { false };
	int ret, dir_index, i, depth;
	char *next = NULL;

	ret = adaptived_path_walk_start(dirs[0], &handle, ADAPTIVED_PATH_WALK_DEFAULT_FLAGS, max_depth);
	ASSERT_EQ(ret, 0);

	do {
		if (next)
			free(next);

		ret = adaptived_path_walk_next(&handle, &next);
		ASSERT_EQ(ret, 0);

		if (next) {
			dir_index = get_dir_index(next);
			ASSERT_GE(dir_index, 0);
			found[dir_index] = true;
		}
	} while (ret == 0 && next);

	adaptived_path_walk_end(&handle);
	ASSERT_EQ(handle, nullptr);

	for (i = 0; i < ARRAY_SIZE(dirs); i++) {
		depth = get_depth(dirs[i]);
		if (depth <= max_depth)
			ASSERT_TRUE(found[i]);
		else
			ASSERT_FALSE(found[i]);
	}
}


TEST_F(PathWalkTest, WalkDirectoriesDepth0)
{
	walk_directories_test(0);
}

TEST_F(PathWalkTest, WalkDirectoriesDepth1)
{
	walk_directories_test(1);
}

TEST_F(PathWalkTest, WalkDirectoriesDepth2)
{
	walk_directories_test(2);
}

TEST_F(PathWalkTest, WalkDirectoriesDepth3)
{
	walk_directories_test(3);
}

static void walk_dirs_and_files_test(int max_depth)
{
	struct adaptived_path_walk_handle *handle = NULL;
	bool found_f[ARRAY_SIZE(files)] = { false };
	bool found_d[ARRAY_SIZE(dirs)] = { false };
	int ret, index, i, depth;
	char *next = NULL;

	ret = adaptived_path_walk_start(dirs[0], &handle,
				     ADAPTIVED_PATH_WALK_LIST_FILES |
				     ADAPTIVED_PATH_WALK_LIST_DIRS, max_depth);
	ASSERT_EQ(ret, 0);

	do {
		if (next)
			free(next);

		ret = adaptived_path_walk_next(&handle, &next);
		ASSERT_EQ(ret, 0);

		if (next) {
			index = get_file_index(next);
			if (index >= 0)
				found_f[index] = true;
			index = get_dir_index(next);
			if (index >= 0)
				found_d[index] = true;
		}
	} while (ret == 0 && next);

	adaptived_path_walk_end(&handle);
	ASSERT_EQ(handle, nullptr);

	for (i = 0; i < ARRAY_SIZE(files); i++) {
		depth = get_depth(files[i]);
		if (depth <= max_depth)
			ASSERT_TRUE(found_f[i]);
		else
			ASSERT_FALSE(found_f[i]);
	}
	for (i = 0; i < ARRAY_SIZE(dirs); i++) {
		depth = get_depth(dirs[i]);
		if (depth <= max_depth)
			ASSERT_TRUE(found_d[i]);
		else
			ASSERT_FALSE(found_d[i]);
	}
}

TEST_F(PathWalkTest, WalkDirsAndFilesDepth0)
{
	walk_dirs_and_files_test(0);
}

TEST_F(PathWalkTest, WalkDirsAndFilesDepth1)
{
	walk_dirs_and_files_test(1);
}

TEST_F(PathWalkTest, WalkDirsAndFilesDepth2)
{
	walk_dirs_and_files_test(2);
}

TEST_F(PathWalkTest, WalkDirsAndFilesDepth3)
{
	walk_dirs_and_files_test(3);
}
