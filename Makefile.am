## Process this file with automake to produce Makefile.in

SUBDIRS = src po data

ACLOCAL_AMFLAGS = -I m4

docdir = ${datadir}/doc/guvcview
doc_DATA = \
	README\
	COPYING\
	AUTHORS\
	ChangeLog\
	INSTALL

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update
	
EXTRA_DIST = \
	bootstrap.sh \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in
		
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

update-pot:
	$(MAKE) -C po "$(GETTEXT_PACKAGE).pot"	

DISTCHECK_CONFIGURE_FLAGS = --enable-maintainer-flags=no

DIST_SUBDIRS = ${SUBDIRS}
	
# Copy all the spec files. Of cource, only one is actually used.
dist-hook:
	for specfile in *.spec; do \
		if test -f $$specfile; then \
			cp -p $$specfile $(distdir); \
		fi \
	done

.PHONY: check-gettext update-po update-gmo force-update-gmo
