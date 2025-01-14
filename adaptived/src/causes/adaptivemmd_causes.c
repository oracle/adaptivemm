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

static void free_opts(struct adaptivemmd_opts * const opts)
{
	if (!opts)
		return;

	free(opts);
}

int adaptivemmd_causes_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval)
{
	struct adaptivemmd_opts *opts;
	int ret = -EINVAL;

	opts = malloc(sizeof(struct adaptivemmd_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}

	memset(opts, 0, sizeof( struct adaptivemmd_opts));

        ret = adaptived_parse_bool(args_obj, "ENABLE_FREE_PAGE_MGMT", &opts->memory_pressure_check_enabled);
        if (ret && ret != -ENOENT) {
		adaptived_err("ENABLE_FREE_PAGE_MGMT failed, ret=%d\n", ret);
                goto error;
        }
        ret = adaptived_parse_bool(args_obj, "ENABLE_NEG_DENTRY_MGMT", &opts->neg_dentry_check_enabled);
        if (ret && ret != -ENOENT) {
		adaptived_err("ENABLE_NEG_DENTRY_MGMT failed, ret=%d\n", ret);
                goto error;
        }
        ret = adaptived_parse_bool(args_obj, "ENABLE_MEMLEAK_CHECK", &opts->memleak_check_enabled);
        if (ret && ret != -ENOENT) {
		adaptived_err("ENABLE_MEMLEAK_CHECK failed, ret=%d\n", ret);
                goto error;
        }
	adaptived_dbg("%s: memory_pressure_check_enabled=%d, neg_dentry_check_enabled=%d, memleak_check_enabled=%d\n",
		__func__, opts->memory_pressure_check_enabled, opts->neg_dentry_check_enabled, opts->memleak_check_enabled);

	if (!opts->memory_pressure_check_enabled && !opts->neg_dentry_check_enabled && !opts->memleak_check_enabled) {
		adaptived_err("%s: Not enables set.\n", __func__);
		ret = -ENOENT;
                goto error;
	}

        ret = adaptived_parse_int(args_obj, "mem_trigger_delta", &opts->mem_trigger_delta);
        if (ret == -ENOENT) {
                opts->mem_trigger_delta = MEM_TRIGGER_DELTA;
        } else if (ret) {
                goto error;
        }
	adaptived_dbg("Minimum %% change in meminfo numbers trigger: %d\n", opts->mem_trigger_delta);
        if (opts->mem_trigger_delta < 0 || opts->mem_trigger_delta > 100) {
		adaptived_err("mem_trigger_delta %d invalid.\n", opts->mem_trigger_delta);
                goto error;
	}

        ret = adaptived_parse_int(args_obj, "unacct_mem_grth_max", &opts->unacct_mem_grth_max);
        if (ret == -ENOENT) {
                opts->unacct_mem_grth_max = UNACCT_MEM_GRTH_MAX;
        } else if (ret) {
                goto error;
        }
	adaptived_dbg("Unaccounted memory growth max samples: %d\n", opts->unacct_mem_grth_max);
        if (opts->unacct_mem_grth_max < 0) {
		adaptived_err("unacct_mem_grth_max %d invalid.\n", opts->unacct_mem_grth_max);
                goto error;
	}

	ret = adaptived_parse_int(args_obj, "neg_dentry_pct", &opts->neg_dentry_pct);
	if (ret == -ENOENT) {
		opts->neg_dentry_pct = MAX_NEGDENTRY_DEFAULT;
	} else if (ret) {
		goto error;
	}
	if (opts->neg_dentry_pct > MAX_NEGDENTRY) {
		adaptived_err("Bad value for negative dentry cap = %d (>%d). "
		    "Proceeding with default of %d\n", opts->neg_dentry_pct,
			MAX_NEGDENTRY, MAX_NEGDENTRY_DEFAULT);
		opts->neg_dentry_pct = MAX_NEGDENTRY_DEFAULT;
	} else if (!opts->neg_dentry_pct)
		opts->neg_dentry_pct = 1;
	if (opts->neg_dentry_pct < 0) {
		adaptived_err("neg_dentry_pct %d invalid.\n", opts->neg_dentry_pct);
		ret = -EINVAL;
		goto error;
	}
	adaptived_dbg("%s: opts->neg_dentry_pct = %d\n", __func__, opts->neg_dentry_pct);

	ret = adaptived_parse_int(args_obj, "maxgap", &opts->maxgap);
	if (ret == -ENOENT) {
		opts->maxgap = 0;
	} else if (ret) {
		goto error;
	}
	adaptived_dbg("%s: opts->maxgap = %d\n", __func__, opts->maxgap);

        ret = adaptived_parse_int(args_obj, "debug_mode", &opts->debug_mode);
        if (ret == -ENOENT) {
                opts->debug_mode = 0;
        } else if (ret) {
                goto error;
        }
        adaptived_dbg("%s: opts->debug_mode = %d\n", __func__, opts->debug_mode);

        ret = adaptived_parse_int(args_obj, "verbose", &opts->verbose);
        if (ret == -ENOENT) {
                opts->verbose = 0;
        } else if (ret) {
                goto error;
        }
        adaptived_dbg("%s: opts->verbose = %d\n", __func__, opts->verbose);
        if (opts->verbose > MAX_VERBOSE) {
                adaptived_err("Verbosity value is greater than %d. Proceeding with defaults", MAX_VERBOSE);
                ret = -EINVAL;
                goto error;
        }

	ret = run_adaptivemm_init(opts, interval);
        if (ret < 0)
                goto error;

	ret = adaptived_cause_set_data(cse, (void *)opts);
        if (ret)
                goto error;

	adaptived_dbg("%s: opts=%p, memory_pressure_check_enabled=%d, neg_dentry_check_enabled=%d, memleak_check_enabled=%d\n",
                __func__, opts, opts->memory_pressure_check_enabled, opts->neg_dentry_check_enabled, opts->memleak_check_enabled);

	return ret;

error:
	adaptived_err("adaptivemmd_causes_init: FAIL, ret=%d\n", ret);
	free_opts(opts);
	return ret;
}

int adaptivemmd_causes_main(struct adaptived_cause * const cse, int time_since_last_run)
{
	struct adaptivemmd_opts *opts = (struct adaptivemmd_opts *)adaptived_cause_get_data(cse);
	int ret;

	adaptived_dbg("%s: time_since_last_run=%d\n", __func__, time_since_last_run);

	ret = run_adaptivemm(opts);
        if (ret < 0)
                goto error;
	
	adaptived_dbg("%s: OK: ret=%d\n", __func__, ret);
	return ret;

error:
	adaptived_dbg("%s: FAIL: ret=%d\n", __func__, ret);

	return ret;
}

void adaptivemmd_causes_exit(struct adaptived_cause * const cse)
{
	struct adaptivemmd_opts *opts = (struct adaptivemmd_opts *)adaptived_cause_get_data(cse);

	free_opts(opts);
}
