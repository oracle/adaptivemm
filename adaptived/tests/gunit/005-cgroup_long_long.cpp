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
 * adaptived googletest for adaptived_cgroup_[get|set]_long_long()
 */

#include <adaptived-utils.h>
#include <adaptived.h>

#include "gtest/gtest.h"

class CgroupLongLongTest : public ::testing::Test {
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

TEST_F(CgroupLongLongTest, InvalidSet)
{
	int ret;

	ret = adaptived_cgroup_set_ll(NULL, 1234, 0);
	ASSERT_EQ(ret, -EINVAL);
}

TEST_F(CgroupLongLongTest, SetAndValidate)
{
	char filename[] = "./CgroupLongLongSetAndValidate";
	long long value = 65432;
	int ret;

	DeleteFile(filename);
	CreateFile(filename, "123456");

	ret = adaptived_cgroup_set_ll(filename, value, ADAPTIVED_CGROUP_FLAGS_VALIDATE);
	ASSERT_EQ(ret, 0);

	DeleteFile(filename);
}

TEST_F(CgroupLongLongTest, InvalidGet)
{
	char filename[] = "CgroupLongLongInvalidGet";
	long long value;
	int ret;

	ret = adaptived_cgroup_get_ll(NULL, &value);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_cgroup_get_ll(filename, NULL);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_cgroup_get_ll(filename, &value);
	ASSERT_EQ(ret, -ENOENT);
}

TEST_F(CgroupLongLongTest, Get)
{
	char filename[] = "./CgroupLongLongGet";
	long long expected_value = 1357924680;
	char buf[FILENAME_MAX] = {0};
	long long value;
	int ret;

	snprintf(buf, FILENAME_MAX - 1, "%lld\n", expected_value);

	CreateFile(filename, buf);

	ret = adaptived_cgroup_get_ll(filename, &value);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(value, expected_value);

	DeleteFile(filename);
}
