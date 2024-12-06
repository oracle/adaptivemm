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
 * adaptived googletest for adaptived_get_pressure()
 */

#include <fcntl.h>

#include <adaptived-utils.h>
#include <adaptived.h>

#include "gtest/gtest.h"

static const char * const PRESSURE_FILE = "002-adaptived_get_pressure_avg.pressure";

/*
static const float SOME_10 = 1.23;
static const float SOME_60 = 4.56;
static const float SOME_300 = 7.89;
static const long long SOME_TOTAL = 123456789;

static const float FULL_10 = 9.87;
static const float FULL_60 = 6.54;
static const float FULL_300 = 3.21;
static const long long FULL_TOTAL = 987654321;
*/

#define STRINGIFY(_val) #_val
#define STR(_val) STRINGIFY(_val)

#define SOME_10 1.23
#define SOME_60 4.56
#define SOME_300 7.89
#define SOME_TOTAL 123456789

#define FULL_10 9.87
#define FULL_60 6.54
#define FULL_300 3.21
#define FULL_TOTAL 987654321

#define SOME_LINE "some avg10=" STR(SOME_10) " avg60=" STR(SOME_60) \
		  " avg300=" STR(SOME_300) " total=" STR(SOME_TOTAL) "\n"
#define FULL_LINE "full avg10=" STR(FULL_10) " avg60=" STR(FULL_60) \
		  " avg300=" STR(FULL_300) " total=" STR(FULL_TOTAL)


class AdaptivedGetPressureAvgTest : public ::testing::Test {
	protected:

	void SetUp() override {
		int ret, fd;

		fd = creat(PRESSURE_FILE, S_IRUSR | S_IWUSR);
		ASSERT_GT(fd, 0);

		ret = write(fd, SOME_LINE, strlen(SOME_LINE));
		ASSERT_EQ(ret, (int)strlen(SOME_LINE));
		ret = write(fd, FULL_LINE, strlen(FULL_LINE));
		ASSERT_EQ(ret, (int)strlen(FULL_LINE));

		close(fd);
	}

	void TearDown() override {
		int ret;

		ret = remove(PRESSURE_FILE);
		ASSERT_EQ(ret, 0);
	}
};

TEST_F(AdaptivedGetPressureAvgTest, GetPressureSomeAvg10)
{
	float val;
	int ret;

	ret = adaptived_get_pressure_avg(PRESSURE_FILE, PRESSURE_SOME_AVG10, &val);
	ASSERT_EQ(ret, 0);
	EXPECT_NEAR(val, SOME_10, 0.01);
}

TEST_F(AdaptivedGetPressureAvgTest, GetPressureSomeAvg60)
{
	float val;
	int ret;

	ret = adaptived_get_pressure_avg(PRESSURE_FILE, PRESSURE_SOME_AVG60, &val);
	ASSERT_EQ(ret, 0);
	EXPECT_NEAR(val, SOME_60, 0.01);
}

TEST_F(AdaptivedGetPressureAvgTest, GetPressureSomeAvg300)
{
	float val;
	int ret;

	ret = adaptived_get_pressure_avg(PRESSURE_FILE, PRESSURE_SOME_AVG300, &val);
	ASSERT_EQ(ret, 0);
	EXPECT_NEAR(val, SOME_300, 0.01);
}

TEST_F(AdaptivedGetPressureAvgTest, GetPressureSomeTotal)
{
	long long val;
	int ret;

	ret = adaptived_get_pressure_total(PRESSURE_FILE, PRESSURE_SOME_TOTAL, &val);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(val, SOME_TOTAL);
}

TEST_F(AdaptivedGetPressureAvgTest, GetPressureFullAvg10)
{
	float val;
	int ret;

	ret = adaptived_get_pressure_avg(PRESSURE_FILE, PRESSURE_FULL_AVG10, &val);
	ASSERT_EQ(ret, 0);
	EXPECT_NEAR(val, FULL_10, 0.01);
}

TEST_F(AdaptivedGetPressureAvgTest, GetPressureFullAvg60)
{
	float val;
	int ret;

	ret = adaptived_get_pressure_avg(PRESSURE_FILE, PRESSURE_FULL_AVG60, &val);
	ASSERT_EQ(ret, 0);
	EXPECT_NEAR(val, FULL_60, 0.01);
}

TEST_F(AdaptivedGetPressureAvgTest, GetPressureFullAvg300)
{
	float val;
	int ret;

	ret = adaptived_get_pressure_avg(PRESSURE_FILE, PRESSURE_FULL_AVG300, &val);
	ASSERT_EQ(ret, 0);
	EXPECT_NEAR(val, FULL_300, 0.01);
}

TEST_F(AdaptivedGetPressureAvgTest, GetPressureFullTotal)
{
	long long val;
	int ret;

	ret = adaptived_get_pressure_total(PRESSURE_FILE, PRESSURE_FULL_TOTAL, &val);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(val, FULL_TOTAL);
}
