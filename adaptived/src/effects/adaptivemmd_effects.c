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
 * Cause to evaluate and respond to memory consumption trend
 *
 */

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#include <pthread.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <stdbool.h>
#include <signal.h>
#include <linux/kernel-page-flags.h>

#include <adaptived-utils.h>
#include <adaptived.h>

#include "adaptived-internal.h"
#include "defines.h"

#include "adaptivemmd.h"

static int debug_mode, verbose;

static void free_opts(struct adaptivemmd_effects_opts * const opts)
{
	if (!opts)
		return;

	free(opts);
}

int adaptivemmd_effects_init(struct adaptived_effect * const eff, struct json_object *args_obj,
               const struct adaptived_cause * const cse)
{
	struct adaptivemmd_effects_opts *opts;
	struct adaptivemmd_opts *cse_opts;
	int ret = -EINVAL;

	opts = malloc(sizeof(struct adaptivemmd_effects_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}

	memset(opts, 0, sizeof(struct adaptivemmd_effects_opts));

	if (strcmp(cse->name, "adaptivemmd_causes") != 0) {
		adaptived_err("This effect (adaptivemmd_effects) is tightly coupled with the "
			      "adaptivemmd_causes cause.  Provided cause: %s is unsupported\n",
			      cse->name);
		ret = -EINVAL;
		goto error;
	}
	if (cse->next != NULL) {
		adaptived_err("Effect adaptivemmd_effects currently only supports cause - "
			      "adaptivemmd_causes\n");
		ret = -EINVAL;
		goto error;
	}

	opts->cse = cse;

	eff->data = (void *)cse->data;

	cse_opts = (struct adaptivemmd_opts *)cse->data;

	debug_mode = cse_opts->debug_mode;
	verbose = cse_opts->verbose;

	adaptived_dbg("%s: cse_opts=%p, memory_pressure_check_enabled=%d, neg_dentry_check_enabled=%d, memleak_check_enabled=%d\n",
                __func__, cse_opts, cse_opts->memory_pressure_check_enabled, cse_opts->neg_dentry_check_enabled, cse_opts->memleak_check_enabled);

	return 0;

error:
	adaptived_err("adaptivemmd_effects_init: FAIL, ret=%d\n", ret);
	free_opts(opts);
	return ret;
}

int adaptivemmd_effects_main(struct adaptived_effect * const eff)
{
	struct adaptivemmd_opts *opts = (struct adaptivemmd_opts *)eff->data;
	int ret;

	if (!opts) {
		adaptived_err("adaptivemmd_effects_main: eff->data (opts) is NULL\n");
		return -EINVAL;
	}

	ret = run_adaptivemm_effects((struct adaptivemmd_opts * const)opts);
        if (ret < 0)
                goto error;
	
	adaptived_dbg("%s: OK: ret=%d\n", __func__, ret);
	return ret;

error:
	adaptived_dbg("%s: FAIL: ret=%d\n", __func__, ret);

	return ret;
}

void adaptivemmd_effects_exit(struct adaptived_effect * const eff)
{
	struct adaptivemmd_effects_opts *opts = (struct adaptivemmd_effects_opts *)eff->data;

	free_opts(opts);
}
