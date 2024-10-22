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
 * An effect to kill the processes in a cgroup that has the largest PSI value
 *
 */

#include <sys/param.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include <adaptived-utils.h>

#include "adaptived-internal.h"
#include "pressure.h"
#include "defines.h"

#define DEFAULT_SIGNAL SIGKILL

struct kill_cg_opts {
	char *cgroup_path;

	enum pressure_type_enum pressure_type;
	enum adaptived_pressure_meas_enum meas;

	int signal; /* optional */
	int max_depth; /* optional */
};

int kill_cgroup_psi_init(struct adaptived_effect * const eff, struct json_object *args_obj,
			 const struct adaptived_cause * const cse)
{
	const char *cgroup_path_str, *meas_str, *type_str;
	struct kill_cg_opts *opts;
	int ret = 0, i;

	opts = malloc(sizeof(struct kill_cg_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}
	opts->cgroup_path = NULL;

	ret = adaptived_parse_string(args_obj, "cgroup", &cgroup_path_str);
	if (ret)
		goto error;

	opts->cgroup_path = malloc(sizeof(char) * strlen(cgroup_path_str) + 1);
	if (!opts->cgroup_path) {
		ret = -ENOMEM;
		goto error;
	}

	strcpy(opts->cgroup_path, cgroup_path_str);
	opts->cgroup_path[strlen(cgroup_path_str)] = '\0';

	ret = adaptived_parse_string(args_obj, "type", &type_str);
	if (ret)
		goto error;

	opts->pressure_type = PRESSURE_TYPE_CNT;
	for (i = 0; i < PRESSURE_TYPE_CNT; i++) {
		if (strcmp(pressure_type_names[i], type_str) == 0) {
			opts->pressure_type = i;
			break;
		}
	}
	if (opts->pressure_type == PRESSURE_TYPE_CNT) {
		adaptived_err("Invalid pressure type provided: %s\n", type_str);
		ret = -EINVAL;
		goto error;
	}

	ret = adaptived_parse_string(args_obj, "measurement", &meas_str);
	if (ret)
		goto error;

	opts->meas = PRESSURE_MEAS_CNT;
	for (i = 0; i < PRESSURE_MEAS_CNT; i++) {
		if (strcmp(meas_names[i], meas_str) == 0) {
			opts->meas = i;
			break;
		}
	}
	if (opts->meas == PRESSURE_MEAS_CNT || opts->meas == PRESSURE_FULL_TOTAL ||
	    opts->meas == PRESSURE_SOME_TOTAL) {
		adaptived_err("Invalid measurement provided: %s\n", meas_str);
		ret = -EINVAL;
		goto error;
	}

	ret = adaptived_parse_int(args_obj, "signal", &opts->signal);
	if (ret == -ENOENT) {
		opts->signal = DEFAULT_SIGNAL;
		ret = 0;
	} else if (ret) {
		goto error;
	}

	ret = adaptived_parse_int(args_obj, "max_depth", &opts->max_depth);
	if (ret == -ENOENT) {
		opts->max_depth = ADAPTIVED_PATH_WALK_UNLIMITED_DEPTH;
		ret = 0;
	} else if (ret) {
		goto error;
	}

	eff->data = (void *)opts;

	return ret;

error:
	if (opts) {
		if (opts->cgroup_path)
			free(opts->cgroup_path);

		free(opts);
	}

	return ret;
}

static int kill_cgroup(struct kill_cg_opts * const opts, const char * const cgroup_path)
{
	int ret, pid_count, i;
	pid_t *pids = NULL;

	ret = adaptived_cgroup_get_procs(cgroup_path, &pids, &pid_count);
	if (ret)
		goto error;

	adaptived_dbg("kill_cgroup_by_psi: Killing %d processes in %s\n", pid_count, cgroup_path);

	for (i = 0; i < pid_count; i++) {
		ret = kill(pids[i], opts->signal);
		if (ret < 0) {
			/*
			 * Processes are transient, and the inability to kill one is not
			 * a significant enough error to propagate up to the user.
			 */
			adaptived_info("kill_cgroup_by_psi: failed to kill process %d, errno = %d\n",
				    pids[i], errno);
			ret = 0;
		}
	}

error:
	if (pids)
		free(pids);

	return ret;
}

static int get_psi(const struct kill_cg_opts * const opts, const char * const cgroup_path,
		   float * const avg)
{
	char pressure_path[FILENAME_MAX] = { '\0' };
	int ret;

	switch(opts->pressure_type) {
	case PRESSURE_TYPE_CPU:
		sprintf(pressure_path, "%s/cpu.pressure", cgroup_path);
		break;
	case PRESSURE_TYPE_MEMORY:
		sprintf(pressure_path, "%s/memory.pressure", cgroup_path);
		break;
	case PRESSURE_TYPE_IO:
		sprintf(pressure_path, "%s/io.pressure", cgroup_path);
		break;
	default:
		return -EINVAL;
	}

	ret = adaptived_get_pressure_avg(pressure_path, opts->meas, avg);
	if (ret)
		return ret;

	return 0;
}

int kill_cgroup_psi_main(struct adaptived_effect * const eff)
{
	struct kill_cg_opts *opts = (struct kill_cg_opts *)eff->data;
	char *cgroup_path = NULL, *kill_cgroup_path = NULL;
	struct adaptived_path_walk_handle *handle = NULL;
	float psi, max_psi = -1.0f;
	int ret;

	ret = adaptived_path_walk_start(opts->cgroup_path, &handle, ADAPTIVED_PATH_WALK_LIST_DIRS,
				     opts->max_depth);
	if (ret)
		goto error;

	do {
		ret = adaptived_path_walk_next(&handle, &cgroup_path);
		if (ret)
			goto error;
		if (!cgroup_path)
			break;

		ret = get_psi(opts, cgroup_path, &psi);
		if (ret)
			goto error;

		if (psi > max_psi) {
			/*
			 * This cgroup is our current candidate for being killed.
			 * Save it off.
			 */
			if (kill_cgroup_path)
				free(kill_cgroup_path);

			kill_cgroup_path = cgroup_path;
			max_psi = psi;
		} else {
			free(cgroup_path);
		}
	} while (true);

	ret = kill_cgroup(opts, kill_cgroup_path);
	if (ret)
		goto error;

error:
	adaptived_path_walk_end(&handle);

	if (kill_cgroup_path)
		free(kill_cgroup_path);

	return ret;
}

void kill_cgroup_psi_exit(struct adaptived_effect * const eff)
{
	struct kill_cg_opts *opts = (struct kill_cg_opts *)eff->data;

	if (opts->cgroup_path)
		free(opts->cgroup_path);
	free(opts);
}
