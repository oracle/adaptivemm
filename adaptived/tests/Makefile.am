@CODE_COVERAGE_RULES@

DIST_SUBDIRS = ftests gunit
SUBDIRS = $(DIST_SUBDIRS)

check-build:
	${MAKE} ${AM_MAKEFLAGS} -C ftests check-build
	${MAKE} ${AM_MAKEFLAGS} -C gunit check-build
