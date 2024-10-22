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
 * Utilities for working with file paths
 *
 */

#include <sys/types.h>
#include <stdbool.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

#include <adaptived-utils.h>
#include <adaptived.h>

#include "adaptived-internal.h"

struct adaptived_path_walk_handle {
	char *path;
	DIR *dirp;
	int flags;
	bool list_top_dir;
	int max_depth;
	int cur_depth;

	struct adaptived_path_walk_handle *child;
};

API int adaptived_path_walk_start(const char * const path, struct adaptived_path_walk_handle **handle,
			       int flags, int max_depth)
{
	struct adaptived_path_walk_handle *whandle;
	int ret = 0;

	if (!path || !handle)
		return -EINVAL;

	if (flags == 0)
		flags = ADAPTIVED_PATH_WALK_DEFAULT_FLAGS;

	whandle = malloc(sizeof(struct adaptived_path_walk_handle));
	if (!whandle)
		return -ENOMEM;

	whandle->path = strdup(path);
	if (!whandle->path) {
		ret = -ENOMEM;
		goto error;
	}

	whandle->cur_depth = 0;
	if (max_depth < 0)
		whandle->max_depth = ADAPTIVED_PATH_WALK_UNLIMITED_DEPTH;
	else
		whandle->max_depth = max_depth;

	/**
	 * Attempt to match the behavior or /usr/bin/find.  Note that we're removing
	 * the trailing "/" to maintain consistency in the results.
	 *
	 * /usr/bin/find -type d /foo 		enumerates /foo in the list
	 * /usr/bin/find -type d /foo/		enumerates /foo/ in the list
	 * /usr/bin/find -type d /foo*		enumerates /foo in the list
	 *
	 * Note there shouldn't be a space between the / and *, but the compiler
	 * doesn't like embedded comments, so a space was inserted to appease the
	 * compiler.
	 * /usr/bin/find -type d /foo/ *	does not enumerate /foo
	 */
	if (flags & ADAPTIVED_PATH_WALK_LIST_DIRS) {
		whandle->list_top_dir = true;
		if (strlen(whandle->path) >= 2) {
			if (whandle->path[strlen(whandle->path) - 1] == '*' &&
			    whandle->path[strlen(whandle->path) - 2] == '/')
				whandle->list_top_dir = false;
		}
	} else {
		whandle->list_top_dir = false;
	}

	/*
	 * Remove trailing "/" and "*" characters.  dirent() doesn't need or
	 * use them
	 */
	if (whandle->path[strlen(whandle->path) - 1] == '*')
		whandle->path[strlen(whandle->path) - 1] = '\0';
	if (whandle->path[strlen(whandle->path) - 1] == '/')
		whandle->path[strlen(whandle->path) - 1] = '\0';

	whandle->dirp = opendir(whandle->path);
	if (whandle->dirp == NULL) {
		ret = -errno;
		goto error;
	}

	whandle->flags = flags;
	whandle->child = NULL;

	*handle = whandle;

	return ret;

error:
	if (whandle->path)
		free(whandle->path);
	if (whandle)
		free(whandle);

	*handle = NULL;

	return ret;
}

static int recurse(struct adaptived_path_walk_handle * const whandle, const char * const child_dir)
{
	char path[FILENAME_MAX] = { '\0' };
	int child_max_depth, ret;

	if (whandle->max_depth >= 0) {
		/* The user has requested finite recursion */
		if (whandle->cur_depth >= whandle->max_depth)
			/*
			 * We have reached the maximum recursion level
			 */
			return 0;

		child_max_depth = whandle->max_depth - 1;
	} else {
		child_max_depth = ADAPTIVED_PATH_WALK_UNLIMITED_DEPTH;
	}

	sprintf(path, "%s/%s", whandle->path, child_dir);

	ret = adaptived_path_walk_start(path, &whandle->child, whandle->flags, child_max_depth);

	/*
	 * We never need to enumerate the parent directory in the child in the recursive
	 * case because it's already been enumerated by the parent path walking
	 */
	whandle->child->list_top_dir = false;

	return ret;
}

API int adaptived_path_walk_next(struct adaptived_path_walk_handle **handle, char **path)
{
	struct adaptived_path_walk_handle *whandle;
	struct dirent *de;
	size_t path_len;
	int ret;

	if (!handle || !path)
		return -EINVAL;

	whandle = *handle;

	if (whandle->list_top_dir) {
		whandle->list_top_dir = false;
		*path = strdup(whandle->path);
		return 0;
	}

	do {
		if (whandle->child != NULL) {
			ret = adaptived_path_walk_next(&whandle->child, path);
			if (ret == 0 && (*path) == NULL)
				/*
				 * We reached the end of this child; destroy it.
				 * Processing continues in the parent handle in the
				 * do-while() loop belo
				 */
				adaptived_path_walk_end(&whandle->child);
			else {
				/*
				 * The child produced a result that needs to be handled
				 * by the user.  Return it to them
				 */
				return ret;
			}
		}

		errno = 0;
		de = readdir(whandle->dirp);
		if (!de) {
			/*
			 * We can get here two ways:
			 * 1. We've successfully walked the tree.  In this case, errno will be 0,
			 *    and path won't be set.  That will signify to the user we've reached
			 *    the end.
			 * 2. There was an error.  Return it.
			 */
			*path = NULL;
			return -errno;
		}

		if (de->d_type == DT_DIR) {
			if (strcmp(".", de->d_name) != 0 &&
			    strcmp("..", de->d_name) != 0) {
				ret = recurse(whandle, de->d_name);
				if (ret)
					return ret;
			}
		}

		if (whandle->flags & ADAPTIVED_PATH_WALK_LIST_DIRS &&
		    de->d_type == DT_DIR) {
			if (strcmp(".", de->d_name) == 0 ||
			    strcmp("..", de->d_name) == 0) {
				if (whandle->flags & ADAPTIVED_PATH_WALK_LIST_DOT_DIRS)
					/* Return the dot directory to the user */
					break;
				else
					/* Ignore the dot directory and move on */
					continue;
			}

			break;
		}

		if (whandle->flags & ADAPTIVED_PATH_WALK_LIST_FILES &&
		    de->d_type == DT_REG)
			break;
	} while(true);

	path_len = strlen(whandle->path) + strlen(de->d_name) + 2;
	*path = malloc(sizeof(char) * path_len);
	if (!path)
		return -ENOMEM;

	sprintf(*path, "%s/%s", whandle->path, de->d_name);
	(*path)[path_len - 1] = '\0';

	return 0;
}

API void adaptived_path_walk_end(struct adaptived_path_walk_handle **handle)
{
	struct adaptived_path_walk_handle *whandle;

	if (!handle || !(*handle))
		return;

	whandle = *handle;

	free(whandle->path);
	closedir(whandle->dirp);
	free(whandle);

	*handle = NULL;
}
