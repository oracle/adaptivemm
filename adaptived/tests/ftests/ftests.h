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
 * Common definitions for adaptived functional tests
 *
 */

#ifndef __ADAPTIVED_FTESTS_H
#define __ADAPTIVED_FTESTS_H

#include <adaptived.h>
#include <assert.h>

#define AUTOMAKE_PASSED 0
#define AUTOMAKE_HARD_ERROR 99

#define ARRAY_SIZE(x)   (sizeof(x) / sizeof((x)[0]))
#define DEFAULT_SLEEP 10

/*
 * Convert the contents of a macro to a string
 */
#define STRINGIFY(_x) _STRINGIFY(_x)
#define _STRINGIFY(_y) #_y

extern const char * const tod_name;
extern const struct adaptived_cause_functions tod_fns;

extern const char * const tod_validate_name;
extern const struct adaptived_effect_functions tod_validate_fns;

/*
 * ftests.c functions
 */


void delete_file(const char * const filename);
void delete_files(const char * const files[], int files_cnt);
void write_file(const char * const filename, const char * const contents);
int compare_files(const char * const file1, const char * const file2);
int verify_int_file(const char * const filename, int expected_value);
int verify_ll_file(const char * const filename, long long expected_value);
int verify_char_file(const char * const filename, const char * const expected_contents);
int verify_lines(const char * const line, const char * const expected_contents);
int vsprintf_wrapper(char * const buf, const char * const format, ...);

void delete_dir(const char * const dir);
void delete_dirs(const char * const dirs[], int dirs_cnt);
int create_dir(const char * const dir);
int create_dirs(const char * const dirs[], int dirs_cnt);

int create_pids(pid_t * const pids, int pids_cnt, int sleep_time);
void wait_for_pids(pid_t * const pids, int pids_cnt);
int verify_pids_were_killed(const pid_t * const pids, int pids_cnt);
int verify_pids_were_not_killed(const pid_t * const pids, int pids_cnt);
int kill_pids(const pid_t * const pids, int pids_cnt);
double time_elapsed(const struct timespec * const start, const struct timespec * const end);

int start_slice(const char *slice_name, const char *cmd_to_run);
int start_unit(const char *unit_name, const char *cmd_to_run);
int stop_transient(const char *transient_name);

#endif /* __ADAPTIVED_FTESTS_H */
