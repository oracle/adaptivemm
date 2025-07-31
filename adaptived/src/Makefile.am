@CODE_COVERAGE_RULES@

DIST_SUBDIRS =
SUBDIRS = ${DIST_SUBDIRS}

SOURCES = \
	adaptived-internal.h \
	causes/always.c \
	causes/cgroup_data.c \
	causes/cgroup_setting.c \
	causes/days_of_the_week.c \
	causes/meminfo.c \
	causes/memorystat.c \
	causes/periodic.c \
	causes/pressure.c \
	causes/pressure_rate.c \
	causes/slabinfo.c \
	causes/time_of_day.c \
	causes/top.c \
	cause.c \
	cause.h \
	defines.h \
	effects/cgroup_setting.c \
	effects/cgroup_setting_by_psi.c \
	effects/copy_cgroup_setting.c \
	effects/kill_cgroup.c \
	effects/kill_cgroup_by_psi.c \
	effects/kill_processes.c \
	effects/logger.c \
	effects/print.c \
	effects/print_schedstat.c \
	effects/sd_bus_setting.c \
	effects/snooze.c \
	effects/validate.c \
	effect.c \
	effect.h \
	log.c \
	main.c \
	parse.c \
	pressure.h \
	rule.c \
	shared_data.c \
	shared_data.h \
	utils/cgroup_utils.c \
	utils/sd_bus_utils.c \
	utils/file_utils.c \
	utils/float_utils.c \
	utils/mem_utils.c \
	utils/path_utils.c \
	utils/pressure_utils.c \
	utils/sched_utils.c

adaptived_SOURCES = ${SOURCES}
adaptived_CFLAGS = ${AM_CFLAGS} ${CFLAGS}  ${CODE_COVERAGE_CFLAGS} -Wall
adaptived_LDFLAGS = ${AM_LDFLAGS} ${LDFLAGS} ${CODE_COVERAGE_LIBS} -ljson-c -lpthread -lsystemd
sbin_PROGRAMS = adaptived

libadaptived_la_SOURCES = ${SOURCES}
libadaptived_la_LIBADD = ${CODE_COVERAGE_LIBS} -ljson-c -lpthread -lsystemd
libadaptived_la_CFLAGS = ${AM_CFLAGS} ${CFLAGS} ${CODE_COVERAGE_CFLAGS} -fPIC \
		      -fvisibility=hidden -Wall
libadaptived_la_LDFLAGS = ${AM_LDFLAGS} ${CODE_COVERAGE_LDFLAGS} ${LDFLAGS} \
		       -version-number ${VERSION_MAJOR}:${VERSION_MINOR}:${VERSION_MICRO}
lib_LTLIBRARIES = libadaptived.la

check-build:
	${MAKE} ${AM_MAKEFLAGS} ${check_PROGRAMS}
