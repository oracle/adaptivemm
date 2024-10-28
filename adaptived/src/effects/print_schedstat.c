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
 * print per-cpu scheduler stats
 *
 *
 *
 */

#include <assert.h>
#include <string.h>
#include <errno.h>

#include <adaptived-utils.h>

#include "adaptived-internal.h"
#include "defines.h"

enum file_enum {
	FILE_STDOUT = 0,
	FILE_STDERR,

	FILE_CNT
};

struct print_opts {
	FILE *file;
	char *schedstat_file;

	const struct adaptived_cause *cse;
};

static const char * const default_schedstat_file = "/proc/schedstat";

int print_schedstat_init(struct adaptived_effect * const eff, struct json_object *args_obj,
	       const struct adaptived_cause * const cse)
{
	const char *file_str, *schedstat_file_str;
	struct print_opts *opts;
	int ret = 0;

	opts = malloc(sizeof(struct print_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}

	memset(opts, 0, sizeof(struct print_opts));

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

	ret = adaptived_parse_string(args_obj, "schedstat_file", &schedstat_file_str);
	if (ret == -ENOENT) {
		opts->schedstat_file = malloc(sizeof(char) * (strlen(default_schedstat_file) + 1));
		if (!opts->schedstat_file) {
			ret = -ENOMEM;
			goto error;
		}

		strcpy(opts->schedstat_file, default_schedstat_file);
	} else if (ret) {
		goto error;
	} else {
		opts->schedstat_file = malloc(sizeof(char) * (strlen(schedstat_file_str) + 1));
		if (!opts->schedstat_file) {
			ret = -ENOMEM;
			goto error;
		}

		strcpy(opts->schedstat_file, schedstat_file_str);
	}

	opts->cse = cse;

	eff->data = (void *)opts;

	return ret;

error:
	if (opts)
		free(opts);

	return ret;
}

int print_schedstat_main(struct adaptived_effect * const eff)
{
	struct print_opts *opts = (struct print_opts *)eff->data;
	const struct adaptived_cause *cse;
	struct adaptived_schedstat_snapshot ss;
	struct adaptived_schedstat_cpu *ss_cpu;
	struct adaptived_schedstat_domain *ss_domain;
	int cpu, domain, ret = 0;

	ret = adaptived_get_schedstat(opts->schedstat_file, &ss);
	if (ret) {
		adaptived_err("print_schedstat_main: failed to get schedstat\n");
		return -errno;
	}

	cse = opts->cse;
	while (cse) {
		fprintf(opts->file, "Timestamp (jiffies/ticks): %lu:\n", ss.timestamp);
		for(cpu = 0; cpu < ss.nr_cpus; cpu++) {
			ss_cpu = &(ss.schedstat_cpus[cpu]);
			fprintf(opts->file, "CPU%d:\n", cpu);
			fprintf(opts->file, "\tNumber of wakeups from this CPU: %u\n", ss_cpu->ttwu);
			fprintf(opts->file, "\tNumber of wakeups to this CPU:  %u\n", ss_cpu->ttwu_local);
			fprintf(opts->file, "\tTotal task run time (nanoseconds):  %llu\n", ss_cpu->run_time);
			fprintf(opts->file, "\tTotal task wait time (nanoseconds):  %llu\n", ss_cpu->run_delay);
			fprintf(opts->file, "\tNumber of timeslices on this CPU:  %lu\n", ss_cpu->nr_timeslices);
			for(domain = 0; domain < ss_cpu->nr_domains; domain++) {
				ss_domain = &(ss_cpu->schedstat_domains[domain]);
				fprintf(opts->file, "Domain%d:\n", domain);
				fprintf(opts->file, "\tNumber of remote wakeups: %u\n", ss_domain->ttwu_remote);
				fprintf(opts->file, "\tNumber of affine wakeups: %u\n", ss_domain->ttwu_move_affine);
			}
		}
		cse = cse->next;
	}

	return 0;
}

void print_schedstat_exit(struct adaptived_effect * const eff)
{
	struct print_opts *opts = (struct print_opts *)eff->data;

	if (opts->schedstat_file)
		free(opts->schedstat_file);

	free(opts);
}
