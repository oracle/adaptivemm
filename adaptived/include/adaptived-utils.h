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
 * Utilities for the adaptived library
 *
 */

#ifndef __ADAPTIVED_UTILS_H
#define __ADAPTIVED_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <unistd.h>

#include <adaptived.h>

/*
 * Opaque structure for walking a directory
 */
struct adaptived_path_walk_handle;

#define MAX_DOMAIN_LEVELS 8
#define MAX_NR_CPUS 2048

/**
 * Enumeration to map to CPU idle type (idle, busy and becoming idle)
 */
enum cpu_idle_type_enum {
	CPU_IDLE,
	CPU_NOT_IDLE,
	CPU_NEWLY_IDLE,

	CPU_MAX_IDLE_TYPES
};

/**
 * Enumeration to map to the various per-cpu schedstat fields
 */
enum adaptived_schedstat_cpu_enum {
	SCHEDSTAT_YLD_DEFUNCT = 1,
	SCHEDSTAT_DEFUNCT,
	SCHEDSTAT_SCHED_CALLED_DEFUNCT,
	SCHEDSTAT_GOIDLE_DEFUNCT,
	SCHEDSTAT_TTWU,
	SCHEDSTAT_TTWU_LOCAL,
	SCHEDSTAT_RUN_TIME,
	SCHEDSTAT_RUN_DELAY,
	SCHEDSTAT_NR_TIMESLICES
};

/**
 * Enumeration to map to the various per-domain schedstat fields
 */
enum adaptived_schedstat_domain_enum {
	SCHEDSTAT_LB_CALLED,
	SCHEDSTAT_LB_BALANCED,
	SCHEDSTAT_LB_FAILED,
	SCHEDSTAT_LB_IMBAL,
	SCHEDSTAT_LB_GAINED,
	SCHEDSTAT_LB_NOT_GAINED,
	SCHEDSTAT_LB_NOBUSY_RQ,
	SCHEDSTAT_LB_NOBUSY_GRP,
	SCHEDSTAT_ALB_CALLED = 24,
	SCHEDSTAT_ALB_FAILED,
	SCHEDSTAT_ALB_PUSHED,
	SCHEDSTAT_SBE_CALLED_DEFUNCT,
	SCHEDSTAT_SBE_BALANCED_DEFUNCT,
	SCHEDSTAT_SBE_PUSHED_DEFUNCT,
	SCHEDSTAT_SBF_CALLED_DEFUNCT,
	SCHEDSTAT_SBF_BALANCED_DEFUNCT,
	SCHEDSTAT_SBF_PUSHED_DEFUNCT,
	SCHEDSTAT_TTWU_REMOTE,
	SCHEDSTAT_TTWU_MOVE_AFFINE,
	SCHEDSTAT_TTWU_MOVE_BALANCE_DEFUNCT
};

/**
 * Structure to represent per-cpu-idle-type load balancing schedstat
 */
struct adaptived_schedstat_domain_lb {
	unsigned int lb_called;
	unsigned int lb_balanced;
	unsigned int lb_failed;
	unsigned int lb_imbal;
	unsigned int lb_gained;
	unsigned int lb_not_gained;
	unsigned int lb_nobusy_rq;
	unsigned int lb_nobusy_grp;
};

/**
 * Structure to represent per-domain schedstat
 */
struct adaptived_schedstat_domain {
	unsigned int cpumask;
	struct adaptived_schedstat_domain_lb lb[CPU_MAX_IDLE_TYPES];
	unsigned int alb_called;
	unsigned int alb_failed;
	unsigned int alb_pushed;

	/*
	 * # of times in this domain try_to_wake_up() awoke a task that last ran on a different
	 * cpu in this domain
	 */
	unsigned int ttwu_remote;
	/*
	 * # of times in this domain try_to_wake_up() moved a task to the waking cpu
	 */
	unsigned int ttwu_move_affine;
};

/**
 * Structure to represent per-cpu schedstat
 */
struct adaptived_schedstat_cpu {
	unsigned int ttwu;		/* # of times on this cpu try_to_wakeup() was called */
	unsigned int ttwu_local;	/* # of times on this cpu try_to_wakeup() awoke a task on the local cpu */
	unsigned long long run_time;	/* sum of all time spent running by tasks on this cpu (in nanoseconds) */
	unsigned long long run_delay;	/* sum of all time spent waiting to run by tasks on this cpu (in nanoseconds) */
	unsigned long nr_timeslices;	/* # of timeslices on this CPU */

	struct adaptived_schedstat_domain schedstat_domains[MAX_DOMAIN_LEVELS];
	int nr_domains;
};

/**
 * Structure to represent all per-cpu and per-domain schedstat
 */
struct adaptived_schedstat_snapshot {
	struct adaptived_schedstat_cpu schedstat_cpus[MAX_NR_CPUS];
	int  nr_cpus;
	unsigned long timestamp;
};

/**
 * Enumeration to map to the various PSI fields
 */
enum adaptived_pressure_meas_enum {
	PRESSURE_SOME_AVG10 = 0,
	PRESSURE_SOME_AVG60,
	PRESSURE_SOME_AVG300,
	PRESSURE_SOME_TOTAL,
	PRESSURE_FULL_AVG10,
	PRESSURE_FULL_AVG60,
	PRESSURE_FULL_AVG300,
	PRESSURE_FULL_TOTAL,

	PRESSURE_MEAS_CNT,
};

/**
 * Structure to represent the PSI data for the some or full row
 */
struct adaptived_pressure_avgs {
	float avg10;
	float avg60;
	float avg300;
	long long total;
};

/**
 * Structure to represent all of the PSI data
 */
struct adaptived_pressure_snapshot {
	struct adaptived_pressure_avgs some;
	struct adaptived_pressure_avgs full;
};

/**
 * Read scheduler stats from schedstat file
 * @param schedstat_file schedstat file to read from
 * @param ss Structure to store schedstat
 */
int adaptived_get_schedstat(const char * const schedstat_file, struct adaptived_schedstat_snapshot * const ss);

/**
 * Read the PSI data from the PSI file
 * @param pressure_file PSI file to read from
 * @param ps Structure to store the data into
 */
int adaptived_get_pressure(const char * const pressure_file,
			struct adaptived_pressure_snapshot * const ps);

/**
 * Read the specified average value from the PSI file
 * @param pressure_file PSI file to read from
 * @param meas PSI measurement (e.g. some-avg10)
 * @param avg Location to store the measurement
 */
int adaptived_get_pressure_avg(const char * const pressure_file, enum adaptived_pressure_meas_enum meas,
			    float * const avg);

/**
 * Read the specified total value from the PSI file
 * @param pressure_file PSI file to read from
 * @param meas PSI measurement (e.g. some-total)
 * @param total Location to store the measurement
 */
int adaptived_get_pressure_total(const char * const pressure_file,
			      enum adaptived_pressure_meas_enum meas, long long * const total);

/**
 * Append a float measurement to a float array
 * @param array Float array
 * @param value New value to append to the float array
 * @param array_len Size of the float array
 * @param samples_in_array Current number of populated entries in the array
 *
 * Insert a float measurement into an array of floats. If the number of samples is less
 * than the array size, the value will be appended to the end, and the samples_in_array
 * variable will be incremented.  If the number of samples in the array equals the array
 * length, then the oldest sample will be discarded and the new value will be appended to
 * the end.
 *
 * NOTE - Users are recommended to only use helper functions to operate on the array, and
 * they are discouraged from directly interacting with array members.
 */
int adaptived_farray_append(float * const array, const float * const value, int array_len,
			 int * const samples_in_array);

/**
 * Use linear regression on a float array to predict a future value
 * @param array Float array
 * @param array_len Size of the float array
 * @param interval Time interval between each measurement (milliseconds)
 * @param interp_x Time value of the point to be interpolated/extrapolated
 * @param interp_y Vertical value of the point to be interpolated/extrapolated
 *
 * Given an array of floats, compute the value of a specified point, interp_x, using
 * linear regression.  The float array is assumed to be evenly spaced by the specified
 * interval (in milliseconds).
 *
 * NOTE - A value of 0 in interp_x would request the interpolated Y from the last
 * measurement of the array. In other words, a value of 0 would mean 0 seconds in the
 * future. A value of 5 would extrapolate 5 seconds beyond the last measurement in the
 * array. A value of -10 would interpolate 10 seconds prior to the last measurement in
 * the array. Note that interp_x is in seconds and not in samples!
 */
int adaptived_farray_linear_regression(float * const array, int array_len, int interval,
				    int interp_x, float * const interp_y);

#define ADAPTIVED_CGROUP_FLAGS_VALIDATE 0x1

/**
 * Write a long long value to a cgroup setting
 * @param setting Complete path to the cgroup setting being modified
 * @param value Long long value to write into the setting file
 * @param flags Optional flags.  See ADAPTIVED_CGROUP_FLAGS_* for supported options
 * @param validate If true, validate that the value was successfully written
 */
int adaptived_cgroup_set_ll(const char * const setting, long long value, uint32_t flags);

/**
 * Read a long long value from a cgroup setting
 * @param setting Complete path to the cgroup setting
 * @param value Location to store the long long
 */
int adaptived_cgroup_get_ll(const char * const setting, long long * const value);

/**
 * Read a float value from a cgroup setting
 * @param setting Complete path to the cgroup setting
 * @param value Location to store the float
 */
int adaptived_cgroup_get_float(const char * const setting, float * const value);

/**
 * Write a string value to a cgroup setting
 * @param setting Complete path to the cgroup setting being modified
 * @param value String value to write into the setting file
 * @param flags Optional flags.  See ADAPTIVED_CGROUP_FLAGS_* for supported options
 */
int adaptived_cgroup_set_str(const char * const setting, const char * const value, uint32_t flags);

/**
 * Read a string value from a cgroup setting
 * @param setting Complete path to the cgroup setting
 * @param value Location to store the string
 *
 * Upon a successful read, adaptived_cgroup_get_ll() will allocate memory for the string. It's
 * the responsibility of the caller to free (*value).
 */
int adaptived_cgroup_get_str(const char * const setting, char ** value);

/**
 * Write a cgroup value to a cgroup setting.  Supports multiple types including string, int, and
 * float
 * @param setting Complete path to the cgroup setting being modified
 * @param value Structure containing the value
 * @param flags Optional flags.  See ADAPTIVED_CGROUP_FLAGS_* for supported options
 */
int adaptived_cgroup_set_value(const char * const setting,
			    const struct adaptived_cgroup_value * const value, uint32_t flags);

/**
 * Read the cgroup value for the specified setting.  Supports multiple types including string,
 * int, and float
 * @param setting Complete path to the cgroup setting
 * @param value Location to store the value
 */
int adaptived_cgroup_get_value(const char * const setting, struct adaptived_cgroup_value * const value);

/**
 * Read the cgroup value for the specified setting, and return true if "max".
 */
bool adaptived_cgroup_setting_is_max(const char * const setting);

/**
 * Read and return the requested memory.stat field value.
 * @param memorystat_file Path to the cgroup's memory.stat file
 * @param field The field in memory.stat file to parse
 * @param ll_valuep Output pointer for storing the value
 *
 * @Note All data is returned in bytes
 */
int adaptived_cgroup_get_memorystat_field(const char * const memorystat_file,
				       const char * const field,
				       long long * const ll_valuep);

/**
 * Read and return the /proc/meminfo field value.
 * @param meminfo_file Path to the meminfo file to be parsed (optional).  If NULL, /proc/meminfo
 * is used
 * @param field The field in the meminfo file to parse
 * @param ll_valuep Output pointer for storing the value
 *
 * @Note For fields that are in bytes, kilobytes, etc., adaptived_get_meminfo_field() will return
 * the data in bytes.
 */
int adaptived_get_meminfo_field(const char * const meminfo_file,
			     const char * const field, long long * const ll_valuep);

/**
 * Read and return the /proc/slabinfo field & column value
 * @param slabinfo_file Path to the slabinfo file to be parsed (optional). If NULL,
 * /proc/slabinfo is used.
 * @param field The field in the slabinfo file to parse
 * @param column The column in the slabinfo file to parse (i.e. <active_objs>)
 * @param ll_valuep Output pointer for storing the field & column value
 */
int adaptived_get_slabinfo_field(const char * const slabinfo_file,
			      const char * const field, const char * const column,
			      long long * const ll_valuep);

/**
 * Get a list of pids in a cgroup
 * @param cgroup_path Cgroup path
 * @param pids List of pids running in the requested cgroup
 * @param pid_count Number of pids in the list
 *
 * @Note It is the responsibility of the user to free the pids list
 */
int adaptived_cgroup_get_procs(const char * const cgroup_path, pid_t ** pids,
			    int * const pid_count);

/**
 * Utility to start walking a directory
 * @param path The path to be walked
 * @param handle Opaque pointer to an internal data structure
 * @param flags Parameters to control how the directory is walked
 * @param max_depth How deep to recurse through the tree.  A negative number
 *        indicates to walk the entire tree. Zero will list the root
 *        directory (if LIST_DIRS is enabled) and its children.  One will
 *        list the root, its children, and its grandchildren.
 *
 * @Note adaptived_path_walk_end() must be called after a successful return of this
 *	 function so that the internal data structure can be freed
 * @Note this function supports extraneous wildcards at the end of the path, e.g.
 *	 /foo/bar/\*
 * @Note this function does not support wildcards that modify the path itself, e.g.
 *	 /foo/bar* (where the user would expect this to match /foo/barf, /foo/bark, ...)
 */
#define ADAPTIVED_PATH_WALK_LIST_DIRS 0x1
#define ADAPTIVED_PATH_WALK_LIST_FILES 0x2
#define ADAPTIVED_PATH_WALK_LIST_DOT_DIRS 0x4
#define ADAPTIVED_PATH_WALK_DEFAULT_FLAGS ADAPTIVED_PATH_WALK_LIST_DIRS

#define ADAPTIVED_PATH_WALK_UNLIMITED_DEPTH -1
int adaptived_path_walk_start(const char * const path, struct adaptived_path_walk_handle **handle,
			   int flags, int max_depth);

/**
 * Walk the directory tree and return the next directory in the tree
 * @param handle Opaque pointer to an internal data structure
 * @param path The next path in the tree
 *
 * @return 0 on success, negative number on error. When the end of the
 * directory tree is reached, 0 will be returned and *path will be NULL
 *
 * @Note it is the responsibility of the caller to free path upon
 * successful return of this function
 */
int adaptived_path_walk_next(struct adaptived_path_walk_handle **handle, char **path);

/**
 * End the directory walk
 * @param handle Opaque pointer to an internal data structure
 *
 * This function will destroy the internal data structure and complete
 * the requested directory walk.
 */
void adaptived_path_walk_end(struct adaptived_path_walk_handle **handle);

/**
 * Check if the file/directory exists.
 * @param path to file/directory
 *
 * @Note if an asterisk char is in the path, it will be replaced with '\0'
 * @return 0 if file exists, else return -EEXIST
 */
int adaptived_file_exists(const char * const path);


enum cg_setting_enum {
        CG_SETTING = 0,
        CG_MEMORY_SETTING
};

/**
 * sd_bus utilites. Similar to the above adaptived_cgroup utilities except
 * all sets and gets are done via sd_bus.
 */

/**
 * Set the specified property to the specified long long value
 * @param target Systemd target (e.g. slice, scope, etc.) to operate on
 * @param property Name of the property field
 * @param value long long to assign to the property
 * @param flags Optional flags
 *
 * @return Returns 0 on success or a negative number on failure
 *
 * Note that this function invokes the sd_bus_get_property() call for the systemd1 DBus listener.
 * Currently it returns results immediately to the caller, but in the future, it may block (or
 * provide a callback) if the operation takes too much time
 */
int adaptived_sd_bus_set_ll(const char * const target, const char * const property, long long value,
			 uint32_t flags);

/**
 * Read a long long value from the specified property
 * @param target Systemd target (e.g. slice, scope, etc.) to operate on
 * @param property Name of the property field
 * @param value Location to store the long long
 *
 * @return Returns 0 on success or a negative number on failure
 *
 * Note that this function invokes the sd_bus_get_property() call for the systemd1 DBus listener.
 * Currently it returns results immediately to the caller, but in the future, it may block (or
 * provide a callback) if the operation takes too much time
 */
int adaptived_sd_bus_get_ll(const char * const target, const char * const property,
			 long long int *ll_value);

/**
 * Set the specified property to the specified string value
 * @param target Systemd target (e.g. slice, scope, etc.) to operate on
 * @param property Name of the property field
 * @param value string to assign to the property
 * @param flags Optional flags
 *
 * @return Returns 0 on success or a negative number on failure
 *
 * Note that this function invokes the sd_bus_get_property() call for the systemd1 DBus listener.
 * Currently it returns results immediately to the caller, but in the future, it may block (or
 * provide a callback) if the operation takes too much time
 */
int adaptived_sd_bus_set_str(const char * const target, const char * const property,
			  const char * const value, uint32_t flags);

/**
 * Read a string value from the specified property
 * @param target Systemd target (e.g. slice, scope, etc.) to operate on
 * @param property Name of the property field
 * @param str_value Location to store the string
 *
 * @return Returns 0 on success or a negative number on failure
 *
 * Note that this function invokes the sd_bus_get_property() call for the systemd1 DBus listener.
 * Currently it returns results immediately to the caller, but in the future, it may block (or
 * provide a callback) if the operation takes too much time
 */
int adaptived_sd_bus_get_str(const char * const target, const char * const property, char **str_value);

/**
 * Read the value for the specified property, and return true if "max".
 * @param target Systemd target (e.g. slice, scope, etc.) to operate on
 * @param property Name of the property field
 *
 * @return Returns true if property value at "max".
 *
 * Note that this function invokes the sd_bus_get_property() call for the systemd1 DBus listener.
 * Currently it returns results immediately to the caller, but in the future, it may block (or
 * provide a callback) if the operation takes too much time
 */
bool adaptived_sd_bus_setting_is_max(const char * const target, const char * const property);

/**
 * Read the value for the specified property.  Supports string and int types.
 * @param target Systemd target (e.g. slice, scope, etc.) to operate on
 * @param property Name of the property field
 * @param value Location to store the value
 *
 * @return Returns 0 on success or a negative number on failure
 */
int adaptived_sd_bus_get_value(const char * const target, const char * const property,
			    struct adaptived_cgroup_value * const value);

/**
 * Write a value to the specified property.  Supports string and int types.
 * @param target Systemd target (e.g. slice, scope, etc.) to operate on
 * @param setting Property setting being modified (e.g. MemoryLow)
 * @param value Structure containing the value
 * @param flags Optional flags.  See ADAPTIVED_CGROUP_FLAGS_* for supported options
 *
 * @return Returns 0 on success or a negative number on failure
 */
int adaptived_sd_bus_set_value(const char * const target, const char * const setting,
			    const struct adaptived_cgroup_value * const value, uint32_t flags);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __ADAPTIVED_UTILS_H */
