# Copyright (C) 2008 Equinox Software, Inc.
# Kevin Beswick <kevinbeswick00@gmail.com>
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
AC_INIT([OpenSRF],[trunk],[open-ils-dev@list.georgialibraries.org])
AM_INIT_AUTOMAKE([OpenSRF], [trunk])
AC_REVISION($Revision: 0.1 $)
AC_CONFIG_SRCDIR([configure.ac])
AC_PREFIX_DEFAULT([/opensrf/])
AC_CONFIG_MACRO_DIR([m4])

AC_SUBST(prefix)
AC_SUBST(sysconfdir)
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
	AC_SUBST([OSRF_JAVA_DEPSDIR], [/opt/java])
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
[  --with-tmp=path                  location for the temporary dir for OpenSRF (default is /tmp)],
[TMP=${withval}],
[TMP=/tmp])
AC_SUBST([TMP])

AC_ARG_WITH([apxs],
[  --with-apxs=path                 location of the apxs Apache configuration tool (default is /usr/bin/apxs2)],
[APXS2=${withval}],
[APXS2=/usr/bin/apxs2])
AC_SUBST([APXS2])

AC_ARG_WITH([apache],
[  --with-apache=path               location of the Apache headers (default is /usr/include/apache2)],
[APACHE2_HEADERS=${withval}],
[APACHE2_HEADERS=/usr/include/apache2])
AC_SUBST([APACHE2_HEADERS])

AC_ARG_WITH([apr],
[  --with-apr=path                  location of the apr headers (default is /usr/include/apr-1.0/)],
[APR_HEADERS=${withval}],
[APR_HEADERS=/usr/include/apr-1.0])
AC_SUBST([APR_HEADERS])

AC_ARG_WITH([libxml],
[  --with-libxml=path               location of the libxml headers (default is /usr/include/libxml2/))],
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

#PYTHON TESTS
if test x$OSRF_INSTALL_PYTHON = xtrue; then
        AC_CHECK_PROG([HAVE_PYTHON],python,yes,no)
        if test $HAVE_PYTHON = "no"; then
                AC_MSG_ERROR([*** python not found, aborting])
        fi
        AC_PYTHON_MOD([setuptools])
fi


#-----------------------------
# Checks for libraries.
#-----------------------------

AC_CHECK_LIB([dl], [dlerror], [],AC_MSG_ERROR(***OpenSRF requires libdl))
AC_SEARCH_LIBS([mc_req_free], [memcache], [], AC_MSG_ERROR(***OpenSRF requires memcache development headers))
AC_CHECK_LIB([ncurses], [initscr], [], AC_MSG_ERROR(***OpenSRF requires ncurses development headers))
AC_CHECK_LIB([readline], [readline], [], AC_MSG_ERROR(***OpenSRF requires readline development headers))
AC_CHECK_LIB([xml2], [xmlAddID], [], AC_MSG_ERROR(***OpenSRF requires xml2 development headers))



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
AC_CHECK_FUNCS([bzero dup2 gethostbyname gethostname gettimeofday memset select socket strcasecmp strchr strdup strerror strncasecmp strndup strrchr strtol])

#------------------------------------
# Configuration and output
#------------------------------------

AC_CONFIG_FILES([Makefile
                 doc/dokuwiki-doc-stubber.pl
                 examples/math_xul_client/Makefile
                 examples/math_bench.pl
                 examples/math_client.py
                 examples/multisession-test.pl
                 src/Makefile
                 src/c-apps/Makefile
                 src/gateway/Makefile
                 src/java/Makefile
                 src/jserver/Makefile
                 src/libopensrf/Makefile
                 src/perl/Makefile
                 src/ports/strn_compat/Makefile
                 src/python/Makefile
                 src/router/Makefile
                 src/srfsh/Makefile
                 bin/opensrf-perl.pl
                 bin/osrf_config
                 bin/osrf_ctl.sh])


AC_OUTPUT

AC_MSG_RESULT([])
AC_MSG_RESULT([--------------------- Configuration options:  -----------------------])

if test "$OSRF_INSTALL_JAVA" = "true" ; then
	AC_MSG_RESULT([OSRF install java?:              yes])
	AC_MSG_RESULT([Java deps dir:                   $OSRF_JAVA_DEPSDIR])
else
	AC_MSG_RESULT([OSRF install java?:              no])
fi

if test "$OSRF_INSTALL_PYTHON" = "true" ; then
        AC_MSG_RESULT([OSRF install python?:            yes])
else
        AC_MSG_RESULT([OSRF install python?:            no])
fi

	AC_MSG_RESULT(Installation directory prefix:		${prefix})
	AC_MSG_RESULT(Tmp dir location:				${TMP})
	AC_MSG_RESULT(APXS2 location:				${APXS2})
	AC_MSG_RESULT(Apache headers location:			${APACHE2_HEADERS})
	AC_MSG_RESULT(APR headers location:			${APR_HEADERS})
	AC_MSG_RESULT(libxml2 headers location:			${LIBXML2_HEADERS})





AC_MSG_RESULT([----------------------------------------------------------------------])
