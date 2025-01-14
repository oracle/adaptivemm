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
 * adaptived effects implementation file
 *
 * "Causes" are events that trigger "Effects".  adaptived
 * will periodically evaluate the causes, and if they fire
 * then the associated effects will be enforced.
 *
 */

#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "adaptived-internal.h"
#include "defines.h"
#include "effect.h"

const char * const effect_op_names[] = {
	"set",
	"add",
	"subtract",
};
static_assert(ARRAY_SIZE(effect_op_names) == EOP_CNT,
	      "op_names[] must be same length as EOP_CNT");

const char * const effect_names[] = {
	"print",
	"validate",
	"snooze",
	"cgroup_setting",
	"cgroup_memory_setting",
	"kill_cgroup",
	"kill_cgroup_by_psi",
	"cgroup_setting_by_psi",
	"copy_cgroup_setting",
	"logger",
	"print_schedstat",
	"setting",
	"sd_bus_setting",
	"kill_processes",
	"signal",
	"adaptivemmd_effects",
};
static_assert(ARRAY_SIZE(effect_names) == EFFECT_CNT,
	      "effect_names[] must be same length as EFFECT_CNT");

const struct adaptived_effect_functions effect_fns[] = {
	{print_init, print_main, print_exit},
	{validate_init, validate_main, validate_exit},
	{snooze_init, snooze_main, snooze_exit},
	{cgroup_setting_init, cgroup_setting_main, cgroup_setting_exit},
	{cgroup_memory_setting_init, cgroup_memory_setting_main, cgroup_setting_exit},
	{kill_cgroup_init, kill_cgroup_main, kill_cgroup_exit},
	{kill_cgroup_psi_init, kill_cgroup_psi_main, kill_cgroup_psi_exit},
	{cgroup_setting_psi_init, cgroup_setting_psi_main, cgroup_setting_psi_exit},
	{copy_cgroup_setting_init, copy_cgroup_setting_main, copy_cgroup_setting_exit},
	{logger_init, logger_main, logger_exit},
	{print_schedstat_init, print_schedstat_main, print_schedstat_exit},
	{cgroup_setting_init, cgroup_setting_main, cgroup_setting_exit},
	{sd_bus_setting_init, sd_bus_setting_main, sd_bus_setting_exit},
	{kill_processes_init, kill_processes_main, kill_processes_exit},
	{signal_init, signal_main, signal_exit},
	{adaptivemmd_effects_init, adaptivemmd_effects_main, adaptivemmd_effects_exit},
};
static_assert(ARRAY_SIZE(effect_fns) == EFFECT_CNT,
	      "effect_fns[] must be same length as EFFECT_CNT");


/*
 * Re-use the adaptived_effect structure to store a linked list of effects that have been
 * added at runtime by the user.  When processing effects, the ->next field is used to
 * chain multiple effects together for one rule (e.g. notify me when it's after 5pm and
 * it's Friday).  In the registered_effects case, the ->next field simply points to the
 * next registered effect that's been added by the user
 */
struct adaptived_effect *registered_effects;

API int adaptived_register_effect(struct adaptived_ctx * const ctx, const char * const name,
			       const struct adaptived_effect_functions * const fns)
{
	struct adaptived_effect *eff = NULL, *tmp_eff;
	struct adaptived_effect *tmp;
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

	eff = malloc(sizeof(struct adaptived_effect));
	if (!eff)
		return -ENOMEM;

	memset(eff, 0, sizeof(struct adaptived_effect));

	eff->name = strdup(name);
	if (!eff->name) {
		ret = -ENOMEM;
		goto err;
	}

	/*
	 * The index is saved off for debugging and convenience.  Since this is a registered
	 * effect, it doesn't have an index into the enum effects enumeration.
	 */
	eff->idx = -1;
	eff->fns = fns;
	eff->next = NULL;

	pthread_mutex_lock(&ctx->ctx_mutex);

	/*
	 * Verify that the name is available in both the built-in effects and the
	 * registered effects
	 */
	for (i = 0; i < EFFECT_CNT; i++) {
		if (strcmp(name, effect_names[i]) == 0) {
			ret = -EEXIST;
			pthread_mutex_unlock(&ctx->ctx_mutex);
			goto err;
		}
	}

	tmp = registered_effects;
	while (tmp) {
		if (strcmp(name, tmp->name) == 0) {
			ret = -EEXIST;
			pthread_mutex_unlock(&ctx->ctx_mutex);
			goto err;
		}

		tmp = tmp->next;
	}

	if (!registered_effects) {
		/*
		 * This is the first effect in the registered linked list
		 */
		registered_effects = eff;
	} else {
		tmp_eff = registered_effects;

		while (tmp_eff->next)
			tmp_eff = tmp_eff->next;

		tmp_eff->next = eff;
	}
	pthread_mutex_unlock(&ctx->ctx_mutex);

	return ret;

err:
	if (eff && eff->name)
		free(eff->name);
	if (eff)
		free(eff);
	return ret;
}

API void *adaptived_effect_get_data(const struct adaptived_effect * const eff)
{

	return eff->data;
}

API int adaptived_effect_set_data(struct adaptived_effect * const eff, void * const data)
{
	eff->data = data;
	return 0;
}

API struct adaptived_effect *adaptived_build_effect(const char * const name)
{
	struct json_object *name_obj;
	struct adaptived_effect *eff;
	int ret;

	eff = effect_init(name);
	if (!eff)
		return NULL;

	eff->json = json_object_new_object();
	if (!eff->json)
		goto err;

	name_obj = json_object_new_string(name);
	if (!name_obj)
		goto err;

	ret = json_object_object_add(eff->json, "name", name_obj);
	if (ret)
		goto err;

	return eff;

err:
	effect_destroy(&eff);
	return NULL;
}

API void adaptived_release_effect(struct adaptived_effect ** eff)
{
	effect_destroy(eff);
}

API int adaptived_effect_add_string_arg(struct adaptived_effect * const eff, const char * const key,
				    const char * const value)
{
	struct json_object *value_obj;
	int ret;

	if (!eff || !key || !value)
		return -EINVAL;

	value_obj = json_object_new_string(value);
	if (!value_obj)
		return -EINVAL;

	ret = insert_into_json_args_obj(eff->json, key, value_obj);
	if (ret)
		return ret;

	return 0;
}

API int adaptived_effect_add_int_arg(struct adaptived_effect * const eff, const char * const key,
				 int value)
{
	struct json_object *value_obj;
	int ret;

	if (!eff || !key || !value)
		return -EINVAL;

	value_obj = json_object_new_int(value);
	if (!value_obj)
		return -EINVAL;

	ret = insert_into_json_args_obj(eff->json, key, value_obj);
	if (ret)
		return ret;

	return ret;
}

struct adaptived_effect *effect_init(const char * const name)
{
	struct adaptived_effect *eff = NULL;

	eff = malloc(sizeof(struct adaptived_effect));
	if (!eff)
		return NULL;

	memset(eff, 0, sizeof(struct adaptived_effect));

	eff->name = malloc(strlen(name) + 1);
	if (!eff->name)
		goto error;

	strcpy(eff->name, name);

	return eff;
error:
	if (eff)
		free(eff);

	return NULL;
}

void effect_destroy(struct adaptived_effect ** eff)
{
	if ((*eff)->fns && (*eff)->fns->exit)
		(*(*eff)->fns->exit)(*eff);
	if ((*eff)->json)
		json_object_put((*eff)->json);
	if ((*eff)->name)
		free((*eff)->name);

	free(*eff);
	*eff = NULL;
}

void effects_init(void)
{
	registered_effects = NULL;
}

void effects_cleanup(void)
{
	struct adaptived_effect *next, *cur;

	cur = registered_effects;

	while (cur) {
		next = cur->next;
		free(cur->name);
		free(cur);

		cur = next;
	}
}
