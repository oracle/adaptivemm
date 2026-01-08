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
 * print effect
 *
 * This file runs the print effect
 *
 */

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "adaptived-internal.h"
#include "defines.h"

enum file_enum {
	FILE_STDOUT = 0,
	FILE_STDERR,

	FILE_CNT
};

struct print_opts {
	FILE *file;
	bool close_file; /* internal flag to tell us to close the file on exit */
	const struct adaptived_cause *cse;
	char *msg;
	bool shared_data;
};

int print_init(struct adaptived_effect * const eff, struct json_object *args_obj,
	       const struct adaptived_cause * const cse)
{
	const char *file_str, *msg_str;
	struct print_opts *opts;
	int ret = 0;

	opts = malloc(sizeof(struct print_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}
	memset(opts, 0, sizeof(struct print_opts));

	ret = adaptived_parse_string(args_obj, "message", &msg_str);
	if (ret == -ENOENT) {
		/* the user didn't provide a message string */
		opts->msg = NULL;
	} else if (ret) {
		goto error;
	} else {
		/* a message was provided.  use it */
		opts->msg = malloc(sizeof(char) * strlen(msg_str) + 1);
		if (!opts->msg) {
			ret = -ENOMEM;
			goto error;
		}

		strcpy(opts->msg, msg_str);
		opts->msg[strlen(msg_str)] = '\0';
	}

	ret = adaptived_parse_string(args_obj, "file", &file_str);
	if (ret)
		goto error;

	if (strncmp(file_str, "stderr", strlen("stderr")) == 0) {
		opts->file = stderr;
		opts->close_file = false;
	} else if (strncmp(file_str, "stdout", strlen("stdout")) == 0) {
		opts->file = stdout;
		opts->close_file = false;
	} else {
		opts->file = fopen(file_str, "w");
		if (!opts->file) {
			adaptived_err("Failed to fopen %s\n", file_str);
			ret = -EINVAL;
			goto error;
		}

		opts->close_file = true;
	}

	ret = adaptived_parse_bool(args_obj, "shared_data", &opts->shared_data);
	if (ret == -ENOENT) {
		opts->shared_data = false;
		ret = 0;
	} else if (ret) {
		adaptived_err("Failed to parse shared_data arg: %d\n", ret);
		goto error;
	}

	opts->cse = cse;

	/* we have successfully setup the print effect */
	eff->data = (void *)opts;

	return ret;

error:
	if (opts) {
		if (opts->msg)
			free(opts->msg);

		free(opts);
	}

	return ret;
}

static void print_custom(const struct print_opts * const opts, void *data)
{
	fprintf(opts->file, "Cause %s shared custom data 0x%p\n", opts->cse->name, data);
}

static void print_str(const struct print_opts * const opts, char *data)
{
	fprintf(opts->file, "Cause %s shared string \"%s\"\n", opts->cse->name, data);
}

static void print_cgroup(const struct print_opts * const opts, struct adaptived_cgroup_value *data)
{
	switch(data->type) {
	case ADAPTIVED_CGVAL_STR:
		fprintf(opts->file, "Cause %s shared string value \"%s\"\n", opts->cse->name,
			data->value.str_value);
		break;
	case ADAPTIVED_CGVAL_LONG_LONG:
		fprintf(opts->file, "Cause %s shared long long value \"%lld\"\n", opts->cse->name,
			data->value.ll_value);
		break;
	case ADAPTIVED_CGVAL_FLOAT:
		fprintf(opts->file, "Cause %s shared float value \"%f\"\n", opts->cse->name,
			data->value.float_value);
		break;
	default:
		adaptived_err("Unsupported cgroup type: %d\n", data->type);
	}
}

static void print_name_value(const struct print_opts * const opts,
			     struct adaptived_name_and_value *data)
{
	switch(data->value->type) {
	case ADAPTIVED_CGVAL_STR:
		fprintf(opts->file, "Cause %s shared name \"%s\" and string value \"%s\"\n",
			opts->cse->name, data->name, data->value->value.str_value);
		break;
	case ADAPTIVED_CGVAL_LONG_LONG:
		fprintf(opts->file, "Cause %s shared name \"%s\" and long long value \"%lld\"\n",
			opts->cse->name, data->name, data->value->value.ll_value);
		break;
	case ADAPTIVED_CGVAL_FLOAT:
		fprintf(opts->file, "Cause %s shared name \"%s\" and float value \"%f\"\n",
			opts->cse->name, data->name, data->value->value.float_value);
		break;
	default:
		adaptived_err("Unsupported cgroup type: %d\n", data->value->type);
	}
}

static void print_cgroup_setting_and_value(const struct print_opts * const opts,
					   struct adaptived_cgroup_setting_and_value *data)
{
	switch(data->value->type) {
	case ADAPTIVED_CGVAL_STR:
		fprintf(opts->file,
			"Cause \"%s\" shared\n\tcgroup \"%s\"\n\tsetting \"%s\"\n\tstring value \"%s\"\n",
			opts->cse->name, data->cgroup_name, data->setting,
			data->value->value.str_value);
		break;
	case ADAPTIVED_CGVAL_LONG_LONG:
		fprintf(opts->file,
			"Cause \"%s\" shared\n\tcgroup \"%s\"\n\tsetting \"%s\"\n\tlong long value \"%lld\"\n",
			opts->cse->name, data->cgroup_name, data->setting,
			data->value->value.ll_value);
		break;
	case ADAPTIVED_CGVAL_FLOAT:
		fprintf(opts->file,
			"Cause \"%s\" shared\n\tcgroup \"%s\"\n\tsetting \"%s\"\n\tfloat value \"%f\"\n",
			opts->cse->name, data->cgroup_name, data->setting,
			data->value->value.float_value);
		break;
	default:
		adaptived_err("Unsupported cgroup type: %d\n", data->value->type);
	}
}

static void print_shared_data(const struct print_opts * const opts)
{
	const struct adaptived_cause *cse = opts->cse;
	enum adaptived_sdata_type type;
	void *read_data;
	int ret, cnt, i;
	uint32_t flags;

	while (cse) {
		cnt = adaptived_get_shared_data_cnt(cse);

		for (i = 0; i < cnt; i++) {
			ret = adaptived_get_shared_data(cse, i, &type, &read_data, &flags);
			if (ret) {
				adaptived_err("Failed to get shared data: %d\n", ret);
				cnt--;
				continue;
			}

			switch(type) {
			case ADAPTIVED_SDATA_CUSTOM:
				print_custom(opts, read_data);
				break;
			case ADAPTIVED_SDATA_STR:
				print_str(opts, read_data);
				break;
			case ADAPTIVED_SDATA_CGROUP:
				print_cgroup(opts, read_data);
				break;
			case ADAPTIVED_SDATA_NAME_VALUE:
				print_name_value(opts, read_data);
				break;
			case ADAPTIVED_SDATA_CGROUP_SETTING_VALUE:
				print_cgroup_setting_and_value(opts, read_data);
				break;
			default:
				adaptived_err("Unsupported shared data type: %d\n", type);
			}
		}

		cse = cse->next;
	}
}

int print_main(struct adaptived_effect * const eff)
{
	struct print_opts *opts = (struct print_opts *)eff->data;
	const struct adaptived_cause *cse;

	if (opts->msg) {
		fprintf(opts->file, "%s", opts->msg);
	} else {
		fprintf(opts->file, "Print effect triggered by:\n");

		cse = opts->cse;
		while (cse) {
			fprintf(opts->file, "\t%s\n", cse->name);

			cse = cse->next;
		}
	}

	if (opts->shared_data)
		print_shared_data(opts);

	return 0;
}

void print_exit(struct adaptived_effect * const eff)
{
	struct print_opts *opts = (struct print_opts *)eff->data;

	if (opts->close_file)
		fclose(opts->file);
	if (opts->msg)
		free(opts->msg);
	free(opts);
}
