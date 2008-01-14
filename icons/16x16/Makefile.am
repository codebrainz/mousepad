# $Id$

iconsdir = $(datadir)/icons/hicolor/16x16/apps
icons_DATA =								\
	Mousepad.png

# install symlink
install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(iconsdir)
	-( cd $(DESTDIR)$(iconsdir) ; ln -sf Mousepad.png accessories-text-editor.png )

EXTRA_DIST =								\
	$(icons_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
