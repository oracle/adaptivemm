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
 * adaptived googletest for float_utils.c
 */

#include <fcntl.h>

#include <adaptived-utils.h>
#include <adaptived.h>

#include "gtest/gtest.h"
#include "defines.h"

class FarrayAppendTest : public ::testing::Test {
};

TEST_F(FarrayAppendTest, AppendToArray)
{
	int ret, array_len, sample_cnt;
	float *array, value;

	sample_cnt = 0;

	array_len = 5;
	array = (float *)malloc(sizeof(float) * array_len);
	ASSERT_NE(array, nullptr);

	value = 12.3f;
	ret = adaptived_farray_append(array, &value, array_len, &sample_cnt);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(sample_cnt, 1);
	EXPECT_NEAR(array[0], value, 0.01);

	value = 4.56f;
	ret = adaptived_farray_append(array, &value, array_len, &sample_cnt);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(sample_cnt, 2);
	EXPECT_NEAR(array[0], 12.3, 0.01);
	EXPECT_NEAR(array[1], value, 0.01);

	value = 789.0f;
	ret = adaptived_farray_append(array, &value, array_len, &sample_cnt);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(sample_cnt, 3);
	EXPECT_NEAR(array[0], 12.3, 0.01);
	EXPECT_NEAR(array[1], 4.56, 0.01);
	EXPECT_NEAR(array[2], value, 0.01);

	value = 9.8f;
	ret = adaptived_farray_append(array, &value, array_len, &sample_cnt);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(sample_cnt, 4);
	EXPECT_NEAR(array[0], 12.3, 0.01);
	EXPECT_NEAR(array[1], 4.56, 0.01);
	EXPECT_NEAR(array[2], 789.0, 0.01);
	EXPECT_NEAR(array[3], value, 0.01);

	value = 7.654f;
	ret = adaptived_farray_append(array, &value, array_len, &sample_cnt);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(sample_cnt, 5);
	EXPECT_NEAR(array[0], 12.3, 0.01);
	EXPECT_NEAR(array[1], 4.56, 0.01);
	EXPECT_NEAR(array[2], 789.0, 0.01);
	EXPECT_NEAR(array[3], 9.8, 0.01);
	EXPECT_NEAR(array[4], value, 0.01);

	value = 3.2f;
	ret = adaptived_farray_append(array, &value, array_len, &sample_cnt);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(sample_cnt, 5);
	EXPECT_NEAR(array[0], 4.56, 0.01);
	EXPECT_NEAR(array[1], 789.0, 0.01);
	EXPECT_NEAR(array[2], 9.8, 0.01);
	EXPECT_NEAR(array[3], 7.654, 0.01);
	EXPECT_NEAR(array[4], value, 0.01);

	value = 1.1111f;
	ret = adaptived_farray_append(array, &value, array_len, &sample_cnt);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(sample_cnt, 5);
	EXPECT_NEAR(array[0], 789.0, 0.01);
	EXPECT_NEAR(array[1], 9.8, 0.01);
	EXPECT_NEAR(array[2], 7.654, 0.01);
	EXPECT_NEAR(array[3], 3.2, 0.01);
	EXPECT_NEAR(array[4], value, 0.01);

	free(array);
}
