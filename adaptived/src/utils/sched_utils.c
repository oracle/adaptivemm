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
 * Utilities for working with task scheduler
 *
 */
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

#include <adaptived-utils.h>
#include <adaptived.h>

#include "adaptived-internal.h"

static unsigned long cpumask_to_hex(char * p) {
	char *src = p, *dst = p;

	while (*src) {
		if (ispunct((unsigned char)*src)) {
			src++;
		} else if (src == dst) {
			src++;
			dst++;
		} else {
			*dst++ = *src++;
		}
	}
	*dst = 0;

	return strtoul(dst, NULL, 16);
}

static int adaptived_get_schedstat_cpu(char * token, struct adaptived_schedstat_cpu * const ss_cpu)
{
	enum adaptived_schedstat_cpu_enum i;
	for (i = 1; token != NULL; i++, token = strtok(NULL, " ")) {
		switch (i) {
		case SCHEDSTAT_YLD_DEFUNCT ... SCHEDSTAT_GOIDLE_DEFUNCT:
			/*
			 * Defunct entries kernel keeps them for ABI compatitbility
			 */
			break;
		case SCHEDSTAT_TTWU:
			ss_cpu->ttwu = strtoul(token, NULL, 10);
			break;
		case SCHEDSTAT_TTWU_LOCAL:
			ss_cpu->ttwu_local = strtoul(token, NULL, 10);
			break;
		case SCHEDSTAT_RUN_TIME:
			ss_cpu->run_time = strtoull(token, NULL, 10);
			break;
		case SCHEDSTAT_RUN_DELAY:
			ss_cpu->run_delay = strtoull(token, NULL, 10);
			break;
		case SCHEDSTAT_NR_TIMESLICES:
			ss_cpu->nr_timeslices = strtoul(token, NULL, 10);
			break;
		default:
			adaptived_err("Invalid entry#: %d\n", i);
			return -EINVAL;
		}
	}
	return 0;
}

static int adaptived_get_schedstat_domain(char * token, struct adaptived_schedstat_domain *ss_domain)
{
	enum adaptived_schedstat_domain_enum i;
	enum cpu_idle_type_enum cpu_type;

	ss_domain->cpumask = cpumask_to_hex(token);

	token = strtok(NULL, " ");
	for (i = 0; token != NULL; i++, token = strtok(NULL, " ")) {
		if (i < CPU_MAX_IDLE_TYPES * 8) {
			cpu_type = i / 8;
			switch (i % 8) {
			case SCHEDSTAT_LB_CALLED:
				ss_domain->lb[cpu_type].lb_called += strtoul(token, NULL, 10);
				break;
			case SCHEDSTAT_LB_BALANCED:
				ss_domain->lb[cpu_type].lb_balanced += strtoul(token, NULL, 10);
				break;
			case SCHEDSTAT_LB_FAILED:
				ss_domain->lb[cpu_type].lb_failed += strtoul(token, NULL, 10);
				break;
			case SCHEDSTAT_LB_IMBAL:
				ss_domain->lb[cpu_type].lb_imbal += strtoul(token, NULL, 10);
				break;
			case SCHEDSTAT_LB_GAINED:
				ss_domain->lb[cpu_type].lb_gained += strtoul(token, NULL, 10);
				break;
			case SCHEDSTAT_LB_NOT_GAINED:
				ss_domain->lb[cpu_type].lb_not_gained += strtoul(token, NULL, 10);
				break;
			case SCHEDSTAT_LB_NOBUSY_RQ:
				ss_domain->lb[cpu_type].lb_nobusy_rq += strtoul(token, NULL, 10);
				break;
			case SCHEDSTAT_LB_NOBUSY_GRP:
				ss_domain->lb[cpu_type].lb_nobusy_grp += strtoul(token, NULL, 10);
				break;
			default:
				adaptived_err("Invalid entry#: %d\n", i);
				return -EINVAL;
			}
		} else {
			switch (i) {
			case SCHEDSTAT_SBE_CALLED_DEFUNCT ... SCHEDSTAT_SBF_PUSHED_DEFUNCT:
			case SCHEDSTAT_TTWU_MOVE_BALANCE_DEFUNCT:
				/*
				 * Defunct entries kernel keeps them for ABI compatitbility
				 */
				break;
			case SCHEDSTAT_ALB_CALLED:
				ss_domain->alb_called += strtoul(token, NULL, 10);
				break;
			case SCHEDSTAT_ALB_FAILED:
				ss_domain->alb_failed += strtoul(token, NULL, 10);
				break;
			case SCHEDSTAT_ALB_PUSHED:
				ss_domain->alb_pushed += strtoul(token, NULL, 10);
				break;
			case SCHEDSTAT_TTWU_REMOTE:
				ss_domain->ttwu_remote += strtoul(token, NULL, 10);
				break;
			case SCHEDSTAT_TTWU_MOVE_AFFINE:
				ss_domain->ttwu_move_affine += strtoul(token, NULL, 10);
				break;
			default:
				adaptived_err("Invalid entry#: %d\n", i);
				return -EINVAL;
			}
		}
	}
	return 0;
}

API int adaptived_get_schedstat(const char * const schedstat_file, struct adaptived_schedstat_snapshot * const ss)
{
	FILE *fp;
        char *line = NULL, *token = NULL;
        size_t len = 0;
        ssize_t nread;
	int ret = 0, cpu = -1, domain = -1, max_domain = 0;

	if (!schedstat_file || !ss)
		return -EINVAL;

        fp = fopen(schedstat_file, "r");
        if (fp == NULL) {
		adaptived_err("Failed to open schedstat file: %s\n", schedstat_file);
		return -EINVAL;
        }

	memset(ss, 0, sizeof(struct adaptived_schedstat_snapshot));

        while (-1 != (nread = getline(&line, &len, fp))) {
		if (0 == strncmp(line, "cpu", 3)) {
			if (-1 != cpu)
				ss->schedstat_cpus[cpu].nr_domains = max_domain + 1;

			token = strtok(line, " ");
			cpu = atoi(token + 3);
			if (cpu < 0 || cpu >= MAX_NR_CPUS) {
				adaptived_err("CPU# must be a nonzero integer less than %d\n", MAX_NR_CPUS);
				ret = -EINVAL;
				goto error;
			}

			ret = adaptived_get_schedstat_cpu(strtok(NULL, " "), &ss->schedstat_cpus[cpu]);
			if (ret) {
				adaptived_err("adaptived_get_schedstat_cpu() failed\n");
				goto error;
			}
		} else if (0 == strncmp(line, "domain", 6)) {
			token = strtok(line, " ");
			domain = atoi(token + 6);
			if (domain < 0 || domain >= MAX_DOMAIN_LEVELS) {
				adaptived_err("Domain# must be a nonzero integer less than %d\n", MAX_DOMAIN_LEVELS);
				ret = -EINVAL;
				goto error;
			}
			max_domain = max(domain, max_domain);
			ret = adaptived_get_schedstat_domain(strtok(NULL, " "), &ss->schedstat_cpus[cpu].schedstat_domains[domain]);
			if (ret) {
				adaptived_err("adaptived_get_schedstat_domain() failed\n");
				goto error;
			}
		} else if (0 == strncmp(line, "timestamp", 9)) {
			token = strtok(line, " ");
			token = strtok(NULL, " ");
			ss->timestamp = strtoll(token, NULL, 10);
		}
        }
	ss->nr_cpus = cpu + 1;
	if (0 != ss->nr_cpus)
		ss->schedstat_cpus[cpu].nr_domains = max_domain + 1;

error:
	fclose(fp);

	free(line);

	return ret;
}
