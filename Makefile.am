ACLOCAL_AMFLAGS = -I m4

SUBDIRS =									\
	mousepad                                                        	\
	po

distclean-local:
	rm -rf *.cache *~

desktopdir = $(datadir)/applications
desktop_in_in_files = 								\
	mousepad.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
%.desktop.in: %.desktop.in.in
	sed -e "s,\@libexecdir\@,$(libexecdir),g" < $< > $@
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp			\
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp)			\
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

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

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
