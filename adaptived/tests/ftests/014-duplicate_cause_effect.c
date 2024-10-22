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
 * Test to register a cause/effect whose name already exists
 *
 */

#include <json-c/json.h>
#include <errno.h>

#include <adaptived.h>

#include "ftests.h"

int invalid_cause_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval)
{
	return -EINVAL;
}

int invalid_cause_main(struct adaptived_cause * const cse, int time_since_last_run)
{
	return -EINVAL;
}

void invalid_cause_exit(struct adaptived_cause * const cse)
{
}

const struct adaptived_cause_functions invalid_cause_fns = {
	invalid_cause_init,
	invalid_cause_main,
	invalid_cause_exit,
};

int invalid_effect_init(struct adaptived_effect * const eff, struct json_object *args_obj,
			const struct adaptived_cause * const cse)
{
	return -EINVAL;
}

int invalid_effect_main(struct adaptived_effect * const eff)
{
	return -EINVAL;
}

void invalid_effect_exit(struct adaptived_effect * const eff)
{
}

const struct adaptived_effect_functions invalid_effect_fns = {
	invalid_effect_init,
	invalid_effect_main,
	invalid_effect_exit,
};

int main(int argc, char *argv[])
{
	struct adaptived_ctx *ctx;
	int ret;

	ctx = adaptived_init(NULL);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	/* collide with a built-in cause */
	ret = adaptived_register_cause(ctx, "days_of_the_week", &invalid_cause_fns);
	if (ret != -EEXIST)
		goto err;

	/* 
	 * Successfully register a new cause then try to register it again
	 */
	ret = adaptived_register_cause(ctx, "invalid", &invalid_cause_fns);
	if (ret != 0)
		goto err;
	ret = adaptived_register_cause(ctx, "invalid", &invalid_cause_fns);
	if (ret != -EEXIST)
		goto err;

	/* register a cause with the same prefix as an existing cause */
	ret = adaptived_register_cause(ctx, "invalid2", &invalid_cause_fns);
	if (ret != 0)
		goto err;
	ret = adaptived_register_cause(ctx, "days_of_the_week_again", &invalid_cause_fns);
	if (ret != 0)
		goto err;

	/* collide with a built-in effect */
	ret = adaptived_register_effect(ctx, "validate", &invalid_effect_fns);
	if (ret != -EEXIST)
		goto err;

	/* 
	 * Successfully register a new effect then try to register it again
	 */
	ret = adaptived_register_effect(ctx, "invalid", &invalid_effect_fns);
	if (ret != 0)
		goto err;
	ret = adaptived_register_effect(ctx, "invalid", &invalid_effect_fns);
	if (ret != -EEXIST)
		goto err;

	/* register an effect with the same prefix as an existing effect */
	ret = adaptived_register_effect(ctx, "invalid2", &invalid_effect_fns);
	if (ret != 0)
		goto err;
	ret = adaptived_register_effect(ctx, "print_more", &invalid_effect_fns);
	if (ret != 0)
		goto err;


	adaptived_release(&ctx);
	return AUTOMAKE_PASSED;

err:
	adaptived_release(&ctx);
	return AUTOMAKE_HARD_ERROR;
}
