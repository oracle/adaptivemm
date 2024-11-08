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
 * Cause to detect when a top command field has changed.
 *
 */

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <adaptived-utils.h>
#include <adaptived.h>

#include "adaptived-internal.h"
#include "defines.h"

/*
 * From /proc/stat:
 * u == user/us, n == nice/ni, s == system/sy, i == idle/id
 * w == IO-wait/wa (io wait time)
 * x == hi (hardware irq time), y == si (software irq time)
 * z == st (virtual steal time)
 */
struct proc_stat {
	long long user;
	long long nice;
	long long system;
	long long idle;
	long long iowait;
	long long hw_irq_time;
	long long sw_irq_time;
	long long vm_steal_time;
	long long total;
};

struct cpu_line {
	float u_line;	// user
	float n_line;	// nice
	float s_line;	// system
	float i_line;	// idle
	float w_line;	// iowait
	float h_line;	// hardware interrupt
	float x_line;	// software interrupt
	float v_line;	// vm steal
};

struct proc_meminfo {
	long long total;
	long long free;
	long long available;
	long long page_cache;
	long long slab_reclaimable;
	long long buffers;
	long long mem_used;
	long long buff_cached;
};

enum top_field_enum {
        TOP_CPU_USER,
        TOP_CPU_SYSTEM,
        TOP_CPU_NICE,
        TOP_CPU_IDLE,
        TOP_CPU_WAIT,
        TOP_CPU_HI,
        TOP_CPU_SI,
        TOP_CPU_ST,

        TOP_MEM_TOTAL,
        TOP_MEM_FREE,
        TOP_MEM_USED,
        TOP_MEM_BUFF_CACHED
};

struct top_opts {
        enum cause_op_enum op;
        char *stat_file;
        char *meminfo_file;
        enum top_field_enum field;
        struct adaptived_cgroup_value threshold;

	struct proc_stat proc_stat;

	struct cpu_line cpu_line;

	long nproc;

	bool display;
};

static void free_opts(struct top_opts * const opts)
{
	if (!opts)
		return;

	if (opts->stat_file)
		free(opts->stat_file);
	if (opts->meminfo_file)
		free(opts->meminfo_file);

	free(opts);
}

static long long get_diff(long long curr, long long prev)
{
	long long x = curr - prev;

	return ((x < 0) ? 0 : x);
}

static int get_proc_stat_total(struct top_opts *opts)
{
	int items = 0;
	char *bp;
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	struct proc_stat prev_proc_stat;
	long long user_tics, nice_tics, system_tics, idle_tics;
	long long iowait_tics, hw_irq_time_tics, sw_irq_time_tics, vm_steal_time_tics;
	long long total;

	fp = fopen(opts->stat_file, "r");
	if (fp == NULL) {
		adaptived_err("get_proc_stat_total: can't open top file %s\n", opts->stat_file);
		return -errno;
	}
	read = getline(&line, &len, fp);
	fclose(fp);
	if (read < 0 || !line) {
		adaptived_err("get_proc_stat_total: read of %s failed.\n", opts->stat_file);
		return -errno;
	}
	line[strcspn(line, "\n")] = '\0';
	bp = line;
	
	memcpy(&prev_proc_stat, &opts->proc_stat, sizeof(struct proc_stat));

	items = sscanf(bp, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
	    &opts->proc_stat.user, &opts->proc_stat.nice, &opts->proc_stat.system,
	    &opts->proc_stat.idle, &opts->proc_stat.iowait, &opts->proc_stat.hw_irq_time,
	    &opts->proc_stat.sw_irq_time, &opts->proc_stat.vm_steal_time);
	free(line);
	if (items != 8) {
		adaptived_err("get_proc_stat_total: sscanf error. Items should be 8, but got %d\n", items);
		return -EINVAL;
	}
	opts->proc_stat.total =
	    opts->proc_stat.user + opts->proc_stat.nice + opts->proc_stat.system +
	    opts->proc_stat.idle + opts->proc_stat.iowait + opts->proc_stat.hw_irq_time +
	    opts->proc_stat.sw_irq_time + opts->proc_stat.vm_steal_time;
	
	if (!opts->proc_stat.total)
		return 0;

	user_tics = get_diff(opts->proc_stat.user, prev_proc_stat.user);
	nice_tics = get_diff(opts->proc_stat.nice, prev_proc_stat.nice);
	system_tics = get_diff(opts->proc_stat.system, prev_proc_stat.system);
	idle_tics = get_diff(opts->proc_stat.idle, prev_proc_stat.idle);
	iowait_tics = get_diff(opts->proc_stat.iowait, prev_proc_stat.iowait);
	hw_irq_time_tics = get_diff(opts->proc_stat.hw_irq_time, prev_proc_stat.hw_irq_time);
	sw_irq_time_tics = get_diff(opts->proc_stat.sw_irq_time, prev_proc_stat.sw_irq_time);
	vm_steal_time_tics = get_diff(opts->proc_stat.vm_steal_time, prev_proc_stat.vm_steal_time);

	total = user_tics + nice_tics + system_tics + idle_tics + iowait_tics +
	    hw_irq_time_tics + sw_irq_time_tics + vm_steal_time_tics;

	adaptived_dbg("user_tics=%lld, nice_tics=%lld, system_tics=%lld, idle_tics=%lld, "
		"iowait_tics=%lld, hw_irq_time_tics=%lld, sw_irq_time_tics=%lld, "
		"vm_steal_time_tics=%lld, total=%lld\n",
	    user_tics, nice_tics, system_tics, idle_tics, iowait_tics, hw_irq_time_tics,
		sw_irq_time_tics, vm_steal_time_tics, total);

	if (total > 0) {
		opts->cpu_line.u_line = 100.0 * (float)user_tics / (float)total;
		opts->cpu_line.n_line = 100.0 * (float)nice_tics / (float)total;
		opts->cpu_line.s_line = 100.0 * (float)system_tics / (float)total;
		opts->cpu_line.i_line = 100.0 * (float)idle_tics / (float)total;
		opts->cpu_line.w_line = 100.0 * (float)iowait_tics / (float)total;
		opts->cpu_line.h_line = 100.0 * (float)hw_irq_time_tics / (float)total;
		opts->cpu_line.x_line = 100.0 * (float)sw_irq_time_tics / (float)total;
		opts->cpu_line.v_line = 100.0 * (float)vm_steal_time_tics / (float)total;
	} else {
		opts->cpu_line.u_line = 0.0;
		opts->cpu_line.n_line = 0.0;
		opts->cpu_line.s_line = 0.0;
		opts->cpu_line.i_line = 0.0;
		opts->cpu_line.w_line = 0.0;
		opts->cpu_line.h_line = 0.0;
		opts->cpu_line.x_line = 0.0;
		opts->cpu_line.v_line = 0.0;
	}

	if (opts->display) {
		adaptived_info("%%Cpu(s) %5.1f us, %5.1f sy, %5.1f ni, %5.1f id, %5.1f wa, "
		    "%5.1f hi, %5.1f si, %5.1f st\n",
		    opts->cpu_line.u_line, opts->cpu_line.s_line,
		    opts->cpu_line.n_line, opts->cpu_line.i_line, opts->cpu_line.w_line,
		    opts->cpu_line.h_line, opts->cpu_line.x_line, opts->cpu_line.v_line);
	}

	return 0;
}

static int calc_meminfo(struct top_opts *opts, struct proc_meminfo *meminfo)
{
	long long ll_main_cached;
	int ret;

	ret = adaptived_get_meminfo_field(opts->meminfo_file, "MemTotal", &meminfo->total);
	if (ret)
		return ret;
	ret = adaptived_get_meminfo_field(opts->meminfo_file, "MemFree", &meminfo->free);
	if (ret)
		return ret;
	ret = adaptived_get_meminfo_field(opts->meminfo_file, "MemAvailable", &meminfo->available);
	if (ret)
		return ret;
	ret = adaptived_get_meminfo_field(opts->meminfo_file, "Cached", &meminfo->page_cache);
	if (ret)
		return ret;
	ret = adaptived_get_meminfo_field(opts->meminfo_file, "SReclaimable", &meminfo->slab_reclaimable);
	if (ret)
		return ret;
	ret = adaptived_get_meminfo_field(opts->meminfo_file, "Buffers", &meminfo->buffers);
	if (ret)
		return ret;
	ll_main_cached = meminfo->page_cache + meminfo->slab_reclaimable;

	if (meminfo->available > meminfo->total)
		meminfo->available = meminfo->free;
	meminfo->mem_used = meminfo->total - meminfo->free - ll_main_cached - meminfo->buffers;
	if (meminfo->mem_used < 0)
		meminfo->mem_used = meminfo->total - meminfo->free;

	meminfo->buff_cached = meminfo->buffers + ll_main_cached;

	if (opts->display) {
		adaptived_info("KiB Mem : %lld total,  %lld free,  %lld used,  %lld buff/cache\n",
		    meminfo->total / 1024, meminfo->free / 1024, meminfo->mem_used / 1024,
		    meminfo->buff_cached / 1024);
	}
	return 0;
}

int top_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval)
{
	const char *stat_file_str, *meminfo_file_str,  *field_str, *component_str;
	struct top_opts *opts;
	int ret = 0;

	opts = malloc(sizeof(struct top_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}

	memset(opts, 0, sizeof( struct top_opts));

	ret = adaptived_parse_string(args_obj, "component", &component_str);
	if (ret) {
		adaptived_err("Failed to parse the component\n");
		goto error;
	}
	ret = adaptived_parse_string(args_obj, "field", &field_str);
	if (ret) {
		adaptived_err("Failed to parse the field\n");
		goto error;
	}
	ret = adaptived_parse_cgroup_value(args_obj, "threshold", &opts->threshold);
	if (ret)
		goto error;

	if (strcmp(component_str, "cpu") == 0) {
		if (opts->threshold.type != ADAPTIVED_CGVAL_FLOAT) {
			adaptived_err("Only float supported for top cpu.\n");
			ret = -EINVAL;
			goto error;
		}
		adaptived_dbg("top_init: opts->threshold.value.float_value = %5.5f\n",
		    opts->threshold.value.float_value);
		ret = adaptived_parse_string(args_obj, "stat_file", &stat_file_str);
		if (ret == -ENOENT) {
			stat_file_str = PROC_STAT;
			ret = 0;
		} else if (ret) {
			adaptived_err("Failed to parse the stat_file\n");
			goto error;
		}
		opts->stat_file = malloc(sizeof(char) * strlen(stat_file_str) + 1);
		if (!opts->stat_file) {
			ret = -ENOMEM;
			goto error;
		}
		strcpy(opts->stat_file, stat_file_str);
		opts->stat_file[strlen(stat_file_str)] = '\0';
		adaptived_dbg("opts->stat_file: %s\n", opts->stat_file);

		if (strncmp(field_str, "us", 2) == 0)
			opts->field = TOP_CPU_USER;
		else if (strncmp(field_str, "sy", 2) == 0)
			opts->field = TOP_CPU_SYSTEM;
		else if (strncmp(field_str, "ni", 2) == 0)
			opts->field = TOP_CPU_NICE;
		else if (strncmp(field_str, "id", 2) == 0)
			opts->field = TOP_CPU_IDLE;
		else if (strncmp(field_str, "wa", 2) == 0)
			opts->field = TOP_CPU_WAIT;
		else if (strncmp(field_str, "hi", 2) == 0)
			opts->field = TOP_CPU_HI;
		else if (strncmp(field_str, "si", 2) == 0)
			opts->field = TOP_CPU_SI;
		else if (strncmp(field_str, "st", 2) == 0)
			opts->field = TOP_CPU_ST;
		else {
			adaptived_err("top_init: unknown cpu field: %s\n", field_str);
			ret = -EINVAL;
			goto error;
		}
	} else if (strcmp(component_str, "mem") == 0) {
		if (opts->threshold.type != ADAPTIVED_CGVAL_LONG_LONG) {
			adaptived_err("Only long long supported for top mem.\n");
			ret = -EINVAL;
			goto error;
		}
		adaptived_dbg("top_init: opts->threshold.value.ll_value = %lld\n",
		    opts->threshold.value.ll_value);
		ret = adaptived_parse_string(args_obj, "meminfo_file", &meminfo_file_str);
		if (ret == -ENOENT) {
			meminfo_file_str = PROC_MEMINFO;
			ret = 0;
		} else if (ret) {
			adaptived_err("Failed to parse the meminfo_file\n");
			goto error;
		}
		opts->meminfo_file = malloc(sizeof(char) * strlen(meminfo_file_str) + 1);
		if (!opts->meminfo_file) {
			ret = -ENOMEM;
			goto error;
		}
		strcpy(opts->meminfo_file, meminfo_file_str);
		opts->meminfo_file[strlen(meminfo_file_str)] = '\0';
		adaptived_dbg("opts->meminfo_file: %s\n", opts->meminfo_file);

		if (strncmp(field_str, "total", strlen("total")) == 0)
			opts->field = TOP_MEM_TOTAL;
		else if (strncmp(field_str, "free", strlen("free")) == 0)
			opts->field = TOP_MEM_FREE;
		else if (strncmp(field_str, "used", strlen("used")) == 0)
			opts->field = TOP_MEM_USED;
		else if (strncmp(field_str, "buff/cache", strlen("buff/cache")) == 0)
			opts->field = TOP_MEM_BUFF_CACHED;
		else {
			adaptived_err("top_init: unknown mem field: %s\n", field_str);
			ret = -EINVAL;
			goto error;
		}
	} else {
		adaptived_err("top_init: unknown component type: %s\n", component_str);
		ret = -EINVAL;
		goto error;
	}
	ret = parse_cause_operation(args_obj, NULL, &opts->op);
	if (ret)
		goto error;

	opts->nproc = sysconf(_SC_NPROCESSORS_ONLN);

	ret = adaptived_parse_bool(args_obj, "display", &opts->display);
	if (ret == -ENOENT) {
		opts->display = false;;
		ret = 0;
	} else if (ret) {
		adaptived_err("Failed to parse the cgroup_setting display arg: %d\n", ret);
		goto error;
	}
	adaptived_dbg("Cgroup setting: display = %d\n", opts->display);

        ret = adaptived_cause_set_data(cse, (void *)opts);
        if (ret)
                goto error;
	return ret;

error:
	free_opts(opts);
	return ret;
}

int top_main(struct adaptived_cause * const cse, int time_since_last_run)
{
	struct top_opts *opts = (struct top_opts *)adaptived_cause_get_data(cse);
	float float_value = 0;
	long long ll_value = 0;
	struct proc_meminfo meminfo;
	int ret;

	if (opts->field <= TOP_CPU_ST) {
		ret = adaptived_file_exists(opts->stat_file);
		if (ret)
			return ret;
		ret = get_proc_stat_total(opts);
		if (ret) {
			adaptived_err("top_main: get_proc_stat_total() failed. ret=%d\n", ret);
			return ret;
		}
		if (!opts->proc_stat.total) {
			adaptived_dbg("top_main: cause percentages not yet ready...\n");
			return 0;
		}
	} else {
		ret = adaptived_file_exists(opts->meminfo_file);
		if (ret)
			return ret;
		memset(&meminfo, 0, sizeof(struct proc_meminfo));
		ret = calc_meminfo(opts, &meminfo);
		if (ret) {
			adaptived_err("top_main: calc_meminfo() failed. ret=%d\n", ret);
			return ret;
		}
	}
	switch (opts->field) {
	case TOP_CPU_USER:
		float_value = opts->cpu_line.u_line;
		break;
	case TOP_CPU_SYSTEM:
		float_value = opts->cpu_line.s_line;
		break;
	case TOP_CPU_NICE:
		float_value = opts->cpu_line.n_line;
		break;
	case TOP_CPU_IDLE:
		float_value = opts->cpu_line.i_line;
		break;
	case TOP_CPU_WAIT:
		float_value = opts->cpu_line.w_line;
		break;
	case TOP_CPU_HI:
		float_value = opts->cpu_line.h_line;
		break;
	case TOP_CPU_SI:
		float_value = opts->cpu_line.x_line;
		break;
	case TOP_CPU_ST:
		float_value = opts->cpu_line.v_line;
		break;
	case TOP_MEM_TOTAL:
		ll_value = meminfo.total;
		break;
	case TOP_MEM_FREE:
		ll_value = meminfo.free;
		break;
	case TOP_MEM_USED:
		ll_value = meminfo.mem_used;
		break;
	case TOP_MEM_BUFF_CACHED:
		ll_value = meminfo.buff_cached;
		break;
	}

	if (opts->threshold.type == ADAPTIVED_CGVAL_FLOAT) {
		adaptived_dbg("top_main: op=%d, float_value = %5.5f, opts->threshold.value.float_value = %5.5f\n",
		    opts->op, float_value, opts->threshold.value.float_value);
		switch (opts->op) {
		case COP_GREATER_THAN:
			if (float_value > opts->threshold.value.float_value) {
				return 1;
			}
			break;
		case COP_LESS_THAN:
			if (float_value < opts->threshold.value.float_value) {
				return 1;
			}
			break;
		case COP_EQUAL:
			if (float_value == opts->threshold.value.float_value)
				return 1;
			break;
		default:
			return -EINVAL;
		}
	} else {
		adaptived_dbg("top_main: op=%d, ll_value = %lld, opts->threshold.value.ll_value = %lld\n",
		    opts->op, ll_value, opts->threshold.value.ll_value);
		switch (opts->op) {
		case COP_GREATER_THAN:
			if (ll_value > opts->threshold.value.ll_value) {
				return 1;
			}
			break;
		case COP_LESS_THAN:
			if (ll_value < opts->threshold.value.ll_value) {
				return 1;
			}
			break;
		case COP_EQUAL:
			if (ll_value == opts->threshold.value.ll_value)
				return 1;
			break;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

void top_exit(struct adaptived_cause * const cse)
{
	struct top_opts *opts = (struct top_opts *)adaptived_cause_get_data(cse);

	free_opts(opts);
}
