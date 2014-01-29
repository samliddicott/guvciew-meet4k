## Process this file with automake to produce Makefile.in

SUBDIRS = v4l2_core \
          guvcview \
          po

#Distribute these directories:
DIST_SUBDIRS =  v4l2_core \
		guvcview \
		po

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = pkgconfig/v4l2core-0.1.pc

ACLOCAL_AMFLAGS = -I m4

#pixmapsdir = ${datadir}/pixmaps/gvideo

#pixmaps_DATA = \
#		pixmaps/gvideo.png\
#		pixmaps/camera.png\
#		pixmaps/movie.png\
#		pixmaps/save.png\
#		pixmaps/image_controls.png\
#		pixmaps/video_controls.png\
#		pixmaps/audio_controls.png\
#		pixmaps/images_folder.png\
#		pixmaps/videos_folder.png\
#		pixmaps/controls_folder.png\
#		pixmaps/close.png


EXTRA_DIST = bootstrap.sh

check-gettext:
	@if test x$(USE_NLS) != "xyes" ; then echo "Missing gettext. Rerun configure and check for" \
	"'checking whether to use NLS... yes'!" ; exit 1 ; fi

update-po: check-gettext
	@find $(srcdir)/src/ -name "*.cpp" -print | sort > $(srcdir)/po/POTFILES.in.2 ; \
	if diff $(srcdir)/po/POTFILES.in $(srcdir)/po/POTFILES.in.2 >/dev/null 2>&1 ; then \
		rm -f $(srcdir)/po/POTFILES.in.2 ; \
	else \
		mv $(srcdir)/po/POTFILES.in.2 $(srcdir)/po/POTFILES.in ; \
	fi
	cd po && $(MAKE) $(AM_MAKEFLAGS) update-po

update-gmo: check-gettext
	cd po && $(MAKE) $(AM_MAKEFLAGS) update-gmo

force-update-gmo: check-gettext
	touch po/*.po
	cd po && $(MAKE) $(AM_MAKEFLAGS) update-gmo

#$(gvideoman_DATA):
#	cd pixmaps && $(MAKE) $(AM_MAKEFLAGS) .manpage

#$(pixmaps_DATA):
#	cd pixmaps && $(MAKE) $(AM_MAKEFLAGS) .png


# Copy all the spec files. Of cource, only one is actually used.
dist-hook:
	for specfile in *.spec; do \
		if test -f $$specfile; then \
			cp -p $$specfile $(distdir); \
		fi \
	done

.PHONY: check-gettext update-po update-gmo force-update-gmo
