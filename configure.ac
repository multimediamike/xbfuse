AC_INIT([xbfuse], [1.0], [mike@multimedia.cx])
AC_CONFIG_AUX_DIR([config])
AM_INIT_AUTOMAKE([xbfuse], [1.0])
AC_CONFIG_HEADER([src/config.h])

AC_PROG_CC
#AC_PROG_LIBTOOL

CPPFLAGS="$CPPFLAGS -Wall `getconf LFS_CFLAGS`"
LDFLAGS="$LDFLAGS `getconf LFS_LDFLAGS`"

AC_CHECK_LIB([fuse], [fuse_main], , AC_MSG_ERROR([Unable to find libfuse]))

AC_HEADER_STDC
AC_CHECK_HEADER([fuse/fuse.h], , AC_MSG_ERROR([Unable to find fuse.h]))
AC_PREPROC_IFELSE(
[#define FUSE_USE_VERSION 25
#include <fuse.h>]
, , AC_MSG_ERROR([You need FUSE version 2.5.x]))

AC_C_CONST

AC_FUNC_MALLOC
AC_FUNC_STAT

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
