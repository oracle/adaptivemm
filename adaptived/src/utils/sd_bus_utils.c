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
 * Utilities for working with systemd bus
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
#include <systemd/sd-bus.h>

#include <adaptived-utils.h>
#include <adaptived.h>

#include "adaptived-internal.h"

#define LL_MAX 8192

typedef enum cgroupType {
	CG_SLICE,
	CG_SCOPE,
	_CG_TYPE_INVALID = -EINVAL,
} cgroupType;

static inline bool isempty(const char *a) {
	return !a || a[0] == '\0';
}

static char *endswith(const char *s, const char *suffix)
{
	size_t sl, pl;

	if (!s || !suffix)
		return NULL;

	sl = strlen(s);
	pl = strlen(suffix);

	if (pl == 0)
		return (char*) s + sl;

	if (sl < pl)
		return NULL;

	if (strcmp(s + sl - pl, suffix) != 0)
		return NULL;

	return (char*) s + sl - pl;
}

#define USEC_PER_SEC  ((uint64_t) 1000000ULL)

/*
 * handle_special_properties:
 * return:
 * 1: property not handled in this function.
 * 0: property handled in this function and success.
 * < 0: property handled in this function and failed.
 */
static int handle_special_properties(sd_bus_message *m, const char *property, const struct adaptived_cgroup_value * const value)
{
	int r = -EINVAL;
	int items = 0;
	long unsigned int x = UINT64_MAX;
	char *real_property = NULL;

	if ((strncmp(property, "CPUQuota", strlen("CPUQuota")) == 0) &&
	    (strlen(property) == strlen("CPUQuota")))
		real_property = "CPUQuotaPerSecUSec";
	else if ((strncmp(property, "CPUQuotaPeriodSec", strlen("CPUQuotaPeriodSec")) == 0) &&
	    (strlen(property) == strlen("CPUQuotaPeriodSec")))
		real_property = "CPUQuotaPeriodUSec";
	else
		return 1;	/* property not handled here */

	switch(value->type) {
	case ADAPTIVED_CGVAL_STR:
		adaptived_dbg("%s: property=%s, value->value.str_value=%s, real property: %s\n",
			__func__, property, value->value.str_value, real_property);
		if (isempty(value->value.str_value)) {
			x = UINT64_MAX;
		} else if (strcmp(real_property, "CPUQuotaPerSecUSec") == 0){
			if (endswith(value->value.str_value, "%")) {
				unsigned int percent;

				items = sscanf(value->value.str_value, "%d%%", &percent);
				if (items != 1) {
					adaptived_err("%s: sd_bus_message_append() failed, r=%d\n",
						   __func__, r);
					return -EINVAL;
				}
				percent *= 100;
				x = (((uint64_t) percent * USEC_PER_SEC) / 10000U);
			} else {
				adaptived_err("handle_special_properties: unknown string value.\n");
				return -EINVAL;
			}
		} else if (strcmp(real_property, "CPUQuotaPeriodUSec") == 0){
			if (endswith(value->value.str_value, "us")) {
				items = sscanf(value->value.str_value, "%ldus", &x);
			} else if (endswith(value->value.str_value, "ms")) {
				items = sscanf(value->value.str_value, "%ldms", &x);
				x *= 1000;
			} else if (endswith(value->value.str_value, "s")) {
				items = sscanf(value->value.str_value, "%lds", &x);
				x *= 1000000;
			}
			if (items != 1) {
				adaptived_err("handle_special_properties: unknown time setting in: %s\n",
					value->value.str_value);
				return -EINVAL;
			}
		}
		break;
	case ADAPTIVED_CGVAL_LONG_LONG:
		adaptived_dbg("%s: property=%s, value->value.ll_value=%lld, real property: %s\n",
			__func__, property, value->value.ll_value, real_property);
		x = value->value.ll_value;
		break;
	default:
		adaptived_err("%s: unsupported type: %d\n", __func__, value->type);
	}

	adaptived_dbg("%s: sd_bus_message_append(m, (sv), %s, t, %lld\n", __func__, real_property, x);
	r = sd_bus_message_append(m, "(sv)", real_property, "t", x);

	if (r < 0)
		adaptived_err("%s: sd_bus_message_append() failed for %s, r=%d\n", __func__, real_property, r);
	else
		r = 0;

	return r;
}

static int update_property(sd_bus_message *m, const char *property,
			   const struct adaptived_cgroup_value * const value)
{
	int r;

	/* if special property, return success or -error_code, else handle here */
	r = handle_special_properties(m, property, value);
	if (r <= 0)
		return r;

	switch (value->type) {
	case ADAPTIVED_CGVAL_STR:
		adaptived_dbg("%s: property=%s, value->value.str_value=%s\n", __func__, property,
			   value->value.str_value);
		if (isempty(value->value.str_value) ||
		    (strcmp(value->value.str_value, "infinity") == 0)) {
			long unsigned int x = UINT64_MAX;
			r = sd_bus_message_append(m, "(sv)", property, "t", x);
		} else {
			r = sd_bus_message_append(m, "(sv)", property, "s", value->value.str_value);
		}
		break;
	case ADAPTIVED_CGVAL_LONG_LONG:
		adaptived_dbg("%s: property=%s, value->value.ll_value=%lld\n", __func__, property,
			   value->value.ll_value);
		r = sd_bus_message_append(m, "(sv)", property, "t", value->value.ll_value);
		break;
	default:
		adaptived_err("update_property: unsupported type: %d\n", value->type);
		r = -EINVAL;
	}
	if (r < 0) {
		adaptived_err("update_property: sd_bus_message_append() failed, property=%s, r=%d\n",
			   property, r);
		return r;
	}

	return r;
}

static cgroupType get_type_from_string(const char *s)
{
	char *subp;

	adaptived_dbg("get_type_from_string: s=%s\n", s);

	subp = strstr(s, "scope");
	if (subp)
		return CG_SCOPE;
	subp = strstr(s, "slice");
	if (subp)
		return CG_SLICE;

	return _CG_TYPE_INVALID;
}

static int property_assignment(sd_bus_message *m, cgroupType t, const char *property,
			       const struct adaptived_cgroup_value * const value)
{
	int r;

	adaptived_dbg("property_assignment: t=%d, property=%s\n", t, property);

	switch (t) {
	case CG_SLICE:
	case CG_SCOPE:
		r = update_property(m, property, value);
		break;
	default:
		adaptived_err("property_assignment: Unsupported type: %d\n", t);
		r = -EINVAL;
	}
	return r;
}

static int set_property(const char *name, const char *property,
			const struct adaptived_cgroup_value * const value, bool arg_runtime)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message *m = NULL;
	sd_bus *bus;
	int r;
	cgroupType t;

	adaptived_dbg("set_property: name=%s, property=%s, arg_runtime=%d\n", name, property, arg_runtime);

	r = sd_bus_default_system(&bus);
	if (r < 0) {
		adaptived_err("set_property: Can't get bus, r=%d\n", r);
		return r;
	}

	r = sd_bus_message_new_method_call(bus, &m, "org.freedesktop.systemd1",
		"/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager",
		"SetUnitProperties");
	if (r < 0) {
		adaptived_err("set_property: sd_bus_message_new_method_call() failed, r=%d\n", r);
		return r;
	}

	t = get_type_from_string(name);
	if (t < 0) {
		adaptived_err("set_property: get_type_from_string(), failed, t=%d\n", t);
		return t;
	}

	r = sd_bus_message_append(m, "sb", name, arg_runtime);
	if (r < 0) {
		adaptived_err("set_property: sd_bus_message_append() failed, r=%d\n", r);
		return r;
	}

	r = sd_bus_message_open_container(m, SD_BUS_TYPE_ARRAY, "(sv)");
	if (r < 0) {
		adaptived_err("set_property: sd_bus_message_open_container() failed, r=%d\n", r);
		return r;
	}

	r = property_assignment(m, t, property, value);
	if (r < 0) {
		adaptived_err("set_property: property_assignment() failed, r=%d\n", r);
		return r;
	}

	r = sd_bus_message_close_container(m);
	if (r < 0) {
		adaptived_err("set_property: sd_bus_message_close_container() failed, r=%d\n", r);
		return r;
	}

	r = sd_bus_call(bus, m, 0, &error, NULL);
	if (r < 0) {
		adaptived_err("set_property: sd_bus_call() failed, r=%d\n", r);
		return r;
	}

	return 0;
}

static int get_property(char *path, char *interface, const char *prop, char *type,
			struct adaptived_cgroup_value *ret_val)
{
	sd_bus *bus;
	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message *reply = NULL;
	uint64_t val;
	const char *s;
	int r;

	adaptived_dbg("%s: path=%s, interface=%s, prop=%s, type=%s\n", __func__, path, interface,
		   prop, type);

	r = sd_bus_default_system(&bus);
	if (r < 0) {
		adaptived_err("get_property: Can't get bus, r=%d\n", r);
		return r;
	}

	memset(ret_val, 0, sizeof(struct adaptived_cgroup_value));

	r = sd_bus_get_property(bus, "org.freedesktop.systemd1", path, interface, prop, &err,
				&reply,	type);
	if (r < 0) {
		adaptived_err("get_property: sd_bus_get_property() failed, r=%d\n", r);
		return r;
	}

	if (strcmp(type, "t") == 0) {
		r = sd_bus_message_read(reply, "t", &val);
		if (r >= 0) {
			adaptived_dbg("get_property: type=t, r=%d, val=%lu\n", r, val);
			ret_val->value.ll_value = val;
			ret_val->type = ADAPTIVED_CGVAL_LONG_LONG;
			r = 0;
		}
	} else if (strcmp(type, "s") == 0) {
		r = sd_bus_message_read(reply, "s", &s);
		if (r >= 0) {
			adaptived_dbg("get_property: type=s, r=%d, s=%s\n", r, s);
			ret_val->value.str_value = strdup(s);
			ret_val->type = ADAPTIVED_CGVAL_STR;
			r = 0;
		}
	} else {
		r = -ENOTSUP;
	}

	sd_bus_error_free(&err);
	if (r < 0) {
		adaptived_err("get_property: sd_bus_message_read() failed, r=%d\n", r);
		return r;
	}

	return 0;
}

API int adaptived_sd_bus_set_ll(const char * const target, const char * const property,
			     long long value, uint32_t flags)
{
	struct adaptived_cgroup_value val;
	int ret = 0;
	bool arg_runtime = false;

	if (!target || !property)
		return -EINVAL;

	memset(&val, 0, sizeof(struct adaptived_cgroup_value));

	val.type = ADAPTIVED_CGVAL_LONG_LONG;
	val.value.ll_value = value;

	if (flags & ADAPTIVED_CGROUP_FLAGS_RUNTIME)
		arg_runtime = true;

	ret = set_property(target, property, &val, arg_runtime);
	if (ret < 0)
		return ret;

	if (ret == 0 && (flags & ADAPTIVED_CGROUP_FLAGS_VALIDATE)) {
		long long validate_value;

		ret = adaptived_sd_bus_get_ll(target, property, &validate_value);
		if (ret)
			return ret;

		if (validate_value != value) {
			adaptived_err("Failed to validate %s.  Expected %lld, read %lld\n",
				   property, value, validate_value);
			return -EFAULT;
		}
		adaptived_dbg("adaptived_sd_bus_set_ll: validate success.\n");
	}

	return ret;
}

API int adaptived_sd_bus_set_str(const char * const target, const char * const property,
			      const char * const value, uint32_t flags)
{
	struct adaptived_cgroup_value val;
	int ret = 0;
	bool arg_runtime = false;

	if (!target || !property || !value)
		return -EINVAL;

	memset(&val, 0, sizeof(struct adaptived_cgroup_value));

	val.type = ADAPTIVED_CGVAL_STR;
	val.value.str_value = (char *)value;

	if (flags & ADAPTIVED_CGROUP_FLAGS_RUNTIME)
		arg_runtime = true;

	ret = set_property(target, property, &val, arg_runtime);
	if (ret < 0)
		return ret;

	if (ret == 0 && (flags & ADAPTIVED_CGROUP_FLAGS_VALIDATE)) {
		if (strcmp(value, "infinity") == 0) {
			long long validate_value;

			ret = adaptived_sd_bus_get_ll(target, property, &validate_value);
			if (ret)
				return ret;

			if (validate_value != UINT64_MAX) {
				adaptived_err("Failed to validate %s.  Expected %lld, read %lld\n",
					   property, UINT64_MAX, validate_value);
				return -EFAULT;
			}
		} else {
			char *validate_value = NULL;

			ret = adaptived_sd_bus_get_str(target, property, &validate_value);
			if (ret) {
				if (validate_value)
					free(validate_value);

				return ret;
			}

			if (!validate_value) {
				adaptived_err("Failed to validate %s.\n", property);
				return -EFAULT;
			}
			if (strlen(validate_value) != strlen(value) ||
			    strncmp(validate_value, value, strlen(validate_value) != 0)) {
				adaptived_err("Failed to validate %s.  Expected %s, read %s\n",
					   property, value, validate_value);
				free(validate_value);
				return -EFAULT;
			}
			free(validate_value);
		}
		adaptived_dbg("adaptived_sd_bus_set_str: validate success.\n");
	}

	return ret;
}

static int get_path_interface(const char * cg_name, char *path, char *interface)
{
	char *name = NULL;
	char *ext = NULL;
	char str[FILENAME_MAX];
	char *subp;

	if (!cg_name || !path)
		return -EINVAL;

	strcpy(str, cg_name);

	subp = strstr(str, ".slice");
	if (subp) {
		*subp = '\0';
		name = str;
		ext = &subp[1];
	} else {
		subp = strstr(str, ".scope");
		if (subp) {
			*subp = '\0';
			name = str;
			ext = &subp[1];
		} else
			return -EINVAL;
	}

	if (!name || !ext)
		return -EINVAL;

	sprintf(path, "/org/freedesktop/systemd1/unit/%s_2e%s", name, ext);

	if (strcmp(ext, "slice") == 0)
		strcpy(interface, "org.freedesktop.systemd1.Slice");
	else if (strcmp(ext, "scope") == 0)
		strcpy(interface, "org.freedesktop.systemd1.Scope");
	else
		return -EINVAL;

	return 0;
}

API int adaptived_sd_bus_get_ll(const char * const target, const char * const property,
			     long long int *ll_value)
{
	int ret = 0;
	char path[FILENAME_MAX];
	char interface[FILENAME_MAX];
	struct adaptived_cgroup_value value;

	if (!target || !property || !ll_value)
		return -EINVAL;

	ret = get_path_interface(target, path, interface);
	if (ret)
		return ret;

	ret = get_property(path, interface, property, "t", &value);

	*ll_value = value.value.ll_value;

	return ret;
}


API int adaptived_sd_bus_get_str(const char * const target, const char * const property,
			      char **str_value)
{
	int ret = 0;
	char path[FILENAME_MAX];
	char interface[FILENAME_MAX];
	struct adaptived_cgroup_value value;

	if (!target || !property || !str_value)
		return -EINVAL;

	ret = get_path_interface(target, path, interface);
	if (ret)
		return ret;

	ret = get_property(path, interface, property, "s", &value);

	*str_value = strdup(value.value.str_value);

	return ret;
}

API bool adaptived_sd_bus_setting_is_max(const char * const target, const char * const property)
{
	long long validate_value;
	int ret;

	ret = adaptived_sd_bus_get_ll(target, property, &validate_value);
	if (ret) {
		adaptived_err("%s: adaptived_sd_bus_get_ll() fail target %s property %s, ret=%d\n",
			__func__, target, property, ret);
		return true;
	}

	if (validate_value == UINT64_MAX) {
		adaptived_dbg("target %s property %s at infinity.\n", target, property);
		return true;
	}
	return false;
}

API int adaptived_sd_bus_get_value(const char * const target, const char * const property,
				struct adaptived_cgroup_value * const value)
{
	int ret;

	if (!target || !property || !value)
		return -EINVAL;

	switch (value->type) {
	case ADAPTIVED_CGVAL_STR:
		ret = adaptived_sd_bus_get_str(target, property, &value->value.str_value);
		break;
	case ADAPTIVED_CGVAL_LONG_LONG:
		ret = adaptived_sd_bus_get_ll(target, property, &value->value.ll_value);
		break;
	default:
		adaptived_err("adaptived_sd_bus_get_value: Invalid cgroup value type: %d\n", value->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

API int adaptived_sd_bus_set_value(const char * const target, const char * const setting,
				const struct adaptived_cgroup_value * const value, uint32_t flags)
{
	int ret;
	long long ll_value = 0;

	if (!target || !setting || !value)
		return -EINVAL;
	switch (value->type) {
	case ADAPTIVED_CGVAL_LONG_LONG:
		ret = adaptived_sd_bus_set_ll(target, setting, value->value.ll_value, flags);
		break;
	case ADAPTIVED_CGVAL_FLOAT:
		adaptived_err("adaptived_sd_bus_set_value(float) is not yet implemented\n");
		ret = -ENOTSUP;
		break;
	case ADAPTIVED_CGVAL_DETECT:
		ret = -EINVAL;
		break;
	case ADAPTIVED_CGVAL_STR:
		ll_value = adaptived_parse_human_readable(value->value.str_value);
		if (ll_value >= 0) {
			adaptived_dbg("%s: parsed human readable value: %s (%lld)\n", __func__,
				value->value.str_value, ll_value);
			ret = adaptived_sd_bus_set_ll(target, setting, ll_value, flags);
			if (ret == 0) {
				struct adaptived_cgroup_value * const update_value =
					(struct adaptived_cgroup_value *)value;

				update_value->type = ADAPTIVED_CGVAL_LONG_LONG;
				free(update_value->value.str_value);
				update_value->value.str_value = NULL;
				update_value->value.ll_value = ll_value;
			} else {
				adaptived_err("%s(long long) failed. ret = %d\n", __func__, ret);
			}
			break;
		}
		ret = adaptived_sd_bus_set_str(target, setting, value->value.str_value, flags);
		break;
	default:
		adaptived_err("adaptived_sd_bus_set_value: unknown type: %d\n", value->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}
