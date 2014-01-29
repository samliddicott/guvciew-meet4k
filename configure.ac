dnl Process this file with autoconf to produce a configure script.

# Save the original $CFLAGS so we can distinguish whether the user set those
# in the environment, or whether autoconf added -O and -g options:
ORIGINAL_CFLAGS="$CFLAGS"

dnl --------------------------------
dnl Initialization macros (sets PACKAGE and VERSION)
dnl --------------------------------

AC_INIT(guvcvideo, 2.0.0, pj.assis@gmail.com)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE(disable)
AM_INIT_AUTOMAKE(gnu dist-bzip2 )
AC_CONFIG_MACRO_DIR([m4])

dnl -----------------------------------------------
dnl Package release
dnl -----------------------------------------------

PACKAGE_MAJOR_VERSION=2
PACKAGE_MINOR_VERSION=0
PACKAGE_MICRO_VERSION=0

PACKAGE_VERSION=$PACKAGE_MAJOR_VERSION.$PACKAGE_MINOR_VERSION.$PACKAGE_MICRO_VERSION
PACKAGE_RELEASE=$PACKAGE_MAJOR_VERSION.$PACKAGE_MINOR_VERSION
AC_SUBST(PACKAGE_RELEASE)
AC_SUBST(PACKAGE_VERSION)


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
dnl guvcview app
dnl -----------------------------------------------

GUVCVIEW_APP_NAME=guvcview
AC_SUBST(GUVCVIEW_APP_NAME)

GUVCVIEW_APP_MAJOR_VERSION=2
GUVCVIEW_APP_MINOR_VERSION=0
GUVCVIEW_APP_MICRO_VERSION=0

# app version
GUVCVIEW_APP_VERSION=$GUVCVIEW_APP_MAJOR_VERSION.$GUVCVIEW_APP_MINOR_VERSION.$GUVCVIEW_APP_MICRO_VERSION
AC_SUBST(GUVCVIEW_APP_VERSION)

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
dnl Set gettext package name
dnl --------------------------------------------------------------------------
GETTEXT_GUVCVIEW = $PACKAGE
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [$PACKAGE])

dnl --------------------------------------------------------------------------
dnl Supported Languages
dnl --------------------------------------------------------------------------
ALL_LINGUAS="pt pt_BR de es fr da tr cs ja ru nl he eu"
AM_GLIB_GNU_GETTEXT([external])

dnl --------------------------------------------------------------------------
dnl check for libv4l2core dependencies
dnl --------------------------------------------------------------------------

PKG_CHECK_MODULES(V4L2CORELIB, [libv4l2 libudev])
AC_SUBST(V4L2CORELIB_CFLAGS)
AC_SUBST(V4L2CORELIB_LIBS)

dnl --------------------------------------------------------------------------
dnl Check if the processor stores words with the most significant byte first
dnl (like Motorola and SPARC, unlike Intel and VAX).
dnl --------------------------------------------------------------------------

AC_C_BIGENDIAN

dnl -----------------------------------------------
dnl Generates Makefile's, configuration files and scripts
dnl -----------------------------------------------

AC_OUTPUT(Makefile \
          pkgconfig/v4l2core-0.1.pc \
          v4l2_core/Makefile \
          guvcview/Makefile \
          po/Makefile.in
)
