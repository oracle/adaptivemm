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
 * Kill processes effect
 *
 * Given a list of process names, this effect will find the process(es) using the most
 * memory and kill them.  This effect could be extended to perform other actions and/or
 * work on other triggers (beyond vsize).
 *
 */

#include <assert.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#include "adaptived-internal.h"
#include "defines.h"

enum field {
	FLD_VSIZE = 0,
	FLD_RSS,

	FLD_CNT,
	FLD_DEFAULT = FLD_RSS
};

struct kill_processes_opts {
	int proc_name_cnt;
	char **proc_names;

	long long count; /* optional */
	int signal; /* optional */
	enum field fld; /* optional */
};

struct pid_info {
	pid_t pid;
	long long value; /* currently supports vsize or rss */
};

static void free_opts(struct kill_processes_opts * const opts)
{
	int i;

	for (i = 0; i < opts->proc_name_cnt; i++)
		free(opts->proc_names[i]);
	free(opts->proc_names);
	free(opts);
}

static int _kill_processes_init(struct adaptived_effect * const eff, struct json_object *args_obj,
				const struct adaptived_cause * const cse, int default_signal)
{
	struct json_object *proc_names_obj, *proc_name_obj;
	const char *proc_name_str, *field_str;
	struct kill_processes_opts *opts;
	json_bool exists;
	int i, ret = 0;

	opts = malloc(sizeof(struct kill_processes_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}

	memset(opts, 0, sizeof(struct kill_processes_opts));

	exists = json_object_object_get_ex(args_obj, "proc_names", &proc_names_obj);
	if (!exists || !proc_names_obj) {
		ret = -EINVAL;
		goto error;
	}

	opts->proc_name_cnt = json_object_array_length(proc_names_obj);

	opts->proc_names = malloc(sizeof(char *) * opts->proc_name_cnt);
	if (!opts->proc_names) {
		ret = -ENOMEM;
		goto error;
	}

	memset(opts->proc_names, '0', sizeof(char *) * opts->proc_name_cnt);

	for (i = 0; i < opts->proc_name_cnt; i++) {
		proc_name_obj = json_object_array_get_idx(proc_names_obj, i);
		if (!proc_name_obj) {
			ret = -EINVAL;
			goto error;
		}

		ret = adaptived_parse_string(proc_name_obj, "name", &proc_name_str);
		if (ret) {
			goto error;
		}

		opts->proc_names[i] = malloc(sizeof(char) * strlen(proc_name_str) + 1);
		if (!opts->proc_names[i]) {
			ret = -ENOMEM;
			goto error;
		}

		strcpy(opts->proc_names[i], proc_name_str);
		opts->proc_names[i][strlen(proc_name_str)] = '\0';
	}

	ret = adaptived_parse_long_long(args_obj, "count", &opts->count);
	if (ret == -ENOENT) {
		opts->count = -1;
		ret = 0;
	} else if (ret) {
		goto error;
	}

	ret = adaptived_parse_int(args_obj, "signal", &opts->signal);
	if (ret == -ENOENT) {
		opts->signal = default_signal;
		ret = 0;
	} else if (ret) {
		goto error;
	}

	ret = adaptived_parse_string(args_obj, "field", &field_str);
	if (ret == -ENOENT) {
		opts->fld = FLD_DEFAULT;
		ret = 0;
	} else if (ret) {
		goto error;
	} else {
		if (strcmp(field_str, "vsize") == 0) {
			opts->fld = FLD_VSIZE;
		} else if (strcmp(field_str, "rss") == 0) {
			opts->fld = FLD_RSS;
		} else {
			adaptived_err("Invalid field: %s\n", field_str);
			ret = -EINVAL;
			goto error;
		}
	}

	eff->data = (void *)opts;

	return ret;

error:
	free_opts(opts);

	return ret;
}

static int get_cmd(char * const buf, char **cmd)
{
	char *left, *right;
	size_t cmd_len;

	left = strchr(buf, '(');
	right = strchr(buf, ')');
	cmd_len = right - left - 1;

	*cmd = strndup(left + 1, cmd_len);
	if ((*cmd) == NULL)
		return -ENOMEM;

	return 0;
}

static int get_vsize(char * const buf, long long * const vsize)
{
	char *copy, *end_paren, *token, *saveptr = NULL;
	int i;

	copy = strdup(buf);
	if (!copy)
		return -ENOMEM;

	/*
	 * It's possible to have spaces in the program's name.  Let's start the
	 * tokenization after the closing parenthesis character in the second field
	 * in the /proc/{pid}/stat output
	 */
	end_paren = strchr(buf, ')');
	token = strtok_r(end_paren + 1, " ", &saveptr);

	/*
	 * vsize is the 23rd entry in the stat field per the man page.  (note that numbering
	 * in the man page starts at field 1 rather than 0 in C arrays.)  we've already
	 * skipped 2 fields, so we need to jump past 19 more fields
	 *
	 * https://man7.org/linux/man-pages/man5/proc_pid_stat.5.html
	 */
	for (i = 0; i <= 19; i++)
		token = strtok_r(NULL, " ", &saveptr);

	sscanf(token, "%lld", vsize);

	free(copy);

	return 0;
}

static int get_rss(char * const buf, long long * const rss)
{
	char *copy, *end_paren, *token, *saveptr = NULL;
	int i;

	copy = strdup(buf);
	if (!copy)
		return -ENOMEM;

	/*
	 * It's possible to have spaces in the program's name.  Let's start the
	 * tokenization after the closing parenthesis character in the second field
	 * in the /proc/{pid}/stat output
	 */
	end_paren = strchr(buf, ')');
	token = strtok_r(end_paren + 1, " ", &saveptr);

	/*
	 * rss is the 24th entry in the stat field per the man page.  (note that numbering
	 * in the man page starts at field 1 rather than 0 in C arrays.)  we've already
	 * skipped 2 fields, so we need to jump past 19 more fields
	 *
	 * https://man7.org/linux/man-pages/man5/proc_pid_stat.5.html
	 */
	for (i = 0; i <= 20; i++)
		token = strtok_r(NULL, " ", &saveptr);

	sscanf(token, "%lld", rss);

	free(copy);

	return 0;
}

/*
 * Arbitrarily pick an initial size of 64 entries for the pid list.  If/When the list grows to
 * this size, then another 64 entries will be added.
 */
#define GROW_SIZE 64
static int insert(struct pid_info **list, int * const list_len, pid_t new_entry, long long value)
{
	if ((*list) == NULL) {
		*list = malloc(sizeof(struct pid_info) * GROW_SIZE);
		*list_len = 0;
	} else if (((*list_len) % GROW_SIZE) == 0) {
		*list = realloc(*list, sizeof(struct pid_info) * ((*list_len) + GROW_SIZE));
	}
	if ((*list) == NULL)
		return -ENOMEM;

	(*list)[*list_len].pid = new_entry;
	(*list)[*list_len].value = value;
	(*list_len)++;

	return 0;
}

static int find_processes(struct kill_processes_opts * const opts,
			  struct pid_info ** match_list, int * const match_cnt)
{
	char path[FILENAME_MAX], buf[FILENAME_MAX];
	struct dirent *pid_dir = NULL;
	int ret = 0, pid, i;
	char *cmd = NULL;
	long long value;
	FILE *fp = NULL;
	DIR *dir;

	dir = opendir("/proc/");
	if (!dir)
		return -EINVAL;

	while ((pid_dir = readdir(dir)) != NULL) {
		ret = sscanf(pid_dir->d_name, "%i", &pid);
		if (ret < 1) {
			/*
			 * This directory isn't an integer, and thus isn't a PID directory.
			 * Move on
			 */
			ret = 0;
			continue;
		}

		ret = snprintf(path, FILENAME_MAX, "/proc/%d/stat", pid);
		if (ret <= 0) {
			adaptived_err("snprintf pid %d, error %d, errno = %d\n", pid, ret, errno);
			goto error;
		}
		if (ret >= FILENAME_MAX) {
			adaptived_wrn("snprintf returned %d for pid %d\n", ret, pid);
			goto error;
		}

		fp = fopen(path, "r");
		if (!fp) {
			/*
			 * This process may have completed between reading the directory and
			 * trying to open the stat file.  Move on
			 */
			ret = 0;
			continue;
		}

		memset(buf, 0, sizeof(buf));
		ret = fread(buf, sizeof(char), FILENAME_MAX, fp);
		if (ret < 0) {
			adaptived_err("fread pid %d, error %d, errno = %d\n", pid, ret, errno);
			goto error;
		}
		if (ret >= FILENAME_MAX) {
			adaptived_wrn("fread returned %d for pid %d\n", ret, pid);
			goto error;
		}

		ret = get_cmd(buf, &cmd);
		if (ret)
			goto error;

		if (opts->count > 0) {
			switch (opts->fld) {
			case FLD_VSIZE:
				ret = get_vsize(buf, &value);
				if (ret)
					goto error;
				break;
			case FLD_RSS:
				ret = get_rss(buf, &value);
				if (ret)
					goto error;
				break;
			default:
				adaptived_err("Invalid field: %d\n", opts->fld);
				goto error;
			}
		} else {
			/*
			 * The user has specified that we will send a signal to every process
			 * we find.  Thus, there's no need to populate the value field with a
			 * meaningful value as we will signal all found processes.
			 */
			value = 1;
		}

		for (i = 0; i < opts->proc_name_cnt; i++) {
			if (strcmp(cmd, opts->proc_names[i]) == 0) {
				ret = insert(match_list, match_cnt, pid, value);
				if (ret)
					goto error;
			}
		}

		free(cmd);
		cmd = NULL;

		fclose(fp);
		fp = NULL;
		ret = 0;
	}

error:
	if (cmd)
		free(cmd);
	if (fp)
		fclose(fp);

	closedir(dir);

	return ret;
}

/*
 * Sort the list in descending order by memory used
 */
API int _sort_pid_list(const void *p1, const void *p2)
{
	const struct pid_info *pp1 = p1;
	const struct pid_info *pp2 = p2;

	if (pp1->value > pp2->value)
		return -1;
	else
		return 1;
}

static int _kill_processes_main(struct adaptived_effect * const eff)
{
	struct kill_processes_opts *opts = (struct kill_processes_opts *)eff->data;
	int ret, pid_cnt = 0, kill_cnt = 0;
	struct pid_info *pid_list = NULL;

	int i;

	ret = find_processes(opts, &pid_list, &pid_cnt);
	if (ret)
		goto error;

	qsort(pid_list, pid_cnt, sizeof(struct pid_info), _sort_pid_list);

	if (opts->count > 0)
		kill_cnt = min(opts->count, pid_cnt);
	else
		kill_cnt = pid_cnt;

	for (i = 0; i < kill_cnt; i++) {
		adaptived_wrn("%s: Sending signal %d to PID %d\n", __func__, opts->signal,
			   pid_list[i].pid);
		(void)kill(pid_list[i].pid, opts->signal);
	}

error:
	if (pid_list)
		free(pid_list);

	return ret;
}

static void _kill_processes_exit(struct adaptived_effect * const eff)
{
	struct kill_processes_opts *opts = (struct kill_processes_opts *)eff->data;

	free_opts(opts);
}

int kill_processes_init(struct adaptived_effect * const eff, struct json_object *args_obj,
			const struct adaptived_cause * const cse)
{
	return _kill_processes_init(eff, args_obj, cse, SIGKILL);
}

int kill_processes_main(struct adaptived_effect * const eff)
{
	return _kill_processes_main(eff);
}

void kill_processes_exit(struct adaptived_effect * const eff)
{
	_kill_processes_exit(eff);
}

int signal_init(struct adaptived_effect * const eff, struct json_object *args_obj,
		const struct adaptived_cause * const cse)
{
	struct kill_processes_opts *opts;
	int ret;

	ret = _kill_processes_init(eff, args_obj, cse, SIGUSR1);
	if (ret)
		return ret;

	opts = (struct kill_processes_opts *)eff->data;

	if (opts->count != -1) {
		adaptived_err("The count arg is currently not supported by the signal effect\n");
		ret = -EINVAL;
		free_opts(opts);
	}

	return ret;
}

int signal_main(struct adaptived_effect * const eff)
{
	return _kill_processes_main(eff);
}

void signal_exit(struct adaptived_effect * const eff)
{
	_kill_processes_exit(eff);
}
