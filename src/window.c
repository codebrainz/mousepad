/*
 *  window.c
 *  This file is part of Mousepad
 *
 *  Copyright (C) 2004 Tarot Osuji
 *  Copyright (C) 2006 Benedikt Meurer <benny@xfce.org>
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

#include "mousepad.h"

/*static void remove_scrollbar_spacing(GtkScrolledWindow *sw)
{
	GtkScrolledWindowClass *sw_class = GTK_SCROLLED_WINDOW_GET_CLASS(sw);
	
	sw_class->scrollbar_spacing = 0;
}*/

static gboolean cb_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	cb_file_quit(data);
	
	return TRUE;
}
/*
static void cb_scroll_event(GtkAdjustment *adj, GtkWidget *view)
{
	gtk_text_view_place_cursor_onscreen(GTK_TEXT_VIEW(view));
}
*/

/* static void cb_mark_set(GtkTextBuffer *buffer, GtkTextIter *arg1, GtkTextMark *arg2, GtkWidget *menubar) */
static void cb_mark_set(GtkTextBuffer *buffer)
{
/*	static gboolean selected_flag = FALSE;
	gboolean selected;
	
	selected = gtk_text_buffer_get_selection_bounds(buffer, NULL, NULL);
	if (selected != selected_flag) {
		menu_toggle_clipboard_item(selected);
		selected_flag = selected;
	}
*/	menu_toggle_clipboard_item(gtk_text_buffer_get_selection_bounds(buffer, NULL, NULL));

/* g_print("MARK_SET!"); */
}
/*
static void cb_text_receive(GtkClipboard *clipboard, const gchar *text, gpointer data)
{
	menu_toggle_paste_item();
g_print("MARK_SET!");
}
*/
MainWindow *create_main_window(StructData *sd)
{
	GtkWidget *window;
	GtkWidget *vbox;
 	GtkWidget *menubar;
  	GtkWidget *ebox;
 	GtkWidget *label;
 	GtkWidget *separator;
	GtkWidget *sw;
 	GtkWidget *textview;
	GtkTextBuffer *textbuffer;
 	GdkPixbuf *icon;
	
	MainWindow *mainwin = g_malloc(sizeof(MainWindow));
	
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(window), sd->conf.width, sd->conf.height);
	gtk_window_set_title(GTK_WINDOW(window), PACKAGE_NAME);
	icon = gdk_pixbuf_new_from_file(ICONDIR G_DIR_SEPARATOR_S PACKAGE ".png", NULL);
	gtk_window_set_icon(GTK_WINDOW(window), icon);
	g_signal_connect(G_OBJECT(window), "delete-event",
		G_CALLBACK(cb_delete_event), sd);
	
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	
	menubar = create_menu_bar(window, sd);
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);
	
	/* check if we need to add the root warning */
	if (G_UNLIKELY (geteuid () == 0))
	{
		/* install default settings for the root warning text box */
		gtk_rc_parse_string("style\"mousepad-window-root-style\"{bg[NORMAL]=\"#b4254b\"\nfg[NORMAL]=\"#fefefe\"}\n"
				"widget\"GtkWindow.*.root-warning\"style\"mousepad-window-root-style\"\n"
				"widget\"GtkWindow.*.root-warning.GtkLabel\"style\"mousepad-window-root-style\"\n");

		/* add the box for the root warning */
		ebox = gtk_event_box_new();
		gtk_widget_set_name(ebox, "root-warning");
		gtk_box_pack_start(GTK_BOX(vbox), ebox, FALSE, FALSE, 0);

		/* add the label with the root warning */
		label = gtk_label_new(_("Warning, you are using the root account, you may harm your system."));
		gtk_misc_set_padding(GTK_MISC(label), 6, 3);
		gtk_container_add(GTK_CONTAINER(ebox), label);

		separator = gtk_hseparator_new();
		gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 0);
	}

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
/*	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
 *		GTK_SHADOW_IN);
 *	remove_scrollbar_spacing(GTK_SCROLLED_WINDOW(sw));
 */
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	
	textview = gtk_text_view_new();
	gtk_container_add(GTK_CONTAINER(sw), textview);
	
	textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
/*	Following code has possibility of confliction if scroll policy of GTK changed
	GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(sw));
	GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(sw));
	
	g_signal_connect_after(G_OBJECT(hadj), "value-changed",
		G_CALLBACK(cb_scroll_event), textview);
	g_signal_connect_after(G_OBJECT(vadj), "value-changed",
		G_CALLBACK(cb_scroll_event), textview);
*/	
	g_signal_connect(G_OBJECT(textbuffer), "mark-set",
		G_CALLBACK(cb_mark_set), menubar);
	g_signal_connect(G_OBJECT(textbuffer), "mark-deleted",
		G_CALLBACK(cb_mark_set), menubar);
	g_signal_connect(G_OBJECT(window), "focus-in-event",
		G_CALLBACK(menu_toggle_paste_item), NULL);
	g_signal_connect_after(G_OBJECT(textview), "copy-clipboard",
		G_CALLBACK(menu_toggle_paste_item), NULL);
	g_signal_connect_after(G_OBJECT(textview), "cut-clipboard",
		G_CALLBACK(menu_toggle_paste_item), NULL);
/*	gtk_clipboard_request_text(
		gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),
		cb_text_receive, NULL);
*/	
	mainwin->window = window;
	mainwin->menubar = menubar;
	mainwin->textview = textview;
	mainwin->textbuffer = textbuffer;
	
	return mainwin;
}

gchar *get_current_file_basename(gchar *filename)
{
	gchar *basename;
	
	if (filename)
		basename = g_path_get_basename(g_filename_to_utf8(filename, -1, NULL, NULL, NULL));
	else
		basename = g_strdup(_("Untitled"));
	
	return basename;
}

void set_main_window_title(StructData *sd)
{
	gchar *basename, *title;
	
	basename = get_current_file_basename(sd->fi->filename);
	if (sd->fi->filename) {
		if (g_file_test(g_filename_to_utf8(sd->fi->filename, -1, NULL, NULL, NULL),
			G_FILE_TEST_EXISTS))
			title = g_strdup(basename);
		else
			title = g_strdup_printf("(%s)", basename);
	} else
		title = g_strdup_printf("(%s)", basename);
/*		title = g_strdup(basename);
 *		title = g_strdup_printf(PACKAGE_NAME);
 */
	gtk_window_set_title(GTK_WINDOW(sd->mainwin->window), title);
	g_free(title);
	g_free(basename);
}
