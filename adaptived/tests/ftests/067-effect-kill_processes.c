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
 * Test for the kill_processes effect
 *
 */

#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

#include <adaptived.h>

#include "ftests.h"

#define EXPECTED_RET -ETIME
#define EXPECTED_COUNT 1

#define TEST_PROCESS_SRC        "./067_process.c"
#define TEST_PROCESS		"./067_process"
#define TEST_PROCESS_ARG	"foo"

#define ALIVE_TIME 5
#define PID_COUNT 7

pid_t pid[PID_COUNT];

static const char * const setting_file = "067-effect-kill_processes.setting";
static int ctr = 0;

typedef int (*adaptived_injection_function)(struct adaptived_ctx * const ctx);
extern int adaptived_register_injection_function(struct adaptived_ctx * const ctx,
					      adaptived_injection_function fn);

static long long value[] = { 493452946, 493452945, 493452944, 493452943, 493452942, 493452944, 493452945 };
long long expected_value = 493452942;

static int inject(struct adaptived_ctx * const ctx)
{
	int fd, ret = 0;
	char buf[FILENAME_MAX];
	ssize_t w;

	if (ctr >= ARRAY_SIZE(value))
		return -E2BIG;

	fd = open(setting_file, O_TRUNC | O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
	if (fd <= 0)
		return -EINVAL;

	memset(buf, 0, sizeof(buf));

	strcpy(buf, "MemTotal:       527700340 kB\n");
	w = sprintf(&buf[strlen(buf)], "MemFree:        %lld kB\n", value[ctr]);
	if (w <= 0 || w >= FILENAME_MAX) {
		ret = -errno;
		goto err;
	}
	strcpy(&buf[strlen(buf)], "MemAvailable:   510850712 kB\n");

	w = write(fd, buf, strlen(buf));
	if (w <= 0)
		ret = -errno;

err:
	close(fd);
	ctr++;

	return ret;
}

int execute(int count)
{
	char cmdline[FILENAME_MAX];
	char bp[FILENAME_MAX];
	int i, ret, fd;
	char *argv[3];
	char *cmd = TEST_PROCESS;

	memset(pid, 0, sizeof(pid_t) * PID_COUNT);

	if ((fd = open(TEST_PROCESS_SRC, O_RDWR | O_CREAT, 0777)) < 0) {
		perror("open");
		return -EINVAL;
	}
	memset(cmdline, 0, FILENAME_MAX);
	memset(bp, 0, FILENAME_MAX);
	strcpy(bp, "#include <unistd.h>\n");
	ret = write(fd, bp, sizeof("#include <unistd.h>\n"));
	strcpy(bp, "int main(int arrc, char **argv) { while (1) sleep(1); return 0; }");
	ret = write(fd, bp, strlen(bp));
			
	sprintf(cmdline, "gcc %s -o %s >/dev/null 2>&1", TEST_PROCESS_SRC, TEST_PROCESS);
	ret = system(cmdline);
	if (ret < 0) {
		adaptived_err("compile error.\n");
		return ret;
	}

	argv[0] = cmd;
	argv[1] = TEST_PROCESS_ARG;
	argv[2] = NULL;

	for (i = 0; i < count; i++) {
		ret = fork();
		if (ret == 0) {
			execvp(cmd, argv);

			adaptived_err("execvp error, errno = %d\n", errno);
			return -EINVAL;
		}
		pid[i] = ret;
		adaptived_dbg("execute: pid[%d] = %d\n", i, pid[i]);
		if (pid[i] < 0) {
			adaptived_err("Fork error, %d\n", pid[i]);
			return pid[i];
		}
	}
	return 0;
}

int wait_pids(int count, int expected_alive_count)
{
	int alive_count = 0;
	int killed_by_effect = 0;
	int i;

	for (i = 0; i < count; i++) {
		if (pid[i] > 0) {
			int status = 0;
			int loops = 0;
			int ret = 0;

			adaptived_dbg("wait_pids: waiting for pid[%d] = %d\n", i, pid[i]);

			waitpid(pid[i], &status, WNOHANG);
			do {
				ret = waitpid(pid[i], &status, WNOHANG);
				if (ret != 0) {
					if (WIFSIGNALED(status) && (WTERMSIG(status) == SIGKILL))
						/* the process was successfully killed by the effect */
						killed_by_effect++;
					break;
				}
				sleep(1);
			} while (ret == 0 && loops++ < ALIVE_TIME);
			if (ret == 0) {
				kill(pid[i], SIGKILL);
				alive_count++;
			}
			adaptived_dbg("wait_pids: pid[%d]=%d, status=%d, ret=%d\n",
				i, pid[i], status, ret);
		} else {
			adaptived_err("wait_pids: process %d not started, %d\n", i, pid[i]);
			return -EINVAL;
		}
	}
	adaptived_dbg("wait_pids: killed_by_effect=%d, alive_count=%d\n",
			killed_by_effect, alive_count);
	if (expected_alive_count != alive_count) {
		adaptived_err("expected_alive_count (%d) != alive_count (%d)\n",
			expected_alive_count, alive_count);
		return -EINVAL;
	}
	if (killed_by_effect != (count - expected_alive_count)) {
		adaptived_err("killed_by_effect is %d, should be: %d\n",
			killed_by_effect, (count - expected_alive_count));
		return -EINVAL;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	char config_path[FILENAME_MAX];
	struct adaptived_ctx *ctx;
	int ret;

	snprintf(config_path, FILENAME_MAX - 1, "%s/067-effect-kill_processes.json", argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	ctx = adaptived_init(config_path);
	if (!ctx)
		goto err;

	ret = adaptived_register_injection_function(ctx, inject);
	if (ret)
		goto err;

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 6);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 4000);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_SKIP_SLEEP, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_DEBUG);
	if (ret)
		goto err;

	ret = execute(PID_COUNT);
	if (ret)
                goto err;

	ret = adaptived_loop(ctx, true);
	if (ret != EXPECTED_RET)
		goto err;

	ret = wait_pids(PID_COUNT, EXPECTED_COUNT);
	if (ret != 0)
		goto err;

	adaptived_release(&ctx);
	(void)remove(setting_file);
	(void)remove(TEST_PROCESS);
	(void)remove(TEST_PROCESS_SRC);
	return AUTOMAKE_PASSED;

err:
	if (ctx)
		adaptived_release(&ctx);

	(void)remove(setting_file);
	(void)remove(TEST_PROCESS);
	(void)remove(TEST_PROCESS_SRC);
	return AUTOMAKE_HARD_ERROR;
}
