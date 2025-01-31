# Copyright (c) 2024, Oracle and/or its affiliates.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# version 2 for more details (a copy is included in the LICENSE file that
# accompanied this code).
#
# You should have received a copy of the GNU General Public License version
# 2 along with this work; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#
# Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
# or visit www.oracle.com if you need additional information or have any
# questions.
#
# adaptived tests Makefile.am
#
TESTS = ftests-wrapper.sh ftests-c-wrapper.sh

EXTRA_DIST_PYTHON_FILES = \
	adaptived.py \
	config.py \
	consts.py \
	ftests.py \
	log.py \
	run.py \
	utils.py

AM_CFLAGS = ${CFLAGS} -Wall
AM_LDFLAGS = ${LDFLAGS} ${CODE_COVERAGE_LIBS}
LDADD = -ljson-c -lpthread ${top_builddir}/src/libadaptived.la

test002_SOURCES = 002-register_cause.c

test003_SOURCES = \
	003-register_plugin_cause.c \
	time_of_day.c

test004_SOURCES = \
	004-register_plugin_effect.c \
	time_of_day.c \
	tod_validate.c

test007_SOURCES = 007-cause-avg300_pressure_above.c
test008_SOURCES = 008-cause-pressure_above_total.c
test009_SOURCES = 009-cause-pressure_below.c
test010_SOURCES = 010-snooze_effect.c
test011_SOURCES = 011-cause-pressure_rate_rising.c
test012_SOURCES = 012-cause-pressure_rate_falling.c
test013_SOURCES = 013-adaptived-asynchronous_loop.c ftests.c
test014_SOURCES = 014-duplicate_cause_effect.c
test015_SOURCES = 015-load_rule_at_runtime.c
test016_SOURCES = 016-unload_rule_at_runtime.c
test017_SOURCES = 017-register_many.c
test018_SOURCES = 018-effect-cgroup_setting_set_str.c ftests.c
test019_SOURCES = 019-effect-cgroup_setting_set_int.c ftests.c
test020_SOURCES = 020-effect-cgroup_setting_add_int.c ftests.c
test021_SOURCES = 021-effect-cgroup_setting_sub_int.c ftests.c
test022_SOURCES = 022-effect-kill_cgroup.c ftests.c
test023_SOURCES = 023-effect-kill_cgroup_recursive.c ftests.c
test024_SOURCES = 024-effect-kill_cgroup_by_psi.c ftests.c
test025_SOURCES = 025-effect-cgroup_setting_by_psi_1.c ftests.c
test026_SOURCES = 026-effect-cgroup_setting_by_psi_2.c ftests.c
test027_SOURCES = 027-effect-cgroup_setting_by_psi_3.c ftests.c
test028_SOURCES = 028-effect-copy_cgroup_setting.c ftests.c
test029_SOURCES = 029-cause-cgroup_setting_ll_gt.c ftests.c
test030_SOURCES = 030-cause-cgroup_setting_ll_lt.c ftests.c
test031_SOURCES = 031-cause-cgroup_setting_float_gt.c ftests.c
test032_SOURCES = 032-cause-cgroup_setting_float_lt.c ftests.c
test033_SOURCES = 033-validate_humanize.c ftests.c
test034_SOURCES = 034-cause-setting_ll_gt.c ftests.c
test035_SOURCES = 035-cause-setting_ll_lt.c ftests.c
test036_SOURCES = 036-cause-setting_float_gt.c ftests.c
test037_SOURCES = 037-cause-setting_float_lt.c ftests.c
test038_SOURCES = 038-cause-setting_ll_eq.c ftests.c
test039_SOURCES = 039-cause-setting_float_eq.c ftests.c
test040_SOURCES = 040-cause-avg300_pressure_equal.c ftests.c
test041_SOURCES = 041-cause-pressure_equal.c ftests.c
test042_SOURCES = 042-cause-periodic.c ftests.c
test043_SOURCES = 043-effect-logger-no-separators.c ftests.c
test044_SOURCES = 044-effect-logger-date-format.c ftests.c
test045_SOURCES = 045-effect-cgroup_memory_setting_add_int_max.c ftests.c
test046_SOURCES = 046-effect-cgroup_memory_setting_sub_int_max.c ftests.c
test047_SOURCES = 047-effect-cgroup_memory_pre_set_add_int.c ftests.c
test048_SOURCES = 048-cause-meminfo_gt.c ftests.c
test049_SOURCES = 049-cause-meminfo_lt.c ftests.c
test050_SOURCES = 050-cause-meminfo_eq.c ftests.c
test051_SOURCES = 051-cause-slabinfo_gt.c ftests.c
test052_SOURCES = 052-cause-slabinfo_lt.c ftests.c
test053_SOURCES = 053-cause-slabinfo_eq.c ftests.c
test055_SOURCES = 055-effect-setting_set_int.c ftests.c
test056_SOURCES = 056-effect-setting_add_int.c ftests.c
test057_SOURCES = 057-effect-setting_sub_int.c ftests.c
test058_SOURCES = 058-cause-memorystat_gt.c ftests.c
test059_SOURCES = 059-cause-memorystat_lt.c ftests.c
test060_SOURCES = 060-cause-memorystat_eq.c ftests.c
test061_SOURCES = 061-cause-top_cpu_gt.c ftests.c
test062_SOURCES = 062-cause-top_cpu_lt.c ftests.c
test063_SOURCES = 063-cause-top_mem_gt.c ftests.c
test064_SOURCES = 064-cause-top_mem_lt.c ftests.c
test065_SOURCES = 065-daemon_test.c ftests.c
test066_SOURCES = 066-cause-cgroup_memory_setting_ll_lt.c ftests.c
test067_SOURCES = 067-effect-kill_processes.c ftests.c
test068_SOURCES = 068-effect-kill_processes_rss.c ftests.c
test069_SOURCES = 069-effect-signal.c ftests.c
test070_SOURCES = 070-rule-multiple_rules.c ftests.c

sudo1000_SOURCES = 1000-sudo-effect-sd_bus_setting_set_int.c ftests.c
sudo1001_SOURCES = 1001-sudo-effect-sd_bus_setting_add_int.c ftests.c
sudo1002_SOURCES = 1002-sudo-effect-sd_bus_setting_sub_int.c ftests.c
sudo1003_SOURCES = 1003-sudo-effect-sd_bus_setting-CPUQuota.c ftests.c
sudo1004_SOURCES = 1004-sudo-effect-sd_bus_setting_add_int_infinity.c ftests.c
sudo1005_SOURCES = 1005-sudo-effect-sd_bus_setting_sub_infinity.c ftests.c
sudo1006_SOURCES = 1006-sudo-effect-sd_bus_setting_set_int_scope.c ftests.c
sudo1007_SOURCES = 1007-sudo-effect-sd_bus_setting_set_str.c ftests.c

check_PROGRAMS = \
	test002 \
	test003 \
	test004 \
	test007 \
	test008 \
	test009 \
	test010 \
	test011 \
	test012 \
	test013 \
	test014 \
	test015 \
	test016 \
	test017 \
	test018 \
	test019 \
	test020 \
	test021 \
	test022 \
	test023 \
	test024 \
	test025 \
	test026 \
	test027 \
	test028 \
	test029 \
	test030 \
	test031 \
	test032 \
	test033 \
	test034 \
	test035 \
	test036 \
	test037 \
	test038 \
	test039 \
	test040 \
	test041 \
	test042 \
	test043 \
	test044 \
	test045 \
	test046 \
	test047 \
	test048 \
	test049 \
	test050 \
	test051 \
	test052 \
	test053 \
	test055 \
	test056 \
	test057 \
	test058 \
	test059 \
	test060 \
	test061 \
	test062 \
	test063 \
	test064 \
	test065 \
	test066 \
	test067 \
	test068 \
	test069 \
	test070 \
	sudo1000 \
	sudo1001 \
	sudo1002 \
	sudo1003 \
	sudo1004 \
	sudo1005 \
	sudo1006 \
	sudo1007

EXTRA_DIST_PYTHON_TESTS = \
	001-cause-time_of_day.py \
	054-effect-print_schedstat.py

EXTRA_DIST_SUDO_CFGS = \
	1000-sudo-effect-sd_bus_setting_set_int.json \
	1001-sudo-effect-sd_bus_setting_add_int.json \
	1002-sudo-effect-sd_bus_setting_sub_int.json \
	1003-sudo-effect-sd_bus_setting-CPUQuota.json \
	1004-sudo-effect-sd_bus_setting_add_int_infinity.json \
	1005-sudo-effect-sd_bus_setting_sub_infinity.json \
	1006-sudo-effect-sd_bus_setting_set_int_scope.json \
	1007-sudo-effect-sd_bus_setting_set_str.json

EXTRA_DIST_CFGS = \
	001-cause-time_of_day.json.token \
	002-register_cause.json \
	003-register_plugin_cause.json \
	004-register_plugin_effect.json \
	007-cause-avg300_pressure_above.json \
	008-cause-pressure_above_total.json \
	009-cause-pressure_below.json \
	010-snooze_effect.json \
	011-cause-pressure_rate_rising.json \
	012-cause-pressure_rate_falling.json \
	013-adaptived-asynchronous_loop.json \
	019-effect-cgroup_setting_set_int.json \
	020-effect-cgroup_setting_add_int.json \
	021-effect-cgroup_setting_sub_int.json \
	022-effect-kill_cgroup.json \
	023-effect-kill_cgroup_recursive.json \
	024-effect-kill_cgroup_by_psi.json \
	025-effect-cgroup_setting_by_psi_1.json \
	026-effect-cgroup_setting_by_psi_2.json \
	027-effect-cgroup_setting_by_psi_3.json \
	028-effect-copy_cgroup_setting.json \
	029-cause-cgroup_setting_ll_gt.json \
	030-cause-cgroup_setting_ll_lt.json \
	031-cause-cgroup_setting_float_gt.json \
	032-cause-cgroup_setting_float_lt.json \
	033-validate_humanize.json \
	034-cause-setting_ll_gt.json \
	035-cause-setting_ll_lt.json \
	036-cause-setting_float_gt.json \
	037-cause-setting_float_lt.json \
	038-cause-setting_ll_eq.json \
	039-cause-setting_float_eq.json \
	040-cause-avg300_pressure_equal.json \
	041-cause-pressure_equal.json \
	042-cause-periodic.json \
	043-effect-logger-no-separators.json \
	044-effect-logger-date-format.json \
	045-effect-cgroup_memory_setting_add_int_max.json \
	045-effect-cgroup_memory_setting_set_add_int.json \
	046-effect-cgroup_memory_setting_sub_int_max.json \
	047-effect-cgroup_memory_pre_set_add_int.json \
	048-cause-meminfo_gt.json \
	049-cause-meminfo_lt.json \
	050-cause-meminfo_eq.json \
	051-cause-slabinfo_gt.json \
	052-cause-slabinfo_lt.json \
	053-cause-slabinfo_eq.json \
	054-effect-print_schedstat.expected \
	054-effect-print_schedstat.schedstat \
	054-effect-print_schedstat.json \
	055-effect-setting_set_int.json \
	056-effect-setting_add_int.json \
	057-effect-setting_sub_int.json \
	058-cause-memorystat_gt.json \
	058-cause-memorystat_gt.token \
	058-cause-memorystat_gt.expected \
	059-cause-memorystat_lt.json \
	059-cause-memorystat_lt.token \
	059-cause-memorystat_lt.expected \
	060-cause-memorystat_eq.json \
	060-cause-memorystat_eq.token \
	060-cause-memorystat_eq.expected \
	061-cause-top_cpu_gt.json \
	061-proc_stat.sample \
	062-cause-top_cpu_lt.json \
	063-cause-top_mem_gt.json \
	064-cause-top_mem_lt.json \
	066-cause-cgroup_memory_setting_ll_lt.json \
	067-effect-kill_processes.json \
	068-effect-kill_processes_rss.json \
	069-effect-signal.json \
	070-rule-multiple_rules.json

EXTRA_DIST_H_FILES = \
	ftests.h

EXTRA_DIST = \
	ftests-wrapper.sh \
	ftests-c-wrapper.sh \
	${EXTRA_DIST_PYTHON_FILES} \
	${EXTRA_DIST_PYTHON_TESTS} \
	${EXTRA_DIST_SUDO_CFGS} \
	${EXTRA_DIST_CFGS} \
	${EXTRA_DIST_H_FILES}

clean-local: clean-local-check
.PHONY: clean-local-check
clean-local-check:
	-rm -f *.pyc

check-build:
	${MAKE} ${AM_MAKEFLAGS} ${check_PROGRAMS}
