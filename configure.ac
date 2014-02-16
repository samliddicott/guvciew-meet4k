dnl Process this file with autoconf to produce a configure script.

# Save the original $CFLAGS so we can distinguish whether the user set those
# in the environment, or whether autoconf added -O and -g options:
ORIGINAL_CFLAGS="$CFLAGS"

dnl --------------------------------------------------------------------------
dnl Package definitions and initialization
dnl --------------------------------------------------------------------------
m4_define([guvcview_major], [1])
m4_define([guvcview_minor], [7])
m4_define([guvcview_micro], [3])
m4_define([guvcview_version],
          [guvcview_major.guvcview_minor.guvcview_micro])

AC_INIT([guvcview],[guvcview_version],[http://guvcview.sourceforge.net/])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AM_INIT_AUTOMAKE([gnu])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS(config.h)
AM_MAINTAINER_MODE(disable)

# autoconf 2.5x defaults to no cache file; we need the cache file's information
# for building the config page.  But start with it empty to avoid confusion by
# people who don't do a "make distclean" after applying patches.
cache_file=config.cache
rm -f config.cache; touch config.cache

AC_PREFIX_DEFAULT(/usr/local)

AC_PROG_CC
AM_PROG_CC_C_O
AC_HEADER_STDC

dnl --------------------------------------------------------------------------
dnl set pthread_libs and pthread_cflags
dnl --------------------------------------------------------------------------
dnl ACX_PTHREAD([ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])
ACX_PTHREAD()

dnl --------------------------------------------------------------------------
dnl Debug/development/test
dnl --------------------------------------------------------------------------

dnl ---------------------------------------------------------------------------
dnl CF_STRIP_G_OPT version: 3 updated: 2002/12/21 19:25:52
dnl --------------
dnl	Remove "-g" option from the compiler options
AC_DEFUN([CF_STRIP_G_OPT],
[$1=`echo ${$1} | sed -e 's%-g %%' -e 's%-g$%%'`])

dnl ---------------------------------------------------------------------------
dnl CF_STRIP_O_OPT version: 3 updated: 2001/02/02 19:06:08
dnl --------------
dnl	Remove "-O" option from the compiler options
AC_DEFUN([CF_STRIP_O_OPT],
[$1=`echo ${$1} | sed -e 's/-O[[1-9]]\? //' -e 's/-O[[1-9]]\?$//'`])

# The comment about adding -g to $CFLAGS is unclear.  Autoconf tries to add
# a -g flag; we remove it if the user's $CFLAGS was not set and debugging is
# disabled.
AC_MSG_CHECKING(if you specifically want to enable debug-code [-g])
AC_ARG_ENABLE(debug, AS_HELP_STRING([--enable-debug],
	[enable debug compile w/ debugging - if \$CFLAGS is set, add -g there, too]),
	[enable_debug=yes],
	[enable_debug=no])

AC_MSG_RESULT($enable_debug)

if test "x$enable_debug" = "xyes" ; then
	test -z "$ORIGINAL_CFLAGS" && CF_STRIP_O_OPT(CFLAGS)
	if test "$CC" = gcc; then
		CFLAGS="$CFLAGS -g"
	fi
#else
#	CF_STRIP_G_OPT(CFLAGS)
fi

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

dnl --------------------------------------------------------------------------
dnl check for dependencies
dnl --------------------------------------------------------------------------
PKG_CHECK_MODULES(DEPS, [gtk+-3.0 >= 3.0.0 glib-2.0 >= 2.10.0 sdl >= 1.2.10 portaudio-2.0 libpng libavcodec libavutil libv4l2 libudev libusb-1.0])
AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

dnl --------------------------------------------------------------------------
dnl set/unset debian menu
dnl --------------------------------------------------------------------------
AC_MSG_CHECKING(if you want to enable debian menu)
AC_ARG_ENABLE(debian-menu, AS_HELP_STRING([--disable-debian-menu],
		[disable debian menu (default: enabled)]),
	[enable_debian_menu=no],
	[enable_debian_menu=yes])

AC_MSG_RESULT($enable_debian_menu)

AM_CONDITIONAL(DEBIAN_MENU, test "$enable_debian_menu" = yes)

dnl --------------------------------------------------------------------------
dnl set/unset desktop file
dnl --------------------------------------------------------------------------
AC_MSG_CHECKING(if you want to enable desktop file)
AC_ARG_ENABLE(desktop, AS_HELP_STRING([--disable-desktop],
		[disable desktop file (default: enabled)]),
	[enable_desktop=no],
	[enable_desktop=yes])

AC_MSG_RESULT($enable_desktop)

AM_CONDITIONAL(DESKTOP, test "$enable_desktop" = yes)

dnl --------------------------------------------------------------------------
dnl Check for pulseaudio
dnl --------------------------------------------------------------------------
AC_MSG_CHECKING(if you want to enable pulse support)
AC_ARG_ENABLE(pulse, AS_HELP_STRING([--disable-pulse],
		[disable pulseaudio support (default: enabled)]),
	[enable_pulse=no],
	[enable_pulse=yes])

AC_MSG_RESULT($enable_pulse)

if test $enable_pulse = yes; then
	PKG_CHECK_MODULES(PULSE, libpulse >= 0.9.15, has_pulse=yes, has_pulse=no)
	AC_SUBST(PULSE_CFLAGS)
	AC_SUBST(PULSE_LIBS)
	if test "$has_pulse" = yes; then
	  AC_DEFINE(PULSEAUDIO,1,[set to 1 if pulseaudio installed])
	else
	  AC_MSG_WARN(libpulse missing... pulse support will be disabled.);
	  enable_pulse=no
	fi
fi

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

dnl --------------------------------------------------------------------------
dnl Check if the processor stores words with the most significant byte first
dnl (like Motorola and SPARC, unlike Intel and VAX).
dnl --------------------------------------------------------------------------
AC_C_BIGENDIAN

dnl Create the Makefiles
AC_CONFIG_FILES([
 Makefile
 data/Makefile
 data/icons/Makefile
 data/guvcview.desktop.in
 data/guvcview.in
 po/Makefile.in
 src/Makefile
])
AC_OUTPUT

dnl Output the results
AC_MSG_NOTICE([

  guvcview $VERSION
  ----------------------

  Prefix         : ${prefix}

  Pulse API      : ${enable_pulse}

  Desktop File   : ${enable_desktop}
  Debian Menu    : ${enable_debian_menu}

  Debug Symbols  : ${enable_debug}

])

