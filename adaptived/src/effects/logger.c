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
 * Log file(s) into another log file.
 *
 */

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "adaptived-internal.h"
#include "defines.h"

struct files {
	char *filename;
	struct files *next;
};

struct logger_opts {
	char *logfile;
	int max_file_size;
	char *separator_prefix;		/* i.e. "<<<<<" */
	char *separator_postfix;	/* i.e. ">>>>>" */
	char *date_format;		/* i.e. "+%Y-%m-%d-%H:%M:%S" */
	char *file_separator;
	bool utc;			/* default: false */
	struct files *file_list;
};

#define MAX_COPY	(32 * 1024)

static void free_list(struct files *fp)
{
	struct files *tmp;

	while (fp) {
		if (fp->filename)
			free(fp->filename);
		tmp = fp;
		fp = fp->next;
		free(tmp);
	}
}

static void free_opts(struct logger_opts *opts)
{
	if (!opts)
		return;

	if (opts->separator_prefix)
		free(opts->separator_prefix);
	if (opts->separator_postfix)
		free(opts->separator_postfix);
	if (opts->date_format)
		free(opts->date_format);
	if (opts->logfile)
		free(opts->logfile);

	free_list(opts->file_list);

	free(opts);
}

int logger_init(struct adaptived_effect * const eff, struct json_object *args_obj,
		const struct adaptived_cause * const cse)
{
	struct json_object *files_obj, *file_obj;
	json_bool exists;
	struct files *filep = NULL, *fp = NULL;
	struct logger_opts *opts;
	const char *logfile_str, *file_str, *separator_prefix_str, *separator_postfix_str;
	const char *file_separator_str, *date_format_str;
	int i;
	int file_cnt;
	int ret = 0;

	opts = malloc(sizeof(struct logger_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}
	memset(opts, 0, sizeof(struct logger_opts));

	ret = adaptived_parse_string(args_obj, "logfile", &logfile_str);
	if (ret)
		goto error;

	opts->logfile = malloc(sizeof(char) * strlen(logfile_str) + 1);
	if (!opts->logfile) {
		ret = -ENOMEM;
		goto error;
	}

	strcpy(opts->logfile, logfile_str);
	opts->logfile[strlen(logfile_str)] = '\0';

	adaptived_dbg("%s: logfile=%s\n", __func__, opts->logfile);

	ret = adaptived_parse_string(args_obj, "separator_prefix", &separator_prefix_str);
	if (ret == 0) {
		opts->separator_prefix = malloc(sizeof(char) * strlen(separator_prefix_str) + 1);
		if (!opts->separator_prefix) {
			ret = -ENOMEM;
			goto error;
		}

		strcpy(opts->separator_prefix, separator_prefix_str);
		opts->separator_prefix[strlen(separator_prefix_str)] = '\0';

		adaptived_dbg("%s: separator_prefix=%s\n", __func__, opts->separator_prefix);
	} else {
		adaptived_dbg("%s: No separator_prefix\n", __func__);
	}

	ret = adaptived_parse_string(args_obj, "separator_postfix", &separator_postfix_str);
	if (ret == 0) {
		opts->separator_postfix = malloc(sizeof(char) * strlen(separator_postfix_str) + 1);
		if (!opts->separator_postfix) {
			ret = -ENOMEM;
			goto error;
		}

		strcpy(opts->separator_postfix, separator_postfix_str);
		opts->separator_postfix[strlen(separator_postfix_str)] = '\0';

		adaptived_dbg("%s: separator_postfix=%s\n", __func__, opts->separator_postfix);
	} else {
		adaptived_dbg("%s: No separator_postfix\n", __func__);
	}

	ret = adaptived_parse_string(args_obj, "date_format", &date_format_str);
	if (ret == 0) {
		opts->date_format = malloc(sizeof(char) * strlen(date_format_str) + 1);
		if (!opts->date_format) {
			ret = -ENOMEM;
			goto error;
		}

		strcpy(opts->date_format, date_format_str);
		opts->date_format[strlen(date_format_str)] = '\0';

		adaptived_dbg("%s: date_format=%s\n", __func__, opts->date_format);
	} else {
		adaptived_dbg("%s: No date_format\n", __func__);
	}

	ret = adaptived_parse_string(args_obj, "file_separator", &file_separator_str);
	if (ret == 0) {
		opts->file_separator = malloc(sizeof(char) * strlen(file_separator_str) + 1);
		if (!opts->file_separator) {
			ret = -ENOMEM;
			goto error;
		}

		strcpy(opts->file_separator, file_separator_str);
		opts->file_separator[strlen(file_separator_str)] = '\0';

		adaptived_dbg("%s: file_separator=%s\n", __func__, opts->file_separator);
	} else {
		adaptived_dbg("%s: No file_separator\n", __func__);
	}

	ret = adaptived_parse_bool(args_obj, "utc", &opts->utc);
	if (ret == -ENOENT) {
		opts->utc = false;
		ret = 0;
	} else if (ret) {
		adaptived_err("%s: utc arg: %d\n", __func__, ret);
		goto error;
	}
	adaptived_dbg("%s: utc = %d\n", __func__, opts->utc);


	ret = adaptived_parse_int(args_obj, "max_file_size", &opts->max_file_size);
	if (ret == -ENOENT) {
		opts->max_file_size = MAX_COPY;
	} else if (ret) {
		goto error;
	}

	exists = json_object_object_get_ex(args_obj, "files", &files_obj);
	if (!exists || !files_obj) {
		adaptived_err("logger_init: can't find 'files'\n");
		ret = -EINVAL;
		goto error;
	}

	file_cnt = json_object_array_length(files_obj);
	adaptived_dbg("logger_init: file_cnt=%d\n", file_cnt);

	for (i = 0; i < file_cnt; i++) {
		file_obj = json_object_array_get_idx(files_obj, i);
		if (!file_obj) {
			adaptived_err("logger_init: file %d can't find 'file_obj'\n", i);
			ret = -EINVAL;
			goto error;
		}

		ret = adaptived_parse_string(file_obj, "file", &file_str);
		if (ret) {
			adaptived_err("logger_init: file %d can't find 'file'\n", i);
			goto error;
		}
		adaptived_dbg("logger_init: file %d: file_str=%s\n", i, file_str);

		fp = malloc(sizeof(struct files));
		if (!fp) {
			ret = -ENOMEM;
			goto error;
		}
		fp->filename = malloc(sizeof(char) * strlen(file_str) + 1);
		if (!fp->filename) {
			free(fp);
			ret = -ENOMEM;
			goto error;
		}
		strcpy(fp->filename, file_str);
		fp->filename[strlen(file_str)] = '\0';

		fp->next = NULL;
		if (i == 0) {
			opts->file_list = fp;
			filep = fp;
		} else {
			filep->next = fp;
			filep = fp;
		}
	}

	/* we have successfully setup the logger effect */
	eff->data = (void *)opts;

	return ret;

error:
	free_opts(opts);

	return ret;
}

int logger_main(struct adaptived_effect * const eff)
{
	struct logger_opts *opts = (struct logger_opts *)eff->data;
	struct files *filep = NULL;
	char separator[FILENAME_MAX];
	char dateline[FILENAME_MAX];
	FILE *log = NULL;
	FILE *fnp = NULL;
	size_t read, write;
	int ret = 0;
	time_t now = time(NULL);
	char *buf = NULL;

	log = fopen(opts->logfile, "a");
	if (log == NULL) {
		adaptived_err("logger_main: can't open log file %s\n", opts->logfile);
		return -errno;
	}
	memset(dateline, 0, FILENAME_MAX);
	memset(separator, 0, FILENAME_MAX);

	*separator = '\n';

	if (opts->separator_prefix) {
		ret = snprintf(separator, FILENAME_MAX - 1, "\n%s", opts->separator_prefix);
		if (ret < 0)
			goto error;
	}
	if (opts->date_format) {
		if (opts->utc)
			strftime(dateline, FILENAME_MAX, opts->date_format, gmtime(&now));
		else
			strftime(dateline, FILENAME_MAX, opts->date_format, localtime(&now));
		strcpy(&separator[strlen(separator)], dateline);
		}
		if (opts->separator_postfix) {
			strcpy(&separator[strlen(separator)], opts->separator_postfix);
			if (ret < 0)
				goto error;
		}
		adaptived_dbg("%s: separator = %s\n", __func__, separator);

		write = fwrite(separator, 1, strlen(separator), log);
		if (write != strlen(separator)) {
			adaptived_err("logger_main: amount written (%d) != strlen(separator) (%d)\n",
				write, strlen(separator));
			ret = -EINVAL;
			goto error;
		}
		filep = opts->file_list;
		do {
			struct stat statbuf;
			long size;

			fnp = fopen(filep->filename, "r");
			if (fnp == NULL)
			{
				adaptived_err("logger_main: can't open file %s for logging.\n",
					filep->filename);
				ret = -errno;
				goto error;
			}
			if (stat(filep->filename, &statbuf) == -1) {
				adaptived_err("logger_main: stat() filed for %s.\n", filep->filename);
				ret = -errno;
				goto error;
			}
			size = statbuf.st_size;
			if (size == 0 || size >= opts->max_file_size)
				size = opts->max_file_size;
			buf = malloc(size + 1);
			if (!buf) {
				ret = -ENOMEM;
				goto error;
			}
			memset(buf, 0, size + 1);
			memset(separator, 0, FILENAME_MAX);

			if (opts->file_separator) {
				ret = snprintf(separator, FILENAME_MAX - 1,
					"\n%s\n%s\n", opts->file_separator, filep->filename);
			} else {
				ret = snprintf(separator, FILENAME_MAX - 1,
					"\n%s\n", filep->filename);
			}
			if (ret < 0)
				goto error;

			read = fread(&buf[strlen(buf)], 1,
				     min(opts->max_file_size, size), fnp);
			fclose(fnp);
			fnp = NULL;
			if (read <= 0) {
				adaptived_err("logger_main: amount read from %s (%ld) != size (%ld)\n",
					filep->filename, read, size);
				ret = -EINVAL;
				goto error;
			}
			write = fwrite(separator, 1, strlen(separator), log);
			if (write != strlen(separator)) {
				adaptived_err("logger_main: amount written (%d) != "
				    "strlen(separator) (%d)\n", write, strlen(separator));
				ret = -EINVAL;
				goto error;
			}
			write = fwrite(buf, 1, strlen(buf), log);
			if (write != strlen(buf)) {
				adaptived_err("logger_main: amount written (%d) != "
				    "strlen(buf) (%d) of %s\n",
					write, strlen(buf), filep->filename);
				ret = -EINVAL;
				goto error;
			}
			filep = filep->next;
			free(buf);
			buf = NULL;
		} while (filep);

		fclose(log);

		return 0;

	error:
		if (buf)
			free(buf);
		if (fnp)
			fclose(fnp);
		if (log)
			fclose(log);

		return ret;
	}

void logger_exit(struct adaptived_effect * const eff)
{
	struct logger_opts *opts = (struct logger_opts *)eff->data;

	free_opts(opts);
}
