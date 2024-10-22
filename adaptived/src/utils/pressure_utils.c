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
 * PSI pressure parser implementation file
 *
 * This file parses PSI pressure
 *
 */

#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <adaptived-utils.h>
#include <adaptived.h>

#include "adaptived-internal.h"

static void get_avgs(const char * const line, struct adaptived_pressure_avgs * const pa)
{
	char *subp;

	subp = strstr(line, "avg10");
	subp = strstr(subp, "=");
	pa->avg10 = strtof(&subp[1], NULL);

	subp = strstr(subp, "avg60");
	subp = strstr(subp, "=");
	pa->avg60 = strtof(&subp[1], NULL);

	subp = strstr(subp, "avg300");
	subp = strstr(subp, "=");
	pa->avg300 = strtof(&subp[1], NULL);

	subp = strstr(subp, "total");
	subp = strstr(subp, "=");
	pa->total = strtoll(&subp[1], 0, 0);
}

API int adaptived_get_pressure(const char * const pressure_file,
			    struct adaptived_pressure_snapshot * const ps)
{
        FILE *fp;
        char *line = NULL;
        size_t len = 0;
        ssize_t nread;

	if (!pressure_file || !ps)
		return -EINVAL;

        fp = fopen(pressure_file, "r");
        if (fp == NULL) {
		adaptived_err("Failed to open pressure file: %s\n", pressure_file);
		return -EINVAL;
        }

        while ((nread = getline(&line, &len, fp)) != -1) {
		if (strncmp(line, "some", 4) == 0)
			get_avgs(line, &ps->some);
		if (strncmp(line, "full", 4) == 0)
			get_avgs(line, &ps->full);
        }
        fclose(fp);

	free(line);

	return 0;
}

API int adaptived_get_pressure_avg(const char * const pressure_file,
				enum adaptived_pressure_meas_enum meas, float * const avg)
{
	struct adaptived_pressure_snapshot ps;
	int ret;

	if (!pressure_file || !avg)
		return -EINVAL;

	ret = adaptived_get_pressure(pressure_file, &ps);
	if (ret)
		return ret;

	switch (meas) {
	case PRESSURE_SOME_AVG10:
		*avg = ps.some.avg10;
		break;
	case PRESSURE_SOME_AVG60:
		*avg = ps.some.avg60;
		break;
	case PRESSURE_SOME_AVG300:
		*avg = ps.some.avg300;
		break;
	case PRESSURE_FULL_AVG10:
		*avg = ps.full.avg10;
		break;
	case PRESSURE_FULL_AVG60:
		*avg = ps.full.avg60;
		break;
	case PRESSURE_FULL_AVG300:
		*avg = ps.full.avg300;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

API int adaptived_get_pressure_total(const char * const pressure_file,
				  enum adaptived_pressure_meas_enum meas, long long * const total)
{
	struct adaptived_pressure_snapshot ps;
	int ret;

	if (!pressure_file || !total)
		return -EINVAL;

	ret = adaptived_get_pressure(pressure_file, &ps);
	if (ret)
		return ret;

	switch (meas) {
	case PRESSURE_SOME_TOTAL:
		(*total) = ps.some.total;
		break;
	case PRESSURE_FULL_TOTAL:
		(*total) = ps.full.total;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
