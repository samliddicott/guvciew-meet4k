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
	avilib.c \
	avilib.h \
	callbacks.h \
	callbacks.c \
	close.h \
	close.c \
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
	img_controls.h \
	img_controls.c \
	jdatatype.h \
	jpgenc.h \
	jpgenc.c \
	marker.c \
	mp2.h \
	mp2.c \
	ms_time.h \
	ms_time.c \
	options.h \
	options.c \
	picture.h \
	picture.c \
	profile.h \
	profile.c \
	quant.c \
	readYUV.c \
	sound.h \
	sound.c \
	snd_devices.h \
	snd_devices.c \
	string_utils.h \
	string_utils.c \
	timers.h \
	timers.c \
	utils.h \
	utils.c \
	v4l2_controls.h \
	v4l2_controls.c \
	v4l2_devices.h \
	v4l2_devices.c \
	v4l2_dyna_ctrls.h \
	v4l2_dyna_ctrls.c \
	v4l2_formats.h \
	v4l2_formats.c \
	v4l2uvc.h \
	v4l2uvc.c \
	video.h \
	video.c

guvcview_LDFLAGS = $(LIBINTL)

guvcview_LDADD = $(PACKAGE_LIBS)

localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@



