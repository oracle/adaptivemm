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
#include <mntent.h>
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

#define MAX_LINES 4096
int compare_files_unsorted(const char * const file1, const char * const file2)
{
	char *f1_line = NULL, *f2_line = NULL;
	int f1_line_cnt = 0, f2_line_cnt = 0;
	bool used[MAX_LINES] = { false };
	FILE *f1 = NULL, *f2 = NULL;
	size_t f1_sz, f2_sz;
	bool found_line;
	ssize_t read;
	int ret, i;

	f1 = fopen(file1, "r");
	if (!f1) {
		ret = -errno;
		goto err;
	}

	f2 = fopen(file2, "r");
	if (!f2) {
		ret = -errno;
		goto err;
	}


	while ((read = getline(&f1_line, &f1_sz, f1)) != -1) {
		f1_line_cnt++;
		rewind(f2);
		i = 0;

		while ((read = getline(&f2_line, &f2_sz, f2)) != -1) {
			if (i < MAX_LINES && used[i] == true) {
				i++;
				continue;
			}

			if (f1_sz != f2_sz) {
				i++;
				continue;
			}

			if (strncmp(f1_line, f2_line, f1_sz) == 0) {
				found_line = true;
				if (i < MAX_LINES)
					used[i] = true;
				break;
			}

			i++;
		}

		if (!found_line) {
			ret = -ENOSTR;
			goto err;
		}
	}

	for (i = 0; i < MAX_LINES && i < f1_line_cnt; i++) {
		if (!used[i]) {
			adaptived_err("Line %d in %s was unused\n", i, file2);
			ret = -ENODATA;
			goto err;
		}
	}

	rewind(f2);
	while ((read = getline(&f2_line, &f2_sz, f2)) != -1)
		f2_line_cnt++;

	if (f1_line_cnt != f2_line_cnt) {
		ret = -ENODATA;
		goto err;
	}

	ret = 0;

err:
	if (f1_line)
		free(f1_line);
	if (f2_line)
		free(f2_line);
	if (f1)
		fclose(f1);
	if (f2)
		fclose(f2);

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
	char *cgrp_path = NULL;
	int ret;

	if (!transient_name)
		return -EINVAL;

	ret = snprintf(cmdline, FILENAME_MAX - 1, "sudo /bin/systemctl stop %s > /dev/null 2>&1", transient_name);
	if (ret < 0)
		return ret;
	ret = system(cmdline);
	adaptived_dbg("stop_transient: %s, ret=%d\n", cmdline, ret);

	ret = build_systemd_cgroup_path(transient_name, &cgrp_path);
	if (ret < 0 || cgrp_path == NULL) {
		adaptived_err("%s: Failed to build the systemd cgroup path for %s, ret: %d\n", __func__, transient_name, ret);
		return ret;
	}

	if (access(cgrp_path, F_OK) == 0) {
		adaptived_dbg("stop_transient: %s still exists...\n", cgrp_path);
		ret = snprintf(cmdline, FILENAME_MAX - 1, "sudo rmdir %s", cgrp_path);
		if (ret < 0)
			return ret;
		ret = system(cmdline);
		adaptived_dbg("stop_transient: %s, ret=%d\n", cmdline, ret);
	}

	if (cgrp_path)
		free(cgrp_path);
	return 0;
}

#define DEFAULT_CMD	"/bin/bash -c 'while true ; do sleep 1 ; done'"
#define CG_MNT_PT	"/sys/fs/cgroup"
#define PROC_MNT	"/proc/self/mounts"

/*
 * Returns 1 for cgroup v1 and 2 for cgroup v2.
 * Returns -1 if the version cannot be determined
 * Does not handle hybrid (both cgroup v1 and v2 on the same machine) cgroups
 *
 * NOTE this is not re-entrant
 */
int get_cgroup_version(int * const version)
{
	bool found_v1 = false, found_v2 = false;
	struct mntent *ent;
	FILE *proc_mnt;

	if (!version)
		return -EINVAL;

	*version = -1;

	proc_mnt = fopen(PROC_MNT, "r");
	if (!proc_mnt) {
		adaptived_err("Failed to open %s\n", PROC_MNT);
		return -EINVAL;
	}

	ent = getmntent(proc_mnt);

	while (ent) {
		if (strcmp(ent->mnt_type, "cgroup") == 0)
			found_v1 = true;

		if (strcmp(ent->mnt_type, "cgroup2") == 0)
			found_v2 = true;

		ent = getmntent(proc_mnt);
	}

	if (found_v1)
		*version = 1;

	if (found_v2 && !found_v1)
		*version = 2;

	fclose(proc_mnt);
	return 0;
}

int build_cgroup_path(const char * const controller, const char * const cgrp, char ** path)
{
	size_t len;

	if (!path || (*path) != NULL)
		return -EINVAL;

	/*
	 * The first two '1's are for the '/' characters.  The last '1' is for the NULL terminator
	 */
	len = strlen(CG_MNT_PT) + 1 + 1 + 1;
	if (controller)
		len += strlen(controller);
	if (cgrp)
		len += strlen(cgrp);

	*path = malloc(sizeof(char) * len);
	if ((*path) == NULL)
		return -ENOMEM;

	memset(*path, 0, len);

	if (controller && cgrp)
		sprintf(*path, "%s/%s/%s", CG_MNT_PT, controller, cgrp);
	else if (controller)
		sprintf(*path, "%s/%s", CG_MNT_PT, controller);
	else if (cgrp)
		sprintf(*path, "%s/%s", CG_MNT_PT, cgrp);
	else
		sprintf(*path, "%s", CG_MNT_PT);

	return 0;
}

int build_systemd_cgroup_path(const char * const cgrp, char ** path)
{
	int version, ret;

	ret = get_cgroup_version(&version);
	if (ret < 0 || version < 0)
		return -EINVAL;

	if (version == 1)
		return build_cgroup_path("systemd", cgrp, path);
	else if (version == 2)
		return build_cgroup_path(NULL, cgrp, path);

	return -EINVAL;
}

int build_systemd_memory_max_file(const char * const cgrp_path, char **file_path)
{
	int ret, version;
	size_t len;

	if (!cgrp_path || !file_path)
		return -EINVAL;

	*file_path = NULL;

	ret = get_cgroup_version(&version);
	if (ret < 0)
		return ret;

	if (version == 1) {
		len = strlen(cgrp_path) + 1 + strlen("memory.limit_in_bytes") + 1;
		*file_path = malloc(sizeof(char) * len);
		if (!(*file_path))
			return -ENOMEM;

		memset(*file_path, 0, len);

		sprintf(*file_path, "%s/memory.limit_in_bytes", cgrp_path);
	} else if (version == 2) {
		len = strlen(cgrp_path) + 1 + strlen("memory.max") + 1;
		*file_path = malloc(sizeof(char) * len);
		if (!(*file_path))
			return -ENOMEM;

		memset(*file_path, 0, len);

		sprintf(*file_path, "%s/memory.max", cgrp_path);
	}

	return 0;
}

int start_slice(const char *slice_name, const char *cmd_to_run)
{
	char cmdline[FILENAME_MAX];
	char cmdbuf[FILENAME_MAX];
	char *cgrp_path = NULL;
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

	ret = build_systemd_cgroup_path(slice_name, &cgrp_path);
	if (ret < 0 || cgrp_path == NULL) {
		adaptived_err("Failed to build the systemd cgroup path: %d\n", ret);
		return ret;
	}

	i = 0;
	while (i++ < max_retries) {
		if (access(cgrp_path, F_OK) == 0)
			break;
		fprintf(stderr, ".");
		sleep(1);
	}

	if (access(cgrp_path, F_OK) != 0) {
		adaptived_err("Can't create cgroup: %s\n", cgrp_path);
		ret = -ENODEV;
		goto err;
	}

err:
	if (cgrp_path)
		free(cgrp_path);

	return ret;
}

int start_unit(const char *unit_name, const char *cmd_to_run)
{
	char cmdline[FILENAME_MAX];
	char cmdbuf[FILENAME_MAX];
	char *cgrp_path, *tmp_path;
	int i = 0, len;
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

	len = strlen(unit_name) + 1 + strlen("system.slice") + 1;
	tmp_path = malloc(sizeof(char) * len);
	if (!tmp_path)
		return -ENOMEM;

	sprintf(tmp_path, "system.slice/%s", unit_name);

	ret = build_systemd_cgroup_path(tmp_path, &cgrp_path);
	free(tmp_path);
	if (ret < 0 || cgrp_path == NULL) {
		adaptived_err("Failed to build the systemd cgroup path: %d\n", ret);
		return ret;
	}

	while (i++ < max_retries) {
		if (access(cgrp_path, F_OK) == 0)
			break;
		sleep(1);
	}

	if (access(cgrp_path, F_OK) != 0) {
		adaptived_err("Can't create cgroup: %s\n", cgrp_path);
		ret = -ENODEV;
		goto err;
	}

err:
	if (cgrp_path)
		free(cgrp_path);

	return ret;
}
