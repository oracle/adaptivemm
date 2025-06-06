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
 * Test for the slabinfo cause
 *
 */

#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>

#include <adaptived.h>

#include "ftests.h"

#define EXPECTED_RET -65

static const char * const setting_file = "052-cause-slabinfo_lt.setting";
static int ctr = 0;

typedef int (*adaptived_injection_function)(struct adaptived_ctx * const ctx);
extern int adaptived_register_injection_function(struct adaptived_ctx * const ctx,
					      adaptived_injection_function fn);

static long long value[] = { 172487, 172486, 172485, 172484, 172483, 172482, 172481 };
long long expected_value = 172483;

#define SLABINFO_LINE0	"# name            <active_objs> <num_objs> <objsize> <objperslab> <pagesperslab> : tunables <limit> <batchcount> <sharedfactor> : slabdata <active_slabs> <num_slabs> <sharedavail>\n"
#define SLABINFO_LINE1	"kmalloc-32        %lld 226688     32  128    1 : tunables    0    0    0 : slabdata   1771   1771      0\n"
#define SLABINFO_LINE2	"kmalloc-16        1021501 1027072     16  256    1 : tunables    0    0    0 : slabdata   4012   4012      0\n"

static int inject(struct adaptived_ctx * const ctx)
{
	int fd, ret = 0;
	char buf[FILENAME_MAX];
	ssize_t w;

	if (ctr >= ARRAY_SIZE(value))
		return -E2BIG;

	fd = open(setting_file, O_TRUNC | O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
	if (fd <= 0)
		return -EINVAL;

	memset(buf, 0, sizeof(buf));

	strcpy(buf, SLABINFO_LINE0);
	w = sprintf(&buf[strlen(buf)], SLABINFO_LINE1, value[ctr]);
	if (w <= 0 || w >= FILENAME_MAX) {
		ret = -errno;
		goto err;
	}
	strcpy(&buf[strlen(buf)], SLABINFO_LINE2);

	w = write(fd, buf, strlen(buf));
	if (w <= 0)
		ret = -errno;

err:
	close(fd);
	ctr++;

	return ret;
}

/*
 * Validate that the trigger occurred at the expected slabinfo value.
 */
static int validate_slabinfo(const char *slabinfo_file, long long expected_value)
{
	FILE * fp;
	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	char buf[FILENAME_MAX];
	int i = 0;

	fp = fopen(slabinfo_file, "r");
	if (fp == NULL)
		exit(EXIT_FAILURE);

	while ((read = getline(&line, &len, fp)) != -1) {
		line[strcspn(line, "\n")] = '\0';

		memset(buf, 0, FILENAME_MAX);
		switch (i) {
		case 0:
			strcpy(buf, SLABINFO_LINE0);
			break;
		case 1:
			sprintf(buf, SLABINFO_LINE1, expected_value);
			buf[FILENAME_MAX - 1] = '\0';
			break;
		case 2:
			strcpy(buf, SLABINFO_LINE2);
			break;
		}
		if (!strlen(buf))
			return -1;

		buf[FILENAME_MAX - 1] = '\0';
		buf[strcspn(buf, "\n")] = '\0';

		if (strncmp(line, buf, strlen(buf)) != 0) {
			adaptived_err("%s:%s:\nexpected %s\n but got %s\n",
				__FILE__, __func__, buf, line);
			return -1;
		}
		if (i == 2)
			break;
		i++;
	}

	fclose(fp);
	if (line)
		free(line);

	if (i != 2) {
		adaptived_err("%s:%s: incorrect line count (%d), should be 2\n",
			__FILE__, __func__, i);
		return -1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	char config_path[FILENAME_MAX];
	struct adaptived_ctx *ctx;
	int ret;

	snprintf(config_path, FILENAME_MAX - 1, "%s/052-cause-slabinfo_lt.json", argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	ctx = adaptived_init(config_path);
	if (!ctx)
		goto err;

	ret = adaptived_register_injection_function(ctx, inject);
	if (ret)
		goto err;

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 6);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 4000);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_SKIP_SLEEP, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_DEBUG);
	if (ret)
		goto err;

	ret = adaptived_loop(ctx, true);
	if (ret != EXPECTED_RET)
		goto err;

	ret = validate_slabinfo(setting_file, expected_value);
	if (ret)
		goto err;

	adaptived_release(&ctx);
	(void)remove(setting_file);
	return AUTOMAKE_PASSED;

err:
	if (ctx)
		adaptived_release(&ctx);

	(void)remove(setting_file);
	return AUTOMAKE_HARD_ERROR;
}
