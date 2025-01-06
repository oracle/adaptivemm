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
 * adaptived googletest for _sort_pid_list() in src/effects/kill_processes.c
 */

#include <fcntl.h>

#include <adaptived-utils.h>
#include <adaptived.h>

#include "adaptived-internal.h"
#include "gtest/gtest.h"
#include "defines.h"

/* TODO - delete this typedef when proc_pid_stats_utils.c is created */
struct pid_info {
	pid_t pid;
	long long vsize;
};

static const int array_size = 10;
static const long long vsizes[] = {
	100,
	50,
	250,
	1000,
	400,
	1500,
	750,
	50000,
	0,
	150,
};

class SortPidListTest : public ::testing::Test {
	protected:

	void SetUp() override {
		ASSERT_EQ(array_size, (int)ARRAY_SIZE(vsizes));
	}
};

TEST_F(SortPidListTest, Sort)
{
	struct pid_info pida[array_size] = { 0 };
	int i;

	for (i = 0; i < array_size; i++) {
		pida[i].pid = i;
		pida[i].vsize = vsizes[i];
	}

	qsort(pida, array_size, sizeof(struct pid_info), _sort_pid_list);

	ASSERT_EQ(pida[0].pid, 7);
	ASSERT_EQ(pida[1].pid, 5);
	ASSERT_EQ(pida[2].pid, 3);
	ASSERT_EQ(pida[3].pid, 6);
	ASSERT_EQ(pida[4].pid, 4);
	ASSERT_EQ(pida[5].pid, 2);
	ASSERT_EQ(pida[6].pid, 9);
	ASSERT_EQ(pida[7].pid, 0);
	ASSERT_EQ(pida[8].pid, 1);
	ASSERT_EQ(pida[9].pid, 8);
}
