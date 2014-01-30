## Process this file with automake to produce Makefile.in

h_sources = v4l2_core.h

cpp_sources = v4l2_core.c \
			  v4l2_formats.c \
			  v4l2_controls.c \
			  v4l2_devices.c \
			  v4l2_xu_ctrls.c \
			  uvc_h264.c


#Install the headers in a versioned directory - guvcvideo-x.x/libv4l2core:
library_includedir=$(includedir)/$(PACKAGE)-$(PACKAGE_RELEASE)/$(V4L2CORE_LIBRARY_NAME)
library_include_HEADERS = $(h_sources)

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/includes

lib_LTLIBRARIES= libv4l2core.la
libv4l2core_la_SOURCES= $(h_sources) $(cpp_sources)
libv4l2core_la_CFLAGS = $(V4L2CORELIB_CFLAGS)
libv4l2core_la_LIBADD= $(V4L2CORELIB_LIBS)
libv4l2core_la_LDFLAGS= -version-info $(V4L2CORE_LIBRARY_VERSION) -release $(V4L2CORE_API_VERSION) -lpthread


