bin_PROGRAMS = \
	mousepad
mousepad_SOURCES = \
	mousepad.h main.c \
	window.h window.c \
	menu.h menu.c \
	callback.h callback.c \
	keyevent.h keyevent.c \
	dnd.h dnd.c \
	dialog.h dialog.c \
	selector.h selector.c \
	file.h file.c \
	encoding.h encoding.c \
	search.h search.c \
	font.h font.c \
	utils.h utils.c \
	undo.h undo.c \
	indent.h indent.c \
	linenum.h linenum.c \
	gtksourceiter.h gtksourceiter.c 
mousepad_CFLAGS = \
	-DLOCALEDIR=\""$(datadir)/locale"\" \
	-DICONDIR=\""$(datadir)/pixmaps"\" \
	@GTK_CFLAGS@ \
	@LIBXFCEGUI4_CFLAGS@
mousepad_LDADD = \
	@GTK_LIBS@ \
	@LIBXFCEGUI4_LIBS@