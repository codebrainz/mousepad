/*
 *  callback.c
 *  This file is part of Mousepad
 *
 *  Copyright (C) 2006 Benedikt Meurer <benny@xfce.org>
 *  Copyright (C) 2005 Erik Harrison
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

#include "mousepad.h"
#include <gtk/gtk.h>
#include <glib.h>

static gint check_text_modification(StructData *sd)
{
	gchar *basename, *str;
	gint res;
	
	GtkTextBuffer *textbuffer = sd->mainwin->textbuffer;
	
	if (gtk_text_buffer_get_modified(textbuffer)) {
		basename = get_current_file_basename(sd->fi->filename);
		str = g_strdup_printf(_("Save changes to '%s'?"), basename);
		res = run_dialog_message_question(sd->mainwin->window, str);
		g_free(str);
		g_free(basename);
		switch (res) {
		case GTK_RESPONSE_NO:
			return 0;
		case GTK_RESPONSE_YES:
			if (!cb_file_save(sd))
				return 0;
		}
		return -1;
	}
	
	return 0;
}

static void set_text_selection_bound(GtkTextBuffer *buffer, gint start, gint end)
{
	GtkTextIter start_iter;
	GtkTextIter end_iter;
	
	gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, start);
	if (end < 0)
		gtk_text_buffer_get_end_iter(buffer, &end_iter);
	else
		gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, end);
	gtk_text_buffer_place_cursor(buffer, &end_iter);
	gtk_text_buffer_move_mark_by_name(buffer, "selection_bound", &start_iter);
}

void cb_file_new(StructData *sd)
{
	GtkTextBuffer *textbuffer = sd->mainwin->textbuffer;
	
	if (!check_text_modification(sd)) {
		undo_block_signal(textbuffer);
		gtk_text_buffer_set_text(textbuffer, "", 0);
		gtk_text_buffer_set_modified(textbuffer, FALSE);
		if (sd->fi->filename)
			g_free(sd->fi->filename);
		sd->fi->filename = NULL;
		if (sd->fi->charset)
			g_free(sd->fi->charset);
		sd->fi->charset = NULL;
		sd->fi->lineend = LF;
		set_main_window_title(sd);
		undo_unblock_signal(textbuffer);
		undo_init(sd->mainwin->textview, textbuffer, sd->mainwin->menubar);
	}
}

void cb_file_new_window(StructData *sd)
{
	g_spawn_command_line_async(PACKAGE, NULL);
}

void cb_file_open(StructData *sd)
{
	FileInfo *fi;
	
	if (!check_text_modification(sd)) {
/*		fi = get_file_info_by_selector(OPEN, sd->fi); */
		fi = get_fileinfo_from_chooser(sd->mainwin->window, sd->fi, OPEN);
		if (fi) {
			if (file_open_real(sd->mainwin->textview, fi)) {
				g_free(fi);
			} else {
				g_free(sd->fi);
				sd->fi = fi;
				set_main_window_title(sd);
				undo_init(sd->mainwin->textview, sd->mainwin->textbuffer, sd->mainwin->menubar);
			}
		}
	}
}

#if GTK_CHECK_VERSION(2,10,0)
void cb_file_open_recent(StructData *sd, GtkRecentChooser *chooser)
{
  FileInfo *fi;
  gchar *uri;

  uri = gtk_recent_chooser_get_current_uri(chooser);
  if (G_LIKELY(uri != NULL)) {
    fi = g_new0(FileInfo, 1);
    fi->lineend = sd->fi->lineend;
    fi->charset = g_strdup(sd->fi->charset);
    fi->manual_charset = g_strdup(sd->fi->manual_charset);
    fi->filename = g_filename_from_uri(uri, NULL, NULL);
    if (G_LIKELY(fi->filename != NULL)) {
			if (file_open_real(sd->mainwin->textview, fi)) {
				g_free(fi);
			} else {
				g_free(sd->fi);
				sd->fi = fi;
				set_main_window_title(sd);
				undo_init(sd->mainwin->textview, sd->mainwin->textbuffer, sd->mainwin->menubar);
			}
    }
    g_free(uri);
  }
}
#endif

gint cb_file_save(StructData *sd)
{
	if (sd->fi->filename == NULL)
		return cb_file_save_as(sd);
	if (!file_save_real(sd->mainwin->textview, sd->fi)) {
		set_main_window_title(sd);
		undo_reset_step_modif();
		return 0;
	}
	return -1;
}

gint cb_file_save_as(StructData *sd)
{
	FileInfo *fi;

	g_return_val_if_fail (sd != NULL, -1);
	fi = get_fileinfo_from_chooser(sd->mainwin->window, sd->fi, SAVE);
	if (fi) {
		if (file_save_real(sd->mainwin->textview, fi))
			g_free(fi);
		else {
			g_free(sd->fi);
			sd->fi = fi;
			set_main_window_title(sd);
			undo_reset_step_modif();
			return 0;
		}
	}
	return -1;
}

gint cb_file_print(StructData *sd)
{
	GtkTextBuffer *textbuffer;
	GtkTextIter start, end;
	gchar       *str;
	gsize       rbytes, wbytes;

	gint   xfprint_pipe;
	GPid   child_pid;
	GError *pipe_err = NULL;
	GError *conv_err = NULL;
	struct stat std_in_stat;
	gchar  *child_argv[3];

	child_argv[0] = "xfprint4";
	child_argv[1] = NULL;
	child_argv[2] = NULL;

	textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(sd->mainwin->textview));

	gtk_text_buffer_get_start_iter(textbuffer, &start);
	gtk_text_buffer_get_end_iter(textbuffer, &end);	
	str = gtk_text_buffer_get_text(textbuffer, &start, &end, TRUE);
	
	switch (sd->fi->lineend) {
	case CR:
		convert_line_ending(&str, CR);
		break;
	case CR+LF:
		convert_line_ending(&str, CR+LF);
	}

	if (!sd->fi->charset)
		sd->fi->charset = g_strdup(get_default_charset());
	str = g_convert(str, -1, sd->fi->charset, "UTF-8", &rbytes, &wbytes, &conv_err);
	if (conv_err) {
		switch (conv_err->code) {
		case G_CONVERT_ERROR_ILLEGAL_SEQUENCE:
			run_dialog_message(gtk_widget_get_toplevel(sd->mainwin->textview),
				GTK_MESSAGE_ERROR, _("Can't convert codeset to '%s'"), sd->fi->charset);
			break;
		default:
			run_dialog_message(gtk_widget_get_toplevel(sd->mainwin->textview),
				GTK_MESSAGE_ERROR, conv_err->message);
		}
		g_error_free(conv_err);
		return -1;
	}

	/* Silly xfprint4 bug, for xfprint 4.2.0, means we can't just pipe to it directly
	 * We need to use /dev/stdin, which we first need to verify is either a char device
	 * or a fifo (it is rumored at least that this is a fifo on Solaris)
	 */
	stat("/dev/stdin", &std_in_stat);
	if ((S_ISCHR(std_in_stat.st_mode)) || (S_ISFIFO(std_in_stat.st_mode))) {
		child_argv[1] = "/dev/stdin";
	}

	g_spawn_async_with_pipes (NULL, child_argv, 
	                          NULL, (G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH), 
	                          NULL, NULL, 
	                          &child_pid, 
	                          &xfprint_pipe, NULL, 
	                          NULL, &pipe_err);
	if (pipe_err) {
		run_dialog_message(gtk_widget_get_toplevel(sd->mainwin->textview),
			GTK_MESSAGE_ERROR, _("Can't open pipe to process"));
		g_error_free(pipe_err);
		return -1;
	}

	gtk_widget_set_sensitive(sd->mainwin->window, FALSE);

	write((int) xfprint_pipe, str, g_utf8_strlen(str, -1));
	close((int) xfprint_pipe);

	while (!waitpid(child_pid, NULL, WNOHANG)) {
		gtk_main_iteration();
	}

	gtk_widget_set_sensitive(sd->mainwin->window, TRUE);
	g_spawn_close_pid(child_pid);
	return 0;
}

void cb_file_quit(StructData *sd)
{
	if (!check_text_modification(sd))
		gtk_main_quit();
}

void cb_edit_undo(StructData *sd)
{
	undo_undo(sd->mainwin->textbuffer);
}

void cb_edit_redo(StructData *sd)
{
	undo_redo(sd->mainwin->textbuffer);
}

void cb_edit_cut(StructData *sd)
{
	gtk_text_buffer_cut_clipboard(sd->mainwin->textbuffer,
		gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), TRUE);
	menu_toggle_paste_item(); /* TODO: remove this line */
}

void cb_edit_copy(StructData *sd)
{
	gtk_text_buffer_copy_clipboard(sd->mainwin->textbuffer,
		gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
	menu_toggle_paste_item(); /* TODO: remove this line */
}

void cb_edit_paste(StructData *sd)
{
	gtk_text_buffer_paste_clipboard(sd->mainwin->textbuffer,
		gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), NULL, TRUE);
	gtk_text_view_scroll_mark_onscreen(
		GTK_TEXT_VIEW(sd->mainwin->textview),
		gtk_text_buffer_get_insert(sd->mainwin->textbuffer));
}

void cb_edit_delete(StructData *sd)
{
	gtk_text_buffer_delete_selection(sd->mainwin->textbuffer,
		TRUE, TRUE);
}

void cb_edit_select_all(StructData *sd)
{
	set_text_selection_bound(sd->mainwin->textbuffer, 0, -1);
}

static void activate_quick_find(StructData *sd)
{
	GtkItemFactory *ifactory;
	static gboolean flag = FALSE;
	
	if (!flag) {
		ifactory = gtk_item_factory_from_widget(sd->mainwin->menubar);
		gtk_widget_set_sensitive(
			gtk_item_factory_get_widget(ifactory, "/Search/Find Next"),
			TRUE);
		gtk_widget_set_sensitive(
			gtk_item_factory_get_widget(ifactory, "/Search/Find Previous"),
			TRUE);
		flag = TRUE;
	}
}
	
void cb_search_find(StructData *sd)
{
	if (run_dialog_find(sd) == GTK_RESPONSE_OK)
		activate_quick_find(sd);
}

void cb_search_find_next(StructData *sd)
{
	document_search_real(sd, 1);
}

void cb_search_find_prev(StructData *sd)
{
	document_search_real(sd, -1);
}

void cb_search_replace(StructData *sd)
{
	if (run_dialog_replace(sd) == GTK_RESPONSE_OK)
		activate_quick_find(sd);
}

void cb_search_jump_to(StructData *sd)
{
	run_dialog_jump_to(sd);
}

void cb_option_font(StructData *sd)
{
	change_text_font_by_selector(sd->mainwin->textview);
}

void cb_option_word_wrap(StructData *sd, guint action, GtkWidget *widget)
{
	GtkItemFactory *ifactory;
	gboolean check;
	
	ifactory = gtk_item_factory_from_widget(widget);
	check = gtk_check_menu_item_get_active(
		GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(ifactory, "/Options/Word Wrap")));
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(sd->mainwin->textview),
		check ? GTK_WRAP_WORD : GTK_WRAP_NONE);
}

void cb_option_line_numbers(StructData *sd, guint action, GtkWidget *widget)
{
	GtkItemFactory *ifactory;
	gboolean state;
	
	ifactory = gtk_item_factory_from_widget(widget);
	state = gtk_check_menu_item_get_active(
		GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(ifactory, "/Options/Line Numbers")));
	show_line_numbers(sd->mainwin->textview, state);
}

void cb_option_auto_indent(StructData *sd, guint action, GtkWidget *widget)
{
	GtkItemFactory *ifactory;
	gboolean state;
	
	ifactory = gtk_item_factory_from_widget(widget);
	state = gtk_check_menu_item_get_active(
		GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(ifactory, "/Options/Auto Indent")));
	indent_set_state(state);
}

void cb_help_about(StructData *sd)
{
	run_dialog_about(sd->mainwin->window,
		PACKAGE_NAME,
		PACKAGE_VERSION,
		_("A text editor for Xfce"),
		"Copyright &#169; 2005-2006 Erik Harrison\nBased on Code by Tarot Osuji and the Gedit team",
		ICONDIR G_DIR_SEPARATOR_S PACKAGE ".png");
}
