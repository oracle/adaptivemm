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

class FarrayLinearRegressionTest : public ::testing::Test {
};

TEST_F(FarrayLinearRegressionTest, IntervalOfOne)
{
	float y[] = {94.6, 88.4, 92.5, 90.1, 84.3, 75.7, 75.9, 80.2, 65.8, 60.9, 62.3,
		     58.9, 58.5, 63.5, 55.4, 59.4, 56.3, 52.1, 51.1, 48.6, 47.9, 51.8,
		     50.3, 45.6, 43.2, 43.1, 46.2, 40.7, 38.9, 37.5, 35.9, 40.2, 38.7};
	int y_len, ret;
	float interp_y;

	y_len = ARRAY_SIZE(y);

	ret = adaptived_farray_linear_regression(y, y_len, 1, -28, &interp_y);
	ASSERT_EQ(ret, 0);
	EXPECT_NEAR(interp_y, 79.29, 0.01);

	ret = adaptived_farray_linear_regression(y, y_len, 1, 0, &interp_y);
	ASSERT_EQ(ret, 0);
	EXPECT_NEAR(interp_y, 31.07, 0.01);

	ret = adaptived_farray_linear_regression(y, y_len, 1, 7, &interp_y);
	ASSERT_EQ(ret, 0);
	EXPECT_NEAR(interp_y, 19.01, 0.01);
}

TEST_F(FarrayLinearRegressionTest, IntervalOfTwo)
{
	float y[] = {7.0, 8.0, 6.0, 10.0, 15.0, 12.0, 14.0, 17.0, 21.0, 26.0, 29.0};
	int y_len, ret;
	float interp_y;

	y_len = ARRAY_SIZE(y);

	ret = adaptived_farray_linear_regression(y, y_len, 2, -20, &interp_y);
	ASSERT_EQ(ret, 0);
	EXPECT_NEAR(interp_y, 4.09, 0.01);

	ret = adaptived_farray_linear_regression(y, y_len, 2, -15, &interp_y);
	ASSERT_EQ(ret, 0);
	EXPECT_NEAR(interp_y, 9.55, 0.01);

	ret = adaptived_farray_linear_regression(y, y_len, 2, 1, &interp_y);
	ASSERT_EQ(ret, 0);
	EXPECT_NEAR(interp_y, 27.0, 0.01);

	ret = adaptived_farray_linear_regression(y, y_len, 2, 20, &interp_y);
	ASSERT_EQ(ret, 0);
	EXPECT_NEAR(interp_y, 47.73, 0.01);
}
