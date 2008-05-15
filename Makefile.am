## Process this file with automake to produce Makefile.in

SUBDIRS = src po pixmaps

ACLOCAL_AMFLAGS = -I m4

guvcviewdocdir = ${prefix}/doc/guvcview
guvcviewdoc_DATA = \
	README\
	COPYING\
	AUTHORS\
	ChangeLog\
	INSTALL\
	NEWS

applicationdir = ${datadir}/applications

application_DATA = pixmaps/guvcview.desktop

pixmapsdir = ${datadir}/pixmaps/guvcview
pixmaps_DATA = \
	       pixmaps/guvcview.png\
	       pixmaps/camera.xpm\
	       pixmaps/movie.xpm


EXTRA_DIST = \
	     config.rpath\
	     src\
	     $(guvcviewdoc_DATA)\
	     $(application_DATA)\
	     $(pixmaps_DATA)

# Copy all the spec files. Of cource, only one is actually used.
dist-hook:
	for specfile in *.spec; do \
		if test -f $$specfile; then \
			cp -p $$specfile $(distdir); \
		fi \
	done

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

guvcview.desktop: 
	cd pixmaps && $(MAKE) $(AM_MAKEFLAGS) guvcview.desktop

guvcview.png: 
	cd pixmaps && $(MAKE) $(AM_MAKEFLAGS) guvcview.png

camera.xpm: 
	cd pixmaps && $(MAKE) $(AM_MAKEFLAGS) camera.xpm

movie.xpm: 
	cd pixmaps && $(MAKE) $(AM_MAKEFLAGS) movie.xpm


.PHONY: check-gettext update-po update-gmo force-update-gmo

