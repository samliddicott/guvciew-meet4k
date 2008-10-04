## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	$(PACKAGE_CFLAGS)  $(JPEG_CFLAGS)

AM_CFLAGS =\
	 -Wall\
	 -g\
	 -D_REENTRANT

bin_PROGRAMS = guvcview

guvcview_SOURCES = \
	autofocus.h \
	autofocus.c \
	avilib.c                  \
	avilib.h                  \
	colorspaces.h \
	colorspaces.c \
	dct.h \
	dct.c \
	defs.h \
	globals.h \
	globals.c \
	guvcview.h \
	guvcview.c \
	huffman.h \
	huffman.c \
	jdatatype.h \
	jpgenc.h \
	jpgenc.c \
	marker.c \
	ms_time.h \
	ms_time.c \
	options.h \
	options.c \
	picture.h \
	picture.c \
	quant.c \
	readYUV.c \
	sound.h \
	sound.c \
	string_utils.h \
	string_utils.c \
	utils.h \
	utils.c \
	v4l2uvc.h \
	v4l2uvc.c \
	video.h \
	video.c

guvcview_LDFLAGS = $(LIBINTL)

guvcview_LDADD = $(PACKAGE_LIBS)

localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@



