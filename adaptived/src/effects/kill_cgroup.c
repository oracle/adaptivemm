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
 * An effect to kill one or more processes in a cgroup
 *
 */

#include <sys/param.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include <adaptived-utils.h>

#include "adaptived-internal.h"
#include "defines.h"

#define DEFAULT_SIGNAL SIGKILL

struct kill_cg_opts {
	char *cgroup_path;

	int signal; /* optional */
	int count; /* optional */
	int max_depth; /* optional */
};

int kill_cgroup_init(struct adaptived_effect * const eff, struct json_object *args_obj,
		     const struct adaptived_cause * const cse)
{
	struct kill_cg_opts *opts;
	const char *cgroup_path_str;
	int ret = 0;

	opts = malloc(sizeof(struct kill_cg_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}

	memset(opts, 0, sizeof(struct kill_cg_opts));

	ret = adaptived_parse_string(args_obj, "cgroup", &cgroup_path_str);
	if (ret)
		goto error;

	ret = adaptived_file_exists(cgroup_path_str);
	if (ret)
		goto error;

	opts->cgroup_path = malloc(sizeof(char) * strlen(cgroup_path_str) + 1);
	if (!opts->cgroup_path) {
		ret = -ENOMEM;
		goto error;
	}

	strcpy(opts->cgroup_path, cgroup_path_str);
	opts->cgroup_path[strlen(cgroup_path_str)] = '\0';

	ret = adaptived_parse_int(args_obj, "signal", &opts->signal);
	if (ret == -ENOENT) {
		opts->signal = DEFAULT_SIGNAL;
		ret = 0;
	} else if (ret) {
		goto error;
	}

	ret = adaptived_parse_int(args_obj, "count", &opts->count);
	if (ret == -ENOENT) {
		opts->count = -1;
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

static int kill_cgroup(struct kill_cg_opts * const opts, const char * const cgroup_path,
		       int * const killed_cnt)
{
	int ret, pid_count, i, max_count;
	pid_t *pids = NULL;

	ret = adaptived_cgroup_get_procs(cgroup_path, &pids, &pid_count);
	if (ret)
		goto error;

	if (opts->count > 0)
		max_count = MIN(opts->count, pid_count);
	else
		max_count = pid_count;

	adaptived_dbg("kill_cgroup: Killing %d processes in %s\n", max_count, cgroup_path);

	for (i = 0; i < max_count; i++) {
		ret = kill(pids[i], opts->signal);
		(*killed_cnt)++;
		if (ret < 0) {
			/*
			 * Processes are transient, and the inability to kill one is not
			 * a significant enough error to propagate up to the user.
			 */
			adaptived_info("kill_cgroup: failed to kill process %d, errno = %d\n",
				    pids[i], errno);
			ret = 0;
		}
	}

error:
	if (pids)
		free(pids);

	return ret;
}

int kill_cgroup_main(struct adaptived_effect * const eff)
{
	struct kill_cg_opts *opts = (struct kill_cg_opts *)eff->data;
	struct adaptived_path_walk_handle *handle = NULL;
	char *cgroup_path = NULL;
	int ret, killed = 0;

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

		ret = kill_cgroup(opts, cgroup_path, &killed);
		if (ret)
			goto error;
		free(cgroup_path);

		if (killed >= opts->count)
			break;
	} while (true);

error:
	adaptived_path_walk_end(&handle);

	return ret;
}

void kill_cgroup_exit(struct adaptived_effect * const eff)
{
	struct kill_cg_opts *opts = (struct kill_cg_opts *)eff->data;

	if (opts->cgroup_path)
		free(opts->cgroup_path);
	free(opts);
}
