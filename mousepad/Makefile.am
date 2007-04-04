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
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = 									\
	mousepad

mousepad_headers = 								\
	mousepad-types.h

mousepad_built_sources =							\
	mousepad-enum-types.c							\
	mousepad-enum-types.h							\
	mousepad-marshal.c							\
	mousepad-marshal.h

mousepad_SOURCES =								\
	$(mousepad_headers)							\
	$(mousepad_built_sources)						\
	$(mousepad_dbus_sources)						\
	main.c									\
	mousepad-application.c							\
	mousepad-application.h							\
	mousepad-dialogs.c							\
	mousepad-dialogs.h							\
	mousepad-document.c							\
	mousepad-document.h							\
	mousepad-exo.c								\
	mousepad-exo.h								\
	mousepad-file.c								\
	mousepad-file.h								\
	mousepad-preferences.c							\
	mousepad-preferences.h							\
	mousepad-private.h							\
	mousepad-search-bar.c							\
	mousepad-search-bar.h							\
	mousepad-statusbar.c							\
	mousepad-statusbar.h							\
	mousepad-view.c								\
	mousepad-view.h								\
	mousepad-window.c							\
	mousepad-window.h							\
	mousepad-window-ui.h

mousepad_CFLAGS =								\
	$(GTK_CFLAGS)								\
	$(LIBXFCE4UTIL_CFLAGS) \
	$(GMODULE_CFLAGS)							\
	$(GTHREAD_CFLAGS)							\
	$(PLATFORM_CFLAGS)

mousepad_LDFLAGS =								\
	-no-undefined								\
	$(PLATFORM_LDFLAGS)

mousepad_LDADD =								\
	$(GTK_LIBS)								\
	$(LIBXFCE4UTIL_LIBS) \
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

if MAINTAINER_MODE
CLEANFILES =									\
	xgen-mmc								\
	xgen-mmh								\
	xgen-metc								\
	xgen-meth

DISTCLEANFILES =								\
	$(mousepad_built_sources)                                       	\
	stamp-mousepad-marshal.h						\
	stamp-mousepad-enum-types.h						\
	mousepad-window-ui.h

BUILT_SOURCES =									\
	$(mousepad_built_sources)                                       	\
	mousepad-window-ui.h

if HAVE_DBUS
mousepad-dbus-infos.h: $(srcdir)/mousepad-dbus-infos.xml Makefile
	dbus-binding-tool --prefix=mousepad_dbus_service --mode=glib-server $(srcdir)/mousepad-dbus-infos.xml > mousepad-dbus-infos.h
endif

mousepad-window-ui.h: Makefile $(srcdir)/mousepad-window-ui.xml
	exo-csource --static --name=mousepad_window_ui $(srcdir)/mousepad-window-ui.xml > mousepad-window-ui.h

mousepad-enum-types.h: stamp-mousepad-enum-types.h
	@true
stamp-mousepad-enum-types.h: $(mousepad_headers) Makefile
	( cd $(srcdir) && glib-mkenums \
		--fhead "#ifndef __MOUSEPAD_ENUM_TYPES_H__\n#define __MOUSEPAD_ENUM_TYPES_H__\n#include <glib-object.h>\nG_BEGIN_DECLS\n" \
		--fprod "/* enumerations from \"@filename@\" */\n" \
		--vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define MOUSEPAD_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
		--ftail "G_END_DECLS\n\n#endif /* !__MOUSEPAD_ENUM_TYPES_H__ */" \
		$(mousepad_headers) ) >> xgen-meth \
	&& ( cmp -s xgen-meth mousepad-enum-types.h || cp xgen-meth mousepad-enum-types.h ) \
	&& rm -f xgen-meth \
	&& echo timestamp > $(@F)
mousepad-enum-types.c: $(mousepad_headers) Makefile
	( cd $(srcdir) && glib-mkenums \
		--fhead "#include <mousepad/mousepad-enum-types.h>\n#include <mousepad/mousepad-types.h>" \
		--fprod "\n/* enumerations from \"@filename@\" */" \
		--vhead "GType\n@enum_name@_get_type (void)\n{\n\tstatic GType type = 0;\n\tif (type == 0) {\n\tstatic const G@Type@Value values[] = {"\
		--vprod "\t{ @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "\t{ 0, NULL, NULL }\n\t};\n\ttype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n\treturn type;\n}\n" \
		$(mousepad_headers) ) >> xgen-metc \
	&& cp xgen-metc mousepad-enum-types.c \
	&& rm -f xgen-metc

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
