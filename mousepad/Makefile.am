# $Id$

INCLUDES =									\
	-I$(top_builddir)							\
	-I$(top_srcdir)								\
	-DBINDIR=\"$(bindir)\"							\
	-DDATADIR=\"$(datadir)\"						\
	-DLIBDIR=\"$(libdir)\"							\
	-DLIBEXECDIR=\"$(libexecdir)\"						\
	-DG_LOG_DOMAIN=\"Mousepad\"						\
	-DLIBEXECDIR=\"$(libexecdir)\"						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"					\
	-DG_DISABLE_DEPRECATED							\
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = 									\
	mousepad

mousepad_built_sources =							\
	mousepad-marshal.c							\
	mousepad-marshal.h

mousepad_SOURCES =								\
	$(mousepad_built_sources)						\
	$(mousepad_dbus_sources)						\
	main.c									\
	mousepad-application.c							\
	mousepad-application.h							\
	mousepad-dialogs.c							\
	mousepad-dialogs.h							\
	mousepad-document.c							\
	mousepad-document.h							\
	mousepad-encoding-dialog.c						\
	mousepad-encoding-dialog.h						\
	mousepad-file.c								\
	mousepad-file.h								\
	mousepad-preferences.c							\
	mousepad-preferences.h							\
	mousepad-print.c							\
	mousepad-print.h							\
	mousepad-private.h							\
	mousepad-replace-dialog.c						\
	mousepad-replace-dialog.h						\
	mousepad-search-bar.c							\
	mousepad-search-bar.h							\
	mousepad-statusbar.c							\
	mousepad-statusbar.h							\
	mousepad-view.c								\
	mousepad-view.h								\
	mousepad-undo.c								\
	mousepad-undo.h								\
	mousepad-util.c								\
	mousepad-util.h								\
	mousepad-window.c							\
	mousepad-window.h							\
	mousepad-window-ui.h

mousepad_CFLAGS =								\
	$(GTK_CFLAGS)								\
	$(LIBXFCE4UTIL_CFLAGS)							\
	$(GMODULE_CFLAGS)							\
	$(GTHREAD_CFLAGS)							\
	$(PLATFORM_CFLAGS)

mousepad_LDFLAGS =								\
	-no-undefined								\
	$(PLATFORM_LDFLAGS)

mousepad_LDADD =								\
	$(GTK_LIBS)								\
	$(LIBXFCE4UTIL_LIBS)							\
	$(GMODULE_LIBS)								\
	$(GTHREAD_LIBS)

if HAVE_DBUS
mousepad_built_sources +=							\
	mousepad-dbus-infos.h

mousepad_dbus_sources =								\
	mousepad-dbus.c								\
	mousepad-dbus.h

mousepad_CFLAGS +=								\
	-DDBUS_API_SUBJECT_TO_CHANGE						\
	$(DBUS_CFLAGS)

mousepad_LDADD +=								\
	$(DBUS_LIBS)
endif

if HAVE_GTK_UNIX_PRINT
mousepad_CFLAGS +=								\
	$(GTK_UNIX_PRINT_CFLAGS)

mousepad_LDADD +=								\
	$(GTK_UNIX_PRINT_LIBS)
endif

if MAINTAINER_MODE
CLEANFILES =									\
	xgen-mmc								\
	xgen-mmh

DISTCLEANFILES =								\
	$(mousepad_built_sources)                                       	\
	stamp-mousepad-marshal.h						\
	mousepad-window-ui.h

BUILT_SOURCES =									\
	$(mousepad_built_sources)                                       	\
	mousepad-window-ui.h

if HAVE_DBUS
mousepad-dbus-infos.h: $(srcdir)/mousepad-dbus-infos.xml Makefile
	dbus-binding-tool --prefix=mousepad_dbus_service --mode=glib-server $(srcdir)/mousepad-dbus-infos.xml > mousepad-dbus-infos.h
endif

mousepad-window-ui.h: Makefile $(srcdir)/mousepad-window-ui.xml
	exo-csource --strip-comments --strip-content --static --name=mousepad_window_ui $(srcdir)/mousepad-window-ui.xml > mousepad-window-ui.h

mousepad-marshal.h: stamp-mousepad-marshal.h
	@true

stamp-mousepad-marshal.h: mousepad-marshal.list Makefile
	( cd $(srcdir) && glib-genmarshal \
		--prefix=_mousepad_marshal \
		--header mousepad-marshal.list ) >> xgen-mmh \
	&& ( cmp -s xgen-mmh mousepad-marshal.h || cp xgen-mmh mousepad-marshal.h ) \
	&& rm -f xgen-mmh \
	&& echo timestamp > $(@F)

mousepad-marshal.c: mousepad-marshal.list Makefile
	( cd $(srcdir) && glib-genmarshal \
		--prefix=_mousepad_marshal \
		--body mousepad-marshal.list ) >> xgen-mmc \
	&& cp xgen-mmc mousepad-marshal.c \
	&& rm -f xgen-mmc
endif

EXTRA_DIST =									\
	mousepad-dbus-infos.xml							\
	mousepad-marshal.list							\
	mousepad-window-ui.xml

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
