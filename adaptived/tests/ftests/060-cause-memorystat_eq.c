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
 * Equality test for the memory.stat cause
 *
 */

#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

#include <adaptived.h>

#include "ftests.h"

#define EXPECTED_RET -3456


typedef int (*adaptived_injection_function)(struct adaptived_ctx * const ctx);
extern int adaptived_register_injection_function(struct adaptived_ctx * const ctx,
					      adaptived_injection_function fn);

static const char * const stat_file = "060-memory.stat";
static const char * const expected_file = "060-cause-memorystat_eq.expected";
static const char * const token_file = "060-cause-memorystat_eq.token";
static const char * const token = "%lld";
static const char *rel_path;

static const long long values[] = {
	123456789LL, 123456788LL, 123456787LL, 123456786LL, 123456786LL,
	123456785LL, 123456784LL, 123456783LL, 123456782LL, 123456781LL,
};
static int ctr;

static int inject(struct adaptived_ctx * const ctx)
{
	char *line = NULL, *line_copy = NULL, *subp;
	FILE *rfp = NULL, *wfp = NULL;
	char token_path[FILENAME_MAX];
	ssize_t cread;
	int ret = 0;
	size_t len;

	snprintf(token_path, FILENAME_MAX - 1, "%s/%s", rel_path, token_file);
	rfp = fopen(token_path, "r");
	if (!rfp) {
		ret = -errno;
		goto err;
	}

	wfp = fopen(stat_file, "w");
	if (!wfp) {
		ret = -errno;
		goto err;
	}

	while ((cread = getline(&line, &len, rfp)) != -1) {
		subp = strstr(line, token);
		if (subp) {
			line_copy = strdup(line);
			if (!line_copy)
				goto err;

			line = realloc(line, sizeof(char) * (strlen(line) + 10));
			if (!line)
				goto err;

			adaptived_dbg("Writing %lld to %s\n", values[ctr], stat_file);
			ret = vsprintf_wrapper(line, line_copy, values[ctr]);
			if (ret) {
				adaptived_err("vsprintf failed: %d\n", ret);
				goto err;
			}

			free(line_copy);
			line_copy = NULL;
		}

		ret = fwrite(line, sizeof(char), strlen(line), wfp);
		if (ret <= 0) {
			adaptived_err("write failed: %d\n", errno);
			goto err;
		}
		ret = 0;
	}

err:
	if (wfp)
		fclose(wfp);
	if (rfp)
		fclose(rfp);

	if (line)
		free(line);
	if (line_copy)
		free(line_copy);

	ctr++;

	return ret;
}

int main(int argc, char *argv[])
{
	char path[FILENAME_MAX];
	struct adaptived_ctx *ctx;
	int ret;

	rel_path = argv[1];
	snprintf(path, FILENAME_MAX - 1, "%s/060-cause-memorystat_eq.json", rel_path);
	path[FILENAME_MAX - 1] = '\0';

	ctr = 0;

	ctx = adaptived_init(path);
	if (!ctx)
		goto err;

	ret = adaptived_register_injection_function(ctx, inject);
	if (ret)
		goto err;

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, ARRAY_SIZE(values));
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 333);
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

	adaptived_release(&ctx);

	snprintf(path, FILENAME_MAX - 1, "%s/%s", rel_path, expected_file);
	ret = compare_files(stat_file, path);
	if (ret)
		goto err;

	(void)remove(stat_file);
	return AUTOMAKE_PASSED;

err:
	if (ctx)
		adaptived_release(&ctx);

	(void)remove(stat_file);
	return AUTOMAKE_HARD_ERROR;
}
