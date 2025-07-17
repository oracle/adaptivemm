/*
 * Copyright (c) 2025, Oracle and/or its affiliates.
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
 * adaptived googletest for shared_data
 */

#include <adaptived-utils.h>
#include <adaptived.h>

#include "gtest/gtest.h"
#include "shared_data.h"
#include "cause.h"

class SharedDataTest : public ::testing::Test {
};

static void populate_cause(struct adaptived_cause * const cse, int idx)
{
	char *name;

	name = (char *)malloc(sizeof(char) * 16);
	ASSERT_NE(name, nullptr);

	sprintf(name, "test012-%d", idx);

	cse->idx = (enum cause_enum)idx;
	cse->name = name;
	cse->fns = NULL;
	cse->sdata = NULL;
	cse->data = NULL;
}

static void free_sdata(void * const data)
{
	return;
}

TEST_F(SharedDataTest, InvalidShare)
{
	enum adaptived_sdata_type type;
	struct adaptived_cause cse;
	uint32_t flags;
	void *vdata;
	char *data;
	int ret;

	data = (char *)malloc(sizeof(char) * 16);
	ASSERT_NE(data, nullptr);

	sprintf(data, "data string");

	ret = adaptived_write_shared_data(NULL, ADAPTIVED_SDATA_STR, data, &free_sdata, 0);
	ASSERT_EQ(ret, -EINVAL);

	populate_cause(&cse, 0);

	ret = adaptived_get_shared_data(&cse, 0, &type, &vdata, &flags);
	ASSERT_EQ(ret, -ERANGE);

	ret = adaptived_write_shared_data(&cse, (enum adaptived_sdata_type)-1, data,
					  &free_sdata, 0);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CNT, data, &free_sdata, 0);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_STR, NULL, &free_sdata,
					  ADAPTIVED_SDATAF_PERSIST);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_STR, data, &free_sdata, 0);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CUSTOM, data, NULL, 0);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_get_shared_data_cnt(&cse);
	ASSERT_EQ(ret, 0);

	ret = adaptived_get_shared_data_cnt(NULL);
	ASSERT_EQ(ret, 0);

	free_shared_data(NULL, true);

	free(cse.name);
	free(data);
}

static void free_sdata2(void * const data)
{
	char ** cdata;

	cdata = (char **)data;

	free(cdata[0]);
	free(cdata[1]);
	free(cdata[2]);
	free(cdata[3]);
	free(cdata);
}

TEST_F(SharedDataTest, CustomDataShare)
{
	char **custom_data, **read_data_ptr;
	enum adaptived_sdata_type type;
	struct adaptived_cause cse;
	uint32_t flags = 0;
	void *read_data;
	int ret, cnt;

	populate_cause(&cse, 0);

	ret = adaptived_update_shared_data(&cse, 0, type, &read_data, 0);
	ASSERT_EQ(ret, -ERANGE);

	custom_data = (char **)malloc(sizeof(char *) * 4);
	ASSERT_NE(custom_data, nullptr);
	custom_data[0] = (char *)malloc(sizeof(char) * 32);
	ASSERT_NE(custom_data[0], nullptr);
	sprintf(custom_data[0], "This is string 0");
	custom_data[1] = (char *)malloc(sizeof(char) * 64);
	ASSERT_NE(custom_data[1], nullptr);
	sprintf(custom_data[1], "This is string 1.  It's twice as long as string 0");
	custom_data[2] = (char *)malloc(sizeof(char) * 12);
	ASSERT_NE(custom_data[2], nullptr);
	sprintf(custom_data[2], "String 2");
	custom_data[3] = (char *)malloc(sizeof(char) * 32);
	ASSERT_NE(custom_data[3], nullptr);
	sprintf(custom_data[3], "This is string 3");

	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CUSTOM, custom_data, free_sdata2,
					  flags);
	ASSERT_EQ(ret, 0);

	cnt = adaptived_get_shared_data_cnt(&cse);
	ASSERT_EQ(cnt, 1);

	ret = adaptived_get_shared_data(&cse, 0, &type, &read_data, &flags);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(flags, (uint32_t)0);
	ASSERT_EQ(type, ADAPTIVED_SDATA_CUSTOM);
	ASSERT_EQ(read_data, custom_data);

	read_data_ptr = (char **)read_data;

	ASSERT_STREQ(read_data_ptr[0], "This is string 0");
	ASSERT_STREQ(read_data_ptr[1], "This is string 1.  It's twice as long as string 0");
	ASSERT_STREQ(read_data_ptr[2], "String 2");
	ASSERT_STREQ(read_data_ptr[3], "This is string 3");

	/*
	 * Do some invalid testing on get_shared_data() now that we have
	 * populated the shared_data field in the cse structure
	 */
	ret = adaptived_get_shared_data(&cse, -1, &type, &read_data, &flags);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_get_shared_data(&cse, 2, &type, &read_data, &flags);
	ASSERT_EQ(ret, -ERANGE);

	ret = adaptived_get_shared_data(&cse, 0, NULL, &read_data, &flags);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_get_shared_data(&cse, 0, &type, NULL, &flags);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_get_shared_data(&cse, 0, &type, &read_data, NULL);
	ASSERT_EQ(ret, -EINVAL);

	/*
	 * Do some invalid testing on update_shared_data()
	 */
	type = ADAPTIVED_SDATA_CUSTOM;
	ret = adaptived_update_shared_data(&cse, 0, type, NULL, 0);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_update_shared_data(NULL, 0, type, &read_data, 0);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_update_shared_data(&cse, -1, type, &read_data, 0);
	ASSERT_EQ(ret, -EINVAL);

	ret = adaptived_update_shared_data(&cse, 4, type, &read_data, 0);
	ASSERT_EQ(ret, -ERANGE);

	type = ADAPTIVED_SDATA_STR;
	ret = adaptived_update_shared_data(&cse, 0, type, &read_data, 0);
	ASSERT_EQ(ret, -EINVAL);

	free_shared_data(&cse, false);
	free(cse.name);
}

TEST_F(SharedDataTest, StringDataShare)
{
	enum adaptived_sdata_type type;
	struct adaptived_cause cse;
	char *src, *read;
	void *read_data;
	uint32_t flags;
	int ret, cnt;

	populate_cause(&cse, 0);

	src = (char *)malloc(sizeof(char) * 12);
	ASSERT_NE(src, nullptr);
	sprintf(src, "test data");

	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_STR, src, NULL,
					  ADAPTIVED_SDATAF_PERSIST);
	ASSERT_EQ(ret, 0);

	cnt = adaptived_get_shared_data_cnt(&cse);
	ASSERT_EQ(cnt, 1);

	ret = adaptived_get_shared_data(&cse, 0, &type, &read_data, &flags);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(type, ADAPTIVED_SDATA_STR);
	ASSERT_EQ(read_data, src);
	ASSERT_EQ(flags, ADAPTIVED_SDATAF_PERSIST);

	read = (char *)read_data;

	ASSERT_STREQ(read, "test data");

	free_shared_data(&cse, false);

	cnt = adaptived_get_shared_data_cnt(&cse);
	ASSERT_EQ(cnt, 1);

	free_shared_data(&cse, true);

	cnt = adaptived_get_shared_data_cnt(&cse);
	ASSERT_EQ(cnt, 0);

	free(cse.name);
}

TEST_F(SharedDataTest, CgroupFloatDataShare)
{
	struct adaptived_cgroup_value *src, *dst;
	enum adaptived_sdata_type type;
	struct adaptived_cause cse;
	void *read_data;
	uint32_t flags;
	int ret, cnt;

	populate_cause(&cse, 0);

	src = (struct adaptived_cgroup_value *)malloc(sizeof(struct adaptived_cgroup_value));
	ASSERT_NE(src, nullptr);
	src->type = ADAPTIVED_CGVAL_FLOAT;
	src->value.float_value = 123.4;

	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CGROUP, src, NULL, 0);
	ASSERT_EQ(ret, 0);

	cnt = adaptived_get_shared_data_cnt(&cse);
	ASSERT_EQ(cnt, 1);

	ret = adaptived_get_shared_data(&cse, 0, &type, &read_data, &flags);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(type, ADAPTIVED_SDATA_CGROUP);
	ASSERT_EQ(read_data, src);
	ASSERT_EQ(flags, (uint32_t)0);

	dst = (struct adaptived_cgroup_value *)read_data;
	ASSERT_EQ(dst->type, ADAPTIVED_CGVAL_FLOAT);
	ASSERT_FLOAT_EQ(dst->value.float_value, 123.4);

	free_shared_data(&cse, false);
	free(cse.name);
}

TEST_F(SharedDataTest, CgroupStringDataShare)
{
	struct adaptived_cgroup_value *src, *dst;
	enum adaptived_sdata_type type;
	struct adaptived_cause cse;
	void *read_data;
	uint32_t flags;
	int ret, cnt;
	char *cstr;

	populate_cause(&cse, 0);

	src = (struct adaptived_cgroup_value *)malloc(sizeof(struct adaptived_cgroup_value));
	ASSERT_NE(src, nullptr);

	cstr = (char *)malloc(sizeof(char) * 128);
	ASSERT_NE(cstr, nullptr);
	sprintf(cstr, "/sys/fs/cgroup/test012.slice/");

	src->type = ADAPTIVED_CGVAL_STR;
	src->value.str_value = cstr;

	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CGROUP, src, NULL, 0);
	ASSERT_EQ(ret, 0);

	cnt = adaptived_get_shared_data_cnt(&cse);
	ASSERT_EQ(cnt, 1);

	ret = adaptived_get_shared_data(&cse, 0, &type, &read_data, &flags);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(type, ADAPTIVED_SDATA_CGROUP);
	ASSERT_EQ(read_data, src);
	ASSERT_EQ(flags, (uint32_t)0);

	dst = (struct adaptived_cgroup_value *)read_data;
	ASSERT_EQ(dst->type, ADAPTIVED_CGVAL_STR);
	ASSERT_STREQ(dst->value.str_value, "/sys/fs/cgroup/test012.slice/");

	free_shared_data(&cse, false);
	free(cse.name);
}

TEST_F(SharedDataTest, NameValueDataShare)
{
	struct adaptived_name_and_value *src, *dst;
	struct adaptived_cgroup_value *cg;
	enum adaptived_sdata_type type;
	struct adaptived_cause cse;
	void *read_data;
	uint32_t flags;
	int ret, cnt;
	char *name;

	populate_cause(&cse, 0);

	src = (struct adaptived_name_and_value *)malloc(sizeof(struct adaptived_name_and_value));
	ASSERT_NE(src, nullptr);

	cg = (struct adaptived_cgroup_value *)malloc(sizeof(struct adaptived_cgroup_value));
	ASSERT_NE(cg, nullptr);

	name = (char *)malloc(sizeof(char) * 32);
	ASSERT_NE(name, nullptr);
	sprintf(name, "database13.scope");

	cg->type = ADAPTIVED_CGVAL_LONG_LONG;
	cg->value.ll_value = 135798642;

	src->name = name;
	src->value = cg;

	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_NAME_VALUE, src, NULL, 0);
	ASSERT_EQ(ret, 0);

	cnt = adaptived_get_shared_data_cnt(&cse);
	ASSERT_EQ(cnt, 1);

	ret = adaptived_get_shared_data(&cse, 0, &type, &read_data, &flags);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(type, ADAPTIVED_SDATA_NAME_VALUE);
	ASSERT_EQ(read_data, src);
	ASSERT_EQ(flags, (uint32_t)0);

	dst = (struct adaptived_name_and_value *)read_data;
	ASSERT_EQ(dst->value->type, ADAPTIVED_CGVAL_LONG_LONG);
	ASSERT_EQ(dst->value->value.ll_value, 135798642);

	free_shared_data(&cse, false);
	free(cse.name);
}

static void free_sdata3(void * const data)
{
	free(data);
}

TEST_F(SharedDataTest, MultipleDataShares)
{
	struct adaptived_cgroup_value *src_cgstr, *dst_cgstr;
	struct adaptived_cgroup_value *src_cgflt, *dst_cgflt;
	char *src_str, *dst_str, *cgstr;
	enum adaptived_sdata_type type;
	struct adaptived_cause cse;
	int *src_custom, *dst_custom;
	void *read_data;
	uint32_t flags;
	int ret, cnt;

	populate_cause(&cse, 0);

	src_custom = (int *)malloc(sizeof(int) * 4);
	ASSERT_NE(src_custom, nullptr);
	src_custom[0] = 111;
	src_custom[1] = 2222;
	src_custom[2] = 33333;
	src_custom[3] = 444444;

	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CUSTOM, src_custom,
					  &free_sdata3, ADAPTIVED_SDATAF_PERSIST);
	ASSERT_EQ(ret, 0);

	cnt = adaptived_get_shared_data_cnt(&cse);
	ASSERT_EQ(cnt, 1);

	src_str = (char *)malloc(sizeof(char) * 12);
	ASSERT_NE(src_str, nullptr);
	sprintf(src_str, "abc123");

	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_STR, src_str, NULL, 0);
	ASSERT_EQ(ret, 0);

	cnt = adaptived_get_shared_data_cnt(&cse);
	ASSERT_EQ(cnt, 2);

	src_cgstr = (struct adaptived_cgroup_value *)malloc(
			sizeof(struct adaptived_cgroup_value));
	ASSERT_NE(src_cgstr, nullptr);

	cgstr = (char *)malloc(sizeof(char) * 128);
	sprintf(cgstr, "/sys/fs/cgroup/database.slice/test012.scope");

	src_cgstr->type = ADAPTIVED_CGVAL_STR;
	src_cgstr->value.str_value = cgstr;

	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CGROUP, src_cgstr, NULL, 0);
	ASSERT_EQ(ret, 0);

	cnt = adaptived_get_shared_data_cnt(&cse);
	ASSERT_EQ(cnt, 3);

	src_cgflt = (struct adaptived_cgroup_value *)malloc(
			sizeof(struct adaptived_cgroup_value));
	ASSERT_NE(src_cgflt, nullptr);
	src_cgflt->type = ADAPTIVED_CGVAL_FLOAT;
	src_cgflt->value.float_value = 56789.1234;

	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CGROUP, src_cgflt,
					  NULL, ADAPTIVED_SDATAF_PERSIST);
	ASSERT_EQ(ret, 0);

	cnt = adaptived_get_shared_data_cnt(&cse);
	ASSERT_EQ(cnt, 4);

	ret = adaptived_get_shared_data(&cse, 0, &type, &read_data, &flags);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(type, ADAPTIVED_SDATA_CUSTOM);
	ASSERT_EQ(read_data, src_custom);
	ASSERT_EQ(flags, ADAPTIVED_SDATAF_PERSIST);

	dst_custom = (int *)read_data;

	ASSERT_EQ(dst_custom[0], 111);
	ASSERT_EQ(dst_custom[1], 2222);
	ASSERT_EQ(dst_custom[2], 33333);
	ASSERT_EQ(dst_custom[3], 444444);

	ret = adaptived_get_shared_data(&cse, 1, &type, &read_data, &flags);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(type, ADAPTIVED_SDATA_STR);
	ASSERT_EQ(read_data, src_str);
	ASSERT_EQ(flags, (uint32_t)0);

	dst_str = (char *)read_data;
	ASSERT_STREQ(dst_str, "abc123");

	ret = adaptived_get_shared_data(&cse, 2, &type, &read_data, &flags);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(type, ADAPTIVED_SDATA_CGROUP);
	ASSERT_EQ(read_data, src_cgstr);
	ASSERT_EQ(flags, (uint32_t)0);

	dst_cgstr = (struct adaptived_cgroup_value *)read_data;
	ASSERT_EQ(dst_cgstr->type, ADAPTIVED_CGVAL_STR);
	ASSERT_STREQ(dst_cgstr->value.str_value, "/sys/fs/cgroup/database.slice/test012.scope");

	ret = adaptived_get_shared_data(&cse, 3, &type, &read_data, &flags);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(type, ADAPTIVED_SDATA_CGROUP);
	ASSERT_EQ(read_data, src_cgflt);
	ASSERT_EQ(flags, ADAPTIVED_SDATAF_PERSIST);

	dst_cgflt = (struct adaptived_cgroup_value *)read_data;
	ASSERT_EQ(dst_cgflt->type, ADAPTIVED_CGVAL_FLOAT);
	ASSERT_FLOAT_EQ(dst_cgflt->value.float_value, 56789.1234);

	ret = adaptived_get_shared_data(&cse, 4, &type, &read_data, &flags);
	ASSERT_EQ(ret, -ERANGE);

	free_shared_data(&cse, false);

	cnt = adaptived_get_shared_data_cnt(&cse);
	ASSERT_EQ(cnt, 2);

	ret = adaptived_get_shared_data(&cse, 0, &type, &read_data, &flags);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(type, ADAPTIVED_SDATA_CUSTOM);
	ASSERT_EQ(read_data, src_custom);
	ASSERT_EQ(flags, ADAPTIVED_SDATAF_PERSIST);

	ret = adaptived_get_shared_data(&cse, 1, &type, &read_data, &flags);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(type, ADAPTIVED_SDATA_CGROUP);
	ASSERT_EQ(read_data, src_cgflt);
	ASSERT_EQ(flags, ADAPTIVED_SDATAF_PERSIST);

	dst_cgflt = (struct adaptived_cgroup_value *)read_data;
	ASSERT_EQ(dst_cgflt->type, ADAPTIVED_CGVAL_FLOAT);
	ASSERT_FLOAT_EQ(dst_cgflt->value.float_value, 56789.1234);

	free_shared_data(&cse, true);

	cnt = adaptived_get_shared_data_cnt(&cse);
	ASSERT_EQ(cnt, 0);

	free(cse.name);
}

TEST_F(SharedDataTest, UpdateSharedData)
{
	struct adaptived_cgroup_value *src, *dst;
	enum adaptived_sdata_type type;
	struct adaptived_cause cse;
	char *cstr, *src_str;
	void *read_data;
	uint32_t flags;
	int ret, cnt;

	populate_cause(&cse, 0);

	src = (struct adaptived_cgroup_value *)malloc(sizeof(struct adaptived_cgroup_value));

	cstr = (char *)malloc(sizeof(char) * 128);
	ASSERT_NE(cstr, nullptr);
	sprintf(cstr, "/sys/fs/cgroup/test012.slice/");

	src->type = ADAPTIVED_CGVAL_STR;
	src->value.str_value = cstr;

	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CGROUP, src, NULL, 0);
	ASSERT_EQ(ret, 0);

	cnt = adaptived_get_shared_data_cnt(&cse);
	ASSERT_EQ(cnt, 1);

	src_str = (char *)malloc(sizeof(char) * 16);
	ASSERT_NE(src_str, nullptr);
	sprintf(src_str, "test012.slice");

	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_STR, src_str, NULL, 0);
	ASSERT_EQ(ret, 0);

	ret = adaptived_get_shared_data(&cse, 0, &type, &read_data, &flags);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(type, ADAPTIVED_SDATA_CGROUP);
	ASSERT_EQ(read_data, src);
	ASSERT_EQ(flags, (uint32_t)0);

	dst = (struct adaptived_cgroup_value *)read_data;
	ASSERT_EQ(dst->type, ADAPTIVED_CGVAL_STR);
	ASSERT_STREQ(dst->value.str_value, "/sys/fs/cgroup/test012.slice/");

	/* free the string stored in the cgroup struct */
	adaptived_free_cgroup_value(dst);
	/* free the entire cgroup struct */
	free(dst);

	/* create and populate a new cgroup struct */
	src = (struct adaptived_cgroup_value *)malloc(sizeof(struct adaptived_cgroup_value));

	cstr = (char *)malloc(sizeof(char) * 128);
	ASSERT_NE(cstr, nullptr);
	sprintf(cstr, "/sys/fs/cgroup/test012.slice/database.scope");

	src->type = ADAPTIVED_CGVAL_STR;
	src->value.str_value = cstr;

	ret = adaptived_update_shared_data(&cse, 0, ADAPTIVED_SDATA_CGROUP, src, 0);
	ASSERT_EQ(ret, 0);

	ret = adaptived_get_shared_data(&cse, 0, &type, &read_data, &flags);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(type, ADAPTIVED_SDATA_CGROUP);
	ASSERT_EQ(read_data, src);
	ASSERT_EQ(flags, (uint32_t)0);

	dst = (struct adaptived_cgroup_value *)read_data;
	ASSERT_EQ(dst->type, ADAPTIVED_CGVAL_STR);
	ASSERT_STREQ(dst->value.str_value, "/sys/fs/cgroup/test012.slice/database.scope");

	ret = adaptived_get_shared_data(&cse, 1, &type, &read_data, &flags);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(type, ADAPTIVED_SDATA_STR);
	ASSERT_EQ(read_data, src_str);
	ASSERT_EQ(flags, (uint32_t)0);

	free(read_data);

	src_str = (char *)malloc(sizeof(char) * 40);
	ASSERT_NE(src_str, nullptr);
	sprintf(src_str, "This is an even longer string :)");

	ret = adaptived_update_shared_data(&cse, 1, ADAPTIVED_SDATA_STR, src_str,
					   ADAPTIVED_SDATAF_PERSIST);
	ASSERT_EQ(ret, 0);

	free_shared_data(&cse, false);

	cnt = adaptived_get_shared_data_cnt(&cse);
	ASSERT_EQ(cnt, 1);

	ret = adaptived_get_shared_data(&cse, 0, &type, &read_data, &flags);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(type, ADAPTIVED_SDATA_STR);
	ASSERT_EQ(read_data, src_str);
	ASSERT_EQ(flags, ADAPTIVED_SDATAF_PERSIST);

	free_shared_data(&cse, true);

	cnt = adaptived_get_shared_data_cnt(&cse);
	ASSERT_EQ(cnt, 0);

	free(cse.name);
}
