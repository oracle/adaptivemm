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
 * daemon test
 *
 */

#include <stdbool.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>

#include <adaptived.h>

#include "ftests.h"

#define EXPECTED_RET -ETIME

int main(int argc, char *argv[])
{
	struct adaptived_ctx *ctx = NULL;
	int ret;

	/* Run as as daemon. */
	ctx = adaptived_init(NULL);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 2);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 1000);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_SKIP_SLEEP, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_DEBUG);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_DAEMON_MODE, 1);
	if (ret)
		goto err;

	ret = adaptived_loop(ctx, false);
	if (ret != EXPECTED_RET)
		goto err;

	adaptived_release(&ctx);

	/* Do not run as daemon. */
	ctx = adaptived_init(NULL);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 2);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 1000);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_SKIP_SLEEP, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_DEBUG);
	if (ret)
		goto err;

	ret = adaptived_loop(ctx, false);
	if (ret != EXPECTED_RET)
		goto err;

	adaptived_release(&ctx);

	return AUTOMAKE_PASSED;

err:
	adaptived_release(&ctx);

	return AUTOMAKE_HARD_ERROR;
}
