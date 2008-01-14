# $Id$

iconsdir = $(datadir)/icons/hicolor/scalable/apps
icons_DATA =								\
	Mousepad.svg

# install symlink
install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(iconsdir)
	-( cd $(DESTDIR)$(iconsdir) ; ln -sf Mousepad.svg accessories-text-editor.svg )

EXTRA_DIST =								\
	$(icons_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
