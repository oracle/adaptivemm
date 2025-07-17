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

#ifndef __ADAPTIVED_H
#define __ADAPTIVED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <json-c/json.h>
#include <stdbool.h>
#include <stdio.h>

/*
 * Opaque structure that contains cause information
 */
struct adaptived_cause;

/*
 * Opaque structure that contains effect information
 */
struct adaptived_effect;

/**
 * Opaque structure that contains the adaptived context
 */
struct adaptived_ctx;

/**
 * adaptived context attributes
 */
enum adaptived_attr {
	ADAPTIVED_ATTR_INTERVAL = 0, /* milliseconds */
	ADAPTIVED_ATTR_MAX_LOOPS,
	ADAPTIVED_ATTR_LOG_LEVEL,
	ADAPTIVED_ATTR_SKIP_SLEEP, /* for speeding up tests. do not use in production */
	ADAPTIVED_ATTR_RULE_CNT,
	ADAPTIVED_ATTR_DAEMON_MODE, /* run as daemon */
	ADAPTIVED_ATTR_DAEMON_NOCHDIR,
	ADAPTIVED_ATTR_DAEMON_NOCLOSE,

	ADAPTIVED_ATTR_CNT
};

enum adaptived_sdata_type {
	/*
	 * Custom data.  Can be used by registered functions without forcing a recompile
	 * of adaptived
	 */
	ADAPTIVED_SDATA_CUSTOM = 0,
	ADAPTIVED_SDATA_STR,
	ADAPTIVED_SDATA_CGROUP,
	ADAPTIVED_SDATA_NAME_VALUE,

	ADAPTIVED_SDATA_CNT
};

enum adaptived_sdata_flags {
	ADAPTIVED_SDATAF_PERSIST = 0x1
};

/**
 * Function to free a custom shared data structure.  Not needed for any other
 * shared data type, as adaptived knows how to free built-in data types.
 */
typedef void (*adaptived_sdata_free)(void * const data);

enum adaptived_cgroup_value_type {
	ADAPTIVED_CGVAL_STR = 0,
	ADAPTIVED_CGVAL_LONG_LONG,
	ADAPTIVED_CGVAL_FLOAT,

	ADAPTIVED_CGVAL_CNT,
	/*
	 * When adaptived_cgroup_get_value() is invoked with ADAPTIVED_CGVAL_DETECT provided,
	 * adaptived will try to automatically detect the contents of the cgroup setting
	 * and return the appropriate value.
	 *
	 * This is an invalid setting for adaptived_cgroup_set_value()
	 */
	ADAPTIVED_CGVAL_DETECT
};

struct adaptived_cgroup_value {
	enum adaptived_cgroup_value_type type;

	union {
		char *str_value;
		long long ll_value;
		float float_value;
	} value;
};

struct adaptived_name_and_value {
	char *name;
	struct adaptived_cgroup_value *value;
};

struct adaptived_rule_stats {
	int cause_cnt;
	int effect_cnt;

	long long loops_run_cnt;
	long long trigger_cnt;
	long long snooze_cnt;
};

/**
 * Initialization routine for a cause
 * @param cse Cause structure for this cause
 * @param args_obj JSON object representation of the args parameter for this cause
 * @param interval How often in milliseconds the adaptived main loop will run
 */
typedef int (*adaptived_cause_init)(struct adaptived_cause * const cse, struct json_object *args_obj,
				 int interval);

/**
 * Main processing logic for a cause
 * @param cse Cause structure for this cause
 * @param time_since_last_run Delta time since this cause last ran
 *
 * Note that currently the adaptived_loop() passes in ctx->interval in the time_since_last_run
 * field.  In the future it could track the actual delta time between runs
 */
typedef int (*adaptived_cause_main)(struct adaptived_cause * const cse, int time_since_last_run);

/**
 * Exit routine for a cause
 * @param cse Cause structure for this cause
 *
 * This routine should clean up any memory allocated by the cause
 */
typedef void (*adaptived_cause_exit)(struct adaptived_cause * const cse);

/**
 * Cause functions structure
 */
struct adaptived_cause_functions {
	adaptived_cause_init init;
	adaptived_cause_main main;
	adaptived_cause_exit exit;
};

/**
 * Initialization routine for an effect
 * @param eff Effect structure for this effect
 * @param args_obj JSON object representation of the args parameter for this effect
 * @param cse Cause structure this effect is acting upon
 */
typedef int (*adaptived_effect_init)(struct adaptived_effect * const eff, struct json_object *args_obj,
			   const struct adaptived_cause * const cse);
/**
 * Main processing logic for an effect
 * @param eff Effect structure for this effect
 */
typedef int (*adaptived_effect_main)(struct adaptived_effect * const eff);
/**
 * Exit routine for an effect
 * @param eff Effect structure for this effect
 */
typedef void (*adaptived_effect_exit)(struct adaptived_effect * const eff);

/**
 * Effect functions structure
 */
struct adaptived_effect_functions {
	adaptived_effect_init init;
	adaptived_effect_main main;
	adaptived_effect_exit exit;
};

/**
 * Log an error message
 * @param fmt message format
 */
void adaptived_err(const char *fmt, ...);

/**
 * Log a warning message
 * @param fmt message format
 */
void adaptived_wrn(const char *fmt, ...);

/**
 * Log an informational message
 * @param fmt message format
 */
void adaptived_info(const char *fmt, ...);

/**
 * Log a debug message
 * @param fmt message format
 */
void adaptived_dbg(const char *fmt, ...);

/**
 * Initialize the adaptived context structure
 * @param config_file Path to the configuration file to be used
 *
 * @return pointer to valid adaptived context.  NULL on failure
 *
 * If config file is NULL, adaptived will use the default config file,
 * /etc/adaptived.json
 */
struct adaptived_ctx *adaptived_init(const char * const config_file);

/**
 * Destroy the adaptived context and release any resources
 * @param ctx adaptived context struct
 *
 * (*ctx) will be NULLed if it is a valid ctx pointer
 */
void adaptived_release(struct adaptived_ctx **ctx);

/**
 * Main adaptived processing loop
 * @param ctx the adaptived configuration context
 * @param parse Run the json parser against the context's config file
 */
int adaptived_loop(struct adaptived_ctx * const ctx, bool parse);

/**
 * Set an attribute in the adaptived context
 * @param ctx adaptived options struct
 * @param attr attribute name
 * @param value attribute value
 */
int adaptived_set_attr(struct adaptived_ctx * const ctx, enum adaptived_attr attr, uint32_t value);

/**
 * Get the value of an attribute in the adaptived context
 * @param ctx adaptived options struct
 * @param attr attribute name
 * @param value attribute value
 */
int adaptived_get_attr(struct adaptived_ctx * const ctx, enum adaptived_attr attr,
		    uint32_t * const value);

/*
 * Get the statistics for the specified rule
 * @param ctx adaptived options struct
 * @param name Rule name
 * @param stats Structure to store the stats info into
 */
int adaptived_get_rule_stats(struct adaptived_ctx * const ctx,
			  const char * const name, struct adaptived_rule_stats * const stats);

/**
 * Get the private data pointer in a cause structure
 * @param cse Cause pointer
 */
void *adaptived_cause_get_data(const struct adaptived_cause * const cse);

/**
 * Set the private data pointer in a cause structure
 * @param cse Cause pointer
 * @param data Private data structure pointer
 */
int adaptived_cause_set_data(struct adaptived_cause * const cse, void * const data);

/**
 * Get the private data pointer in an effect structure
 * @param eff Effect pointer
 */
void *adaptived_effect_get_data(const struct adaptived_effect * const eff);

/**
 * Set the private data pointer in an effect structure
 * @param eff Effect pointer
 * @param data Private data structure pointer
 */
int adaptived_effect_set_data(struct adaptived_effect * const eff, void * const data);

/**
 * Register a cause with adaptived
 * @param ctx adaptived context
 * @param name cause name
 * @param fns cause function handlers
 */
int adaptived_register_cause(struct adaptived_ctx * const ctx, const char * const name,
			  const struct adaptived_cause_functions * const fns);

/**
 * Register a effect with adaptived
 * @param ctx adaptived context
 * @param name effect name
 * @param fns effect function handlers
 */
int adaptived_register_effect(struct adaptived_ctx * const ctx, const char * const name,
			  const struct adaptived_effect_functions * const fns);

/**
 * Parse a string from the JSON configuration file
 * @param obj JSON object
 * @param key JSON object key
 * @param value pointer that on success will point at the requested string
 */
int adaptived_parse_string(struct json_object * const obj, const char * const key, const char **value);

/**
 * Parse an integer from the JSON configuration file
 * @param obj JSON object
 * @param key JSON object key
 * @param value destination to store the integer value
 */
int adaptived_parse_int(struct json_object * const obj, const char * const key, int * const value);

/**
 * Parse a float from the JSON configuration file
 * @param obj JSON object
 * @param key JSON object key
 * @param value destination to store the float value
 */
int adaptived_parse_float(struct json_object * const obj, const char * const key, float * const value);

/**
 * Parse a long long from the JSON configuration file
 * @param obj JSON object
 * @param key JSON object key
 * @param value destination to store the long long value
 */
int adaptived_parse_long_long(struct json_object * const obj, const char * const key,
			   long long * const value);

/**
 * Parse a cgroup value from the JSON configuration file
 * @param obj JSON object
 * @param key JSON object key
 * @param value destination to store the cgroup value
 *
 * @Note - it's the responsibility of the caller to free this structure by
 * calling adaptived_free_cgroup_value()
 */
int adaptived_parse_cgroup_value(struct json_object * const obj, const char * const key,
			      struct adaptived_cgroup_value * const value);

/**
 * Free a adaptived_cgroup_value structure
 * @param pointer to the struct to be freed
 */
void adaptived_free_cgroup_value(struct adaptived_cgroup_value * const val);

/**
 * Parse a boolean from the JSON configuration file
 * @param obj JSON object
 * @param key JSON object key
 * @param value destination to store the bool value
 */
int adaptived_parse_bool(struct json_object * const obj, const char * const key, bool * const value);

/**
 * Build a cause object in memory
 * @param name The name of the cause that will be inserted into the rule
 *
 * @return A pointer to a newly-allocated cause struct.  This cause is typically freed
 *         when the cause is added to a rule, but it can also be freed by calling
 *         adaptived_release_cause()
 *
 * Build a cause object in memory.  This function is currently only used if rules are being
 * added at runtime
 */
struct adaptived_cause *adaptived_build_cause(const char * const name);

/**
 * Free an in-memory cause
 * @param Pointer to the cause structure
 */
void adaptived_release_cause(struct adaptived_cause ** cse);

/**
 * Add a string key-value pair to an in-memory cause
 * @param cse Cause structure
 * @param key Corresponds to a JSON key in the args JSON field
 * @param value String value associated with the key
 */
int adaptived_cause_add_string_arg(struct adaptived_cause * const cse, const char * const key,
				const char * const value);

/**
 * Add an integer key-value pair to an in-memory cause
 * @param cse Cause structure
 * @param key Corresponds to a JSON key in the args JSON field
 * @param value Integer value associated with the key
 */
int adaptived_cause_add_int_arg(struct adaptived_cause * const cse, const char * const key, int value);

/**
 * Build a effect object in memory
 * @param name The name of the effect that will be inserted into the rule
 *
 * @return A pointer to a newly-allocated effect struct.  This effect is typically freed
 *         when the effect is added to a rule, but it can also be freed by calling
 *         adaptived_release_effect()
 *
 * Build a effect object in memory.  This function is currently only used if rules are being
 * added at runtime
 */
struct adaptived_effect *adaptived_build_effect(const char * const name);

/**
 * Free an in-memory effect
 * @param Pointer to the effect structure
 */
void adaptived_release_effect(struct adaptived_effect ** eff);

/**
 * Add a string key-value pair to an in-memory effect
 * @param eff Cause structure
 * @param key Corresponds to a JSON key in the args JSON field
 * @param value String value associated with the key
 */
int adaptived_effect_add_string_arg(struct adaptived_effect * const eff, const char * const key,
				const char * const value);

/**
 * Add an integer key-value pair to an in-memory effect
 * @param eff Cause structure
 * @param key Corresponds to a JSON key in the args JSON field
 * @param value Integer value associated with the key
 */
int adaptived_effect_add_int_arg(struct adaptived_effect * const eff, const char * const key, int value);

/**
 * Build a rule object in memory
 * @param name The name of the rule
 *
 * @return A pointer to the newly-allocated rule
 *
 * BUild a rule object in memory.  This function is currently only used if rules are being
 * added at runtime
 */
struct adaptived_rule *adaptived_build_rule(const char * const name);

/**
 * Free an in-memory rule
 * @param Pointer to the rule structure to be freed
 */
void adaptived_release_rule(struct adaptived_rule ** rule);

/**
 * Add a cause to an in-memory rule
 * @param rule Pointer to the rule object
 * @param cse Pointer to the cause object
 */
int adaptived_rule_add_cause(struct adaptived_rule * const rule, const struct adaptived_cause * const cse);


/**
 * Add an effect to an in-memory rule
 * @param rule Pointer to the rule object
 * @param eff Pointer to the effect object
 */
int adaptived_rule_add_effect(struct adaptived_rule * const rule, const struct adaptived_effect * const eff);

/**
 * Given an in-memory rule, load it into the adaptived loop for processing
 * @param ctx adaptived context
 * @param rule In-memory rule to be loaded
 */
int adaptived_load_rule(struct adaptived_ctx * const ctx, struct adaptived_rule * const rule);

/**
 * Unload a rule from the adaptived processing loop
 * @param ctx adaptived context
 * @param name Rule name to be unloaded
 */
int adaptived_unload_rule(struct adaptived_ctx * const ctx, const char * const name);

/**
 * Method to write shared data to a cause.  Data can be used by downstream effect(s)
 * @param cse adaptived cause structure
 * @param type shared data enumeration describing the type of data being store
 * @param data pointer to the data to be stored
 * @param free_fn Function to free the shared data.  Only needed for custom data types
 * @param flags See enum adaptived_sdata_flags
 *
 * @note - shared data is deleted/freed at the end of each adaptive main loop unless
 * 	   the persist flag is set
 * @note - freeing of data is done by free(), so the *data pointer must be created
 * 	   via malloc.  It would be possible to avoid this by creating a new type,
 * 	   e.g. ADAPTIVED_SDATA_INT, and adding handling for it in free_shared_data().
 * @note - persist can be used to share data bidirectionally between a cause and some
 * 	   effect(s).  Use with caution, as this tightly couples these causes and
 * 	   effect(s).
 */
int adaptived_write_shared_data(struct adaptived_cause * const cse,
				enum adaptived_sdata_type type, void *data,
				adaptived_sdata_free free_fn,
				uint32_t flags);

/**
 * Update shared data
 * @param cse adaptived cause structure
 * @param index shared data object index
 * @param type shared data enumeration describing the type of data being store
 * @param data pointer to the data to be stored
 * @param flags See enum adaptived_sdata_flags
 *
 * @note - If the data pointer is changed, it's up to the caller of this function
 * 	   to ensure that the previous *data pointer is freed.
 */
int adaptived_update_shared_data(struct adaptived_cause * const cse, int index,
				 enum adaptived_sdata_type type, void *data,
				 uint32_t flags);

/**
 * Get the number of shared data objects in this cause
 * @param cse adaptived cause
 *
 * @return number of shared data objects
 */
int adaptived_get_shared_data_cnt(const struct adaptived_cause * const cse);

/**
 * Retrieve one shared data object from the cause
 * @param cse adaptived cause
 * @param index shared data object index
 * @param type Output parameter that tells what type of data is stored in this object
 * @param data Output parameter that contains the stored data
 * @param flags See enum adaptived_sdata_flags
 *
 * @note You do not need to free the data in the shared data object.  adaptived will
 * 	 automatically do that at the end of each main processing loop
 */
int adaptived_get_shared_data(const struct adaptived_cause * const cse, int index,
			      enum adaptived_sdata_type * const type, void **data,
			      uint32_t * const flags);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __ADAPTIVED_H */
