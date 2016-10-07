# Copyright (C) 2008 Equinox Software, Inc.
# Kevin Beswick <kevinbeswick00@gmail.com>
# Copyright (C) 2009-2010 Dan Scott <dscott@laurentian.ca>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# Process this file with autoconf to produce a configure script.


#-------------------------------
# Initialization
#-------------------------------

export PATH=${PATH}:/usr/sbin
AC_PREREQ(2.59)

# Get our version number from one file
m4_include([version.m4])

# Version number gets turned into @PACKAGE_VERSION@
AC_INIT([OpenSRF],m4_defn([VERSION_NUMBER]),[open-ils-dev@list.georgialibraries.org])
AM_INIT_AUTOMAKE([OpenSRF], m4_defn([VERSION_NUMBER]))

AC_REVISION($Revision: 0.1 $)
AC_CONFIG_SRCDIR([configure.ac])

# Enable $prefix to resolve to a reasonable value in substitutions in
# scripts if no explict value was passed in to configure
if test "$prefix" == "NONE"; then
   prefix=$ac_default_prefix
fi

# Perl and Python scripts don't want ${prefix} if no value was specified
eval "eval CONF_DIR=$sysconfdir"
eval "eval PID_DIR=$localstatedir"
AC_SUBST([CONF_DIR])
AC_SUBST([PID_DIR])

AC_SUBST(prefix)
AC_SUBST(bindir)


AC_DEFUN([AC_PYTHON_MOD],[
    if test -z $PYTHON;
    then
        PYTHON="python"
    fi
    AC_MSG_CHECKING($PYTHON_NAME module: $1)
        $PYTHON -c "import $1" 2>/dev/null
        if test $? -eq 0;
        then
                AC_MSG_RESULT(yes)
                eval AS_TR_CPP(HAVE_PYMOD_$1)=yes
        else
                AC_MSG_ERROR(failed to find required module $1)
		exit 1
        fi
])

#-------------------------------
# Installation options
#-------------------------------

# build and install the java libs?
AC_ARG_ENABLE([java],
[  --enable-java    enable building and installing the java libraries],
[case "${enableval}" in
    yes) OSRF_INSTALL_JAVA=true ;;
    no) OSRF_INSTALL_JAVA=false ;; 
  *) AC_MSG_ERROR([please choose another value for --enable-java (supported values are yes or no)]) ;;
esac],
[OSRF_INSTALL_JAVA=false])

AM_CONDITIONAL([BUILDJAVA], [test x$OSRF_INSTALL_JAVA = xtrue])
AC_SUBST([OSRF_INSTALL_JAVA])

# install the javascript files?
AC_ARG_ENABLE([javascript],
[  --disable-javascript    disable installing JavaScript libraries],
[case "${enableval}" in
    yes) OSRF_INSTALL_JAVASCRIPT=true ;;
    no) OSRF_INSTALL_JAVASCRIPT=false ;; 
  *) AC_MSG_ERROR([please choose another value for --disable-javascript (supported values are yes or no)]) ;;
esac],
[OSRF_INSTALL_JAVASCRIPT=true])

AM_CONDITIONAL([INSTALLJAVASCRIPT], [test x$OSRF_INSTALL_JAVASCRIPT = xtrue])
AC_SUBST([OSRF_INSTALL_JAVASCRIPT])

# install the OpenSRF core files?
AC_ARG_ENABLE([core],
[  --disable-core    disable installing core files],
[case "${enableval}" in
    yes) OSRF_INSTALL_CORE=true ;;
    no) OSRF_INSTALL_CORE=false ;; 
  *) AC_MSG_ERROR([please choose another value for --disable-core (supported values are yes or no)]) ;;
esac],
[OSRF_INSTALL_CORE=true])

AM_CONDITIONAL([BUILDCORE], [test x$OSRF_INSTALL_CORE = xtrue])
AC_SUBST([OSRF_INSTALL_CORE])


# build and install the python modules
AC_ARG_ENABLE([python],
[  --enable-python  enable building and installing python modules],
[case "${enableval}" in
  yes) OSRF_INSTALL_PYTHON=true ;;
  no) OSRF_INSTALL_PYTHON=false ;;
  *) AC_MSG_ERROR([please choose another value for --enable-python (supported values are yes or no)]) ;;
esac],
[OSRF_INSTALL_PYTHON=false])

AM_CONDITIONAL([BUILDPYTHON], [test x$OSRF_INSTALL_PYTHON = xtrue])
AC_SUBST([OSRF_INSTALL_PYTHON])

# enable debug?

AC_ARG_ENABLE(debug,
[  --enable-debug    Turn on debugging],
[case "${enableval}" in
  yes) debug=true ;;
  no)  debug=false ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
esac],[debug=false])
AM_CONDITIONAL(DEBUG, test x$debug = xtrue)
 
# path to the directory containing the java dependency jar files (included if java installs)
if test $OSRF_INSTALL_JAVA; then
	AC_SUBST([OSRF_JAVA_DEPSDIR], [deps])
    AC_CONFIG_FILES([src/java/Makefile])
fi

#--------------------------------
# Checks for programs.
#--------------------------------

AC_PROG_LIBTOOL
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

#------------------------------
# Set install path variables
#------------------------------
AC_ARG_WITH([tmp],
[  --with-tmp=path                  location for the temporary directory for OpenSRF (default is /tmp)],
[TMP=${withval}],
[TMP=/tmp])
AC_SUBST([TMP])

AC_ARG_WITH([apxs],
[  --with-apxs=path                 location of the apxs (Apache extension) tool (default is /usr/bin/apxs2)],
[APXS2=${withval}],
[APXS2=/usr/bin/apxs2])
if ! test -x "$APXS2"; then
	for i in /usr/bin /usr/sbin /usr/local/apache/bin /usr/local/apache2/bin ; do
		for j in apxs apxs2 ; do
			if test -x "$i/$j"; then
				APXS2="$i/$j"
				break
			fi
		done
	done
fi
AC_SUBST([APXS2])

AC_ARG_WITH([apache],
[  --with-apache=path               location of the Apache headers (default is /usr/include/apache2)],
[APACHE2_HEADERS=${withval}],
[APACHE2_HEADERS=/usr/include/apache2])
if ! test -d "$APACHE2_HEADERS"; then
	for i in /usr/include/httpd ; do
		if test -d "$i"; then
			APACHE2_HEADERS="$i"
			break
		fi
	done
fi
AC_SUBST([APACHE2_HEADERS])

AC_ARG_WITH([apr],
[  --with-apr=path                  location of the Apache Portable Runtime headers (default is /usr/include/apr-1.0/)],
[APR_HEADERS=${withval}],
[APR_HEADERS=/usr/include/apr-1.0])
if ! test -d "$APR_HEADERS"; then
	for i in /usr/include/apr-1 ; do
		if test -d "$i"; then
			APR_HEADERS="$i"
			break
		fi
	done
fi
AC_SUBST([APR_HEADERS])

AC_ARG_WITH([perlbase],
[  --with-perlbase=path             base location to install Perl modules (default based on Config.pm)],
[PERL_BASE=${withval}],
[PERL_BASE=x])
AC_SUBST([PERL_BASE])

# The following Apache version detection code is adapted from
# http://www.gnu.org/software/autoconf-archive/ax_prog_apache.html
# licensed under version 2 of the GNU General Public License, or
# (at your discretion) any later version.
#
# Copyright (c) 2008 Loic Dachary <loic@senga.org>
#
# Collect apache version number. If for nothing else, this
# guarantees that httpd is a working apache executable.
#
APACHE=`$APXS2 -q progname`
AC_PATH_PROG(APACHE_PATH, [$APACHE], [])
if test -z "$APACHE_PATH" ; then
	for i in /usr/bin /usr/sbin /usr/local/apache/bin /usr/local/apache2/bin ; do
        if test -x "$i/$APACHE"; then
            APACHE_PATH="$i/$APACHE"
            break
        fi
	done
fi
changequote(<<, >>)dnl
APACHE_READABLE_VERSION=`$APACHE_PATH -v | grep 'Server version' | sed -e 's;.*Apache/\([0-9\.][0-9\.]*\).*;\1;'`
changequote([, ])dnl
APACHE_VERSION=`echo $APACHE_READABLE_VERSION | sed -e 's/\.//g'`
if test -z "$APACHE_VERSION" ; then
    AC_MSG_ERROR("could not determine apache version number");
fi
APACHE_MAJOR=`expr $APACHE_VERSION : '\(..\)'`
APACHE_MINOR=`expr $APACHE_VERSION : '..\(.*\)'`
AM_CONDITIONAL(APACHE_MIN_24, test "$APACHE_MAJOR" -ge "24")
AC_SUBST([APACHE_MIN_24])

AC_ARG_WITH([libxml],
[  --with-libxml=path               location of the libxml2 headers (default is /usr/include/libxml2/))],
[LIBXML2_HEADERS=${withval}],
[LIBXML2_HEADERS=/usr/include/libxml2/])
AC_SUBST([LIBXML2_HEADERS])

AC_ARG_WITH([includes],
[  --with-includes=DIRECTORIES      a colon-separated list of directories that will be added to the list the compiler searches for header files (Example: --with-includes=/path/headers:/anotherpath/moreheaders)],
[EXTRA_USER_INCLUDES=${withval}])

AC_ARG_WITH([libraries],
[  --with-libraries=DIRECTORIES     a colon-separated list of directories to search for libraries (Example: --with-libraries=/lib:/usr/lib)],
[EXTRA_USER_LIBRARIES=${withval}])

# Change these lists to proper compiler/linker options

IFSBAK=${IFS}
IFS="${IFS}:"

for dir in $EXTRA_USER_INCLUDES; do
	if test -d "$dir"; then
	  INCLUDES="$INCLUDES -I$dir"
	else
	  AC_MSG_WARN([*** Include directory $dir does not exist.])
	fi
done
AC_SUBST(INCLUDES)

for dir in $EXTRA_USER_LIBRARIES; do
	if test -d "$dir"; then
	  LIBDIRS="$LIBDIRS -L$dir"
	else
	  AC_MSG_WARN([*** Library directory $dir does not exist.])
	fi
done
AC_SUBST(LIBDIRS)

IFS=${IFSBAK}

AC_CONFIG_FILES([Makefile
	src/Makefile])

#PYTHON TESTS
if test x$OSRF_INSTALL_PYTHON = xtrue; then
    AC_CHECK_PROG([HAVE_PYTHON],python,yes,no)
    if test $HAVE_PYTHON = "no"; then
        AC_MSG_ERROR([*** python not found, aborting])
    fi
    AC_PYTHON_MOD([setuptools])
    AC_CONFIG_FILES([
        examples/math_client.py
        examples/simple_text.py
        src/python/Makefile
    ])
fi
# Check Unit test framework
PKG_CHECK_MODULES([CHECK], [check >= 0.9.0], [enable_tests=yes],
                  [enable_tests=no])
AM_CONDITIONAL(CHECK_TESTS, test x$enable_tests = xyes)

if test "x$enable_tests" = "xno"; then
  AC_MSG_WARN(Check unit testing framework not found.)
fi

if test "x$OSRF_INSTALL_CORE" = "xtrue"; then
	#--------------------------------
	# Check for dependencies.
	#--------------------------------

	#APACHE PREFORK DEV TEST
	AC_MSG_CHECKING([APXS])
	if test -f "${APXS2}"; then
	AC_MSG_RESULT([yes])
	else
	AC_MSG_ERROR([*** apxs not found, aborting])
	fi  

	#-----------------------------
	# Checks for libraries.
	#-----------------------------

	AC_SEARCH_LIBS([dlerror], [dl], [],AC_MSG_ERROR([***OpenSRF requires a library (typically libdl) that provides dlerror()]))
	AC_CHECK_LIB([ncurses], [initscr], [], AC_MSG_ERROR(***OpenSRF requires ncurses development headers))
	AC_CHECK_LIB([readline], [readline], [], AC_MSG_ERROR(***OpenSRF requires readline development headers))
	AC_CHECK_LIB([xml2], [xmlAddID], [], AC_MSG_ERROR(***OpenSRF requires xml2 development headers))
	# Check for libmemcached and set flags accordingly
	PKG_CHECK_MODULES(memcached, libmemcached >= 0.8.0)
	AC_SUBST(memcached_CFLAGS)
	AC_SUBST(memcached_LIBS)

	#-----------------------------
	# Checks for header files.
	#-----------------------------

	AC_HEADER_STDC
	AC_HEADER_SYS_WAIT
	AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h malloc.h netdb.h netinet/in.h stdlib.h string.h strings.h sys/socket.h sys/time.h sys/timeb.h syslog.h unistd.h])

	#------------------------------------------------------------------
	# Checks for typedefs, structures, and compiler characteristics.
	#------------------------------------------------------------------

	AC_C_CONST
	AC_C_INLINE
	AC_TYPE_PID_T
	AC_TYPE_SIZE_T
	AC_HEADER_TIME
	AC_STRUCT_TM
    AM_PROG_CC_C_O

	#----------------------------------
	# Checks for library functions.
	#----------------------------------

	AC_FUNC_FORK
	AC_FUNC_MALLOC
	AC_FUNC_SELECT_ARGTYPES
	AC_TYPE_SIGNAL
	AC_FUNC_STRFTIME
	AC_FUNC_STRTOD
	AC_FUNC_VPRINTF
	AC_CHECK_FUNCS([bzero dup2 gethostbyname gethostname gettimeofday malloc_stats memset select socket strcasecmp strchr strdup strerror strncasecmp strndup strrchr strtol])

	#------------------------------------
	# Configuration and output
	#------------------------------------

	AC_CONFIG_FILES([doc/dokuwiki-doc-stubber.pl
			 examples/math_xul_client/Makefile
			 examples/math_bench.pl
			 examples/multisession-test.pl
			 src/c-apps/Makefile
			 src/gateway/Makefile
			 src/libopensrf/Makefile
			 src/perl/Makefile
			 src/ports/strn_compat/Makefile
			 src/python/opensrf.py
			 src/router/Makefile
			 src/srfsh/Makefile
             tests/Makefile
			 bin/opensrf-perl.pl
			 bin/osrf_config])
fi

AC_OUTPUT

AC_MSG_RESULT([])
AC_MSG_RESULT([--------------------- Configuration options:  -----------------------])

if test "$OSRF_INSTALL_JAVA" = "true" ; then
        AC_MSG_RESULT([OSRF install Java support?       yes])
        AC_MSG_RESULT([Java support files               $OSRF_JAVA_DEPSDIR])
else
        AC_MSG_RESULT([OSRF install Java support?       no])
fi

if test "$OSRF_INSTALL_PYTHON" = "true" ; then
        AC_MSG_RESULT([OSRF install Python support?     yes])
else
        AC_MSG_RESULT([OSRF install Python support?     no])
fi

        AC_MSG_RESULT(Installation directory prefix:   ${prefix})
        AC_MSG_RESULT(Temporary directory:             ${TMP})
        AC_MSG_RESULT(APXS2 location:                  ${APXS2})
        AC_MSG_RESULT(Apache headers location:         ${APACHE2_HEADERS})
        AC_MSG_RESULT(APR headers location:            ${APR_HEADERS})
        AC_MSG_RESULT(Apache version:                  ${APACHE_READABLE_VERSION})
        AC_MSG_RESULT(libxml2 headers location:        ${LIBXML2_HEADERS})

AC_MSG_RESULT([----------------------------------------------------------------------])
