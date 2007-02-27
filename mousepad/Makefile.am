# $Id$

INCLUDES =								\
	-I$(top_builddir)						\
	-I$(top_srcdir)							\
	-DBINDIR=\"$(bindir)\"						\
	-DDATADIR=\"$(datadir)\"					\
	-DLIBDIR=\"$(libdir)\"						\
	-DLIBEXECDIR=\"$(libexecdir)\"					\
	-DG_LOG_DOMAIN=\"Mousepad\"					\
	-DLIBEXECDIR=\"$(libexecdir)\"					\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = 								\
	mousepad

mousepad_SOURCES =							\
	main.c								\
	mousepad-application.c						\
	mousepad-application.h						\
	mousepad-file.c							\
	mousepad-file.h							\
	mousepad-preferences.c						\
	mousepad-preferences.h						\
	mousepad-private.h						\
	mousepad-screen.c						\
	mousepad-screen.h						\
	mousepad-tab-label.c						\
	mousepad-tab-label.h						\
	mousepad-view.c							\
	mousepad-view.h							\
	mousepad-window.c						\
	mousepad-window.h						\
	mousepad-window-ui.h

mousepad_CFLAGS =							\
	$(EXO_CFLAGS)							\
	$(GTHREAD_CFLAGS)						\
	$(PLATFORM_CFLAGS)

mousepad_LDFLAGS =							\
	-no-undefined							\
	$(LIBSM_LDFLAGS)						\
	$(PLATFORM_LDFLAGS)

mousepad_LDADD =							\
	$(EXO_LIBS)							\
	$(GTHREAD_LIBS)

if HAVE_DBUS
mousepad_build_sources =						\
	mousepad-dbus-infos.h

mousepad_SOURCES +=							\
	mousepad-dbus.c							\
	mousepad-dbus.h

mousepad_CFLAGS +=							\
	-DDBUS_API_SUBJECT_TO_CHANGE					\
	$(DBUS_CFLAGS)

mousepad_LDADD +=							\
	$(DBUS_LIBS)
endif

if MAINTAINER_MODE
DISTCLEANFILES =							\
	mousepad-window-ui.h

BUILT_SOURCES =								\
	$(mousepad_build_sources)                                       \
	mousepad-window-ui.h

if HAVE_DBUS
mousepad-dbus-infos.h: $(srcdir)/mousepad-dbus-infos.xml Makefile
	dbus-binding-tool --prefix=mousepad_dbus_service --mode=glib-server $(srcdir)/mousepad-dbus-infos.xml > mousepad-dbus-infos.h
endif

mousepad-window-ui.h: Makefile $(srcdir)/mousepad-window-ui.xml
	exo-csource --static --name=mousepad_window_ui $(srcdir)/mousepad-window-ui.xml > mousepad-window-ui.h
endif

EXTRA_DIST =								\
	mousepad-dbus-infos.xml                                         \
	mousepad-window-ui.xml

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
