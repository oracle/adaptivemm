/*
 *  * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 *  * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *  *
 *  * This code is free software; you can redistribute it and/or modify it
 *  * under the terms of the GNU General Public License version 2 only, as
 *  * published by the Free Software Foundation.
 *  *
 *  * This code is distributed in the hope that it will be useful, but WITHOUT
 *  * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  * version 2 for more details (a copy is included in the LICENSE file that
 *  * accompanied this code).
 *  *
 *  * You should have received a copy of the GNU General Public License version
 *  * 2 along with this work; if not, write to the Free Software Foundation,
 *  * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *  *
 *  * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 *  * or visit www.oracle.com if you need additional information or have any
 *  * questions.
 *  */
#ifndef PREDICT_H
#define	PREDICT_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	MAX_ORDER		11
#define	LSQ_LOOKBACK		8
#define MEMPREDICT_RECLAIM	0x01
#define MEMPREDICT_COMPACT	0x02
#define MEMPREDICT_LOWER_WMARKS	0x04

extern long compaction_rate, reclaim_rate;
extern int debug_mode, verbose;

struct lsq_struct {
	int next;
	int ready;
	long long y[LSQ_LOOKBACK];
	long long x[LSQ_LOOKBACK];
};

enum output_type {
	OUTPUT_OBSERVATIONS,
	OUTPUT_PREDICTIONS,
	MAX_OUTPUT
};

struct frag_info {
	long long free_pages;
	long long msecs;
};

unsigned long predict(struct frag_info *, struct lsq_struct *,
			unsigned long, int);

#define log_err(...)	log_msg(LOG_ERR, __VA_ARGS__)
#define log_warn(...)	log_msg(LOG_WARNING, __VA_ARGS__)
#define log_dbg(...)	if (debug_mode)	\
				log_msg(LOG_DEBUG, __VA_ARGS__)
#define log_info(verb_level, ...)	if (verbose >= verb_level) \
						log_msg(LOG_INFO, __VA_ARGS__)

/* Use pr_info to log info irrespective of verbosity level */
#define pr_info(...)	log_msg(LOG_INFO, __VA_ARGS__)

extern void log_msg(int level, char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* PREDICT_H */
