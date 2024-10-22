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
 * Test to register a cause at runtime and utilize it
 *
 */

#include <json-c/json.h>
#include <syslog.h>
#include <errno.h>

#include <adaptived.h>

#include "ftests.h"

int main(int argc, char *argv[])
{
	struct adaptived_rule_stats stats;
	char config_path[FILENAME_MAX];
	struct adaptived_ctx *ctx;
	uint32_t trigger_cnt;
	int ret;

	snprintf(config_path, FILENAME_MAX - 1, "%s/010-snooze_effect.json", argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	ctx = adaptived_init(config_path);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 6);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 1000);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_DEBUG);
	if (ret)
		goto err;

	ret = adaptived_loop(ctx, true);
	if (ret != -ETIME)
		goto err;

	/* we want the trigger count for rule 0 */
	ret = adaptived_get_rule_stats(ctx, "Ensure the snooze effect silences effects as expected",
				    &stats);
	if (ret != 0)
		goto err;

	if (stats.trigger_cnt != 6)
		goto err;
	if (stats.snooze_cnt != 4)
		goto err;

	adaptived_release(&ctx);
	return AUTOMAKE_PASSED;

err:
	adaptived_release(&ctx);
	return AUTOMAKE_HARD_ERROR;
}
