ACLOCAL_AMFLAGS = -I m4

SUBDIRS =									\
	icons									\
	mousepad								\
	po

distclean-local:
	rm -rf *.cache *~

distuninstallcheck_listfiles = 						\
	find . -type f \! -path ./share/icons/hicolor/icon-theme.cache

desktopdir = $(datadir)/applications
desktop_in_in_files = 								\
	mousepad.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
%.desktop.in: %.desktop.in.in
	sed -e "s,\@libexecdir\@,$(libexecdir),g" < $< > $@
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

#
# appdata
#
@INTLTOOL_XML_RULE@
appdatadir = $(datadir)/metainfo
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
appdata_in_files = mousepad.appdata.xml.in

polkit_policydir = $(datadir)/polkit-1/actions
polkit_in_in_files =			\
	org.xfce.mousepad.policy.in.in
polkit_in_files = $(polkit_in_in_files:.policy.in.in=.policy.in)
%.policy.in: %.policy.in.in
	sed -e "s,\@bindir\@,$(bindir),g" < $< > $@
polkit_policy_DATA = $(polkit_in_files:.policy.in=.policy)
@INTLTOOL_POLICY_RULE@


.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp			\
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp)			\
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

EXTRA_DIST =									\
	README.md									\
	intltool-extract.in							\
	intltool-merge.in							\
	intltool-update.in							\
	$(desktop_in_in_files)						\
	$(appdata_in_files)							\
	$(polkit_in_in_files)

DISTCLEANFILES =								\
	README.md									\
	intltool-extract							\
	intltool-merge								\
	intltool-update								\
	$(desktop_in_files)							\
	$(desktop_DATA)								\
	$(appdata_DATA)								\
	$(polkit_in_files)							\
	$(polkit_policy_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
