SUBDIRS = icons
@INTLTOOL_DESKTOP_RULE@

dist_man_MANS = guvcview.1

if DEBIAN_MENU

MENU_IN_FILES= guvcview.in
MENU_FILES= guvcview

$(MENU_FILES): $(MENU_IN_FILES)
	cp $? $@

menudir = ${datadir}/menu
menu_DATA = $(MENU_FILES)

endif

if DESKTOP

DESKTOP_IN_FILES= guvcview.desktop.in
DESKTOP_FILES= $(DESKTOP_IN_FILES:.desktop.in=.desktop)

desktopdir = $(datadir)/applications
desktop_DATA = $(DESKTOP_FILES)

endif

CLEANFILES = \
		$(menu_DATA)\
		$(desktop_DATA)

EXTRA_DIST = \
		$(DESKTOP_IN_FILES)\
		$(MENU_IN_FILES)

clean-generic:
	-test -z "$(CONFIG_CLEAN_FILES)" || rm -f $(CONFIG_CLEAN_FILES); \
	-test -z "$(CLEANFILES)" || rm -f $(CLEANFILES)
