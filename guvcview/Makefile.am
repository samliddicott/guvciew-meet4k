## Process this file with automake to produce Makefile.in

INCLUDES = \
           -DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
           -DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
           -DPACKAGE_DATA_DIR=\""$(datadir)"\" \
           $(EXTRA_CFLAGS) -I$(top_srcdir) -I$(top_srcdir)/includes \
	   -I$(top_srcdir)/v4l2_core \
           -D__STDC_CONSTANT_MACROS

AM_CFLAGS = \
            -D_REENTRANT\
            -Wall

bin_PROGRAMS = guvcview

h_sources =

c_sources = guvcview.c

guvcview_SOURCES = $(h_sources) $(c_sources)

guvcview_CFLAGS = $(GUVCVIEW_APP_CFLAGS)

guvcview_LDFLAGS = $(LIBINTL)

guvcview_LDADD = $(GUVCVIEW_APP_LIBS) \
                   ../v4l2_core/$(V4L2CORE_LIBRARY_NAME).la

localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@



