#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.69)
AC_INIT(aegee-milter, 1.0, aegee-milter@aegee.org)
AM_INIT_AUTOMAKE([-Wall -Werror dist-bzip2 subdir-objects silent-rules])
AM_SILENT_RULES([yes])
AC_CONFIG_SRCDIR([src/core/aegee-milter.cpp])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_MACRO_DIRS([m4])
# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O
AM_PROG_AR
gl_VISIBILITY
AH_BOTTOM([#if HAVE_VISIBILITY
#define EXPORTED __attribute__((__visibility__("default")))
#define HIDDEN   __attribute__((__visibility__("hidden")))
#else
#define EXPORTED
#define HIDDEN
#endif])

LT_PREREQ([2.4.2])
LT_INIT([disable-shared, dlopen])
AC_SUBST([LIBTOOL_DEPS])
LT_CONFIG_LTDL_DIR([libltdl])
LTDL_INIT([nonrecursive])
AC_CHECK_HEADERS_ONCE(signal.h, errno.h, sys/time.h, sys/stat.h, sys/types.h, libmilter/mfapi.h)
# Checks for libraries.
AC_CHECK_LIB([milter], [smfi_main],,,[-lpthread])
dnl AC_CHECK_LIB([pthread],[])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
PKG_PROG_PKG_CONFIG(0.25)
PKG_CHECK_MODULES([glib], [glib-2.0 >= 2.28.1], [check_glib=yes], [check_glib=no])
PKG_CHECK_MODULES([gmime], [gmime-2.6 >= 2.6.20], [check_gmime=yes], [check_gmime=no])
dnl PKG_CHECK_MODULES([libclamav], [libclamav >= 6:3:0], [check_libclamav=yes], [check_libclamav=no])
PKG_CHECK_MODULES([libiodbc], [libiodbc >= 3.52.7], [check_libiodbc=yes], [check_libiodbc=no])
PKG_CHECK_MODULES([liblistserv], [liblistserv >= 0.1], [check_liblistserv=yes], [check_liblistserv=no])
dnl AM_CONDITIONAL([MOD_CLAMAV], [test "$check_libclamav" = "yes"])
AM_CONDITIONAL([LIST_ODBC], [test "$check_libiodbc" = "yes_but_still_disable"])
AM_CONDITIONAL([LIST_LISTSERV], [test "$check_liblistserv" = "yes"])
PKG_CHECK_MODULES([libmemcached], [libmemcached >= 1.0.18], [check_memcached=yes], [check_memcached=no])
AM_CONDITIONAL([LIST_MEMCACHED], [test "$check_memcached" = "yes"])
PKG_CHECK_MODULES([libsieve], [libsieve >= 2.3], [check_libsieve=yes], [check_libsieve=no])
AM_CONDITIONAL([MOD_LIBSIEVE], [test "$check_libsieve" = "yes"])
PKG_CHECK_MODULES([libcyrus_sieve], [libcyrus_sieve >= 2.5], [check_libcyrus_sieve=yes], [check_libcyrus_sieve=no])
AM_CONDITIONAL([MOD_CYRUS_SIEVE], [test "$check_libcyrus_sieve" = "yes"])

dnl AX_BERKELEY_DB([4.6.21], [check_libdb=yes], [check_libdb=no])
dnl AM_CONDITIONAL([LIST_BERKELEY], [test "$check_libdb" = "yes"])
AM_CONDITIONAL([LIST_LOG], [true])
#check_dns=no
#AM_CONDITIONAL([LIST_DNS], [test "$check_dns" = "yes" ])
# Checks for library functions.
#AC_FUNC_MALLOC
AC_HEADER_STDC
AC_CONFIG_FILES([Makefile])
AC_CONFIG_HEADERS([config.h])
AC_OUTPUT

echo "
    aegee-milter configuration summary:

          lists built by default:
            list_log
            list_timsieved_scripts

          conditionally built lists:
            list_listserv:           $check_liblistserv
            list_memcached:          $check_memcached
            list_odbc:               $check_libiodbc

          modules built by default:
            mod_block_sender
            mod_delay
            mod_equal
            mod_relayed
            mod_simple
            mod_via
            mod_x_scc_headers

          conditionally built modules:
            mod_arf:                 $check_liblistserv
            mod_sieve:               $check_libsieve
"
