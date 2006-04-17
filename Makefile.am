# $Id$

SUBDIRS =								\
	po								\
	src

pixmapsdir = $(datadir)/pixmaps
pixmaps_DATA =								\
	mousepad.png

desktopdir = $(datadir)/applications
desktop_in_files = mousepad.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

AUTOMAKE_OPTIONS =							\
	1.8								\
	dist-bzip2

EXTRA_DIST =								\
	intltool-extract.in						\
	intltool-merge.in						\
	intltool-update.in						\
	$(desktop_in_files)						\
	$(pixmaps_DATA)

DISTCLEANFILES =							\
	intltool-extract						\
	intltool-merge							\
	intltool-update							\
	$(desktop_DATA)

update_desktop_database = update-desktop-database $(datadir)/applications

install-data-hook:
	@-if test -z "$(DESTDIR)"; then					\
                echo "Updating desktop database.";			\
                $(update_desktop_database);				\
        else								\
                echo "*** Desktop database not updated. Remember to run:";\
		echo "***";						\
                echo "***   $(update_desktop_database)";		\
		echo "***";						\
        fi

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
