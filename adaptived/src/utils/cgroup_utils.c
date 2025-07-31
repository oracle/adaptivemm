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
 * Utilities for working with cgroups
 *
 */

#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

#include <adaptived-utils.h>
#include <adaptived.h>

#include "adaptived-internal.h"

#define LL_MAX 8192

API int adaptived_cgroup_set_ll(const char * const setting, long long value, uint32_t flags)
{
	long long validate_value;
	ssize_t bytes_written;
	char buf[LL_MAX];
	int ret = 0;
	int fd;

	if (!setting)
		return -EINVAL;

	fd = open(setting, O_WRONLY | O_TRUNC, S_IWUSR | S_IWGRP);
	if (fd < 0)
		return -errno;

	snprintf(buf, sizeof(buf), "%lld", value);

	bytes_written = write(fd, buf, strlen(buf));
	if (bytes_written < 0) {
		ret = -errno;
		goto out;
	}
	if (bytes_written > strlen(buf)) {
		ret = -EINVAL;
		goto out;
	}
	if (bytes_written >= sizeof(buf)) {
		ret = -EOVERFLOW;
		goto out;
	}

	ret = 0;
out:
	close(fd);

	if (ret == 0 && (flags & ADAPTIVED_CGROUP_FLAGS_VALIDATE)) {
		ret = adaptived_cgroup_get_ll(setting, &validate_value);
		if (ret)
			return ret;

		if (validate_value != value) {
			adaptived_err("Failed to validate %s.  Expected %lld, read %lld\n",
				   setting, value, validate_value);
			return -EFAULT;
		}
	}

	return ret;
}

API int adaptived_cgroup_get_ll(const char * const setting, long long * const value)
{
	size_t bytes_read;
	char buf[LL_MAX];
	char *endptr;
	int ret = 0;
	FILE *f;

	if (!setting || !value)
		return -EINVAL;

	f = fopen(setting, "r");
	if (!f)
		return -errno;

	bytes_read = fread(buf, 1, sizeof(buf), f);
	if (bytes_read >= sizeof(buf)) {
		ret = -EOVERFLOW;
		goto out;
	}
	if (bytes_read <= 0) {
		ret = -EINVAL;
		goto out;
	}

	buf[bytes_read] = '\0';

	errno = 0;
	*value = strtoll(buf, &endptr, 10);
	if (errno != 0) {
		ret = -errno;
		goto out;
	}
	if (endptr[0] != '\0' && endptr[0] != '\n') {
		/* There was unparsable data in the string */
		ret = -EINVAL;
		goto out;
	}
	if (endptr == buf) {
		/* There was unparsable data in the string */
		ret = -EINVAL;
		goto out;
	}
	if (*value == LLONG_MIN || *value == LLONG_MAX) {
		ret = -ERANGE;
		goto out;
	}

	ret = 0;

out:
	(void)fclose(f);

	return ret;
}

API int adaptived_cgroup_get_float(const char * const setting, float * const value)
{
	size_t bytes_read;
	char buf[LL_MAX];
	char *endptr;
	int ret = 0;
	FILE *f;

	if (!setting || !value)
		return -EINVAL;

	f = fopen(setting, "r");
	if (!f)
		return -errno;

	bytes_read = fread(buf, 1, sizeof(buf), f);
	if (bytes_read >= sizeof(buf)) {
		ret = -EOVERFLOW;
		goto out;
	}
	if (bytes_read <= 0) {
		ret = -EINVAL;
		goto out;
	}

	buf[bytes_read] = '\0';

	errno = 0;
	*value = strtof(buf, &endptr);
	if (errno != 0) {
		ret = -errno;
		goto out;
	}
	if (endptr[0] != '\0' && endptr[0] != '\n') {
		/* There was unparsable data in the string */
		ret = -EINVAL;
		goto out;
	}
	if (endptr == buf) {
		/* There was unparsable data in the string */
		ret = -EINVAL;
		goto out;
	}

	ret = 0;

out:
	(void)fclose(f);

	return ret;
}

API int adaptived_cgroup_set_str(const char * const setting, const char * const value, uint32_t flags)
{
	char *validate_value;
	ssize_t bytes_written;
	int ret = 0;
	int fd;

	if (!setting || !value)
		return -EINVAL;

	adaptived_dbg("cgroup: Writing %s to %s\n", value, setting);

	fd = open(setting, O_WRONLY | O_TRUNC, S_IWUSR | S_IWGRP);
	if (fd < 0)
		return -errno;

	bytes_written = write(fd, value, strlen(value));
	if (bytes_written < 0) {
		ret = -errno;
		goto out;
	}
	if (bytes_written > strlen(value)) {
		ret = -EINVAL;
		goto out;
	}

	ret = 0;
out:
	close(fd);

	if (ret == 0 && (flags & ADAPTIVED_CGROUP_FLAGS_VALIDATE)) {
		ret = adaptived_cgroup_get_str(setting, &validate_value);
		if (ret)
			return ret;

		if (strlen(validate_value) != strlen(value) ||
		    strncmp(validate_value, value, strlen(validate_value) != 0)) {
			adaptived_err("Failed to validate %s.  Expected %s, read %s\n",
				   setting, value, validate_value);
			free(validate_value);
			return -EFAULT;
		}

		free(validate_value);
	}

	return ret;
}

API int adaptived_cgroup_get_str(const char * const setting, char ** value)
{
	size_t bytes_read;
	char buf[LL_MAX];
	int ret = 0;
	FILE *f;

	if (!setting || !value)
		return -EINVAL;

	*value = NULL;

	f = fopen(setting, "r");
	if (!f)
		return -errno;

	bytes_read = fread(buf, 1, sizeof(buf), f);
	if (bytes_read >= sizeof(buf)) {
		ret = -EOVERFLOW;
		goto out;
	}
	if (bytes_read <= 0) {
		ret = -EINVAL;
		goto out;
	}

	buf[bytes_read] = '\0';

	*value = strdup(buf);
	if (!(*value)) {
		ret = -ENOMEM;
		goto out;
	}

out:
	(void)fclose(f);

	return ret;
}

API int adaptived_cgroup_get_procs(const char * const cgroup_path, pid_t ** pids,
				int * const pid_count)
{
#define ALLOC_SIZE 16
	char cgroup_procs_path[FILENAME_MAX] = { '\0' };
	int pids_array_size = ALLOC_SIZE;
	pid_t *pids_array;
	int ret, i = 0;
	FILE *procs_f;
	pid_t pid;

	if (!cgroup_path || !pids || !pid_count)
		return -EINVAL;

	pids_array = malloc(sizeof(pid_t) * pids_array_size);
	if (!pids_array)
		return -ENOMEM;

	sprintf(cgroup_procs_path, "%s/cgroup.procs", cgroup_path);

	procs_f = fopen(cgroup_procs_path, "r");
	if (!procs_f) {
		ret = -errno;
		goto error;
	}

	while (!feof(procs_f)) {
		ret = fscanf(procs_f, "%u", &pid);
		if (ret == EOF)
			break;

		pids_array[i] = pid;
		i++;

		if (i >= pids_array_size) {
			pids_array_size += ALLOC_SIZE;
			pids_array = realloc(pids_array, sizeof(pid_t) * pids_array_size);
			if (!pids_array) {
				ret = -ENOMEM;
				goto error;
			}
		}
	}

	fclose(procs_f);

	*pids = pids_array;
	*pid_count = i;
	return 0;

error:
	if (procs_f)
		fclose(procs_f);

	if (pids_array)
		free(pids_array);

	*pids = NULL;
	*pid_count = 0;

	return ret;
}

static const char * const upper_human_readable_suffix = "KMGT";
static const char * const lower_human_readable_suffix = "kmgt";

long long adaptived_parse_human_readable(const char * const input)
{
        char *endp = (char *)input;
        char *match = NULL;
        long long shift = 0;
        errno = 0;

        long double dvalue = strtold(input, &endp);
        if (errno || *endp == '\0' ||endp == input || dvalue < 0)
                return -1;

	if (isupper(*endp)) {
		if (!(match = strchr(upper_human_readable_suffix, *endp)))
			return -1;

		if (*match)
			shift = (match - upper_human_readable_suffix + 1) * 10;
	} else {
		if (!(match = strchr(lower_human_readable_suffix, *endp)))
			return -1;

		if (*match)
			shift = (match - lower_human_readable_suffix + 1) * 10;
	}

        return (dvalue * (1LU << shift));
}

API int adaptived_cgroup_set_value(const char * const setting,
				const struct adaptived_cgroup_value * const value, uint32_t flags)
{
	int ret;
	long long ll_value = 0;

	if (!setting || !value)
		return -EINVAL;

	switch (value->type) {
	case ADAPTIVED_CGVAL_LONG_LONG:
		ret = adaptived_cgroup_set_ll(setting, value->value.ll_value, flags);
		break;
	case ADAPTIVED_CGVAL_FLOAT:
		adaptived_err("adaptived_cgroup_set_value(float) is not yet implemented\n");
		ret = -ENOTSUP;
		break;
	case ADAPTIVED_CGVAL_DETECT:
		ret = -EINVAL;
		break;
	case ADAPTIVED_CGVAL_STR:
		ll_value = adaptived_parse_human_readable(value->value.str_value);
		if (ll_value >= 0) {
			adaptived_dbg("adaptived_cgroup_set_value: parsed human readable value: %s (%lld)\n",
				value->value.str_value, ll_value);
			ret = adaptived_cgroup_set_ll(setting, ll_value, flags);
			if (ret == 0) {
				struct adaptived_cgroup_value * const update_value =
					(struct adaptived_cgroup_value *)value;

				update_value->type = ADAPTIVED_CGVAL_LONG_LONG;
				free(update_value->value.str_value);
				update_value->value.str_value = NULL;
				update_value->value.ll_value = ll_value;
			} else {
				adaptived_err("adaptived_cgroup_set_value(long long) failed. ret = %d\n", ret);
			}
			break;
		}
	default:
		ret = adaptived_cgroup_set_str(setting, value->value.str_value, flags);
		break;
	}

	return ret;
}

API int adaptived_cgroup_get_value(const char * const setting,
				struct adaptived_cgroup_value * const value)
{
	int ret;

	if (!setting || !value)
		return -EINVAL;

	switch (value->type) {
	case ADAPTIVED_CGVAL_STR:
		ret = adaptived_cgroup_get_str(setting, &value->value.str_value);
		break;
	case ADAPTIVED_CGVAL_LONG_LONG:
		ret = adaptived_cgroup_get_ll(setting, &value->value.ll_value);
		break;
	case ADAPTIVED_CGVAL_FLOAT:
		ret = adaptived_cgroup_get_float(setting, &value->value.float_value);
		break;
	case ADAPTIVED_CGVAL_DETECT:
		ret = adaptived_cgroup_get_ll(setting, &value->value.ll_value);
		if (ret == 0) {
			value->type = ADAPTIVED_CGVAL_LONG_LONG;
			return ret;
		} else {
			adaptived_dbg("setting from %s is not a long long.\n", setting);
		}

		ret = adaptived_cgroup_get_float(setting, &value->value.float_value);
		if (ret == 0) {
			value->type = ADAPTIVED_CGVAL_FLOAT;
			return ret;
		} else {
			adaptived_dbg("setting from %s is not a float: %d\n", setting, ret);
		}

		ret = adaptived_cgroup_get_str(setting, &value->value.str_value);
		if (ret == 0) {
			value->type = ADAPTIVED_CGVAL_STR;
			return ret;
		} else {
			adaptived_dbg("setting from %s is not a string: %d\n", setting, ret);
		}

		adaptived_err("Failed to detect setting type for %s\n", setting);
		break;
	default:
		adaptived_err("Invalid cgroup value type: %d\n", value->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

API bool adaptived_cgroup_setting_is_max(const char * const setting)
{
	struct adaptived_cgroup_value val;
        int ret;

	val.type = ADAPTIVED_CGVAL_STR;
	ret = adaptived_cgroup_get_str(setting, &val.value.str_value);
	if (ret == 0 && (strcmp(val.value.str_value, "max") == 0)) {
		return true;
	}
	return false;

}

API int adaptived_cgroup_get_memorystat_field(const char * const memorystat_file,
					   const char * const field,
					   long long * const ll_valuep)
{
	return get_ll_field_in_file(memorystat_file, field, " ", ll_valuep);
}
