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
