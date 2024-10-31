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
 * Utilities for the adaptived functional tests
 *
 */

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <adaptived.h>

#include "ftests.h"

#define AUTOMAKE_PASSED 0
#define AUTOMAKE_HARD_ERROR 99

void delete_file(const char * const filename)
{
	remove(filename);
}

void delete_files(const char * const files[], int files_cnt)
{
	int i;

	for (i = 0; i < files_cnt; i++)
		delete_file(files[i]);
}

void write_file(const char * const filename, const char * const contents)
{
	FILE *f;

	f = fopen(filename, "w");
        if (!f) {
                adaptived_err("write_file: cannot open %s\n", filename);
                return;
        }

	if (strlen(contents))
		fprintf(f, "%s", contents);
	else
		fprintf(f, "\n");

	fclose(f);
}

int compare_files(const char * const file1, const char * const file2)
{
	char f1_buf[4096], f2_buf[4096];
	ssize_t f1_sz, f2_sz;
	int fd1 = 0, fd2 = 0;
	int ret;

	fd1 = open(file1, O_RDONLY);
	if (fd1 < 0) {
		ret = -errno;
		goto err;
	}

	fd2 = open(file2, O_RDONLY);
	if (fd2 < 0) {
		ret = -errno;
		goto err;
	}

	do {
		memset(f1_buf, 0, sizeof(f1_buf));
		memset(f2_buf, 0, sizeof(f2_buf));

		f1_sz = read(fd1, f1_buf, sizeof(f1_buf));
		f2_sz = read(fd2, f2_buf, sizeof(f2_buf));

		if (f1_sz != f2_sz) {
			ret = -ENODATA;
			break;
		}

		if (f1_sz == 0) {
			/*
			 * We've successfully reached the end of both files
			 */
			ret = 0;
			break;
		}

		if (strcmp(f1_buf, f2_buf) != 0) {
			fprintf(stderr, "f1:\n\t%s\n", f1_buf);
			fprintf(stderr, "f2:\n\t%s\n", f2_buf);
			ret = -ENOSTR;
			break;
		}
	} while (true);

err:
	if (fd1)
		close(fd1);
	if (fd2)
		close(fd2);

	return ret;
}

int verify_int_file(const char * const filename, int expected_value)
{
	char buf[1024] = { '\0' };
	int value, ret = 0;
	size_t bytes_read;
	FILE *f;

	f = fopen(filename, "r");
        if (!f) {
                adaptived_err("verify_int_file: cannot open %s\n", filename);
                return -EINVAL;
        }

	bytes_read = fread(buf, 1, sizeof(buf), f);
	if (bytes_read <= 0) {
		adaptived_dbg("bytes_read = %d\n", bytes_read);
		ret = -EIO;
		goto out;
	}

	value = atoi(buf);

	if (value != expected_value) {
		adaptived_dbg("%s: expected %d but got %d\n", filename, expected_value, value);
		ret = -EIO;
		goto out;
	}

out:
	fclose(f);
	return ret;
}

int verify_ll_file(const char * const filename, long long expected_value)
{
	char buf[1024] = { '\0' };
	long long value;
	int ret = 0;
	size_t bytes_read;
	FILE *f;

	f = fopen(filename, "r");
        if (!f) {
                adaptived_err("verify_ll_file: cannot open %s\n", filename);
                return -EINVAL;
        }

	bytes_read = fread(buf, 1, sizeof(buf), f);
	if (bytes_read <= 0) {
		adaptived_dbg("bytes_read = %d\n", bytes_read);
		ret = -EIO;
		goto out;
	}

	value = strtoll(buf, 0, 0);

	if (value != expected_value) {
		adaptived_dbg("%s: expected %lld but got %lld\n", filename, expected_value, value);
		ret = -EIO;
		goto out;
	}

out:
	fclose(f);
	return ret;
}

int verify_char_file(const char * const filename, const char * const expected_contents)
{
	char buf[1024] = { '\0' };
	size_t bytes_read;
	int ret = 0;
	FILE *f;

	f = fopen(filename, "r");
        if (!f) {
                adaptived_err("verify_char_file: cannot open %s\n", filename);
                return -EINVAL;
        }

	bytes_read = fread(buf, 1, sizeof(buf), f);
	if (bytes_read <= 0) {
		adaptived_dbg("bytes_read = %d\n", bytes_read);
		ret = -EIO;
	}

	if (strlen(buf) != strlen(expected_contents)) {
		adaptived_dbg("strlens don't match\n\t%s\n\t%s\n", buf, expected_contents);
		ret = -EIO;
	}
	if (strncmp(expected_contents, buf, strlen(expected_contents)) != 0) {
		adaptived_dbg("strings don't match\n\t%s\n\t%s\n", buf, expected_contents);
		ret = -EIO;
	}

	fclose(f);
	return ret;
}

int verify_lines(const char * const line, const char * const expected_contents)
{
	if (strlen(line) != strlen(expected_contents)) {
		adaptived_dbg("strlens don't match\n\t%s\n\t%s\n", line, expected_contents);
		return -EIO;
	}
	if (strncmp(expected_contents, line, strlen(expected_contents)) != 0) {
		adaptived_dbg("strings don't match\n\t%s\n\t%s\n", line, expected_contents);
		return -EIO;
	}
	return 0;
}

int vsprintf_wrapper(char * const buf, const char * const format, ...)
{
	va_list args;
	int ret;

	va_start(args, format);
	ret = vsprintf(buf, format, args);
	va_end(args);

	if (ret > 0)
		ret = 0;
	else
		ret = -errno;

	return ret;
}

void delete_dir(const char * const dir)
{
	rmdir(dir);
}

void delete_dirs(const char * const dirs[], int dirs_cnt)
{
	int i;

	for (i = dirs_cnt - 1; i >= 0; i--)
		delete_dir(dirs[i]);
}

int create_dir(const char * const dir)
{
	return mkdir(dir, S_IRWXU | S_IRWXG | S_IRWXO);
}

int create_dirs(const char * const dirs[], int dirs_cnt)
{
	int ret, i;

	for (i = 0; i < dirs_cnt; i++) {
		ret = create_dir(dirs[i]);
		if (ret)
			return ret;
	}

	return 0;
}

int create_pids(pid_t * const pids, int pids_cnt, int sleep_time)
{
	pid_t pid;
	int i;

	if (sleep_time <= 0)
		sleep_time = DEFAULT_SLEEP;

	for (i = 0; i < pids_cnt; i++) {
		pid = fork();
		if (pid < 0) {
			return -errno;
		} else if (pid == 0) {
			/* child */
			sleep(sleep_time);
			exit(0);
		} else {
			/* parent */
			pids[i] = pid;
		}
	}

	return 0;
}

void wait_for_pids(pid_t * const pids, int pids_cnt)
{
	int wstatus, i;

	for (i = 0; i < pids_cnt; i++)
		(void)waitpid(pids[i], &wstatus, 0);
}

int verify_pids_were_killed(const pid_t * const pids, int pids_cnt)
{
	int overall_ret = 0, ret = 0, i;

	for (i = 0; i < pids_cnt; i++) {
		ret = kill(pids[i], 0);
		if (ret == 0) {
			adaptived_err("pid %d is still alive\n", pids[i]);
			overall_ret = -EEXIST;
		} else if (ret < 0) {
			if (errno != ESRCH) {
				adaptived_err("pid %d resulted in errno %d\n", pids[i], errno);
				overall_ret = -EINVAL;
			}
		}
	}

	return overall_ret;
}

int verify_pids_were_not_killed(const pid_t * const pids, int pids_cnt)
{
	int overall_ret = 0, ret = 0, i;

	for (i = 0; i < pids_cnt; i++) {
		errno = 0;
		ret = kill(pids[i], 0);
		if (ret >= 0) {
			overall_ret = -EINVAL;
		} else if (ret < 0) {
			if (errno == ESRCH)
				continue;

			adaptived_err("pid %d resulted in errno %d\n", pids[i], errno);
			overall_ret = -EINVAL;
		}
	}

	return overall_ret;
}

int kill_pids(const pid_t * const pids, int pids_cnt)
{
	int overall_ret = 0, ret = 0, i;

	for (i = 0; i < pids_cnt; i++) {
		adaptived_dbg("ftests killing PID %d\n", pids[i]);
		ret = kill(pids[i], SIGKILL);
		if (ret)
			overall_ret = ret;
	}

	return overall_ret;
}

double time_elapsed(const struct timespec * const start, const struct timespec * const end)
{
	double diff;

	diff = (end->tv_nsec - start->tv_nsec) / 1000000000.0;
	diff += (end->tv_sec - start->tv_sec);

	return diff;
}

int stop_transient(const char *transient_name)
{
	char cmdline[FILENAME_MAX];
	int ret;

	if (!transient_name)
		return -EINVAL;

	ret = snprintf(cmdline, FILENAME_MAX - 1, "sudo /bin/systemctl stop %s > /dev/null 2>&1", transient_name);
	if (ret < 0)
		return ret;
	ret = system(cmdline);
	adaptived_dbg("stop_transient: %s, ret=%d\n", cmdline, ret);
	return 0;
}

#define DEFAULT_CMD	"/bin/bash -c 'while true ; do sleep 1 ; done'"
#define CG_MNT_PT	"/sys/fs/cgroup"

int start_slice(const char *slice_name, const char *cmd_to_run)
{
	char buf[FILENAME_MAX];
	char cmdline[FILENAME_MAX];
	char cmdbuf[FILENAME_MAX];
	int i;
	int max_retries = 10;
	int ret;

	if (!slice_name)
		return -EINVAL;
	ret = stop_transient(slice_name);
	if (ret)
		return ret;
	strcpy(cmdbuf, (cmd_to_run) ? cmd_to_run : DEFAULT_CMD);
	memset(cmdline, 0, FILENAME_MAX);
	ret = snprintf(cmdline, FILENAME_MAX - 1, "sudo /bin/systemd-run --slice=%s %s &", slice_name, cmdbuf);
	if (ret < 0)
		return ret;
	adaptived_dbg("start_slice: %s\n", cmdline);
	ret = system(cmdline);
	if (ret) {
		adaptived_err("Command failed: %s\n", cmdline);
		return ret;
	}
	ret = snprintf(buf, FILENAME_MAX - 1, "%s/%s", CG_MNT_PT, slice_name);
	if (ret < 0)
		return ret;
	i = 0;
	while (i++ < max_retries) {
		if (access(buf, F_OK) == 0)
			break;
		fprintf(stderr, ".");
		sleep(1);
	}
	if (access(buf, F_OK) != 0) {
		adaptived_err("Can't create cgroup: %s\n", buf);
		return -ENODEV;
	}
	return 0;
}

int start_unit(const char *unit_name, const char *cmd_to_run)
{
	char buf[FILENAME_MAX];
	char cmdline[FILENAME_MAX];
	char cmdbuf[FILENAME_MAX];
	int i;
	int max_retries = 10;
	int ret;

	if (!unit_name)
		return -EINVAL;
	ret = stop_transient(unit_name);
	if (ret)
		return ret;
	strcpy(cmdbuf, (cmd_to_run) ? cmd_to_run : DEFAULT_CMD);
	ret = snprintf(cmdline, FILENAME_MAX - 1, "sudo /bin/systemd-run --scope --unit=%s %s &", unit_name, cmdbuf);
	if (ret < 0)
		return ret;
	adaptived_dbg("start_unit: %s\n", cmdline);
	ret = system(cmdline);
	if (ret) {
		adaptived_err("Command failed: %s\n", cmdline);
		return ret;
	}
	ret= snprintf(buf, FILENAME_MAX - 1, "%s/system.slice/%s", CG_MNT_PT, unit_name);
	if (ret < 0)
		return ret;
	i = 0;
	while (i++ < max_retries) {
		if (access(buf, F_OK) == 0)
			break;
		sleep(1);
	}
	if (access(buf, F_OK) != 0) {
		adaptived_err("Can't create cgroup: %s\n", buf);
		return -ENODEV;
	}
	return 0;
}
