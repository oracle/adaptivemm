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
 * adaptived header file for shared data
 */

#ifndef __ADAPTIVED_SHARED_DATA_H
#define __ADAPTIVED_SHARED_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <adaptived.h>
#include <stdbool.h>

struct shared_data {
	enum adaptived_sdata_type type;
	void *data;
	adaptived_sdata_free free_fn;

	uint32_t flags;
	struct shared_data *next;
};

/*
 * If force_delete is true, then the shared data will be deleted (regardless
 * of the value of the persist structure member.
 */
void free_shared_data(struct adaptived_cause * const cse, bool force_delete);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __ADAPTIVED_SHARED_DATA_H */
