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

dnl ------------------------------------------------
dnl Pretty output
dnl ------------------------------------------------
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

AC_PROG_CC
AM_PROG_CC_C_O
AC_HEADER_STDC
AC_PROG_LIBTOOL
AM_SANITY_CHECK

dnl --------------------------------------------------------------------------
dnl set pthread_libs and pthread_cflags
dnl --------------------------------------------------------------------------
dnl ACX_PTHREAD([ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])

ACX_PTHREAD()

AC_SUBST(CFLAGS)

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
dnl libgviewv4l2core name and version number
dnl -----------------------------------------------

GVIEWV4L2CORE_LIBRARY_NAME=libgviewv4l2core
AC_SUBST(GVIEWV4L2CORE_LIBRARY_NAME)
GVIEWV4L2CORE_LD_NAME=gviewv4l2core
AC_SUBST(GVIEWV4L2CORE_LD_NAME)

#release versioning
GVIEWV4L2CORE_MAJOR_VERSION=0
GVIEWV4L2CORE_MINOR_VERSION=1
GVIEWV4L2CORE_MICRO_VERSION=0

#API version (SONAME)
GVIEWV4L2CORE_API_VERSION=$GVIEWV4L2CORE_MAJOR_VERSION.$GVIEWV4L2CORE_MINOR_VERSION
AC_SUBST(GVIEWV4L2CORE_API_VERSION)

#shared library versioning
GVIEWV4L2CORE_LIBRARY_VERSION=$GVIEWV4L2CORE_MAJOR_VERSION:$GVIEWV4L2CORE_MINOR_VERSION:$GVIEWV4L2CORE_MICRO_VERSION
#
#             current:revision:age
#                |       |     |
#                |       |     +- increment if interfaces have been added
#                |       |        set to zero if interfaces have been removed
#                                  or changed
#                |       +- increment if source code has changed
#                |          set to zero if current is incremented
#                +- increment if interfaces have been added, removed or changed

AC_SUBST(GVIEWV4L2CORE_LIBRARY_VERSION)

dnl --------------------------------------------------------------------------
dnl check for libgviewv4l2core dependencies
dnl --------------------------------------------------------------------------

PKG_CHECK_MODULES(GVIEWV4L2CORE, [libv4l2 libudev libusb-1.0 libavcodec])
AC_SUBST(GVIEWV4L2CORE_CFLAGS)
AC_SUBST(GVIEWV4L2CORE_LIBS)

dnl --------------------------------------------------------------------------
dnl Check for avcodec.h installation path
dnl --------------------------------------------------------------------------

dnl Check for avcodec.h directly
dnl if all tests fail must add avcodec.h path to C_INCLUDE_PATH before running configure
AC_CHECK_HEADER([avcodec.h], [has_avcodec_h="yes"])

dnl Check old install path for avcodec.h (ffmpeg/avcodec.h)
AC_CHECK_HEADER([ffmpeg/avcodec.h], [has_ffmpeg_dir="yes"])

dnl Check new install path for avcodec.h (libavcodec/avcodec.h)
dnl if all the checks fail we include "libavcodec/avcodec.h"
AC_CHECK_HEADER([libavcodec/avcodec.h], [has_libavcodec_dir="yes"])

if test "$has_avcodec_h" = yes; then
  AC_DEFINE(HAS_AVCODEC_H,1,[Define if avcodec.h can be acessed directly])
fi
if test "$has_ffmpeg_dir" = yes; then
  AC_DEFINE(HAS_FFMPEG_AVCODEC_H,1,[Define if avcodec.h installed to ffmpeg dir])
fi
if test "$has_libavcodec_dir" = yes; then
  AC_DEFINE(HAS_LIBAVCODEC_AVCODEC_H,1,[Define if avcodec.h installed to libavcodec dir])
fi

dnl -----------------------------------------------
dnl libgviewrender name and version number
dnl -----------------------------------------------

GVIEWRENDER_LIBRARY_NAME=libgviewrender
AC_SUBST(GVIEWRENDER_LIBRARY_NAME)
GVIEWRENDER_LD_NAME=gviewrender
AC_SUBST(GVIEWRENDER_LD_NAME)

#release versioning
GVIEWRENDER_MAJOR_VERSION=0
GVIEWRENDER_MINOR_VERSION=1
GVIEWRENDER_MICRO_VERSION=0

#API version (SONAME)
GVIEWRENDER_API_VERSION=$GVIEWRENDER_MAJOR_VERSION.$GVIEWRENDER_MINOR_VERSION
AC_SUBST(GVIEWRENDER_API_VERSION)

#shared library versioning
GVIEWRENDER_LIBRARY_VERSION=$GVIEWRENDER_MAJOR_VERSION:$GVIEWRENDER_MINOR_VERSION:$GVIEWRENDER_MICRO_VERSION
#
#             current:revision:age
#                |       |     |
#                |       |     +- increment if interfaces have been added
#                |       |        set to zero if interfaces have been removed
#                                  or changed
#                |       +- increment if source code has changed
#                |          set to zero if current is incremented
#                +- increment if interfaces have been added, removed or changed

AC_SUBST(GVIEWRENDER_LIBRARY_VERSION)

dnl --------------------------------------------------------------------------
dnl check for libgviewrender dependencies
dnl --------------------------------------------------------------------------

PKG_CHECK_MODULES(GVIEWRENDER, [sdl >= 1.2.10])
AC_SUBST(GVIEWRENDER_CFLAGS)
AC_SUBST(GVIEWRENDER_LIBS)

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
    pkgconfig/gviewv4l2core-0.1.pc
    pkgconfig/gviewrender-0.1.pc
    gview_v4l2core/Makefile
    gview_render/Makefile
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



