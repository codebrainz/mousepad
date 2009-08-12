INCLUDES = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-DBINDIR=\"$(bindir)\" \
	-DDATADIR=\"$(datadir)\" \
	-DLIBDIR=\"$(libdir)\" \
	-DLIBEXECDIR=\"$(libexecdir)\" \
	-DG_LOG_DOMAIN=\"Mousepad\" \
	-DLIBEXECDIR=\"$(libexecdir)\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	-DG_DISABLE_DEPRECATED \
	-DGTK_DISABLE_DEPRECATED \
	-DGDK_DISABLE_DEPRECATED \
	-DG_DISABLE_SINGLE_INCLUDES \
	-DPANGO_DISABLE_SINGLE_INCLUDES \
	-DGDK_PIXBUF_DISABLE_SINGLE_INCLUDES \
	-DGTK_DISABLE_SINGLE_INCLUDES \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	mousepad

mousepad_built_sources = \
	mousepad-marshal.c \
	mousepad-marshal.h

mousepad_SOURCES = \
	$(mousepad_built_sources) \
	$(mousepad_dbus_sources) \
	main.c \
	mousepad-application.c \
	mousepad-application.h \
	mousepad-dialogs.c \
	mousepad-dialogs.h \
	mousepad-document.c \
	mousepad-document.h \
	mousepad-encoding.c \
	mousepad-encoding.h \
	mousepad-encoding-dialog.c \
	mousepad-encoding-dialog.h \
	mousepad-file.c \
	mousepad-file.h \
	mousepad-preferences.c \
	mousepad-preferences.h \
	mousepad-print.c \
	mousepad-print.h \
	mousepad-private.h \
	mousepad-replace-dialog.c \
	mousepad-replace-dialog.h \
	mousepad-search-bar.c \
	mousepad-search-bar.h \
	mousepad-statusbar.c \
	mousepad-statusbar.h \
	mousepad-view.c \
	mousepad-view.h \
	mousepad-undo.c \
	mousepad-undo.h \
	mousepad-util.c \
	mousepad-util.h \
	mousepad-window.c \
	mousepad-window.h \
	mousepad-window-ui.h

mousepad_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GTHREAD_CFLAGS) \
	$(PLATFORM_CFLAGS)

mousepad_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

mousepad_LDADD = \
	$(GLIB_LIBS) \
	$(GTK_LIBS)	\
	$(GTHREAD_LIBS)

if HAVE_DBUS
mousepad_built_sources +=	\
	mousepad-dbus-infos.h

mousepad_dbus_sources = \
	mousepad-dbus.c \
	mousepad-dbus.h

mousepad_CFLAGS += \
	-DDBUS_API_SUBJECT_TO_CHANGE \
	$(DBUS_CFLAGS)

mousepad_LDADD +=	\
	$(DBUS_LIBS)
endif

if MAINTAINER_MODE
DISTCLEANFILES = \
	$(mousepad_built_sources) \
	mousepad-window-ui.h

BUILT_SOURCES = \
	$(mousepad_built_sources) \
	mousepad-window-ui.h

if HAVE_DBUS
mousepad-dbus-infos.h: mousepad-dbus-infos.xml Makefile
	dbus-binding-tool --prefix=mousepad_dbus_service --mode=glib-server $< > $@
endif

mousepad-window-ui.h: mousepad-window-ui.xml Makefile
	exo-csource --strip-comments --strip-content --static --name=mousepad_window_ui $< > $@

mousepad-marshal.h: mousepad-marshal.list Makefile
	glib-genmarshal --header --prefix=_mousepad_marshal $< > $@

mousepad-marshal.c: mousepad-marshal.list Makefile
	echo "#include <mousepad/mousepad-marshal.h>" > $@ \
	&& glib-genmarshal --body --prefix=_mousepad_marshal $< >> $@
endif

EXTRA_DIST = \
	mousepad-dbus-infos.xml \
	mousepad-marshal.list \
	mousepad-window-ui.xml

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
