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
 * File for parsing and managing the config file
 *
 */

#include <json-c/json.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <adaptived.h>

#include "adaptived-internal.h"
#include "effect.h"
#include "cause.h"

static int get_file_size(FILE * const fd, long * const file_size)
{
	int ret;

	ret = fseek(fd, 0, SEEK_END);
	if (ret) {
		adaptived_err("fseek SEEK_END failed: %d\n", ret);
		return -errno;
	}

	*file_size = ftell(fd);

	ret = fseek(fd, 0, SEEK_SET);
	if (ret) {
		adaptived_err("fseek SEEK_SET failed: %d\n", ret);
		return -errno;
	}

	return ret;
}

API int adaptived_parse_string(struct json_object * const obj, const char * const key, const char **value)
{
	struct json_object *key_obj;
	json_bool exists;
	int ret = 0;

	if (!value) {
		ret = -EINVAL;
		goto error;
	}

	exists = json_object_object_get_ex(obj, key, &key_obj);
	if (!exists || !key_obj) {
		adaptived_info("Failed to find key %s\n", key);
		ret = -ENOENT;
		goto error;
	}

	*value = json_object_get_string(key_obj);
	if (!(*value)) {
		adaptived_err("Failed to get value for key %s\n", key);
		ret = -EINVAL;
		goto error;
	}

	return ret;

error:
	if (value)
		(*value) = NULL;

	return ret;
}

API int adaptived_parse_int(struct json_object * const obj, const char * const key, int * const value)
{
	const char *str_value;
	int ret = 0;
	char *end;

	if (!value) {
		ret = -EINVAL;
		goto error;
	}

	ret = adaptived_parse_string(obj, key, &str_value);
	if (ret)
		goto error;

	*value = strtol(str_value, &end, 10);
	if (str_value == end) {
		ret = -EINVAL;
		goto error;
	}

error:
	return ret;
}

API int adaptived_parse_float(struct json_object * const obj, const char * const key,
			   float * const value)
{
	const char *str_value;
	int ret = 0;
	char *end;

	if (!value) {
		ret = -EINVAL;
		goto error;
	}

	ret = adaptived_parse_string(obj, key, &str_value);
	if (ret)
		goto error;

	*value = strtof(str_value, &end);
	if (str_value == end) {
		ret = -EINVAL;
		goto error;
	}

error:
	return ret;
}

API int adaptived_parse_long_long(struct json_object * const obj, const char * const key,
			       long long * const value)
{
	const char *str_value;
	int ret = 0;
	char *end;

	if (!value) {
		ret = -EINVAL;
		goto error;
	}

	ret = adaptived_parse_string(obj, key, &str_value);
	if (ret)
		goto error;

	*value = strtoll(str_value, &end, 10);
	if (str_value == end) {
		ret = -EINVAL;
		goto error;
	}

error:
	return ret;
}

API int adaptived_parse_bool(struct json_object * const obj, const char * const key,
                          bool * const value)
{
	struct json_object *key_obj;
	json_bool exists, json_val;
	int ret = 0;

	if (!obj || !key || !value) {
		ret = -EINVAL;
		goto error;
	}

	exists = json_object_object_get_ex(obj, key, &key_obj);
	if (!exists || !key_obj) {
		adaptived_info("Failed to find key %s\n", key);
		ret = -ENOENT;
		goto error;
	}

	json_val = json_object_get_boolean(key_obj);

	if (json_val)
		*value = true;
	else
		*value = false;

	return ret;

error:
	return ret;
}

API int adaptived_parse_cgroup_value(struct json_object * const obj, const char * const key,
				  struct adaptived_cgroup_value * const value)
{
	struct json_object *value_obj;
	const char *value_str;
	enum json_type j_type;
	json_bool exists;
	int ret = 0;

	if (!obj || !key || !value) {
		ret = -EINVAL;
		goto error;
	}

	value->type = ADAPTIVED_CGVAL_CNT;

	exists = json_object_object_get_ex(obj, key, &value_obj);
	if (!exists || !value_obj) {
		adaptived_info("Failed to find key %s\n", key);
		ret = -ENOENT;
		goto error;
	}

	j_type = json_object_get_type(value_obj);
	switch(j_type) {
	case json_type_string:
		ret = adaptived_parse_string(obj, key, &value_str);
		if (ret)
			goto error;

		value->value.ll_value = adaptived_parse_human_readable(value_str);
		if (value->value.ll_value >= 0) {
			value->type = ADAPTIVED_CGVAL_LONG_LONG;
			adaptived_dbg("Parsed cgroup human readable value: %s (%lld)\n",
				value_str, value->value.ll_value);
			break;
		}

		value->value.str_value = malloc(sizeof(char) * strlen(value_str) + 1);
		if (!value->value.str_value) {
			ret = -ENOMEM;
			goto error;
		}

		strcpy(value->value.str_value, value_str);
		value->value.str_value[strlen(value_str)] = '\0';
		value->type = ADAPTIVED_CGVAL_STR;
		adaptived_dbg("Parsed cgroup value: string = %s\n", value->value.str_value);
		break;
	case json_type_int:
		ret = adaptived_parse_long_long(obj, key, &value->value.ll_value);
		if (ret)
			goto error;

		value->type = ADAPTIVED_CGVAL_LONG_LONG;
		adaptived_dbg("Parsed cgroup value: ll_value = %lld\n", value->value.ll_value);
		break;
	case json_type_double:
		ret = adaptived_parse_float(obj, key, &value->value.float_value);
		if (ret)
			goto error;

		value->type = ADAPTIVED_CGVAL_FLOAT;
		adaptived_dbg("Parsed cgroup value: float_value = %f\n", value->value.float_value);
		break;
	default:
		adaptived_err("Currently unsupported json type: %d\n", j_type);
		goto error;
	}

error:
	return ret;
}

API void adaptived_free_cgroup_value(struct adaptived_cgroup_value * const val)
{
	if (!val)
		return;

	if (val->type == ADAPTIVED_CGVAL_STR)
		free(val->value.str_value);
}

static int parse_cause(struct adaptived_ctx * const ctx, struct adaptived_rule * const rule,
		       struct json_object * const cause_obj)
{
	struct adaptived_cause *cse = NULL, *reg_cse, *tmp_cse;
	struct json_object *args_obj;
	bool found_cause = false;
	const char *name;
	json_bool exists;
	int ret = 0;
	int i;

	ret = adaptived_parse_string(cause_obj, "name", &name);
	if (ret )
		goto error;

	cse = cause_init(name);
	if (!cse) {
		ret = -ENOMEM;
		goto error;
	}

	exists = json_object_object_get_ex(cause_obj, "args", &args_obj);
	if (!exists || !args_obj) {
		ret = -EINVAL;
		goto error;
	}

	for (i = 0; i < CAUSE_CNT; i++) {
		if (strlen(cause_names[i]) != strlen(name))
			continue;

		if (strcmp(name, cause_names[i]) == 0) {
			found_cause = true;
			cse->idx = i;
			cse->fns = &cause_fns[i];

			adaptived_dbg("Initializing cause %s\n", cse->name);
			ret = (*cse->fns->init)(cse, args_obj, ctx->interval);
			if (ret)
				goto error;

			break;
		}
	}

	if (!found_cause) {
		/*
		 * We didn't find the cause in the built-in causes.  Now search the
		 * registered causes
		 */
		reg_cse = registered_causes;

		while (reg_cse) {
			if (strcmp(name, reg_cse->name) == 0) {
				found_cause = true;
				memcpy(cse, reg_cse, sizeof(struct adaptived_cause));
				cse->name = strdup(reg_cse->name);
				if (!cse->name) {
					ret = -ENOMEM;
					goto error;
				}

				/*
				 * Do not copy the ->next member from the registered causes
				 * linked list as it could point to another cause in the
				 * list of registered causes.  That cause may not be next in
				 * the list of causes for this rule.
				 */
				cse->next = NULL;

				adaptived_dbg("Initializing cause %s\n", cse->name);
				ret = (*cse->fns->init)(cse, args_obj, ctx->interval);
				if (ret)
					goto error;

				break;
			}

			reg_cse = reg_cse->next;
		}
	}

	if (!found_cause) {
		adaptived_err("Invalid cause provided: %s\n", name);
		ret = -EINVAL;
		goto error;
	}

	/*
	 * do not goto error after this point.  we have added the cse
	 * to the causes linked list
	 */
	if (!rule->causes) {
		rule->causes = cse;
	} else {
		tmp_cse = rule->causes;

		while (tmp_cse->next)
			tmp_cse = tmp_cse->next;
		tmp_cse->next = cse;
	}

	return ret;

error:
	if (cse && cse->name)
		free(cse->name);

	if (cse)
		free(cse);

	return ret;
}

static int parse_effect(struct adaptived_rule * const rule, struct json_object * const effect_obj)
{
	struct adaptived_effect *eff = NULL, *reg_eff, *tmp_eff;
	struct json_object *args_obj;
	bool found_effect = false;
	json_bool exists;
	const char *name;
	int ret = 0;
	int i;

	ret = adaptived_parse_string(effect_obj, "name", &name);
	if (ret )
		goto error;

	eff = effect_init(name);
	if (!eff) {
		ret = -ENOMEM;
		goto error;
	}

	exists = json_object_object_get_ex(effect_obj, "args", &args_obj);
	if (!exists || !args_obj) {
		ret = -EINVAL;
		goto error;
	}

	for (i = 0; i < EFFECT_CNT; i++) {
		if (strlen(effect_names[i]) != strlen(name))
			continue;

		if (strcmp(name, effect_names[i]) == 0) {
			found_effect = true;
			eff->idx = i;
			eff->fns = &effect_fns[i];

			adaptived_dbg("Initializing effect %s\n", eff->name);
			ret = (*eff->fns->init)(eff, args_obj, rule->causes);
			if (ret)
				goto error;

			break;
		}
	}

	if (!found_effect) {
		/*
		 * We didn't find the effect in the built-in effects.  Now search the
		 * registered effects
		 */
		reg_eff = registered_effects;

		while (reg_eff) {
			if (strcmp(name, reg_eff->name) == 0) {
				found_effect = true;
				memcpy(eff, reg_eff, sizeof(struct adaptived_effect));
				eff->name = strdup(reg_eff->name);
				if (!eff->name) {
					ret = -ENOMEM;
					goto error;
				}

				/*
				 * Do not copy the ->next member from the registered effects
				 * linked list as it could point to another effect in the
				 * list of registered effects.  That effect may not be next in
				 * the list of effects for this rule.
				 */
				eff->next = NULL;

				adaptived_dbg("Initializing effect %s\n", eff->name);
				ret = (*eff->fns->init)(eff, args_obj, rule->causes);
				if (ret)
					goto error;

				break;
			}

			reg_eff = reg_eff->next;
		}
	}

	if (!found_effect) {
		adaptived_err("Invalid effect provided: %s\n", name);
		ret = -EINVAL;
		goto error;
	}

	/*
	 * do not goto error after this point.  we have added the eff
	 * to the effects linked list
	 */
	if (!rule->effects) {
		rule->effects = eff;
	} else {
		tmp_eff = rule->effects;

		while (tmp_eff->next)
			tmp_eff = tmp_eff->next;
		tmp_eff->next = eff;
	}

	return ret;

error:
	if (eff && eff->name)
		free(eff->name);

	if (eff)
		free(eff);

	return ret;
}

int parse_rule(struct adaptived_ctx * const ctx, struct json_object * const rule_obj)
{
	struct json_object *causes_obj, *cause_obj, *effects_obj, *effect_obj;
	struct adaptived_rule *rule = NULL, *tmp_rule = NULL;
	int i, cause_cnt, effect_cnt;
	json_bool exists;
	const char *name;
	int ret = 0;

	ret = adaptived_parse_string(rule_obj, "name", &name);
	if (ret )
		goto error;

	rule = rule_init(name);
	if (!rule) {
		ret = -ENOMEM;
		goto error;
	}

	/*
	 * Verify that this rule has a unique name
	 */
	tmp_rule = ctx->rules;
	while (tmp_rule) {
		if (strcmp(name, tmp_rule->name) == 0) {
			adaptived_err("A rule with name %s already exists\n", name);
			ret = -EEXIST;
			goto error;
		}

		tmp_rule = tmp_rule->next;
	}

	/*
	 * Parse the causes
	 */
	exists = json_object_object_get_ex(rule_obj, "causes", &causes_obj);
	if (!exists || !causes_obj) {
		adaptived_err("Failed to find \"causes\" object\n");
		ret = -EINVAL;
		goto error;
	}

	cause_cnt = json_object_array_length(causes_obj);
	rule->stats.cause_cnt = cause_cnt;

	for (i = 0; i < cause_cnt; i++) {
		cause_obj = json_object_array_get_idx(causes_obj, i);
		if (!cause_obj) {
			adaptived_err("Failed to get cause object #%d\n", i);
			ret = -EINVAL;
			goto error;
		}

		ret = parse_cause(ctx, rule, cause_obj);
		if (ret)
			goto error;
	}

	/*
	 * Parse the effects
	 */
	exists = json_object_object_get_ex(rule_obj, "effects", &effects_obj);
	if (!exists || !effects_obj) {
		adaptived_err("Failed to find \"effects\" object\n");
		ret = -EINVAL;
		goto error;
	}

	effect_cnt = json_object_array_length(effects_obj);
	rule->stats.effect_cnt = effect_cnt;

	for (i = 0; i < effect_cnt; i++) {
		effect_obj = json_object_array_get_idx(effects_obj, i);
		if (!effect_obj) {
			adaptived_err("Failed to get effect object #%d\n", i);
			ret = -EINVAL;
			goto error;
		}

		ret = parse_effect(rule, effect_obj);
		if (ret)
			goto error;
	}

	/*
	 * do not goto error after this point.  we have added the rule
	 * to the rules linked list
	 */
	if (!ctx->rules) {
		ctx->rules = rule;
	} else {
		tmp_rule = ctx->rules;

		while (tmp_rule->next)
			tmp_rule = tmp_rule->next;
		tmp_rule->next = rule;
	}

	return ret;

error:
	if (rule)
		rule_destroy(&rule);

	return ret;
}

static int parse_json(struct adaptived_ctx * const ctx, const char * const buf)
{
	struct json_object *obj, *rules_obj, *rule_obj;
	enum json_tokener_error err;
	json_bool exists;
	int ret = 0, i;
	int rule_cnt;

	obj = json_tokener_parse_verbose(buf, &err);
	if (!obj || err) {
		if (err)
			adaptived_err("%s: %s\n", __func__, json_tokener_error_desc(err));
		ret = -EINVAL;
		goto out;
	}

	exists = json_object_object_get_ex(obj, "rules", &rules_obj);
	if (!exists || !rules_obj) {
		adaptived_err("Failed to get \"rules\" object\n");
		ret = -EINVAL;
		goto out;
	}

	rule_cnt = json_object_array_length(rules_obj);

	for (i = 0; i < rule_cnt; i++) {
		rule_obj = json_object_array_get_idx(rules_obj, i);
		if (!rule_obj) {
			adaptived_err("Failed to get rule object #%d\n", i);
			ret = -EINVAL;
			goto out;
		}

		ret = parse_rule(ctx, rule_obj);
		if (ret)
			goto out;
	}

out:
	return ret;
}

int parse_config(struct adaptived_ctx * const ctx)
{
	FILE *config_fd = NULL;
	long config_size = 0;
	size_t chars_read;
	char *buf = NULL;
	int ret;

	config_fd = fopen(ctx->config, "r");
	if (!config_fd) {
		adaptived_err("Failed to fopen %s\n", ctx->config);
		ret = -errno;
		goto out;
	}

	ret = get_file_size(config_fd, &config_size);
	if (ret)
		goto out;

	buf = malloc(sizeof(char) * (config_size + 1));
	if (!buf) {
		ret = -ENOMEM;
		goto out;
	}

	chars_read = fread(buf, sizeof(char), config_size, config_fd);
	if (chars_read != config_size) {
		adaptived_err("Expected to read %ld bytes but read %ld bytes\n",
			   config_size, chars_read);
		ret = -EIO;
		goto out;
	}
	buf[config_size] = '\0';

	ret = parse_json(ctx, buf);
	if (ret)
		goto out;

out:
	if (config_fd)
		fclose(config_fd);

	if (buf)
		free(buf);

	return ret;
}

int insert_into_json_args_obj(struct json_object * const parent, const char * const key,
			      struct json_object * const value_obj)
{
	struct json_object *args_obj;
	json_bool exists;
	int ret;

	exists = json_object_object_get_ex(parent, "args", &args_obj);
	if (!exists || !args_obj)
		/*
		 * The args object doesn't exist.  We'll create a local one
		 * then add it at the end
		 */
		args_obj = json_object_new_object();

	ret = json_object_object_add(args_obj, key, value_obj);
	if (ret) {
		adaptived_err("Failed to add key %s to args object\n", key);
		return -EINVAL;
	}

	if (!exists) {
		ret = json_object_object_add(parent, "args", args_obj);
		if (ret) {
			adaptived_err("Failed to add args object\n");
			return ret;
		}
	}

	return 0;
}

static const char * const default_cause_op_name = "operator";
int parse_cause_operation(struct json_object * const args_obj, const char * const name,
			  enum cause_op_enum * const op)
{
	const char *op_str, *op_name;
	int ret, i;

	if (name)
		op_name = name;
	else
		op_name = default_cause_op_name;

	ret = adaptived_parse_string(args_obj, op_name, &op_str);
	if (ret)
		return ret;

	for (i = 0; i < COP_CNT; i++) {
		if (strncmp(op_str, cause_op_names[i], strlen(cause_op_names[i])) == 0) {
			*op = i;
			return 0;
		}
	}

	adaptived_err("Invalid operator provided: %s\n", op_str);
	return -EINVAL;
}
