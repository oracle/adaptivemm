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
 * Test to set a cgroup setting
 *
 */

#include <stdbool.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>

#include <adaptived.h>

#include "ftests.h"

#define EXPECTED_RET -ETIME

static const char * const rule_name = "test 018";
static const char * const cgroup_file = "./test018.cgroup";
static const char * const expected_value = "This was written by the cgroup_setting effect";

int always_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval)
{
	return 0;
}

int always_main(struct adaptived_cause * const cse, int time_since_last_run)
{
	/* always trigger */
	return 1;
}

void always_exit(struct adaptived_cause * const cse)
{
}

const struct adaptived_cause_functions always_fns = {
	always_init,
	always_main,
	always_exit,
};

int main(int argc, char *argv[])
{
	struct adaptived_effect *eff = NULL;
	struct adaptived_rule *rule = NULL;
	struct adaptived_ctx *ctx = NULL;
	struct adaptived_cause *cse;
	int ret;

	ctx = adaptived_init(NULL);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	write_file(cgroup_file, "Doesn't match the expected value");

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 12000);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_SKIP_SLEEP, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_DEBUG);
	if (ret)
		goto err;
	ret = adaptived_register_cause(ctx, "registered_always", &always_fns);
	if (ret)
		goto err;

	rule = adaptived_build_rule(rule_name);
	if (!rule)
		goto err;

	cse = adaptived_build_cause("registered_always");
	if (!cse)
		goto err;

	/* Add an arbitrary and unused arg to appease the parser */
	ret = adaptived_cause_add_string_arg(cse, "name", "Always");
	if (ret) 
		goto err;

	ret = adaptived_rule_add_cause(rule, cse);
	if (ret)
		goto err;

	eff = adaptived_build_effect("cgroup_setting");
	if (!eff)
		goto err;

	ret = adaptived_effect_add_string_arg(eff, "setting", cgroup_file);
	if (ret) 
		goto err;
	ret = adaptived_effect_add_string_arg(eff, "value", expected_value);
	if (ret) 
		goto err;
	ret = adaptived_effect_add_string_arg(eff, "operator", "set");
	if (ret) 
		goto err;

	ret = adaptived_rule_add_effect(rule, eff);
	if (ret)
		goto err;
	
	ret = adaptived_load_rule(ctx, rule);
	if (ret)
		goto err;

	ret = adaptived_loop(ctx, false);
	if (ret != EXPECTED_RET)
		goto err;

	ret = verify_char_file(cgroup_file, expected_value);
	if (ret)
		goto err;

	delete_file(cgroup_file);
	if (cse)
		adaptived_release_cause(&cse);
	if (eff)
		adaptived_release_effect(&eff);
	if (rule)
		adaptived_release_rule(&rule);
	adaptived_release(&ctx);

	return AUTOMAKE_PASSED;

err:
	delete_file(cgroup_file);
	if (cse)
		adaptived_release_cause(&cse);
	if (eff)
		adaptived_release_effect(&eff);
	if (rule)
		adaptived_release_rule(&rule);
	adaptived_release(&ctx);

	return AUTOMAKE_HARD_ERROR;
}
