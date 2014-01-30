dnl Process this file with autoconf to produce a configure script.

# Save the original $CFLAGS so we can distinguish whether the user set those
# in the environment, or whether autoconf added -O and -g options:
ORIGINAL_CFLAGS="$CFLAGS"

dnl --------------------------------
dnl Initialization macros (sets PACKAGE and VERSION)
dnl --------------------------------
m4_define([guvcview_major], [2])
m4_define([guvcview_minor], [0])
m4_define([guvcview_micro], [0])
m4_define([guvcview_version],
          [guvcview_major.guvcview_minor.guvcview_micro])
          
AC_INIT([guvcview], [guvcview_version], [http://guvcview.sourceforge.net/])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AM_INIT_AUTOMAKE([gnu])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS(config.h)
AM_MAINTAINER_MODE(disable)

dnl -----------------------------------------------
dnl Package release
dnl -----------------------------------------------

# autoconf 2.5x defaults to no cache file; we need the cache file's information
# for building the config page.  But start with it empty to avoid confusion by
# people who don't do a "make distclean" after applying patches.
cache_file=config.cache
rm -f config.cache; touch config.cache

AC_PREFIX_DEFAULT(/usr/local)

AC_ISC_POSIX
AC_PROG_CC
AC_HEADER_STDC

dnl --------------------------------------------------------------------------
dnl set pthread_libs and pthread_cflags
dnl --------------------------------------------------------------------------
dnl ACX_PTHREAD([ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])

ACX_PTHREAD

dnl --------------------------------------------------------------------------
dnl Set gettext package name
dnl --------------------------------------------------------------------------

GETTEXT_PACKAGE=guvcview
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [guvcview])

IT_PROG_INTLTOOL([0.40])

dnl --------------------------------------------------------------------------
dnl Languages supported by guvcview.
dnl --------------------------------------------------------------------------

ALL_LINGUAS="bg bs cs da de en_AU es eu fo fr gl he hr it ja lv nl pl pt pt_BR ru si sr tr uk zh_TW"
AM_GLIB_GNU_GETTEXT([external])

dnl -----------------------------------------------
dnl libv4l2core name and version number
dnl -----------------------------------------------

V4L2CORE_LIBRARY_NAME=libv4l2core
AC_SUBST(V4L2CORE_LIBRARY_NAME)
V4L2CORE_LD_NAME=v4l2core
AC_SUBST(V4L2CORE_LD_NAME)

#release versioning
V4L2CORE_MAJOR_VERSION=0
V4L2CORE_MINOR_VERSION=1
V4L2CORE_MICRO_VERSION=0

#API version (SONAME)
V4L2CORE_API_VERSION=$V4L2CORE_MAJOR_VERSION.$V4L2CORE_MINOR_VERSION
AC_SUBST(V4L2CORE_API_VERSION)

#shared library versioning
V4L2CORE_LIBRARY_VERSION=$V4L2CORE_MAJOR_VERSION:$V4L2CORE_MINOR_VERSION:$V4L2CORE_MICRO_VERSION
#
#             current:revision:age
#                |       |     |
#                |       |     +- increment if interfaces have been added
#                |       |        set to zero if interfaces have been removed
#                                  or changed
#                |       +- increment if source code has changed
#                |          set to zero if current is incremented
#                +- increment if interfaces have been added, removed or changed

AC_SUBST(V4L2CORE_LIBRARY_VERSION)


dnl -----------------------------------------------
dnl Checks for programs.
dnl -----------------------------------------------

AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL
AM_SANITY_CHECK

AC_PREFIX_DEFAULT(/usr/local)
AC_SUBST(CFLAGS)

dnl --------------------------------------------------------------------------
dnl check for libv4l2core dependencies
dnl --------------------------------------------------------------------------

PKG_CHECK_MODULES(V4L2CORELIB, [libv4l2 libudev])
AC_SUBST(V4L2CORELIB_CFLAGS)
AC_SUBST(V4L2CORELIB_LIBS)

dnl --------------------------------------------------------------------------
dnl check for guvcview dependencies
dnl --------------------------------------------------------------------------

PKG_CHECK_MODULES(GUVCVIEW, [libv4l2])
AC_SUBST(GUVCVIEW_CFLAGS)
AC_SUBST(GUVCVIEW_LIBS)

dnl --------------------------------------------------------------------------
dnl Check if the processor stores words with the most significant byte first
dnl (like Motorola and SPARC, unlike Intel and VAX).
dnl --------------------------------------------------------------------------

AC_C_BIGENDIAN

dnl -----------------------------------------------
dnl Generates Makefiles, configuration files and scripts
dnl -----------------------------------------------

AC_CONFIG_FILES([
	Makefile
    pkgconfig/v4l2core-0.1.pc
    v4l2_core/Makefile
    guvcview/Makefile
    po/Makefile.in
])
AC_OUTPUT

dnl Output the results
AC_MSG_NOTICE([

  guvcview $VERSION
  ----------------------

  Prefix         : ${prefix}

])



