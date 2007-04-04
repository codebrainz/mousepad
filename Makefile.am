# $Id$

SUBDIRS =									\
	icons									\
	mousepad                                                        	\
	po

distclean-local:
	rm -rf *.spec *.cache *~

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz

desktopdir = $(datadir)/applications
desktop_in_in_files = 								\
	Mousepad.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
%.desktop.in: %.desktop.in.in
	sed -e "s,\@libexecdir\@,$(libexecdir),g" < $< > $@
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =									\
	intltool-extract.in							\
	intltool-merge.in							\
	intltool-update.in							\
	$(desktop_in_in_files)

DISTCLEANFILES =								\
	intltool-extract							\
	intltool-merge								\
	intltool-update								\
	$(desktop_in_files)							\
	$(desktop_DATA)

DISTCHECK_CONFIGURE_FLAGS =							\
	--enable-gtk-doc

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
