ACLOCAL_AMFLAGS = -I m4
DIST_SUBDIRS = dist doc include src tests
SUBDIRS = ${DIST_SUBDIRS}

EXTRA_DIST = \
	CONTRIBUTING.md \
	LICENSE.txt \
	README.md \
	SECURITY.md \
	THIRD_PARTY_LICENSES.txt

help:
	@echo "adaptived build system"
	@echo " make targets:"
	@echo "  (none):           build the library"
	@echo "  check:            build the library and run tests"
	@echo "  check-build:      build the library and all tests"

check-build: all
	${MAKE} ${AM_MAKEFLAGS} -C src check-build
	${MAKE} ${AM_MAKEFLAGS} -C tests check-build
