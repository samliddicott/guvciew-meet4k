## Process this file with automake to produce Makefile.in

EXTRA_CFLAGS = @PULSE_CFLAGS@
EXTRA_LIBS = @PULSE_LIBS@

bin_PROGRAMS = guvcview

guvcview_SOURCES = \
	audio_effects.h \
	audio_effects.c \
	audio_tab.h \
	audio_tab.c \
	autofocus.h \
	autofocus.c \
	acodecs.h \
	acodecs.c \
	avilib.c \
	avilib.h \
	callbacks.h \
	callbacks.c \
	close.h \
	close.c \
	colorspaces.h \
	colorspaces.c \
	create_video.h \
	create_video.c \
	create_image.h \
	create_image.c \
	dct.h \
	dct.c \
	defs.h \
	file_io.h \
	file_io.c \
	globals.h \
	globals.c \
	guvcview.h \
	guvcview.c \
	huffman.h \
	huffman.c \
	img_controls.h \
	img_controls.c \
	image_format.h \
	image_format.c\
	io_stream.h \
	io_stream.c \
	jdatatype.h \
	jpgenc.h \
	jpgenc.c \
	lavc_common.h \
	lavc_common.c \
	matroska.h \
	matroska.c \
	marker.c \
	menubar.h \
	menubar.c \
	ms_time.h \
	ms_time.c \
	options.h \
	options.c \
	osd.h \
	osd.c \
	picture.h \
	picture.c \
	profile.h \
	profile.c \
	port_audio.h \
	port_audio.c \
	pulse_audio.h \
	pulse_audio.c \
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
	uvc_h264.h \
	uvc_h264.c \
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
	vcodecs.h \
	vcodecs.c \
	video.h \
	video.c \
	video_filters.h \
	video_filters.c \
	video_format.h \
	video_format.c \
	video_tab.h \
	video_tab.c

guvcview_CFLAGS = -DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
				  -DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
				  -DPACKAGE_DATA_DIR=\""$(datadir)"\" \
				  $(DEPS_CFLAGS) \
				  $(EXTRA_CFLAGS) \
				  $(PTHREAD_CFLAGS) \
				  -D_REENTRANT \
	 			  -D_FILE_OFFSET_BITS=64 \
	 			  -Wall

guvcview_LDFLAGS = $(LIBINTL)

guvcview_LDADD = $(DEPS_LIBS) $(EXTRA_LIBS) $(PTHREAD_LIBS)

localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@
