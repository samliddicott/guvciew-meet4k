## Process this file with automake to produce Makefile.in

#AM_CFLAGS = 

bin_PROGRAMS = guvcview

guvcview_SOURCES = guvcview.c

guvcview_CFLAGS = $(GUVCVIEW_CFLAGS) \
		  $(PTHREAD_CFLAGS) \
		  -D_REENTRANT\
		  -D_FILE_OFFSET_BITS=64\
		  -Wall\
		  -DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
		  -DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
		  -DPACKAGE_DATA_DIR=\""$(datadir)"\" \
		  $(DEPS_CFLAGS) $(EXTRA_CFLAGS) $(PTHREAD_CFLAGS) \
		  $(EXTRA_CFLAGS) -I$(top_srcdir) -I$(top_srcdir)/includes \
		  -I$(top_srcdir)/v4l2_core
		  
guvcview_LDFLAGS = $(LIBINTL)

guvcview_LDADD = $(GUVCVIEW_LIBS) \
		 $(PTHREAD_LIBS) \
                   ../v4l2_core/$(V4L2CORE_LIBRARY_NAME).la

localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@



