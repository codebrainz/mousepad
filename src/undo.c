/*
 *  undo.c
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

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "keyevent.h"
#include "undo.h"

#define DV(x)

typedef struct {
	gint command;
	gint start;
	gint end;
	gboolean seq; // sequency flag
	gchar *str;
} UndoInfo;

enum {
	BS = 0,
	DEL,
	INS
};

static GtkWidget *undo_menu_item = NULL;
static GtkWidget *redo_menu_item = NULL;

static GList *undo_list = NULL;
static GList *redo_list = NULL;
static GString *undo_gstr;
static UndoInfo *ui_tmp;
static gint step_modif;
static GtkWidget *view;
static gboolean overwrite_mode = FALSE;
static guint keyval;
static guint prev_keyval;//, keyevent_setval(0);

static void cb_toggle_overwrite(void)
{
	overwrite_mode =! overwrite_mode;
DV(g_print("toggle-overwrite: %d\n", overwrite_mode));
}

static void undo_clear_undo_info(void)
{
	while (g_list_length(undo_list)) {
		g_free(((UndoInfo *)undo_list->data)->str);
		g_free(undo_list->data);
		undo_list = g_list_delete_link(undo_list, undo_list);
	}
}

static void undo_clear_redo_info(void)
{
	while (g_list_length(redo_list)) {
		g_free(((UndoInfo *)redo_list->data)->str);
		g_free(redo_list->data);
		redo_list = g_list_delete_link(redo_list, redo_list);
	}
}

static void undo_append_undo_info(GtkTextBuffer *buffer, gint command, gint start, gint end, gchar *str)
{
	UndoInfo *ui = g_malloc(sizeof(UndoInfo));
	
	ui->command = command;
	ui->start = start;
	ui->end = end;
	ui->seq = FALSE;
	ui->str = str;
	undo_list = g_list_append(undo_list, ui);
DV(g_print("undo_cb: %d %s (%d-%d)\n", command, str, start, end));
}

static void undo_create_undo_info(GtkTextBuffer *buffer, gint command, gint start, gint end)
{
	GtkTextIter start_iter, end_iter;
	gboolean seq_flag = FALSE;
	gchar *str;
	
	gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, start);
	gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, end);
	str = gtk_text_buffer_get_text(buffer, &start_iter, &end_iter, FALSE);
	
	if (undo_gstr->len) {
		if (end - start == 1 && command == ui_tmp->command) {
			switch (keyval) {
			case GDK_BackSpace:
				if (end == ui_tmp->start)
					seq_flag = TRUE;
				break;
			case GDK_Delete:
				if (start == ui_tmp->start)
					seq_flag = TRUE;
				break;
		/*	case GDK_Return:
				if (start == ui_tmp->end) {
					if (prev_keyval == GDK_Return)
						if (!g_strstr_len(undo_gstr->str, 1, "\n"))
							break;
					seq_flag = TRUE;
				}
				break; */
			case GDK_Tab:
			case GDK_space:
				if (start == ui_tmp->end)
					seq_flag = TRUE;
				break;
			default:
				if (start == ui_tmp->end)
					if (keyval && keyval < 0xF000)
						switch (prev_keyval) {
						case GDK_Return:
						case GDK_Tab:
						case GDK_space:
							break;
						default:
							seq_flag = TRUE;
						}
			}
		}
		if (seq_flag) {
			switch (command) {
			case BS:
				undo_gstr = g_string_prepend(undo_gstr, str);
				ui_tmp->start--;
				break;
			default:
				undo_gstr = g_string_append(undo_gstr, str);
				ui_tmp->end++;
			}
			undo_clear_redo_info();
			prev_keyval = keyval;
			keyevent_setval(0);
			gtk_widget_set_sensitive(undo_menu_item, TRUE);
			gtk_widget_set_sensitive(redo_menu_item, FALSE);
			return;
		}
		undo_append_undo_info(buffer, ui_tmp->command, ui_tmp->start, ui_tmp->end, g_strdup(undo_gstr->str));
		undo_gstr = g_string_erase(undo_gstr, 0, -1);
	}
	
	if (end - start == 1 &&
		((keyval && keyval < 0xF000) ||
		  keyval == GDK_BackSpace || keyval == GDK_Delete || keyval == GDK_Tab)) {
		ui_tmp->command = command;
		ui_tmp->start = start;
		ui_tmp->end = end;
		undo_gstr = g_string_erase(undo_gstr, 0, -1);
		g_string_append(undo_gstr, str);
	} else 
		undo_append_undo_info(buffer, command, start, end, g_strdup(str));
	
	undo_clear_redo_info();
	prev_keyval = keyval;
	keyevent_setval(0);
	gtk_widget_set_sensitive(undo_menu_item, TRUE);
	gtk_widget_set_sensitive(redo_menu_item, FALSE);
}

static void cb_delete_range(GtkTextBuffer *buffer, GtkTextIter *start_iter, GtkTextIter *end_iter)
{
	gint start, end;
	gint command;
	
	keyval = keyevent_getval();
	start = gtk_text_iter_get_offset(start_iter);
	end = gtk_text_iter_get_offset(end_iter);
	
DV(g_print("delete-range: keyval = 0x%x\n", keyval));
	
	if (!keyval && prev_keyval)
		undo_set_sequency(TRUE);
	if (keyval == 0x10000 && prev_keyval > 0x10000) // for Ctrl+V overwrite
		undo_set_sequency(TRUE);
	if (keyval == GDK_BackSpace)
		command = BS;
	else
		command = DEL;
	undo_create_undo_info(buffer, command, start, end);
}

static void cb_insert_text(GtkTextBuffer *buffer, GtkTextIter *iter, gchar *str)
{
	gint start, end;
	
	keyval = keyevent_getval();
	end = gtk_text_iter_get_offset(iter);
	start = end - g_utf8_strlen(str, -1);
	
DV(g_print("insert-text: keyval = 0x%x\n", keyevent_getval()));
	
	if (!keyval && prev_keyval)
		undo_set_sequency(TRUE);
//	if (keyval == 0x10000 && prev_keyval > 0x10000) // don't need probably
//		undo_set_sequency(TRUE);
	undo_create_undo_info(buffer, INS, start, end);
}

static void set_main_window_title_with_asterisk(gboolean flag)
{
	const gchar *old_title =
		gtk_window_get_title(GTK_WINDOW(gtk_widget_get_toplevel(view)));
	gchar *new_title;
	
	if (flag)
		new_title = g_strconcat("*", old_title, NULL);
	else {
		GString *gstr = g_string_new(old_title);
		gstr = g_string_erase(gstr, 0, 1);
		new_title = g_string_free(gstr, FALSE);
	}
	gtk_window_set_title(GTK_WINDOW(gtk_widget_get_toplevel(view)), new_title);
	g_free(new_title);
}

static void cb_modified_changed(void)
{
	set_main_window_title_with_asterisk(TRUE);
}

void undo_reset_step_modif(void)
{
	step_modif = g_list_length(undo_list);
DV(g_print("undo_reset_step_modif: Reseted step_modif by %d\n", step_modif));
}

static void undo_check_step_modif(GtkTextBuffer *buffer)
{
	if (g_list_length(undo_list) == step_modif) {
		g_signal_handlers_block_by_func(G_OBJECT(buffer), G_CALLBACK(cb_modified_changed), NULL);
		gtk_text_buffer_set_modified(buffer, FALSE);
		g_signal_handlers_unblock_by_func(G_OBJECT(buffer), G_CALLBACK(cb_modified_changed), NULL);
		set_main_window_title_with_asterisk(FALSE);
	}
}

static gint undo_connect_signal(GtkTextBuffer *buffer)
{
	g_signal_connect(G_OBJECT(buffer), "delete-range",
		G_CALLBACK(cb_delete_range), buffer);
	g_signal_connect_after(G_OBJECT(buffer), "insert-text",
		G_CALLBACK(cb_insert_text), buffer);
	return 
	g_signal_connect(G_OBJECT(buffer), "modified-changed",
		G_CALLBACK(cb_modified_changed), NULL);
}

void undo_init(GtkWidget *textview, GtkTextBuffer *buffer, GtkWidget *menubar)
{
	GtkItemFactory *ifactory;
	static guint init_flag = 0; // TODO: divide to undo_clear()
	
	if (undo_list)
		undo_clear_undo_info();
	if (redo_list)
		undo_clear_redo_info();
	undo_reset_step_modif();
DV(g_print("undo_init: list reseted\n"));
	
	if (!undo_menu_item) {
		ifactory = gtk_item_factory_from_widget(menubar);
		undo_menu_item = gtk_item_factory_get_widget(ifactory, "/Edit/Undo");
		redo_menu_item = gtk_item_factory_get_widget(ifactory, "/Edit/Redo");
	}
	gtk_widget_set_sensitive(undo_menu_item, FALSE);
	gtk_widget_set_sensitive(redo_menu_item, FALSE);
	
	if (!init_flag) {
		g_signal_connect(G_OBJECT(textview), "toggle-overwrite",
			G_CALLBACK(cb_toggle_overwrite), NULL);
		view = textview;
		init_flag = undo_connect_signal(buffer);
		keyevent_setval(0);
		ui_tmp = g_malloc(sizeof(UndoInfo));
		//ui_tmp->str = g_strdup("");
		undo_gstr = g_string_new("");
	}
	ui_tmp->command = INS;
	undo_gstr = g_string_erase(undo_gstr, 0, -1);
}

gint undo_block_signal(GtkTextBuffer *buffer)
{
	return 
	g_signal_handlers_block_by_func(G_OBJECT(buffer), G_CALLBACK(cb_delete_range), buffer) +
//	g_signal_handlers_block_by_func(G_OBJECT(buffer), G_CALLBACK(cb_modified_changed), buffer) +
	g_signal_handlers_block_by_func(G_OBJECT(buffer), G_CALLBACK(cb_insert_text), buffer);
}

gint undo_unblock_signal(GtkTextBuffer *buffer)
{
	return 
	g_signal_handlers_unblock_by_func(G_OBJECT(buffer), G_CALLBACK(cb_delete_range), buffer) +
//	g_signal_handlers_unblock_by_func(G_OBJECT(buffer), G_CALLBACK(cb_modified_changed), buffer) +
	g_signal_handlers_unblock_by_func(G_OBJECT(buffer), G_CALLBACK(cb_insert_text), buffer);
}

gint undo_disconnect_signal(GtkTextBuffer *buffer) //must be not needed
{
	return 
	g_signal_handlers_disconnect_by_func(G_OBJECT(buffer), G_CALLBACK(cb_delete_range), buffer) +
//	g_signal_handlers_disconnect_by_func(G_OBJECT(buffer), G_CALLBACK(cb_modified_changed), buffer) +
	g_signal_handlers_disconnect_by_func(G_OBJECT(buffer), G_CALLBACK(cb_insert_text), buffer);
}

void undo_set_sequency(gboolean seq)
{
	UndoInfo *ui;
	
	if (g_list_length(undo_list)) {
		ui = g_list_last(undo_list)->data;
		ui->seq = seq;
	}
DV(g_print("<undo_set_sequency: %d>\n", seq));	
}

gboolean undo_undo(GtkTextBuffer *buffer)
{
	GtkTextIter start_iter, end_iter;
	UndoInfo *ui;
	
	if (undo_gstr->len) {
		undo_append_undo_info(buffer, ui_tmp->command, ui_tmp->start, ui_tmp->end, g_strdup(undo_gstr->str));
		undo_gstr = g_string_erase(undo_gstr, 0, -1);
	}
	if (g_list_length(undo_list)) {
		undo_block_signal(buffer);
		ui = g_list_last(undo_list)->data;
		gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, ui->start);
		switch (ui->command) {
		case INS:
			gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, ui->end);
			gtk_text_buffer_delete(buffer, &start_iter, &end_iter);
			break;
		default:
			gtk_text_buffer_insert(buffer, &start_iter, ui->str, -1);
		}
		redo_list = g_list_append(redo_list, ui);
		undo_list = g_list_delete_link(undo_list, g_list_last(undo_list));
DV(g_print("cb_edit_undo: undo list left %d\n", g_list_length(undo_list)));
		undo_unblock_signal(buffer);
		undo_check_step_modif(buffer);
		if (g_list_length(undo_list)) {
			if (((UndoInfo *)g_list_last(undo_list)->data)->seq)
				return TRUE;
		} else
			gtk_widget_set_sensitive(undo_menu_item, FALSE);
		gtk_widget_set_sensitive(redo_menu_item, TRUE);
		if (ui->command == DEL)
			gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, ui->start);
		gtk_text_buffer_place_cursor(buffer, &start_iter);
		gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(view), &start_iter,
			0.1, FALSE, 0.5, 0.5);
	}
	return FALSE;
}

gboolean undo_redo(GtkTextBuffer *buffer)
{
	GtkTextIter start_iter, end_iter;
	UndoInfo *ri;
	
	if (g_list_length(redo_list)) {
		undo_block_signal(buffer);
		ri = g_list_last(redo_list)->data;
		gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, ri->start);
		switch (ri->command) {
		case INS:
			gtk_text_buffer_insert(buffer, &start_iter, ri->str, -1);
			break;
		default:
			gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, ri->end);
			gtk_text_buffer_delete(buffer, &start_iter, &end_iter);
		}
		undo_list = g_list_append(undo_list, ri);
		redo_list = g_list_delete_link(redo_list, g_list_last(redo_list));
DV(g_print("cb_edit_redo: redo list left %d\n", g_list_length(redo_list)));
		undo_unblock_signal(buffer);
		undo_check_step_modif(buffer);
		if (ri->seq)
			return TRUE;
		if (!g_list_length(redo_list))
			gtk_widget_set_sensitive(redo_menu_item, FALSE);
		gtk_widget_set_sensitive(undo_menu_item, TRUE);
		gtk_text_buffer_place_cursor(buffer, &start_iter);
		gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(view), &start_iter,
			0.1, FALSE, 0.5, 0.5);
	}
	return FALSE;
}
