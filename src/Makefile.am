## Process this file with automake to produce Makefile.in

## Created by Anjuta

gladedir = $(datadir)/guvcview/glade
INCLUDES = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	$(PACKAGE_CFLAGS)  $(JPEG_CFLAGS)

AM_CFLAGS =\
	 -Wall\
	 -g

bin_PROGRAMS = guvcview

guvcview_SOURCES = \
	acconfig.h\
	avilib.c                  \
	avilib.h                  \
	guvcview.c                  \
	huffman.h                  \
	utils.c                  \
	utils.h                  \
	v4l2uvc.c                  \
	v4l2uvc.h                  \
	datatype.h	\
	dct.c	\
	encoder.c	\
	huffdata.h	\
	huffman.c	\
	jdatatype.h	\
	marker.c	\
	prototype.h	\
	quant.c	\
	readYUV.c	

guvcview_LDFLAGS =

guvcview_LDADD = $(PACKAGE_LIBS)

EXTRA_DIST = $(glade_DATA)
