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
 * adaptived causes implementation file
 *
 * "Causes" are events that trigger "Effects".  adaptived
 * will periodically evaluate the causes, and if they fire
 * then the associated effects will be enforced.
 *
 */

#include <json-c/json.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include <adaptived.h>

#include "adaptived-internal.h"
#include "defines.h"
#include "cause.h"

const char * const cause_names[] = {
	"time_of_day",
	"days_of_the_week",
	"pressure",
	"pressure_rate",
	"always",
	"cgroup_setting",
	"setting",
	"periodic",
	"meminfo",
	"slabinfo",
	"memory.stat",
	"top",
	"cgroup_memory_setting",
	"adaptivemmd_causes",
};
static_assert(ARRAY_SIZE(cause_names) == CAUSE_CNT,
	      "cause_names[] must be same length as CAUSE_CNT");

const char * const cause_op_names[] = {
	"greaterthan",
	"lessthan",
	"equal",
};
static_assert(ARRAY_SIZE(cause_op_names) == COP_CNT,
	      "operation_names[] must be same length as COP_CNT");

const struct adaptived_cause_functions cause_fns[] = {
	{time_of_day_init, time_of_day_main, time_of_day_exit},
	{days_of_the_week_init, days_of_the_week_main, days_of_the_week_exit },
	{pressure_init, pressure_main, pressure_exit},
	{pressure_rate_init, pressure_rate_main, pressure_rate_exit},
	{always_init, always_main, always_exit},
	{cgset_init, cgset_main, cgset_exit},
	{cgset_init, cgset_main, cgset_exit},
	{periodic_init, periodic_main, periodic_exit},
	{meminfo_init, meminfo_main, meminfo_exit},
	{slabinfo_init, slabinfo_main, slabinfo_exit},
	{memorystat_init, memorystat_main, memorystat_exit},
	{top_init, top_main, top_exit},
	{cgset_memory_init, cgset_memory_main, cgset_exit},
	{adaptivemmd_causes_init, adaptivemmd_causes_main, adaptivemmd_causes_exit},
};
static_assert(ARRAY_SIZE(cause_fns) == CAUSE_CNT,
	      "cause_fns[] must be same length as CAUSE_CNT");

/*
 * Re-use the adaptived_cause structure to store a linked list of causes that have been
 * added at runtime by the user.  When processing causes, the ->next field is used to
 * chain multiple causes together for one rule (e.g. notify me when it's after 5pm and
 * it's Friday).  In the registered_causes case, the ->next field simply points to the
 * next registered cause that's been added by the user
 */
struct adaptived_cause *registered_causes;

API int adaptived_register_cause(struct adaptived_ctx * const ctx, const char * const name,
			      const struct adaptived_cause_functions * const fns)
{
	struct adaptived_cause *cse = NULL, *tmp_cse;
	struct adaptived_cause *tmp;
	int ret = 0;
	int i;

	if (!ctx)
		return -EINVAL;
	if (!name)
		return -EINVAL;
	if (!fns)
		return -EINVAL;
	if (!fns->init || !fns->main || !fns->exit)
		return -EINVAL;

	cse = malloc(sizeof(struct adaptived_cause));
	if (!cse)
		return -ENOMEM;

	memset(cse, 0, sizeof(struct adaptived_cause));

	cse->name = strdup(name);
	if (!cse->name) {
		ret = -ENOMEM;
		goto err;
	}

	/*
	 * The index is saved off for debugging and convenience.  Since this is a registered
	 * cause, it doesn't have an index into the enum causes enumeration.
	 */
	cse->idx = -1;
	cse->fns = fns;
	cse->next = NULL;

	pthread_mutex_lock(&ctx->ctx_mutex);

	/*
	 * Verify that the name is available in both the built-in causes and the
	 * registered causes
	 */
	for (i = 0; i < CAUSE_CNT; i++) {
		if (strcmp(name, cause_names[i]) == 0) {
			ret = -EEXIST;
			pthread_mutex_unlock(&ctx->ctx_mutex);
			goto err;
		}
	}

	tmp = registered_causes;
	while (tmp) {
		if (strcmp(name, tmp->name) == 0) {
			ret = -EEXIST;
			pthread_mutex_unlock(&ctx->ctx_mutex);
			goto err;
		}

		tmp = tmp->next;
	}

	if (!registered_causes) {
		/*
		 * This is the first cause in the registered linked list
		 */
		registered_causes = cse;
	} else {
		tmp_cse = registered_causes;

		while (tmp_cse->next)
			tmp_cse = tmp_cse->next;

		tmp_cse->next = cse;
	}
	pthread_mutex_unlock(&ctx->ctx_mutex);

	return ret;

err:
	if (cse && cse->name)
		free(cse->name);
	if (cse)
		free(cse);
	return ret;
}

API void *adaptived_cause_get_data(const struct adaptived_cause * const cse)
{
	return cse->data;
}

API int adaptived_cause_set_data(struct adaptived_cause * const cse, void * const data)
{
	cse->data = data;
	return 0;
}

API struct adaptived_cause *adaptived_build_cause(const char * const name)
{
	struct json_object *name_obj;
	struct adaptived_cause *cse;
	int ret;

	cse = cause_init(name);
	if (!cse)
		return NULL;

	cse->json = json_object_new_object();
	if (!cse->json)
		goto err;

	name_obj = json_object_new_string(name);
	if (!name_obj)
		goto err;

	ret = json_object_object_add(cse->json, "name", name_obj);
	if (ret)
		goto err;

	return cse;

err:
	cause_destroy(&cse);
	return NULL;
}

API void adaptived_release_cause(struct adaptived_cause ** cse)
{
	cause_destroy(cse);
}

API int adaptived_cause_add_string_arg(struct adaptived_cause * const cse, const char * const key,
				    const char * const value)
{
	struct json_object *value_obj;
	int ret;

	if (!cse || !key || !value)
		return -EINVAL;

	value_obj = json_object_new_string(value);
	if (!value_obj)
		return -EINVAL;

	ret = insert_into_json_args_obj(cse->json, key, value_obj);
	if (ret)
		return ret;

	return 0;
}

API int adaptived_cause_add_int_arg(struct adaptived_cause * const cse, const char * const key,
				 int value)
{
	struct json_object *value_obj;
	int ret;

	if (!cse || !key || !value)
		return -EINVAL;

	value_obj = json_object_new_int(value);
	if (!value_obj)
		return -EINVAL;

	ret = insert_into_json_args_obj(cse->json, key, value_obj);
	if (ret)
		return ret;

	return ret;
}

struct adaptived_cause *cause_init(const char * const name)
{
	struct adaptived_cause *cse = NULL;

	cse = malloc(sizeof(struct adaptived_cause));
	if (!cse)
		return NULL;

	memset(cse, 0, sizeof(struct adaptived_cause));

	cse->name = malloc(strlen(name) + 1);
	if (!cse->name)
		goto error;

	strcpy(cse->name, name);

	return cse;
error:
	if (cse)
		free(cse);

	return NULL;
}

void cause_destroy(struct adaptived_cause ** cse)
{
	if ((*cse)->fns && (*cse)->fns->exit)
		(*(*cse)->fns->exit)(*cse);
	if ((*cse)->json)
		json_object_put((*cse)->json);
	if ((*cse)->name)
		free((*cse)->name);

	free(*cse);
	*cse = NULL;
}

void causes_init(void)
{
	registered_causes = NULL;
}

void causes_cleanup(void)
{
	struct adaptived_cause *next, *cur;

	cur = registered_causes;

	while (cur) {
		next = cur->next;
		free(cur->name);
		free(cur);

		cur = next;
	}
}
