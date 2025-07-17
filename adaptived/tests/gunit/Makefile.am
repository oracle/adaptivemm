AM_CPPFLAGS = -I$(top_srcdir)/include \
	      -I$(top_srcdir)/src \
	      -I$(top_srcdir)/src/utils \
	      -I$(top_srcdir)/googletest/googletest/include \
	      -I$(top_srcdir)/googletest/googletest \
	      -std=c++11 \
	      -Wno-write-strings \
	      -Wall
LDADD = $(top_builddir)/src/.libs/libadaptived.la

EXTRA_DIST = $(top_srcdir)/googletest/googletest/libgtest.so \
	     $(top_srcdir)/googletest/googletest/libgtest_main.so \
	     $(top_srcdir)/googletest/googletest/include

check_PROGRAMS = gtest
TESTS = gtest

gtest_SOURCES = gtest.cpp \
		001-adaptived_get_pressure.cpp \
		002-adaptived_get_pressure_avg.cpp \
		003-adaptived_farray_append.cpp \
		004-adaptived_farray_linear_regression.cpp \
		005-cgroup_long_long.cpp \
		006-cgroup_str.cpp \
		007-path_walk.cpp \
		008-cgroup_get_procs.cpp \
		009-cgroup_detect.cpp \
		010-adaptived_get_schedstats.cpp \
		011-kill_processes_sort.cpp \
		012-shared_data.cpp

gtest_LDFLAGS = -L$(top_srcdir)/googletest/googletest -l:libgtest.so \
		-rpath $(abs_top_srcdir)/googletest/googletest

check-build:
	${MAKE} ${AM_MAKEFLAGS} ${check_PROGRAMS}
