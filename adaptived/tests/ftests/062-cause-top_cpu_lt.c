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
 * Cause test for the top cpu field <
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

static const char * const setting_file = "062-cause-top_cpu_lt.setting";
static const char * const sample_file = "061-proc_stat.sample";
static int ctr = 0;

typedef int (*adaptived_injection_function)(struct adaptived_ctx * const ctx);
extern int adaptived_register_injection_function(struct adaptived_ctx * const ctx,
					      adaptived_injection_function fn);

#define TOP_LINE0	"cpu  2244370900 3881564 530297572 301974353243 19064147 38541878 47012448 0 1920866777 0\n"
#define TOP_LINE1	"cpu0 34044480 20273 4696010 2296319680 84779 622442 29521478 0 31223869 0\n"
#define TOP_LINE2	"cpu1 7551106 18995 4618200 2349991588 107141 266673 3260637 0 4825500 0\n"

static const char * const expected_value = TOP_LINE0;
static char *path;

static int inject(struct adaptived_ctx * const ctx)
{
	int fd, i, ret = 0;
	char buf[FILENAME_MAX];
	ssize_t w;
	FILE *fp;
        char *line = NULL;
        size_t len = 0;
        ssize_t read;
	char sample_fn[FILENAME_MAX];

	snprintf(sample_fn, FILENAME_MAX - 1, "%s/%s", path, sample_file);
	sample_fn[FILENAME_MAX - 1] = '\0';

	fp = fopen(sample_fn, "r");
	if (fp == NULL) {
                adaptived_err("062-cause-top_cpu_lt: can't open %s\n", sample_file);
                return -errno;
        }
	i = 0;
        while ((read = getline(&line, &len, fp)) != -1) {
                if (strncmp("cpu ", line, strlen("cpu ")) == 0) {
			if (i == ctr) {
				break;
			}
			i++;
		}
        }
        fclose(fp);

	if (i != ctr)
		return -E2BIG;

	fd = open(setting_file, O_TRUNC | O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
	if (fd <= 0)
		return -EINVAL;

	if (strlen(line) >= FILENAME_MAX)
		ret = -EINVAL;

	memset(buf, 0, sizeof(buf));

	strcpy(buf, line);
	strcpy(&buf[strlen(buf)], TOP_LINE1);
	strcpy(&buf[strlen(buf)], TOP_LINE2);

	w = write(fd, buf, strlen(buf));
	if (w <= 0)
		ret = -errno;

	close(fd);
	ctr++;

	return ret;
}

/*
 * Validate that the trigger occurred at the expected top value.
 */
static int validate_top(const char *top_file, const char *expected_value_arg)
{
	FILE * fp;
	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	char buf[FILENAME_MAX];
	int i = 0;

	fp = fopen(top_file, "r");
	if (fp == NULL)
		exit(EXIT_FAILURE);

	while ((read = getline(&line, &len, fp)) != -1) {
		line[strcspn(line, "\n")] = '\0';

		memset(buf, 0, FILENAME_MAX);
		switch (i) {
		case 0:
			strcpy(buf, expected_value_arg);
			buf[FILENAME_MAX - 1] = '\0';
			break;
		case 1:
			strcpy(buf, TOP_LINE1);
			break;
		case 2:
			strcpy(buf, TOP_LINE2);
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

	snprintf(config_path, FILENAME_MAX - 1, "%s/062-cause-top_cpu_lt.json", argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	ctx = adaptived_init(config_path);
	if (!ctx)
		goto err;

	ret = adaptived_register_injection_function(ctx, inject);
	if (ret)
		goto err;

	path = argv[1];

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 66);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 3000);
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

	ret = validate_top(setting_file, expected_value);
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
