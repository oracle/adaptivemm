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
 * adaptived googletest for adaptived_cgroup_[get|set]_str()
 */

#include <adaptived-utils.h>
#include <adaptived.h>

#include "gtest/gtest.h"

class CgroupStringTest : public ::testing::Test {
};

static void CreateFile(const char * const filename, const char * const contents)
{
	FILE *f;

	f = fopen(filename, "w");
	ASSERT_NE(f, nullptr);

	fprintf(f, "%s", contents);
	fclose(f);
}

static void DeleteFile(const char * const filename)
{
	remove(filename);
}

TEST_F(CgroupStringTest, InvalidSet)
{
	int ret;

	ret = adaptived_cgroup_set_str(NULL, "abcd", 0);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_cgroup_set_str("bad_filename", NULL, 0);
	ASSERT_EQ(ret, -EINVAL);
}

TEST_F(CgroupStringTest, SetAndValidate)
{
	char filename[] = "./CgroupStringSetAndValidate";
	char value[] = "abc123";
	int ret;

	DeleteFile(filename);
	CreateFile(filename, "Hello World");

	ret = adaptived_cgroup_set_str(filename, value, ADAPTIVED_CGROUP_FLAGS_VALIDATE);
	ASSERT_EQ(ret, 0);

	DeleteFile(filename);
}

TEST_F(CgroupStringTest, InvalidGet)
{
	char filename[] = "CgroupStringInvalidGet";
	char *value;
	int ret;

	ret = adaptived_cgroup_get_str(NULL, &value);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_cgroup_get_str(filename, NULL);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_cgroup_get_str(filename, &value);
	ASSERT_EQ(ret, -ENOENT);
	ASSERT_EQ(value, nullptr);
}

TEST_F(CgroupStringTest, Get)
{
	char filename[] = "./CgroupStringGet";
	char expected_value[] = "1234\n5678";
	char *value;
	int ret;

	CreateFile(filename, expected_value);

	ret = adaptived_cgroup_get_str(filename, &value);
	ASSERT_EQ(ret, 0);
	ASSERT_STREQ(value, expected_value);

	free(value);

	DeleteFile(filename);
}
