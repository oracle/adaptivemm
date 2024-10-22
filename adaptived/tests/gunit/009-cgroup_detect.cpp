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
 * adaptived googletest for auto-detecting the cgroup value type
 */

#include <adaptived-utils.h>
#include <adaptived.h>

#include "gtest/gtest.h"

static const char * const llfile = "./test009.longlong";
static const char * const strfile = "./test009.string";

static const char * const llcontents = "123456789";
static const long long llcontents_ll = 123456789; /* must be the same as the line above */
static const char * const strcontents = "Soy un perdedor";

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

class CgroupDetectTest : public ::testing::Test {
	protected:

	void SetUp() override {
		CreateFile(llfile, llcontents);
		CreateFile(strfile, strcontents);
	}

	void TearDown() override {
		DeleteFile(llfile);
		DeleteFile(strfile);
	}
};

TEST_F(CgroupDetectTest, InvalidParams)
{
	struct adaptived_cgroup_value value;
	int ret;

	ret = adaptived_cgroup_get_value(NULL, &value);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_cgroup_get_value(llfile, NULL);
	ASSERT_EQ(ret, -EINVAL);

	value.type = ADAPTIVED_CGVAL_DETECT;
	ret = adaptived_cgroup_set_value(llfile, &value, 0);
	ASSERT_EQ(ret, -EINVAL);
}

TEST_F(CgroupDetectTest, DetectLongLong)
{
	struct adaptived_cgroup_value value;
	int ret;

	value.type = ADAPTIVED_CGVAL_DETECT;
	ret = adaptived_cgroup_get_value(llfile, &value);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(value.type, ADAPTIVED_CGVAL_LONG_LONG);
	ASSERT_EQ(value.value.ll_value, llcontents_ll);
}

TEST_F(CgroupDetectTest, DetectString)
{
	struct adaptived_cgroup_value value;
	int ret;

	value.type = ADAPTIVED_CGVAL_DETECT;
	ret = adaptived_cgroup_get_value(strfile, &value);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(value.type, ADAPTIVED_CGVAL_STR);
	ASSERT_STREQ(value.value.str_value, strcontents);
}
