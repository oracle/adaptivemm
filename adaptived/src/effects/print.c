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
	const struct adaptived_cause *cse;
	char *msg;
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
	} else if (strncmp(file_str, "stdout", strlen("stdout")) == 0) {
		opts->file = stdout;
	} else {
		ret = -EINVAL;
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

	return 0;
}

void print_exit(struct adaptived_effect * const eff)
{
	struct print_opts *opts = (struct print_opts *)eff->data;

	if (opts->msg)
		free(opts->msg);
	free(opts);
}
