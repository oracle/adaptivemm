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
 * Utilities for working with memory
 *
 */

#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

#include <adaptived-utils.h>
#include <adaptived.h>

#include "adaptived-internal.h"

struct slabinfo_columns {
        char name[FILENAME_MAX];
        long long active_objs;
        long long num_objs;
        long long objsize;
        long long objperslab;
        long long pagesperslab;
        long long limit;
        long long batchcount;
        long long sharedfactor;
        long long active_slabs;
        long long num_slabs;
        long long sharedavail;
};

API int adaptived_get_meminfo_field(const char * const meminfo_file,
				 const char * const field, long long * const ll_valuep)
{
	return get_ll_field_in_file(meminfo_file, field, ": ", ll_valuep);
}

API int adaptived_get_slabinfo_field(const char * const slabinfo_file,
				 const char * const field, const char * const column,
				 long long * const ll_valuep)
{
	FILE * fp;
	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	char *bp;
	int ret = 0;
	int items = 0;
	struct slabinfo_columns columns;
	bool slabinfo_found = false;
	bool column_found = false;

	if ((!field) || (!column) || (!ll_valuep))
		return -EINVAL;

	if (slabinfo_file)
		fp = fopen(slabinfo_file, "r");
	else
		fp = fopen(PROC_SLABINFO, "r");
	if (fp == NULL) {
		adaptived_err("adaptived_get_slabinfo_field: can't open slabinfo file.\n");
		return -errno;
	}
        while ((read = getline(&line, &len, fp)) != -1) {
		if (!slabinfo_found) {
			if (strncmp(line, "# name", strlen("# name")) == 0)
				slabinfo_found = true;
			continue;
		}
                line[strcspn(line, "\n")] = '\0';

                bp = line;
                items = sscanf(bp, "%s %lld %lld %lld %lld %lld : tunables %lld %lld %lld : slabdata %lld %lld %lld",
                    columns.name, &columns.active_objs, &columns.num_objs, &columns.objsize, &columns.objperslab,
                    &columns.pagesperslab, &columns.limit, &columns.batchcount,
                    &columns.sharedfactor, &columns.active_slabs, &columns.num_slabs, &columns.sharedavail);
                if (items != 12) {
                        adaptived_err("adaptived_get_slabinfo_field: sscanf error. Items should be 12, but got %d\n", items);
                        return -EINVAL;
                }
                if (strcmp(field, columns.name) == 0) {
			column_found = true;
                        break;
		}
        }
	if (!column_found)
		goto out;
        adaptived_dbg("adaptived_get_slabinfo_field: %s %lld %lld %lld %lld %lld : tunables %lld %lld %lld : slabdata %lld %lld %lld\n",
                columns.name, columns.active_objs, columns.num_objs, columns.objsize, columns.objperslab,
                columns.pagesperslab, columns.limit, columns.batchcount,
                columns.sharedfactor, columns.active_slabs, columns.num_slabs, columns.sharedavail);

        if (strncmp(column, ACTIVE_OBJS, strlen(ACTIVE_OBJS)) == 0)
                *ll_valuep = columns.active_objs;
        else if (strncmp(column, NUM_OBJS, strlen(NUM_OBJS)) == 0)
                *ll_valuep = columns.num_objs;
        else if (strncmp(column, OBJSIZE, strlen(OBJSIZE)) == 0)
                *ll_valuep = columns.objsize;
        else if (strncmp(column, OBJPERSLAB, strlen(OBJPERSLAB)) == 0)
                *ll_valuep = columns.objperslab;
        else if (strncmp(column, PAGESPERSLAB, strlen(PAGESPERSLAB)) == 0)
                *ll_valuep = columns.pagesperslab;
        else if (strncmp(column, LIMIT, strlen(LIMIT)) == 0)
                *ll_valuep = columns.limit;
        else if (strncmp(column, BATCHCOUNT, strlen(BATCHCOUNT)) == 0)
                *ll_valuep = columns.batchcount;
        else if (strncmp(column, SHAREDFACTOR, strlen(SHAREDFACTOR)) == 0)
                *ll_valuep = columns.sharedfactor;
        else if (strncmp(column, ACTIVE_SLABS, strlen(ACTIVE_SLABS)) == 0)
                *ll_valuep = columns.active_slabs;
        else if (strncmp(column, NUM_SLABS, strlen(NUM_SLABS)) == 0)
                *ll_valuep = columns.num_slabs;
        else if (strncmp(column, SHAREDAVAIL, strlen(SHAREDAVAIL)) == 0)
                *ll_valuep = columns.sharedavail;
	else {
		adaptived_err("adaptived_get_slabinfo_field: unknow column: %s\n", column);
		ret = -EINVAL;
	}

out:
	fclose(fp);
	if (line)
		free(line);

	return ret;
}
