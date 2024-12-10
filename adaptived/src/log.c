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
 * Logging for adaptived
 *
 */

#include <stdbool.h>
#include <assert.h>
#include <stdarg.h>
#include <syslog.h>
#include <systemd/sd-journal.h>

#include "adaptived-internal.h"

int log_level = LOG_ERR;
enum log_location log_loc = LOG_LOC_STDERR;

static void _log(int priority, const char *fmt, va_list *ap)
{
	switch(log_loc) {
		case LOG_LOC_JOURNAL:
			sd_journal_printv(priority, fmt, *ap);
			break;
		case LOG_LOC_SYSLOG:
			vsyslog(priority, fmt, *ap);
			break;
		case LOG_LOC_STDOUT:
			vfprintf(stdout, fmt, *ap);
			break;
		case LOG_LOC_STDERR:
			vfprintf(stderr, fmt, *ap);
			break;
		default:
			assert(true);
			break;
	}
	va_end(*ap);
}

API void adaptived_err(const char *fmt, ...)
{
	va_list ap;

	if (log_level >= LOG_ERR) {
		va_start(ap, fmt);
		_log(LOG_ERR, fmt, &ap);
		va_end(ap);
	}
}

API void adaptived_wrn(const char *fmt, ...)
{
	va_list ap;

	if (log_level >= LOG_WARNING) {
		va_start(ap, fmt);
		_log(LOG_WARNING, fmt, &ap);
		va_end(ap);
	}
}

API void adaptived_info(const char *fmt, ...)
{
	va_list ap;

	if (log_level >= LOG_INFO) {
		va_start(ap, fmt);
		_log(LOG_INFO, fmt, &ap);
		va_end(ap);
	}
}

API void adaptived_dbg(const char *fmt, ...)
{
	va_list ap;

	if (log_level >= LOG_DEBUG) {
		va_start(ap, fmt);
		_log(LOG_DEBUG, fmt, &ap);
		va_end(ap);
	}
}
