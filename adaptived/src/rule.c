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
 * adaptived rule implementation file
 *
 * A rule consists of zero or more causes and zero or more
 * effects.  (While it's technically allowed to have a rule
 * with no causes and/or effects, it obviously doesn't make
 * sense.)  If the cause(s) in a rule all are triggered, then
 * the effects in that rule will be run.
 *
 */

#include <json-c/json.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#include <adaptived.h>

#include "adaptived-internal.h"

struct adaptived_rule *rule_init(const char * const name)
{
	struct adaptived_rule *rule = NULL;

	rule = malloc(sizeof(struct adaptived_rule));
	if (!rule)
		return NULL;

	memset(rule, 0, sizeof(struct adaptived_rule));

	rule->name = malloc(strlen(name) + 1);
	if (!rule->name)
		goto error;

	strcpy(rule->name, name);

	return rule;
error:
	rule_destroy(&rule);
	return NULL;
}

void rule_destroy(struct adaptived_rule ** rule)
{
	struct adaptived_effect *eff, *eff_next;
	struct adaptived_cause *cse, *cse_next;

	cse = (*rule)->causes;
	while (cse) {
		cse_next = cse->next;
		adaptived_dbg("Cleaning up cause %s\n", cse->name);
		cause_destroy(&cse);
		cse = cse_next;
	}

	eff = (*rule)->effects;
	while (eff) {
		eff_next = eff->next;
		adaptived_dbg("Cleaning up effect %s\n", eff->name);
		effect_destroy(&eff);
		eff = eff_next;
	}

	if ((*rule)->json)
		json_object_put((*rule)->json);

	if ((*rule)->name)
		free((*rule)->name);

	free(*rule);
	(*rule) = NULL;
}

API struct adaptived_rule *adaptived_build_rule(const char * const name)
{
	struct json_object *name_obj, *cse_obj, *eff_obj;
	struct adaptived_rule *rule;
	int ret;

	rule = rule_init(name);
	if (!rule)
		return NULL;

	rule->json = json_object_new_object();
	if (!rule->json)
		goto err;

	name_obj = json_object_new_string(name);
	if (!name_obj)
		goto err;

	ret = json_object_object_add(rule->json, "name", name_obj);
	if (ret)
		goto err;

	cse_obj = json_object_new_array();
	if (!cse_obj)
		goto err;

	ret = json_object_object_add(rule->json, "causes", cse_obj);
	if (ret)
		goto err;

	eff_obj = json_object_new_array();
	if (!eff_obj)
		goto err;

	ret = json_object_object_add(rule->json, "effects", eff_obj);
	if (ret)
		goto err;

	return rule;

err:
	rule_destroy(&rule);
	return NULL;
}

API void adaptived_release_rule(struct adaptived_rule ** rule)
{
	rule_destroy(rule);
}

API int adaptived_rule_add_cause(struct adaptived_rule * const rule,
			      const struct adaptived_cause * const cse)
{
	struct json_object *cses;
	json_bool exists;
	int ret;

	if (!rule || !cse)
		return -EINVAL;

	exists = json_object_object_get_ex(rule->json, "causes", &cses);
	if (!exists || !cses)
		return -EINVAL;

	ret = json_object_array_add(cses, cse->json);
	if (ret)
		return ret;

	return ret;
}

API int adaptived_rule_add_effect(struct adaptived_rule * const rule,
			       const struct adaptived_effect * const eff)
{
	struct json_object *effs;
	json_bool exists;
	int ret;

	if (!rule || !eff)
		return -EINVAL;

	exists = json_object_object_get_ex(rule->json, "effects", &effs);
	if (!exists || !effs)
		return -EINVAL;

	ret = json_object_array_add(effs, eff->json);
	if (ret)
		return ret;

	return ret;
}

API int adaptived_load_rule(struct adaptived_ctx * const ctx, struct adaptived_rule * const rule)
{
	int ret;

	pthread_mutex_lock(&ctx->ctx_mutex);
	ret = parse_rule(ctx, rule->json);
	if (!ret) {
		/*
		 * The rule was successfully inserted and now the json-c library
		 * owns the json struct.  Forget our pointer to it.
		 */
		free(rule->json);
		rule->json = NULL;
	}
	pthread_mutex_unlock(&ctx->ctx_mutex);

	return ret;
}

API int adaptived_unload_rule(struct adaptived_ctx * const ctx, const char * const name)
{
	struct adaptived_rule *rule, *prev, *next;
	bool found = false;

	pthread_mutex_lock(&ctx->ctx_mutex);

	rule = ctx->rules;
	prev = NULL;
	while (rule) {
		if (strncmp(name, rule->name, strlen(rule->name)) == 0) {
			next = rule->next;
			found = true;

			rule_destroy(&rule);
			break;
		}

		prev = rule;
		rule = rule->next;
	}

	if (!found) {
		pthread_mutex_unlock(&ctx->ctx_mutex);
		return -ENOENT;
	}

	if (prev) {
		prev->next = next;
	} else {
		ctx->rules = next;
	}

	pthread_mutex_unlock(&ctx->ctx_mutex);
	return 0;
}
