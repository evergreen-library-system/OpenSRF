#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(FULL-PACKAGE-NAME, VERSION, BUG-REPORT-ADDRESS)
AC_CONFIG_SRCDIR([include/objson/json2xml.h])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Checks for libraries.
# FIXME: Replace `main' with a function in `-ldl':
#AC_CHECK_LIB([dl], [main])
# Test for libmemcache.
AC_CHECK_LIB([memcache], [mcMemSetup])
# Test for ncurses.
AC_CHECK_LIB([ncurses], [initscr])
# FIXME: Replace `main' with a function in `-lobjson':
#AC_CHECK_LIB([objson], [main])
# FIXME: Replace `main' with a function in `-lopensrf':
#AC_CHECK_LIB([opensrf], [main])
# Test for libreadline.
AC_CHECK_LIB([readline], [readline])
# Test for libxml2.
AC_CHECK_LIB([xml2], [xmlInitParser])

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h sys/timeb.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM
AC_HEADER_STDBOOL

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_FUNC_STRFTIME
AC_FUNC_STRNLEN
AC_FUNC_STRTOD
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([bzero dup2 gethostbyname gettimeofday memchr memmove memset select socket strcasecmp strchr strdup strerror strncasecmp strndup strtol])

AC_CONFIG_FILES([Makefile
                 examples/math_xul_client/Makefile
                 src/Makefile
                 src/c-apps/Makefile
                 src/gateway/Makefile
                 src/java/Makefile
                 src/jserver/Makefile
                 src/libopensrf/Makefile
                 src/objson/Makefile
                 src/ports/strn_compat/Makefile
                 src/router/Makefile
                 src/srfsh/Makefile])
AC_OUTPUT
