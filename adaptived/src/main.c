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
 * Main loop for adaptived
 *
 */
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include <adaptived.h>

#include "adaptived-internal.h"
#include "shared_data.h"
#include "defines.h"

void cleanup(struct adaptived_ctx *ctx);

static const char * const log_files[] = {
	"syslog",
	"stdout",
	"stderr",
	"journalctl",
};
static_assert(ARRAY_SIZE(log_files) == LOG_LOC_CNT,
	      "log_files[] must be the same length as LOG_LOC_CNT");

static const char * const default_config_file = "/etc/adaptived.json";
static const int default_interval = 5000; /* milliseconds */

static void usage(FILE *fd)
{
	fprintf(fd, "\nadaptived: a daemon for managing and prioritizing resources\n\n");
	fprintf(fd, "Usage: adaptived [options]\n\n");
	fprintf(fd, "Optional arguments:\n");
	fprintf(fd, "  -c --config=CONFIG        Configuration file (default: %s)\n",
		default_config_file);
	fprintf(fd, "  -h --help                 Show this help message\n");
	fprintf(fd, "  -i --interval=INTERVAL    Polling interval in milliseconds (default: %d)\n",
		default_interval);
	fprintf(fd, "  -L --loglocation=LOCATION Location to write adaptived logs\n");
	fprintf(fd, "  -l --loglevel=LEVEL       Log level. See <syslog.h>\n");
	fprintf(fd, "  -m --maxloops=COUNT       Maximum number of loops to run."
						 "Useful for testing\n");
	fprintf(fd, "  -d --daemon_mode          Run as a daemon\n");
}

static int _adaptived_init(struct adaptived_ctx * const ctx)
{
	int ret;

	if (!ctx)
		return -EINVAL;

	memset(ctx, 0, sizeof(struct adaptived_ctx));

	ctx->interval = default_interval;
	ctx->max_loops = 0;
	ctx->rules = NULL;
	ctx->inject_fn = NULL;
	ctx->skip_sleep = false;
	ctx->daemon_mode = false;
	ctx->daemon_nochdir = 1;
	ctx->daemon_noclose = 1;

	ret = pthread_mutex_init(&ctx->ctx_mutex, NULL);
	if (ret) {
		adaptived_err("mutex init failed: %d\n", ret);
		return ret;
	}

	causes_init();
	effects_init();

	return 0;
}

API struct adaptived_ctx *adaptived_init(const char * const config_file)
{
	struct adaptived_ctx *ctx = NULL;
	int ret;

	ctx = malloc(sizeof(struct adaptived_ctx));
	if (!ctx)
		return NULL;

	ret = _adaptived_init(ctx);
	if (ret)
		goto err;

	if (config_file)
		strncpy(ctx->config, config_file, FILENAME_MAX - 1);
	else
		strncpy(ctx->config, default_config_file, FILENAME_MAX - 1);

	return ctx;
err:
	if (ctx)
		free(ctx);

	return NULL;
}

API void adaptived_release(struct adaptived_ctx **ctx)
{
	if (ctx == NULL)
		return;

	cleanup(*ctx);
	free(*ctx);

	(*ctx) = NULL;
}

API int adaptived_set_attr(struct adaptived_ctx * const ctx, enum adaptived_attr attr, uint32_t value)
{
	int ret = 0;

	pthread_mutex_lock(&ctx->ctx_mutex);

	switch (attr) {
	case ADAPTIVED_ATTR_INTERVAL:
		ctx->interval = (int)value;
		break;
	case ADAPTIVED_ATTR_MAX_LOOPS:
		ctx->max_loops = (int)value;
		break;
	case ADAPTIVED_ATTR_LOG_LEVEL:
		if ((int)value > LOG_DEBUG) {
			ret = -EINVAL;
			break;
		}
		log_level = (int)value;
		break;
	case ADAPTIVED_ATTR_SKIP_SLEEP:
		if ((int)value > 0)
			ctx->skip_sleep = true;
		else
			ctx->skip_sleep = false;
		break;
	case ADAPTIVED_ATTR_DAEMON_MODE:
		if ((int)value > 0)
			ctx->daemon_mode = true;
		else
			ctx->daemon_mode = false;
		break;
	case ADAPTIVED_ATTR_DAEMON_NOCHDIR:
		if ((int)value == 0)
			ctx->daemon_nochdir = 0;
		else
			ctx->daemon_nochdir = 1;
		break;
	case ADAPTIVED_ATTR_DAEMON_NOCLOSE:
		if ((int)value == 0)
			ctx->daemon_noclose = 0;
		else
			ctx->daemon_noclose = 1;
		break;
	case ADAPTIVED_ATTR_RULE_CNT:
	default:
		ret = -EINVAL;
		break;
	}

	pthread_mutex_unlock(&ctx->ctx_mutex);

	return ret;
}

API int adaptived_get_attr(struct adaptived_ctx * const ctx, enum adaptived_attr attr,
		    uint32_t * const value)
{
	struct adaptived_rule *rule;
	int ret = 0;
	int i = 0;

	if (!value)
		return -EINVAL;

	pthread_mutex_lock(&ctx->ctx_mutex);

	switch (attr) {
	case ADAPTIVED_ATTR_INTERVAL:
		*value = (uint32_t)ctx->interval;
		break;
	case ADAPTIVED_ATTR_MAX_LOOPS:
		*value = (uint32_t)ctx->max_loops;
		break;
	case ADAPTIVED_ATTR_LOG_LEVEL:
		*value = (uint32_t)log_level;
		break;
	case ADAPTIVED_ATTR_SKIP_SLEEP:
		if (ctx->skip_sleep)
			*value = (uint32_t)1;
		else
			*value = (uint32_t)0;
		break;
	case ADAPTIVED_ATTR_DAEMON_MODE:
		*value = ctx->daemon_mode;
		break;
	case ADAPTIVED_ATTR_DAEMON_NOCHDIR:
		*value = ctx->daemon_nochdir;
		break;
	case ADAPTIVED_ATTR_DAEMON_NOCLOSE:
		*value = ctx->daemon_noclose;
		break;
	case ADAPTIVED_ATTR_RULE_CNT:
		i = 0;
		rule = ctx->rules;
		while (rule) {
			i++;
			rule = rule->next;
		}

		*value = (uint32_t)i;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	pthread_mutex_unlock(&ctx->ctx_mutex);

	return ret;
}

API int adaptived_get_rule_stats(struct adaptived_ctx * const ctx,
			      const char * const name, struct adaptived_rule_stats * const stats)
{
	struct adaptived_rule *tmp_rule;
	bool found_rule = false;

	if (!name || !stats)
		return -EINVAL;

	pthread_mutex_lock(&ctx->ctx_mutex);

	tmp_rule = ctx->rules;
	while (tmp_rule) {
		if (strcmp(name, tmp_rule->name) == 0) {
			found_rule = true;
			break;
		}

		tmp_rule = tmp_rule->next;
	}

	if (!found_rule) {
		pthread_mutex_unlock(&ctx->ctx_mutex);
		return -EEXIST;
	}

	memcpy(stats, &tmp_rule->stats, sizeof(struct adaptived_rule_stats));

	pthread_mutex_unlock(&ctx->ctx_mutex);

	return 0;
}

int parse_opts(int argc, char *argv[], struct adaptived_ctx * const ctx)
{
	struct option long_options[] = {
		{"help",		no_argument, NULL, 'h'},
		{"config",	  required_argument, NULL, 'c'},
		{"interval",	  required_argument, NULL, 'i'},
		{"loglocation",	  required_argument, NULL, 'L'},
		{"loglevel",	  required_argument, NULL, 'l'},
		{"maxloops",	  required_argument, NULL, 'm'},
		{"daemon_mode",		no_argument, NULL, 'd'},
		{NULL, 0, NULL, 0}
	};
	const char *short_options = "c:hi:L:l:m:d";

	int ret = 0, i;
	int tmp_level;
	bool found;

	ret = _adaptived_init(ctx);
	if (ret) {
		usage(stderr);
		return ret;
	}

	pthread_mutex_lock(&ctx->ctx_mutex);

	while (1) {
		int c;

		c = getopt_long(argc, argv, short_options, long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'c':
			strncpy(ctx->config, optarg, FILENAME_MAX - 1);
			ctx->config[FILENAME_MAX - 1] = '\0';
			break;
		case 'h':
			usage(stdout);
			exit(0);
		case 'i':
			ctx->interval = atoi(optarg);
			if (ctx->interval < 1) {
				adaptived_err("Invalid interval: %s\n", optarg);
				ret = 1;
				goto err;
			}
			break;
		case 'l':
			tmp_level = atoi(optarg);
			if (tmp_level < 1) {
				adaptived_err("Invalid log level: %s.  See <syslog.h>\n", optarg);
				ret = 1;
				goto err;
			}

			log_level = tmp_level;
			break;
		case 'L':
			found = false;
			for (i = 0; i < LOG_LOC_CNT; i++) {
				if (strncmp(log_files[i], optarg,
					    strlen(log_files[i])) == 0) {
					found = true;
					log_loc = i;
					break;
				}
			}

			if (!found) {
				adaptived_err("Invalid log location: %s\n", optarg);
				ret = 1;
				goto err;
			}
			break;
		case 'm':
			ctx->max_loops = atoi(optarg);
			if (ctx->max_loops < 1) {
				adaptived_err("Invalid maxloops: %s\n", optarg);
				ret = 1;
				goto err;
			}
			break;
		case 'd':
			ctx->daemon_mode = true;
			break;

		default:
			ret = 1;
			goto err;
		}
	}

err:
	pthread_mutex_unlock(&ctx->ctx_mutex);

	if (ret)
		usage(stderr);

	return ret;
}

void cleanup(struct adaptived_ctx *ctx)
{
	struct adaptived_rule *rule, *rule_next;

	pthread_mutex_lock(&ctx->ctx_mutex);

	rule = ctx->rules;

	while (rule) {
		adaptived_dbg("Cleaning up rule %s\n", rule->name);
		rule_next = rule->next;
		rule_destroy(&rule);
		rule = rule_next;
	}

	/*
	 * Now that the rules have been cleaned up, we can clean up the
	 * registered causes and effects.
	 */
	causes_cleanup();
	effects_cleanup();

	pthread_mutex_unlock(&ctx->ctx_mutex);
	pthread_mutex_destroy(&ctx->ctx_mutex);
}

API int adaptived_register_injection_function(struct adaptived_ctx * const ctx,
					    adaptived_injection_function fn)
{
	if (!ctx)
		return -EINVAL;

	pthread_mutex_lock(&ctx->ctx_mutex);
	ctx->inject_fn = fn;
	pthread_mutex_unlock(&ctx->ctx_mutex);

	return 0;
}

static void free_rule_shared_data(struct adaptived_rule * const rule, bool force_delete)
{
	struct adaptived_cause *cse;

	cse = rule->causes;

	while(cse) {
		free_shared_data(cse, force_delete);
		cse = cse->next;
	}
}

API int adaptived_loop(struct adaptived_ctx * const ctx, bool parse)
{
	struct adaptived_effect *eff;
	struct adaptived_rule *rule;
	struct adaptived_cause *cse;
	bool triggered = true;
	int interval, ret = 0;
	struct timespec sleep;
	bool skip_sleep;

	if (parse) {
		ret = parse_config(ctx);
		if (ret)
			return ret;
	}

	pthread_mutex_lock(&ctx->ctx_mutex);
	rule = ctx->rules;
	while(rule) {
		adaptived_dbg("Rule \"%s\" loaded\n", rule->name);
		rule = rule->next;
	}

        if (ctx->daemon_mode) {
		adaptived_dbg("adaptived_loop: Try to run as daemon, nochdir = %d, noclose = %d\n",
			ctx->daemon_nochdir, ctx->daemon_noclose);
		ret = daemon(ctx->daemon_nochdir, ctx->daemon_noclose);
		if (ret) {
			adaptived_err("Failed to become daemon: %d.\n", errno);
			pthread_mutex_unlock(&ctx->ctx_mutex);
			return -errno;
		}
		adaptived_dbg("adaptived_loop: running as daemon.\n");
	} else {
		adaptived_dbg("adaptived_loop: Debug mode. Skip running as daemon.\n");
	}

	ctx->loop_cnt = 0;
	pthread_mutex_unlock(&ctx->ctx_mutex);

	while (1) {
		pthread_mutex_lock(&ctx->ctx_mutex);
		rule = ctx->rules;

		while (rule) {
			/*
			 * Intentionally undocumented API that allows a user to modify
			 * values/settings during each loop.  This feature is targeted at
			 * automated testing where it's difficult to force certain
			 * behaviors, e.g. PSI thresholds
			 */
			if (ctx->inject_fn) {
				ret = (*ctx->inject_fn)(ctx);
				if (ret)
					goto out;
			}

			adaptived_dbg("Running rule %s\n", rule->name);
			rule->stats.loops_run_cnt++;
			cse = rule->causes;

			triggered = true;
			while (cse) {
				ret = (*cse->fns->main)(cse, ctx->interval);
				if (ret < 0) {
					adaptived_dbg("%s raised error %d\n", cse->name, ret);
					goto out;
				} else if (ret == 0) {
					adaptived_dbg("%s did not trigger\n", cse->name);
					triggered = false;
				} else if (ret > 0) {
					adaptived_dbg("%s triggered\n", cse->name);
				}

				cse = cse->next;
			}

			if (triggered) {
				rule->stats.trigger_cnt++;

				/*
				 * The cause(s) for this rule were all triggered, invoke the
				 * effect(s)
				 */
				eff = rule->effects;

				while (eff) {
					adaptived_dbg("Running effect %s\n", eff->name);
					ret = (*eff->fns->main)(eff);
					if (ret == -EALREADY) {
						/*
						 * This effect has requested to skip the
						 * remaining effects in this rule
						 */
						adaptived_dbg("Skipping effects in rule: %s\n",
							   rule->name);
						rule->stats.snooze_cnt++;
						break;
					} else if (ret) {
						adaptived_dbg("Effect %s returned %d\n", eff->name,
							   ret);
						goto out;
					}

					eff = eff->next;
				}
			}

			free_rule_shared_data(rule, false);
			rule = rule->next;
		}

		ctx->loop_cnt++;
		if (ctx->max_loops > 0 && ctx->loop_cnt >= ctx->max_loops) {
			adaptived_dbg("adaptived main loop exceeded max loops\n");
			ret = -ETIME;
			break;
		}

		/*
		 * Once we've unlocked the ctx mutex, we can't safely access any variables
		 * within ctx.  Make a stack copy before unlocking
		 */
		interval = ctx->interval;
		skip_sleep = ctx->skip_sleep;

		pthread_mutex_unlock(&ctx->ctx_mutex);

		if (!skip_sleep) {
			sleep.tv_sec = interval / 1000;
			sleep.tv_nsec = (interval % 1000) * 1000000LL;
			adaptived_dbg("sleeping for %ld seconds and %ld nanoseconds\n",
				      sleep.tv_sec, sleep.tv_nsec);

			ret = nanosleep(&sleep, NULL);
			if (ret)
				adaptived_wrn("nanosleep returned %d\n", ret);
		}
	}

out:
	rule = ctx->rules;
	while (rule) {
		free_rule_shared_data(rule, true);
		rule = rule->next;
	}

	pthread_mutex_unlock(&ctx->ctx_mutex);

	return ret;
}

int main(int argc, char *argv[])
{
	struct adaptived_ctx ctx;
	int ret;

	ret = parse_opts(argc, argv, &ctx);
	if (ret)
		goto out;

	ret = adaptived_loop(&ctx, true);
	if (ret)
		goto out;

out:
	cleanup(&ctx);

	return -ret;
}
