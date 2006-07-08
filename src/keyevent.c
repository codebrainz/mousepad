/*
 *  keyevent.c
 *  This file is part of Leafpad
 *
 *  Copyright (C) 2004 Tarot Osuji
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "keyevent.h"
#include "indent.h"
#include "undo.h"

static guint keyval = 0;
/* static gchar ctrl_key_flag = 0; */
static GdkWindow *gdkwin;

guint keyevent_getval(void)
{
	GdkModifierType mask;
	gchar flag = 0;
	
	gdk_window_get_pointer(gdkwin, NULL, NULL, &mask);
	if (mask &= GDK_CONTROL_MASK)
		flag = 1;
	
	return keyval + 0x10000 * flag;
}

void keyevent_setval(guint val)
{
	keyval = val;
}

static gboolean check_preedit(GtkWidget *text_view)
{
	gchar *str;
	
	gtk_im_context_get_preedit_string(
		GTK_TEXT_VIEW(text_view)->im_context, &str, NULL, NULL);
	if (strlen(str)) {
		g_free(str);
		return TRUE;
	}
	g_free(str);
	return FALSE;
}

static gboolean check_selection_bound(GtkTextBuffer *buffer)
{
	GtkTextIter start_iter, end_iter;
	gchar *str;
	
	if (gtk_text_buffer_get_selection_bounds(buffer, &start_iter, &end_iter)) {
		str = gtk_text_iter_get_text(&start_iter, &end_iter);
		if (g_strrstr(str, "\n")) {
			g_free(str);
			return TRUE;
		}
		g_free(str);
	}
	return FALSE;
}

static gboolean cb_key_press_event(GtkWidget *text_view, GdkEventKey *event)
{
	GtkTextBuffer *buffer =
		gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
	
	if (event->keyval)
		keyval = event->keyval;
	
	switch (event->keyval) {
	case GDK_Return:
		if (check_preedit(text_view))
			return FALSE;
		if ((indent_get_state() && !(event->state &= GDK_SHIFT_MASK)) ||
			(!indent_get_state() && (event->state &= GDK_SHIFT_MASK))) {
			indent_real(text_view);
			return TRUE;
		}
		break;
	case GDK_Tab:
		if (event->state &= GDK_CONTROL_MASK) {
			indent_toggle_tab_width(text_view);
			return TRUE;
		}
	case GDK_ISO_Left_Tab:
		if (event->state &= GDK_SHIFT_MASK) {
			keyval = 0;
			indent_multi_line_unindent(buffer);
		} else if (!check_selection_bound(buffer))
			break;
		else {
			keyval = 0;
			indent_multi_line_indent(buffer);
		}
		return TRUE;
/*	case GDK_Control_L:
	case GDK_Control_R:
		ctrl_key_flag = 1;
		g_print(">>>>ctrl_key_flag = 1\n");
*/	}
	
	return FALSE;
}
/*
static gboolean cb_key_release_event(GtkWidget *text_view, GdkEventKey *event)
{
	switch (event->keyval) {
	case GDK_Control_L:
	case GDK_Control_R:
		ctrl_key_flag = 0;
		g_print("<<<<ctrl_key_flag = 0\n");
	}
	
	return FALSE;
}
*/
static gboolean cb_button_press_event(GtkWidget *text_view, GdkEventButton *event)
{
	keyval =- event->button;
	
	return FALSE;
}

void keyevent_init(GtkWidget *text_view)
{
	gdkwin = gtk_text_view_get_window(GTK_TEXT_VIEW(text_view), GTK_TEXT_WINDOW_WIDGET);
	g_signal_connect(G_OBJECT(text_view), "key-press-event",
		G_CALLBACK(cb_key_press_event), NULL);
#if 0
	g_signal_connect(G_OBJECT(text_view), "key-release-event",
		G_CALLBACK(cb_key_release_event), NULL);
#endif
	g_signal_connect(G_OBJECT(text_view), "button-press-event",
		G_CALLBACK(cb_button_press_event), NULL);
}
