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
 * Test for the cgroup memory setting cause
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

#define EXPECTED_RET -66

static const char * const setting_file = "066-cause-cgroup_memory_setting.setting";
static int ctr = 0;

typedef int (*adaptived_injection_function)(struct adaptived_ctx * const ctx);
extern int adaptived_register_injection_function(struct adaptived_ctx * const ctx,
					      adaptived_injection_function fn);

static int value[] = { 0, 10000, 9000, 8000, 7000, 6000, 5000, };
static int expected_value = 5000;
static char maxbuf[FILENAME_MAX];

static int inject(struct adaptived_ctx * const ctx)
{
	int fd, ret = 0;
	char buf[16];
	ssize_t w, r;

	if (ctr >= ARRAY_SIZE(value))
		return -E2BIG;

	if (ctr == 1) { /* save value converted from "max" */
		fd = open(setting_file, O_RDONLY);
		if (fd <= 0)
			return -EINVAL;
		memset(maxbuf, 0, FILENAME_MAX);
		r = read(fd, maxbuf, FILENAME_MAX);
		close(fd);
		if (r <= 0) {
			ret = -errno;
			goto err;
		}
	}
	fd = open(setting_file, O_TRUNC | O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
	if (fd <= 0)
		return -EINVAL;

	memset(buf, 0, sizeof(buf));

	if (ctr == 0) {
		strcpy(buf, "max");
	} else {
		w = sprintf(buf, "%d", value[ctr]);
		if (w <= 0 || w >= 16) {
			ret = -errno;
			goto err;
		}
	}
	w = write(fd, buf, strlen(buf));
	if (w <= 0)
		ret = -errno;

err:
	close(fd);
	ctr++;

	return ret;
}

int main(int argc, char *argv[])
{
	char config_path[FILENAME_MAX];
	struct adaptived_ctx *ctx;
	int ret, br;
	char buf[FILENAME_MAX];
	long long memtotal, val;
	FILE *fp;
	char *line = NULL;
	size_t len = 0;

	snprintf(config_path, FILENAME_MAX - 1, "%s/066-cause-cgroup_memory_setting_ll_lt.json", argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	ctx = adaptived_init(config_path);
	if (!ctx)
		goto err;

	ret = adaptived_register_injection_function(ctx, inject);
	if (ret)
		goto err;

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 7);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 5000);
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

	ret = verify_int_file(setting_file, expected_value);
	if (ret)
		goto err;
	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		adaptived_err("Test 066: can't open top file %s\n", "/proc/meminfo");
		return -errno;
	}
	br = getline(&line, &len, fp);
	fclose(fp);
	if (br < 0 || !line) {
		adaptived_err("Test 066: read of %s failed.\n", "/proc/meminfo");
		return -errno;
	}
	line[strcspn(line, "\n")] = '\0';
	memset(buf, 0, FILENAME_MAX);
	strcpy(buf, line);
	/* MemTotal:       527700340 kB */
	sscanf(buf, "MemTotal:       %lld kB", &memtotal);
	memtotal *= 1024;

	val = strtol(maxbuf, 0, 0);
	if (val != memtotal) {
		adaptived_err("Test 066: max converted value (%lld) != MemTotal (%lld)\n", val, memtotal);
		goto err;
	}
	adaptived_release(&ctx);
	(void)remove(setting_file);
	return AUTOMAKE_PASSED;

err:
	if (ctx)
		adaptived_release(&ctx);

	(void)remove(setting_file);
	return AUTOMAKE_HARD_ERROR;
}
