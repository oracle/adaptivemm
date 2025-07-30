/*
 * Copyright (c) 2025, Oracle and/or its affiliates.
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
 * adaptived file for sharing data between causes and effects
 */

#include <adaptived.h>
#include <stdbool.h>
#include <errno.h>

#include "adaptived-internal.h"
#include "shared_data.h"
#include "cause.h"

/*
 * Method for a cause to share data with effect(s) in the same rule.
 *
 * Note that the shared data is deleted at the end of each run of the
 * adaptived main loop
 */
API int adaptived_write_shared_data(struct adaptived_cause * const cse,
				    enum adaptived_sdata_type type, void *data,
				    adaptived_sdata_free free_fn,
				    uint32_t flags)
{
	struct shared_data *sdata, *prev;

	if (!cse || !data)
		return -EINVAL;

	if (type < 0)
		return -EINVAL;
	if (type >= ADAPTIVED_SDATA_CNT)
		return -EINVAL;
	if (type == ADAPTIVED_SDATA_CUSTOM && free_fn == NULL)
		return -EINVAL;
	if (type != ADAPTIVED_SDATA_CUSTOM && free_fn != NULL)
		return -EINVAL;

	sdata = malloc(sizeof(struct shared_data));
	if (!sdata)
		return -ENOMEM;

	sdata->type = type;
	sdata->data = data;
	sdata->free_fn = free_fn;
	sdata->flags = flags;
	sdata->next = NULL;

	if (cse->sdata == NULL) {
		cse->sdata = sdata;
	} else {
		prev = cse->sdata;

		while (prev != NULL) {
			if (prev->next == NULL)
				break;

			prev = prev->next;
		}

		prev->next = sdata;
	}

	return 0;
}

API int adaptived_update_shared_data(struct adaptived_cause * const cse, int index,
				     enum adaptived_sdata_type type, void *data,
				     uint32_t flags)
{
	struct shared_data *sdata;

	if (cse == NULL || data == NULL)
		return -EINVAL;

	if (index < 0)
		return -EINVAL;

	sdata = cse->sdata;

	if (sdata == NULL)
		return -ERANGE;

	while (index > 0) {
		if (sdata->next == NULL)
			return -ERANGE;

		sdata = sdata->next;
		index--;
	}

	if (sdata->type != type)
		/* Don't allow the changing of the data type */
		return -EINVAL;

	/*
	 * It's up to the user to ensure that the old data field is properly freed and not
	 * leaked
	 */
	sdata->data = data;
	sdata->flags = flags;

	return 0;
}

API int adaptived_get_shared_data_cnt(const struct adaptived_cause * const cse)
{
	struct shared_data *sdata = NULL;
	int cnt = 0;

	if (cse == NULL)
		return 0;

	sdata = cse->sdata;

	while (sdata) {
		cnt++;
		sdata = sdata->next;
	}

	return cnt;
}

API int adaptived_get_shared_data(const struct adaptived_cause * const cse, int index,
				  enum adaptived_sdata_type * const type, void **data,
				  uint32_t * const flags)
{
	struct shared_data *sdata;

	if (cse == NULL || type == NULL || data == NULL || flags == NULL)
		return -EINVAL;

	if (index < 0)
		return -EINVAL;

	sdata = cse->sdata;

	if (sdata == NULL)
		return -ERANGE;

	while (index > 0) {
		if (sdata->next == NULL)
			return -ERANGE;

		sdata = sdata->next;
		index--;
	}

	*type = sdata->type;
	*data = sdata->data;
	*flags = sdata->flags;

	return 0;
}

API void free_shared_data(struct adaptived_cause * const cse, bool force_delete)
{
	struct shared_data *cur, *next, *prev_valid = NULL, *first_valid = NULL;
	struct adaptived_name_and_value *name_value;
	bool do_free, persist;

	if (cse == NULL)
		return;

	if (cse->sdata == NULL)
		return;

	cur = cse->sdata;

	while (cur != NULL) {
		next = cur->next;

		persist = (bool)(cur->flags & ADAPTIVED_SDATAF_PERSIST);

		do_free = force_delete || !persist;

		if (!do_free) {
			if (!first_valid)
				first_valid = cur;

			if (prev_valid)
				prev_valid->next = cur;

			prev_valid = cur;
			cur = next;
			continue;
		}

		switch(cur->type) {
		case ADAPTIVED_SDATA_CUSTOM:
			(*cur->free_fn)(cur->data);
			break;
		case ADAPTIVED_SDATA_CGROUP:
			adaptived_free_cgroup_value(cur->data);
			free(cur->data);
			break;
		case ADAPTIVED_SDATA_NAME_VALUE:
			name_value = (struct adaptived_name_and_value *)cur->data;

			free(name_value->name);
			adaptived_free_cgroup_value(name_value->value);
			free(cur->data);
			break;
		default:
			free(cur->data);
			break;
		}

		free(cur);
		cur = next;
	}

	cse->sdata = first_valid;
}
