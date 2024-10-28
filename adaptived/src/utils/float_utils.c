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
 * Utilities for managing arrays of floats
 *
 */

#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <adaptived-utils.h>
#include <adaptived.h>

#include "adaptived-internal.h"

API int adaptived_farray_append(float * const array, const float * const value, int array_len,
			     int * const samples_in_array)
{
	if (!array || !value || !samples_in_array || array_len <= 0)
		return -EINVAL;

	if (*samples_in_array > array_len)
		return -EINVAL;

	if (*samples_in_array < array_len) {
		array[*samples_in_array] = *value;
		(*samples_in_array)++;
	} else {
		memmove(array, &array[1], sizeof(float) * (array_len - 1));
		array[array_len - 1] = *value;
	}

	return 0;
}

API int adaptived_farray_linear_regression(float * const array, int array_len, int interval,
					int interp_x, float * const interp_y)
{
	float xmean, ymean, numer, denom, intercept, slope;
	int i;

	if (!array || !interp_y)
		return -EINVAL;
	if (array_len <= 0 || interval <= 0)
		return -EINVAL;

	xmean = 0.0f;
	ymean = 0.0f;
	for (i = 0; i < array_len; i++) {
		ymean += array[i];
		xmean += (float)((i + 1) * interval);
	}

	ymean = ymean / array_len;
	xmean = xmean / array_len;

	numer = 0.0f;
	denom = 0.0f;
	for (i = 0; i < array_len; i++) {
		numer += (((i + 1) * interval) - xmean) * (array[i] - ymean);
		denom += (((i + 1) * interval) - xmean) * (((i + 1) * interval) - xmean);
	}

	slope = numer / denom;
	intercept = ymean - (slope * xmean);
	adaptived_dbg("LinInterp: slope = %.2f yintcpt = %.2f\n", slope, intercept);

	*interp_y = (slope * ((array_len * interval) + interp_x)) + intercept;
	adaptived_dbg("LinInterp: interp_y = %.2f\n", *interp_y);

	return 0;
}
