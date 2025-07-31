/*
 * Copyright (c) 2024-2025, Oracle and/or its affiliates.
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
 * Cause that will gather cgroup data into a shared data structure
 *
 */

#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include <adaptived-utils.h>
#include <adaptived.h>

#include "adaptived-internal.h"
#include "shared_data.h"
#include "defines.h"

static const char * const slash_path = "/";

struct cgroup_data_opts {
	char *cgroup_path;
	size_t cgroup_path_len; /* calculated from the original path provided */
	char **settings;
	int settings_cnt; /* calculated from the length of the settings json array */
	int max_depth; /* optional */
	bool rel_paths; /* optional */
};

static void free_opts(struct cgroup_data_opts * const opts)
{
	int i;

	if (!opts)
		return;

	if (opts->cgroup_path)
		free(opts->cgroup_path);

	for (i = 0; i < opts->settings_cnt; i++) {
		if (opts->settings[i])
			free(opts->settings[i]);
	}

	free(opts);
}

int cgroup_data_init(struct adaptived_cause * const cse, struct json_object *args_obj,
		     int interval)
{
	struct json_object *settings_obj, *setting_obj;
	const char *cgroup_path, *setting;
	struct cgroup_data_opts *opts;
	json_bool exists;
	int i, ret = 0;

	opts = malloc(sizeof(struct cgroup_data_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}

	memset(opts, 0, sizeof(struct cgroup_data_opts));

	ret = adaptived_parse_string(args_obj, "cgroup", &cgroup_path);
	if (ret) {
		adaptived_err("Failed to parse the cgroup path: %d\n", ret);
		goto error;
	}

	/* +1 to add a trailing "/", +1 to add a trailing "*", and +1 for the NULL charactoer */
	opts->cgroup_path = malloc(sizeof(char) * (strlen(cgroup_path) + 3));
	if (!opts->cgroup_path) {
		ret = -ENOMEM;
		goto error;
	}

	strcpy(opts->cgroup_path, cgroup_path);
	opts->cgroup_path[strlen(cgroup_path)] = '\0';

	/* Remove all trailing "/" characters */
	i = strlen(opts->cgroup_path) - 1;
	while (opts->cgroup_path[i] == '/') {
		opts->cgroup_path[i] = '\0';
		i--;
	}

	opts->cgroup_path_len = strlen(opts->cgroup_path);

	/*
	 * Add a trailing '/' and '*' to the path.  This instructs the path walk code to
	 * not list the starting directory.
	 */
	opts->cgroup_path[strlen(opts->cgroup_path)] = '/';
	opts->cgroup_path[strlen(opts->cgroup_path)] = '*';

	exists = json_object_object_get_ex(args_obj, "settings", &settings_obj);
	if (!exists || !settings_obj) {
		ret = -EINVAL;
		goto error;
	}

	opts->settings_cnt = json_object_array_length(settings_obj);

	opts->settings = malloc(sizeof(char *) * opts->settings_cnt);
	if (!opts->settings) {
		ret = -ENOMEM;
		goto error;
	}
	memset(opts->settings, 0, sizeof(char *) * opts->settings_cnt);

	for (i = 0; i < opts->settings_cnt; i++) {
		setting_obj = json_object_array_get_idx(settings_obj, i);
		if (!setting_obj) {
			ret = -EINVAL;
			goto error;
		}

		ret = adaptived_parse_string(setting_obj, "setting", &setting);
		if (ret) {
			adaptived_err("Failed to parse the setting: %d\n", ret);
			goto error;
		}

		opts->settings[i] = malloc(sizeof(char) * (strlen(setting) + 1));
		if (!opts->settings[i]) {
			ret = -ENOMEM;
			goto error;
		}

		strcpy(opts->settings[i], setting);
		opts->settings[i][strlen(setting)] = '\0';
	}

	ret = adaptived_parse_int(args_obj, "max_depth", &opts->max_depth);
	if (ret == -ENOENT) {
		/* If unspecified, only enumerate the direct children */
		opts->max_depth = 0;
		ret = 0;
	} else if (ret) {
		goto error;
	}

	ret = adaptived_parse_bool(args_obj, "rel_paths", &opts->rel_paths);
	if (ret == -ENOENT) {
		opts->rel_paths = true;
		ret = 0;
	} else if (ret) {
		adaptived_err("Failed to parse rel_paths arg: %d", ret);
		goto error;
	}

	ret = adaptived_cause_set_data(cse, (void *)opts);
	if (ret)
		goto error;

	return ret;

error:
	free_opts(opts);
	return ret;
}

static int read_settings(struct adaptived_cause * const cse, struct cgroup_data_opts * const opts,
			 const char * const cg_path)
{
	char *setting_path = NULL;
	struct adaptived_cgroup_value val;
	size_t setting_path_len;
	const char *sdata_path;
	int ret, i;

	for (i = 0; i < opts->settings_cnt; i++) {
		/* Add 1 for the middle "/" and 1 for the null character */
		setting_path_len = sizeof(char) * (strlen(cg_path) + strlen(opts->settings[i]) + 2);

		setting_path = malloc(setting_path_len);
		if (!setting_path) {
			ret = -ENOMEM;
			goto error;
		}

		sprintf(setting_path, "%s/%s", cg_path, opts->settings[i]);

		val.type = ADAPTIVED_CGVAL_DETECT;
		ret = adaptived_cgroup_get_value(setting_path, &val);
		if (ret == -ENOENT) {
			/*
			 * This cgroup doesn't have the requested file.  It's likely that
			 * the particular controller isn't enabled for this cgroup, and
			 * thus this isn't a fatal error.  Continue on
			 */
			free(setting_path);
			setting_path = NULL;
			ret = 0;
			continue;
		} else if (ret) {
			goto error;
		}

		if (opts->rel_paths) {
			if (strlen(cg_path) <= opts->cgroup_path_len + 1)
				/*
				 * Since we currently don't publish cgroup data for the root
				 * directory, this line should be impossible to hit, but I'm
				 * leaving it in out of an abundance of caution.
				 */
				sdata_path = slash_path;
			else
				sdata_path = &cg_path[opts->cgroup_path_len + 1];
		} else {
			sdata_path = cg_path;
		}

		ret = write_sdata_cgroup_setting_value(cse, sdata_path, opts->settings[i], &val,
						       ADAPTIVED_SDATAF_PERSIST);
		if (ret)
			goto error;

		free(setting_path);
		setting_path = NULL;
	}

error:
	if (setting_path)
		free(setting_path);

	return ret;
}

int cgroup_data_main(struct adaptived_cause * const cse, int time_since_last_run)
{
	struct cgroup_data_opts *opts = (struct cgroup_data_opts *)adaptived_cause_get_data(cse);
	struct adaptived_path_walk_handle *handle = NULL;
	char *cur_path = NULL;
	int ret;

	free_shared_data(cse, true);

	ret = adaptived_path_walk_start(opts->cgroup_path, &handle,
					ADAPTIVED_PATH_WALK_LIST_DIRS, opts->max_depth);
	if (ret)
		goto error;

	do {
		ret = adaptived_path_walk_next(&handle, &cur_path);
		if (ret)
			goto error;
		if (!cur_path)
			/* We've reached the end of the tree */
			break;

		ret = read_settings(cse, opts, cur_path);
		if (ret)
			goto error;

		free(cur_path);
	} while (true);

error:
	adaptived_path_walk_end(&handle);
	if (cur_path)
		free(cur_path);

	if (ret == 0)
		/* Unless there is an error, this cause always triggers */
		return 1;

	return ret;
}

void cgroup_data_exit(struct adaptived_cause * const cse)
{
	struct cgroup_data_opts *opts = (struct cgroup_data_opts *)adaptived_cause_get_data(cse);

	free_shared_data(cse, true);
	free_opts(opts);
}
