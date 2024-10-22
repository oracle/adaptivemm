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
 * Test periodic cause & log file effect and verify that
 * that config without separators works.
 *
 */

#include <stdbool.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>

#include <adaptived.h>

#include "ftests.h"

#define EXPECTED_RET -ETIME

static const char * const test_file1 = "test043_test_file1.txt";
static const char * const test_file2 = "test043_test_file2.txt";
static const char * const log_file = "mem.log";

int main(int argc, char *argv[])
{
	char config_path[FILENAME_MAX];
	char expected_buf1[FILENAME_MAX];
	char expected_buf2[FILENAME_MAX];
	struct adaptived_ctx *ctx = NULL;
	int ret;
	FILE *fp = NULL;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int line_no = 0;
	int lines_found = 0;

	adaptived_dbg("\ntest with no separators\n");
	/* test with no separators */
	snprintf(config_path, FILENAME_MAX - 1, "%s/043-effect-logger-no-separators.json", argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	/* test int */
	ctx = adaptived_init(config_path);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	strcpy(expected_buf1, "This was written to test043_test_file1.txt");
	write_file(test_file1, expected_buf1);
	strcpy(expected_buf2, "This was written to test043_test_file2.txt");
	write_file(test_file2, expected_buf2);

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 8);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 500);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_DEBUG);
	if (ret)
		goto err;

	ret = adaptived_loop(ctx, true);
	if (ret != EXPECTED_RET) {
		adaptived_err("Test 043 returned: %d, expected: %d\n", ret, EXPECTED_RET);
		goto err;
	}
	fp = fopen(log_file, "r");
	if (fp == NULL) {
		adaptived_err("logger failure, can't open %s\n", log_file);
		goto err;
	}

	line_no = 0;
	lines_found = 0;
	while ((read = getline(&line, &len, fp)) != -1) {
		adaptived_dbg("%d: %s", line_no, line);
		line[strcspn(line, "\n")] = 0;

		switch(line_no) {
		case 0:
		case 1:
		case 6:
		case 11:
		case 16:
			if (strlen(line) !=0 ) {
				adaptived_err("separator failure, strlen(line) = %d, should be: 0\n",
					strlen(line));
				goto err;
			}
			lines_found++;
			break;
		case 2:
		case 7:
		case 12:
		case 17:
			ret = verify_lines(line, test_file1);
			if (ret)
				goto err;
			lines_found++;
			break;
		case 3:
		case 8:
		case 13:
		case 18:
			ret = verify_lines(line, expected_buf1);
			if (ret)
				goto err;
			lines_found++;
			break;
		case 4:
		case 9:
		case 14:
		case 19:
			ret = verify_lines(line, test_file2);
			if (ret)
				goto err;
			lines_found++;
			break;
		case 5:
		case 10:
		case 15:
		case 20:
			ret = verify_lines(line, expected_buf2);
			if (ret)
				goto err;
			lines_found++;
			break;
		default:
			adaptived_err("unknown line: %s\n", line);
			goto err;
		}
		line_no++;
	}
	fclose(fp);
	if (line_no != lines_found) {
		adaptived_err("%s line numbers (%d) != lines found (%d)\n",
			log_file, line_no, lines_found);
		goto err;
	}
	delete_file(test_file1);
	delete_file(test_file2);
	delete_file(log_file);

	adaptived_release(&ctx);

	return AUTOMAKE_PASSED;

err:
	if (fp)
		fclose(fp);
	delete_file(test_file1);
	delete_file(test_file2);
	delete_file(log_file);

	adaptived_release(&ctx);

	return AUTOMAKE_HARD_ERROR;
}
