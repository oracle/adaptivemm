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
 * Test to build, load, and unload a rule at runtime
 *
 */

#include <pthread.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>

#include <adaptived.h>

#include "ftests.h"

#define LOOP_CNT 5

static const char * const rule_name = "test 016";

static void *adaptived_wrapper(void *arg)
{
	struct adaptived_ctx *ctx = arg;
	uintptr_t ret;

	ret = adaptived_loop(ctx, false);

	return (void *)ret;
}

char *get_current_time(void)
{
#define LEN (2 + 1 + 2 + 1 + 2 + 2)

	struct tm *cur_tm;
	time_t cur_time;
	char *time_str;

	time(&cur_time);
	cur_tm = localtime(&cur_time);

	time_str = malloc(sizeof(char) * LEN);
	if (!time_str)
		return NULL;

	snprintf(time_str, LEN - 1, "%02d:%02d:%02d", cur_tm->tm_hour, cur_tm->tm_min,
		 cur_tm->tm_sec);
	time_str[LEN - 1] = '\0';

	return time_str;
}

int main(int argc, char *argv[])
{
	struct adaptived_rule_stats stats;
	uint32_t rule_cnt;
	struct adaptived_effect *eff;
	struct adaptived_cause *cse;
	struct adaptived_rule *rule;
	pthread_t adaptived_thread;
	struct adaptived_ctx *ctx = NULL;
	char *time_str = NULL;
	void *tret;
	int ret;

	cse = adaptived_build_cause("time_of_day");
	if (!cse)
		goto err;

	time_str = get_current_time();
	if (!time_str)
		goto err;

	ret = adaptived_cause_add_string_arg(cse, "time", time_str);
	if (ret) 
		goto err;
	ret = adaptived_cause_add_string_arg(cse, "operator", "greaterthan");
	if (ret)
		goto err;

	eff = adaptived_build_effect("print");
	if (!eff)
		goto err;

	ret = adaptived_effect_add_string_arg(eff, "message", "016 triggered\n");
	if (ret)
		goto err;
	ret = adaptived_effect_add_string_arg(eff, "file", "stdout");
	if (ret)
		goto err;

	rule = adaptived_build_rule(rule_name);
	if (!rule)
		goto err;
	ret = adaptived_rule_add_cause(rule, cse);
	if (ret)
		goto err;
	ret = adaptived_rule_add_effect(rule, eff);
	if (ret)
		goto err;

	ctx = adaptived_init(NULL);
	if (!ctx)
		goto err;

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_DEBUG);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 1000);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, LOOP_CNT);
	if (ret)
		goto err;

	ret = pthread_create(&adaptived_thread, NULL, &adaptived_wrapper, ctx);
	if (ret)
		goto err;

	ret = adaptived_load_rule(ctx, rule);
	if (ret)
		goto err;

	ret = adaptived_get_attr(ctx, ADAPTIVED_ATTR_RULE_CNT, &rule_cnt);
	if (ret)
		goto err;
	if (rule_cnt != 1)
		goto err;

	sleep(2);
	ret = adaptived_unload_rule(ctx, rule_name);
	if (ret)
		goto err;

	ret = adaptived_get_attr(ctx, ADAPTIVED_ATTR_RULE_CNT, &rule_cnt);
	if (ret)
		goto err;
	if (rule_cnt != 0)
		goto err;

	pthread_join(adaptived_thread, &tret);

	if (tret != (void *)-ETIME)
		goto err;

	ret = adaptived_get_rule_stats(ctx, rule_name, &stats);
	if (ret != -EEXIST)
		goto err;

	if (time_str)
		free(time_str);

	adaptived_release_cause(&cse);
	adaptived_release_effect(&eff);
	adaptived_release_rule(&rule);

	adaptived_unload_rule(ctx, rule_name);
	adaptived_release(&ctx);

	return AUTOMAKE_PASSED;

err:
	if (time_str)
		free(time_str);

	adaptived_release_cause(&cse);
	adaptived_release_effect(&eff);
	adaptived_release_rule(&rule);

	if (ctx) {
		adaptived_unload_rule(ctx, rule_name);
		adaptived_release(&ctx);
	}

	return AUTOMAKE_HARD_ERROR;
}
