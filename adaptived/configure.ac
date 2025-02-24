dnl # Copyright (c) 2024-2025, Oracle and/or its affiliates.
dnl # DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
dnl #
dnl # This code is free software; you can redistribute it and/or modify it
dnl # under the terms of the GNU General Public License version 2 only, as
dnl # published by the Free Software Foundation.
dnl #
dnl # This code is distributed in the hope that it will be useful, but WITHOUT
dnl # ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
dnl # FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
dnl # version 2 for more details (a copy is included in the LICENSE file that
dnl # accompanied this code).
dnl #
dnl # You should have received a copy of the GNU General Public License version
dnl # 2 along with this work; if not, write to the Free Software Foundation,
dnl # Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
dnl #
dnl # Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
dnl # or visit www.oracle.com if you need additional information or have any
dnl # questions.

AC_INIT([adaptived],[1.0.1])

dnl #
dnl # autoconf configuration
dnl #
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([configure.h])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE(-Wall foreign subdir-objects)

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

dnl #
dnl # build flags
dnl #
AM_CPPFLAGS="-I\${top_srcdir}/include -I\${top_builddir}/include"
AM_CFLAGS="-Wall"
AM_LDFLAGS="-Wl,-z -Wl,relro"
AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_LDFLAGS])

dnl #
dnl # version information
dnl #
VERSION_MAJOR=$(echo ${VERSION} | cut -d'.' -f 1)
VERSION_MINOR=$(echo ${VERSION} | cut -d'.' -f 2)
VERSION_MICRO=$(echo ${VERSION} | cut -d'.' -f 3)
AC_SUBST([VERSION_MAJOR])
AC_SUBST([VERSION_MINOR])
AC_SUBST([VERSION_MICRO])

dnl #
dnl # Determine which C compiler to use
dnl #
AC_PROG_CXX
AC_PROG_CC
AM_PROG_CC_C_O

dnl #
dnl # Dependencies
dnl #
AC_CHECK_LIB([json-c], [json_object_object_get_ex], [],
	     [AC_MSG_ERROR([please install json-c])])

AC_CHECK_HEADERS([json-c/json.h], [],
		 [AC_MSG_ERROR([please install the json-c development package])])

LT_INIT

dnl #
dnl # Enable/Disable code coverage
dnl #
AX_CODE_COVERAGE

dnl #
dnl # makefiles
dnl #
AC_CONFIG_FILES([
	Makefile
	dist/Makefile
	doc/Makefile
	doc/examples/Makefile
	include/Makefile
	src/Makefile
	tests/Makefile
	tests/ftests/Makefile
	tests/gunit/Makefile
])
AC_CONFIG_FILES([dist/adaptived.spec:dist/adaptived.spec.in])

dnl #
dnl # generate the configure and makefiles
dnl #
AC_OUTPUT
