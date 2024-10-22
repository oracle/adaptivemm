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
 * Effect to validate that a cause successfully fired
 *
 * Returns a positive integer to the main adaptived loop when
 * the validator main() is invoked.  This will cause adaptived
 * to exit the main loop and can be useful for testing that
 * causes have executed properly
 *
 *
 */

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <adaptived.h>

const int default_return_value = 7;

const char * const tod_validate_name = "tod_validate";

struct validate_opts {
	int	ret;
};

int tod_validate_init(struct adaptived_effect * const eff, struct json_object *args_obj,
	       const struct adaptived_cause * const cse)
{
	struct validate_opts *opts;
	int ret = 0;

	opts = malloc(sizeof(struct validate_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}
	opts->ret = default_return_value;

	ret = adaptived_parse_int(args_obj, "return_value", &opts->ret);
	if (ret)
		goto error;

	/* we have successfully setup the validate effect */
	ret = adaptived_effect_set_data(eff, (void *)opts);
	if (ret)
		goto error;

	return ret;

error:
	if (opts)
		free(opts);

	return ret;
}

int tod_validate_main(struct adaptived_effect * const eff)
{
	struct validate_opts * opts;

	opts = (struct validate_opts *)adaptived_effect_get_data(eff);

	/* main() will negate our return value */
	return -opts->ret;
}

void tod_validate_exit(struct adaptived_effect * const eff)
{
	struct validate_opts *opts;

	opts = (struct validate_opts *)adaptived_effect_get_data(eff);

	free(opts);
}

const struct adaptived_effect_functions tod_validate_fns = {
	tod_validate_init,
	tod_validate_main,
	tod_validate_exit,
};
