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
 * Utilities for working with files
 *
 */

#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include "adaptived-internal.h"

static bool ends_with(const char * const line, const char * const suffix)
{
	size_t line_len, suffix_len;

	line_len = strlen(line);
	suffix_len = strlen(suffix);

	if (suffix_len > line_len)
		return false;

	if (strncmp(line + line_len - suffix_len, suffix, suffix_len) == 0)
		return true;

	return false;
}

static int parse_suffix(char * const line, long long * const multiplier)
{
	if (ends_with(line, "kb") || ends_with(line, "kB")) {
		line[strlen(line) - 2] = '\0';
		*multiplier = 1024;
	} else if (ends_with(line, "mb") || ends_with(line, "mB")) {
		line[strlen(line) - 2] = '\0';
		*multiplier = 1024 * 1024;
	} else if (ends_with(line, "gb") || ends_with(line, "gB")) {
		line[strlen(line) - 2] = '\0';
		*multiplier = 1024 * 1024 * 1024;
	} else {
		*multiplier = 1;
	}

	return 0;
}

int get_ll_field_in_file(const char * const file, const char * const field,
			 const char * const separator, long long * const ll_valuep)
{
	char *subp, *subp2, *str;
	long long multiplier = 0;
	char * line = NULL;
	int ret = -ENOENT;
	size_t len = 0;
	ssize_t read;
	FILE * fp;

	if (!file || !field || !separator || !ll_valuep)
		return -EINVAL;

	fp = fopen(file, "r");
	if (fp == NULL) {
		adaptived_err("Failed to open %s: errno = %d\n", file, errno);
		return -errno;
	}

	while ((read = getline(&line, &len, fp)) != -1) {
		line[strcspn(line, "\n")] = '\0';
		subp = strstr(line, field);
		if (subp) {
			subp2 = strstr(subp, separator);
			if (subp2) {
				char *endptr;

				str = &subp2[1];

				ret = parse_suffix(str, &multiplier);
				if (ret)
					goto out;

				*ll_valuep = strtoll(str, &endptr, 10);
				if (endptr[0] != '\0' && endptr[0] != '\n' && endptr[0] != ' ') {
					/* There was unparsable data in the string */
					ret = -EINVAL;
					goto out;
				}
				if (endptr == str) {
					/* There was unparsable data in the string */
					ret = -EINVAL;
					goto out;
				}
				if (*ll_valuep == LLONG_MIN || *ll_valuep == LLONG_MAX) {
					ret = -ERANGE;
					goto out;
				}

				ret = 0;
			}
			break;
		}
	}
out:
	fclose(fp);
	if (line)
		free(line);

	if (ret == 0)
		*ll_valuep *= multiplier;

	return ret;
}
