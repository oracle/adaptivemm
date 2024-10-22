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
 * Test to register many causes and effects at runtime
 *
 */

#include <stdbool.h>
#include <syslog.h>
#include <errno.h>

#include <adaptived.h>

#include "ftests.h"

#define EXPECTED_RET -ETIME
#define MAX_LOOPS 1000
#define EFFECT_CNT 20
#define CAUSE_CNT 10

static bool test_failed;
static const char * const rule_name = "test 017";

struct count_opts {
	int cnt;
};

int count_cause_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval)
{
	struct count_opts *opts;
	int ret = 0;

	opts = malloc(sizeof(struct count_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}

	opts->cnt = 0;

	ret = adaptived_cause_set_data(cse, (void *)opts);
	if (ret)
		goto error;

	return ret;

error:
	if (opts)
		free(opts);

	return ret;
}

int count_cause_main(struct adaptived_cause * const cse, int time_since_last_run)
{
	struct count_opts *opts;

	opts = (struct count_opts *)adaptived_cause_get_data(cse);

	opts->cnt++;

	/* always trigger */
	return 1;
}

void count_cause_exit(struct adaptived_cause * const cse)
{
	struct count_opts *opts;

	opts = (struct count_opts *)adaptived_cause_get_data(cse);

	if (opts->cnt != MAX_LOOPS) {
		adaptived_err("Cause expected %d loops but only ran %d times\n",
			   MAX_LOOPS, opts->cnt);
		test_failed = true;
	}

	free(opts);
}

const struct adaptived_cause_functions count_cause_fns = {
	count_cause_init,
	count_cause_main,
	count_cause_exit,
};

int count_effect_init(struct adaptived_effect * const eff, struct json_object *args_obj,
		      const struct adaptived_cause * const cse)
{
	struct count_opts *opts;
	int ret = 0;

	opts = malloc(sizeof(struct count_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}

	ret = adaptived_effect_set_data(eff, (void *)opts);
	if (ret)
		goto error;

	return ret;

error:
	if (opts)
		free(opts);

	return ret;
}

int count_effect_main(struct adaptived_effect * const eff)
{
	struct count_opts * opts;

	opts = (struct count_opts *)adaptived_effect_get_data(eff);

	opts->cnt++;

	return 0;
}

void count_effect_exit(struct adaptived_effect * const eff)
{
	struct count_opts *opts;

	opts = (struct count_opts *)adaptived_effect_get_data(eff);

	if (opts->cnt != MAX_LOOPS) {
		adaptived_err("Cause expected %d loops but only ran %d times\n",
			   MAX_LOOPS, opts->cnt);
		test_failed = true;
	}

	free(opts);
}

const struct adaptived_effect_functions count_effect_fns = {
	count_effect_init,
	count_effect_main,
	count_effect_exit,
};

int main(int argc, char *argv[])
{
	struct adaptived_effect *eff[EFFECT_CNT] = { NULL };
	struct adaptived_cause *cse[CAUSE_CNT] = { NULL };
	struct adaptived_rule *rule = NULL;
	struct adaptived_ctx *ctx = NULL;
	char name[64] = { 0 };
	int ret, i;

	test_failed = false;

	ctx = adaptived_init(NULL);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, MAX_LOOPS);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 7000);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_SKIP_SLEEP, 1);
	if (ret)
		goto err;

	rule = adaptived_build_rule(rule_name);
	if (!rule)
		goto err;

	for (i = 0; i < CAUSE_CNT; i++) {
		snprintf(name, sizeof(name) - 1, "cause_count%03d", i);

		ret = adaptived_register_cause(ctx, name, &count_cause_fns);
		if (ret)
			goto err;

		cse[i] = adaptived_build_cause(name);
		if (!cse[i])
			goto err;
		/* Add an arbitrary and unused arg to appease the parser */
		ret = adaptived_cause_add_string_arg(cse[i], "name", name);
		if (ret) 
			goto err;

		ret = adaptived_rule_add_cause(rule, cse[i]);
		if (ret)
			goto err;
	}

	for (i = 0; i < EFFECT_CNT; i++) {
		snprintf(name, sizeof(name) - 1, "effect_count%03d", i);

		ret = adaptived_register_effect(ctx, name, &count_effect_fns);
		if (ret)
			goto err;

		eff[i] = adaptived_build_effect(name);
		if (!eff[i])
			goto err;
		/* Add an arbitrary and unused arg to appease the parser */
		ret = adaptived_effect_add_string_arg(eff[i], "name", name);
		if (ret) 
			goto err;

		ret = adaptived_rule_add_effect(rule, eff[i]);
		if (ret)
			goto err;
	}
	
	ret = adaptived_load_rule(ctx, rule);
	if (ret)
		goto err;

	ret = adaptived_loop(ctx, false);
	if (ret != EXPECTED_RET)
		goto err;

	if (test_failed)
		goto err;

	for (i = 0; i < CAUSE_CNT; i++) {
		if (cse[i])
			adaptived_release_cause(&cse[i]);
	}
	for (i = 0; i < EFFECT_CNT; i++) {
		if (eff[i])
			adaptived_release_effect(&eff[i]);
	}
	if (rule)
		adaptived_release_rule(&rule);
	adaptived_release(&ctx);

	return AUTOMAKE_PASSED;

err:
	for (i = 0; i < CAUSE_CNT; i++) {
		if (cse[i])
			adaptived_release_cause(&cse[i]);
	}
	for (i = 0; i < EFFECT_CNT; i++) {
		if (eff[i])
			adaptived_release_effect(&eff[i]);
	}
	if (rule)
		adaptived_release_rule(&rule);
	adaptived_release(&ctx);

	return AUTOMAKE_HARD_ERROR;
}
