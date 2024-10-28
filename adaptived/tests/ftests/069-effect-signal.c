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
 * Test for the signal effect
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

#define TEST_PROCESS_RET 7
static const char * const test_process_src = "./069_process.c";
static const char * const test_process = "./069_process";
static const char * const setting_file = "069-effect-signal.setting";

static const char * const program =
	"#include <stdbool.h>\n"
	"#include <signal.h>\n"
	"#include <unistd.h>\n"
	"\n"
	"static bool received = false;\n"
	"\n"
	"static void handler(int sig)\n"
	"{\n"
	"	received = true;\n"
	"}\n"
	"\n"
	"int main(void)\n"
	"{\n"
	"	if (signal(SIGINT, handler))\n"
	"		return -1;\n"
	"\n"
	"	while(!received)\n"
	"		sleep(1);\n"
	"\n"
	"	return "
	STRINGIFY(TEST_PROCESS_RET)
	";\n"
	"}";

int execute(pid_t * const child_pid)
{
	char cmdline[FILENAME_MAX];
	char *argv[2] = { NULL };
	int i, ret = 0, fd;

	memset(cmdline, 0, FILENAME_MAX);

	fd = open(test_process_src, O_RDWR | O_CREAT, 0777);
	if (fd < 0)
		return -EINVAL;

	ret = write(fd, program, strlen(program));
			
	sprintf(cmdline, "gcc %s -o %s >/dev/null 2>&1", test_process_src, test_process);
	ret = system(cmdline);
	if (ret < 0) {
		adaptived_err("Compilation of %s failed: ret = %d, errno = %d\n",
			   test_process_src, ret, errno);
		return ret;
	}

	argv[0] = strdup(test_process);
	argv[1] = NULL;

	ret = fork();
	if (ret < 0) {
		ret = -ECHILD;
		goto err;
	} else if (ret == 0) {
		execvp(test_process, argv);

		/* we should never get here */
		adaptived_err("execvp error, errno = %d\n", errno);
		ret = -EINVAL;
		goto err;
	} else {
		*child_pid = ret;
	}

	ret = 0;

err:
	if (argv[0])
		free(argv[0]);
	return ret;
}

int main(int argc, char *argv[])
{
	char config_path[FILENAME_MAX];
	char cmdline[FILENAME_MAX];
	struct adaptived_ctx *ctx;
	int ret, i, status;
	pid_t child_pid;

	snprintf(config_path, FILENAME_MAX - 1, "%s/069-effect-signal.json", argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	ctx = adaptived_init(config_path);
	if (!ctx)
		goto err;

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 7000);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_SKIP_SLEEP, 1);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_DEBUG);
	if (ret)
		goto err;

	ret = execute(&child_pid);
	if (ret)
                goto err;

	ret = adaptived_loop(ctx, true);
	if (ret != -ETIME)
		goto err;

	/*
	 * Wait for the child process to exit
	 */
	sleep(2);

	waitpid(child_pid, &status, WNOHANG);
	adaptived_dbg("child pid = %d status = %d\n", child_pid, status);
	if (WIFEXITED(status)) {
		if( WEXITSTATUS(status) != TEST_PROCESS_RET) {
			adaptived_err("%s returned %d, expected %d\n", test_process,
				   WEXITSTATUS(status), TEST_PROCESS_RET);
			ret = -ECHILD;
			goto err;
		}
	} else {
		adaptived_err("child is still running :(\n");
		kill(child_pid, SIGKILL);
		ret = -ECHILD;
		goto err;
	}

	adaptived_release(&ctx);
	(void)remove(setting_file);
	(void)remove(test_process);
	(void)remove(test_process_src);
	return AUTOMAKE_PASSED;

err:
	if (ctx)
		adaptived_release(&ctx);

	(void)remove(setting_file);
	(void)remove(test_process);
	(void)remove(test_process_src);
	return AUTOMAKE_HARD_ERROR;
}
