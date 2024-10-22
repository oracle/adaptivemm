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
 * Test for the top mem cause
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

static const char * const setting_file = "063-cause-top_mem_gt.setting";
static int ctr = 0;

typedef int (*adaptived_injection_function)(struct adaptived_ctx * const ctx);
extern int adaptived_register_injection_function(struct adaptived_ctx * const ctx,
					      adaptived_injection_function fn);

static long long value[] = { 493452920, 493452940, 493452960, 493452980, 493453000, 493453005, 493453025, 493453045 };
long long expected_value = 493453025;

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

	strcpy(buf, "MemTotal:       527700340 kB\n");
	w = sprintf(&buf[strlen(buf)], "MemFree:        %lld kB\n", value[ctr]);
	adaptived_dbg("value[%d] = %lld\n", ctr, value[ctr]);
	if (w <= 0 || w >= FILENAME_MAX) {
		ret = -errno;
		goto err;
	}
	strcpy(&buf[strlen(buf)], "MemAvailable:   510850712 kB\n");
	strcpy(&buf[strlen(buf)], "Cached:   23147140 kB\n");
	strcpy(&buf[strlen(buf)], "SReclaimable:   4536548 kB\n");
	strcpy(&buf[strlen(buf)], "Buffers:   1076 kB\n");
	w = write(fd, buf, strlen(buf));
	if (w <= 0) {
		ret = -errno;
		goto err;
	}
err:
	close(fd);
	ctr++;

	return ret;
}

/*
 * Validate that the trigger occurred at the expected meminfo value.
 */
static int validate_meminfo(const char *meminfo_file, long long expected_value)
{
	FILE * fp;
	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	char *pat;
	char buf[FILENAME_MAX];
	int i = 0;

	fp = fopen(meminfo_file, "r");
	if (fp == NULL)
		exit(EXIT_FAILURE);

	while ((read = getline(&line, &len, fp)) != -1) {
		line[strcspn(line, "\n")] = '\0';
		pat = NULL;

		switch (i) {
		case 0:
			pat = "MemTotal:       527700340 kB";
			break;
		case 1:
			sprintf(buf, "MemFree:        %lld kB", expected_value);
			buf[FILENAME_MAX - 1] = '\0';
			pat = buf;
			break;
		case 2:
			pat = "MemAvailable:   510850712 kB";
			break;
		}
		if (!pat)
			return -1;

		if (strncmp(line, pat, strlen(pat)) != 0) {
			adaptived_err("%s:%s: expected %s, but got %s\n",
				__FILE__, __func__, pat, line);
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

	snprintf(config_path, FILENAME_MAX - 1, "%s/063-cause-top_mem_gt.json", argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	ctx = adaptived_init(config_path);
	if (!ctx)
		goto err;

	ret = adaptived_register_injection_function(ctx, inject);
	if (ret)
		goto err;

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 20);
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
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_DAEMON_MODE, 1);
	if (ret)
		goto err;

	ret = adaptived_loop(ctx, true);
	if (ret != EXPECTED_RET)
		goto err;

	ret = validate_meminfo(setting_file, expected_value);
	if (ret)
		goto err;

	adaptived_release(&ctx);
	(void)remove(setting_file);
	adaptived_dbg("OK\n");
	return AUTOMAKE_PASSED;

err:
	if (ctx)
		adaptived_release(&ctx);

	(void)remove(setting_file);
	adaptived_err("FAIL: ret=%d\n", ret);
	return AUTOMAKE_HARD_ERROR;
}
