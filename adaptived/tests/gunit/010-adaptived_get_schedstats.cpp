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
 * adaptived googletest for adaptived_get_schedstats()
 */

#include <adaptived-utils.h>
#include <adaptived.h>

#include "gtest/gtest.h"

static const char * const schedstats_file = "./test010.schedstats";

static const char * const schedstats_contents =
	"version 15\n"
	"timestamp 5979263307\n"
	"cpu0 0 0 0 0 0 0 43076095314418 394914672557 428184882\n"
	"domain0 00003 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 2 0\n"
	"domain1 fffff 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 3 4 0\n"
	"cpu1 0 0 0 0 0 0 10913795519351 136791545255 100532045\n"
	"domain0 00003 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 5 6 0\n"
	"domain1 fffff 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 7 8 0\n"
	"cpu2 0 0 0 0 0 0 54048015265649 599770051010 527416276\n"
	"domain0 0000c 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 9 10 0\n"
	"domain1 fffff 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 11 12 0\n"
	"cpu3 0 0 0 0 0 0 9043880713438 172082515008 83848679\n"
	"domain0 0000c 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 13 14 0\n"
	"domain1 fffff 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 15 16 0\n";

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

class GetSchedstatsTest : public ::testing::Test {
	protected:

	void SetUp() override {
		CreateFile(schedstats_file, schedstats_contents);
	}

	void TearDown() override {
		DeleteFile(schedstats_file);
	}
};

TEST_F(GetSchedstatsTest, InvalidParams)
{
	struct adaptived_schedstat_snapshot ss;
	int ret;

	ret = adaptived_get_schedstat(NULL, &ss);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_get_schedstat(schedstats_file, NULL);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_get_schedstat("invalid_filename", &ss);
	ASSERT_EQ(ret, -EINVAL);
}

TEST_F(GetSchedstatsTest, GetSnapshot)
{
	struct adaptived_schedstat_snapshot ss;
	int ret;

	ret = adaptived_get_schedstat(schedstats_file, &ss);
	ASSERT_EQ(ret, 0);

	ASSERT_EQ(ss.nr_cpus, 4);
	ASSERT_EQ(ss.timestamp, 5979263307LU);

	ASSERT_EQ(ss.schedstat_cpus[0].ttwu, 0U);
	ASSERT_EQ(ss.schedstat_cpus[0].ttwu_local, 0U);
	ASSERT_EQ(ss.schedstat_cpus[0].run_time, 43076095314418LLU);
	ASSERT_EQ(ss.schedstat_cpus[0].run_delay, 394914672557LLU);
	ASSERT_EQ(ss.schedstat_cpus[0].nr_timeslices, 428184882LU);
	ASSERT_EQ(ss.schedstat_cpus[0].schedstat_domains[0].ttwu_remote, 1U);
	ASSERT_EQ(ss.schedstat_cpus[0].schedstat_domains[0].ttwu_move_affine, 2U);
	ASSERT_EQ(ss.schedstat_cpus[0].schedstat_domains[1].ttwu_remote, 3U);
	ASSERT_EQ(ss.schedstat_cpus[0].schedstat_domains[1].ttwu_move_affine, 4U);


	ASSERT_EQ(ss.schedstat_cpus[1].ttwu, 0U);
	ASSERT_EQ(ss.schedstat_cpus[1].ttwu_local, 0U);
	ASSERT_EQ(ss.schedstat_cpus[1].run_time, 10913795519351LLU);
	ASSERT_EQ(ss.schedstat_cpus[1].run_delay, 136791545255LLU);
	ASSERT_EQ(ss.schedstat_cpus[1].nr_timeslices, 100532045LU);
	ASSERT_EQ(ss.schedstat_cpus[1].schedstat_domains[0].ttwu_remote, 5U);
	ASSERT_EQ(ss.schedstat_cpus[1].schedstat_domains[0].ttwu_move_affine, 6U);
	ASSERT_EQ(ss.schedstat_cpus[1].schedstat_domains[1].ttwu_remote, 7U);
	ASSERT_EQ(ss.schedstat_cpus[1].schedstat_domains[1].ttwu_move_affine, 8U);

	ASSERT_EQ(ss.schedstat_cpus[2].ttwu, 0U);
	ASSERT_EQ(ss.schedstat_cpus[2].ttwu_local, 0U);
	ASSERT_EQ(ss.schedstat_cpus[2].run_time, 54048015265649LLU);
	ASSERT_EQ(ss.schedstat_cpus[2].run_delay, 599770051010LLU);
	ASSERT_EQ(ss.schedstat_cpus[2].nr_timeslices, 527416276LU);
	ASSERT_EQ(ss.schedstat_cpus[2].schedstat_domains[0].ttwu_remote, 9U);
	ASSERT_EQ(ss.schedstat_cpus[2].schedstat_domains[0].ttwu_move_affine, 10U);
	ASSERT_EQ(ss.schedstat_cpus[2].schedstat_domains[1].ttwu_remote, 11U);
	ASSERT_EQ(ss.schedstat_cpus[2].schedstat_domains[1].ttwu_move_affine, 12U);

	ASSERT_EQ(ss.schedstat_cpus[3].ttwu, 0U);
	ASSERT_EQ(ss.schedstat_cpus[3].ttwu_local, 0U);
	ASSERT_EQ(ss.schedstat_cpus[3].run_time, 9043880713438LLU);
	ASSERT_EQ(ss.schedstat_cpus[3].run_delay, 172082515008LLU);
	ASSERT_EQ(ss.schedstat_cpus[3].nr_timeslices, 83848679LU);
	ASSERT_EQ(ss.schedstat_cpus[3].schedstat_domains[0].ttwu_remote, 13U);
	ASSERT_EQ(ss.schedstat_cpus[3].schedstat_domains[0].ttwu_move_affine, 14U);
	ASSERT_EQ(ss.schedstat_cpus[3].schedstat_domains[1].ttwu_remote, 15U);
	ASSERT_EQ(ss.schedstat_cpus[3].schedstat_domains[1].ttwu_move_affine, 16U);
}
