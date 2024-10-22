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
 * PSI pressure cause and trigger header file
 *
 */

#ifndef __PRESSURE_H
#define __PRESSURE_H

#include <adaptived.h>

extern const char * const pressure_name;
extern const char * const pressure_rate_name;

extern const char * const meas_names[];
extern const char * const pressure_type_names[];

enum pressure_type_enum {
	PRESSURE_TYPE_CPU = 0,
	PRESSURE_TYPE_MEMORY,
	PRESSURE_TYPE_IO,

	PRESSURE_TYPE_CNT
};

struct pressure_common_opts {
	/*
	 * JSON tag: pressure_file
	 * Description: path to the PSI pressure file
	 * Required: Yes
	 * Example:
	 * 	/sys/fs/cgroup/machine.slice/cpu.pressure
	 *	/proc/pressure/cpu
	 */
	char *pressure_file;

	/*
	 * JSON tag: measurement
	 * Description: Which PSI measurement to monitor
	 * Required: Yes
	 * Valid Options:
	 * 	some-avg10, some-avg60, some-avg300, some-total,
	 * 	full-avg10, full-avg60, full-avg300, full-total
	 */
	enum adaptived_pressure_meas_enum meas;

	/*
	 * JSON tag: threshold
	 * Description: Threshold at which to trigger this cause
	 * Required: Yes
	 * Note: For total measurements, the long long version is used.
	 *       For avgXX measurements, the float version is used.
	 */
	union {
		float avg;
		long long total;
	} threshold;
};

struct pressure_opts {
	struct pressure_common_opts common;

	/*
	 * JSON tag: duration
	 * Description: Time duration that must be exceeded for this cause to trigger
	 * Required: No
	 * Note: If not provided, this cause will trigger each and every time the PSI
	 * 	 measurement exceeds the threshold
	 */
	int duration;

	/*
	 * JSON tag: operator
	 * Description: Mathematical operation to use to compare against the PSI value
	 * Required: Yes
	 * Valid Options: greaterthan, lessthan
	 */
	enum cause_op_enum op;

	/* internal variables */
	int current_duration; /* how long PSI has currently exceeded the threshold */
};

enum action_enum {
	ACTION_FALLING = 0,
	ACTION_RISING,
	ACTION_CNT
};

struct pressure_rate_opts {
	/* inputs from the user */
	struct pressure_common_opts common;
	enum action_enum action;
	int window_size;
	int advanced_warning;

	/* internal data for calculating the linear regression */
	int data_len;
	int data_sample_cnt;
	float *data;
};

#endif /* __PRESSURE_H */
