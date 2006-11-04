/*
 *  file.c
 *  This file is part of Mousepad
 *
 *  Copyright (C) 2006 Benedikt Meurer <benny@xfce.org>
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

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include "encoding.h"
#include "dialog.h"
#include "undo.h"
#include "file.h"

gint file_open_real(GtkWidget *textview, FileInfo *fi)
{
	gchar *contents;
	gsize length;
	GError *err = NULL;
	const gchar *charset;
	gchar *str = NULL;
	GtkTextIter iter;
#if GTK_CHECK_VERSION(2,10,0)
  GtkRecentData recent_data;
  gchar *uri;
#endif
	
	GtkTextBuffer *textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	
	if (!g_file_get_contents(fi->filename, &contents, &length, &err)) {
		if (g_file_test(fi->filename, G_FILE_TEST_EXISTS)) {
			run_dialog_message(gtk_widget_get_toplevel(textview), GTK_MESSAGE_ERROR, err->message);
			g_error_free(err);
			return -1;
		}
		g_error_free(err);
		err = NULL;
		contents = g_strdup("");
	}
	
	fi->lineend = detect_line_ending(contents);
	if (fi->lineend != LF)
		convert_line_ending_to_lf(contents);
	
	if (fi->charset)
		charset = fi->charset;
	else
		charset = detect_charset(contents);
	if (!charset) {
		if (fi->manual_charset)
			charset = fi->manual_charset;
		else
			charset = get_default_charset();
	}

	if (length)
		do {
			if (err) {
				if (strcmp(charset, "GB2312") == 0)
					charset = "GB18030";
				else if (strcasecmp(charset, "BIG5") == 0)
					charset = "CP950";
				else if (strcasecmp(charset, "CP950") == 0)
					charset = "BIG5-HKSCS";
				else if (strcasecmp(charset, "EUC-KR") == 0)
					charset = "CP949";
				else if (strcasecmp(charset, "CP949") == 0)
					charset = "CP1361";
				else if (strcasecmp(charset, "VISCII") == 0)
					charset = "CP1258";
				else if (strcasecmp(charset, "TIS-620") == 0)
					charset = "CP874";
				else
					charset = "ISO-8859-1";
				g_error_free(err);
				err = NULL;
			}
			str = g_convert(contents, -1, "UTF-8", charset, NULL, NULL, &err);
		} while (err);
	else
		str = g_strdup("");
	
	if (charset != fi->charset) {
		g_free(fi->charset);
		fi->charset = g_strdup(charset);
	}
	if (fi->manual_charset)
		if (strcmp(fi->manual_charset, fi->charset) != 0) {
			g_free(fi->manual_charset);
			fi->manual_charset = NULL;
		}
	g_free(contents);
	
/*	undo_disconnect_signal(textbuffer); */
	undo_block_signal(textbuffer);
	gtk_text_buffer_set_text(textbuffer, "", 0);
	gtk_text_buffer_get_start_iter(textbuffer, &iter);
	gtk_text_buffer_insert(textbuffer, &iter, str, strlen(str));
	gtk_text_buffer_get_start_iter(textbuffer, &iter);
	gtk_text_buffer_place_cursor(textbuffer, &iter);
	gtk_text_buffer_set_modified(textbuffer, FALSE);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(textview), &iter, 0, FALSE, 0, 0);
	g_free(str);
	undo_unblock_signal(textbuffer);
	
#if GTK_CHECK_VERSION(2,10,0)
  /* generate the recently-used data */
  recent_data.display_name = NULL;
  recent_data.description = NULL;
  recent_data.mime_type = "text/plain";
  recent_data.app_name = "Mousepad Text Editor";
  recent_data.app_exec = "mousepad %f";
  recent_data.groups = NULL;
  recent_data.is_private = FALSE;

  /* add the file to the recently-used database */
  uri = g_filename_to_uri(fi->filename, NULL, NULL);
  if (G_LIKELY(uri != NULL))
    {
      gtk_recent_manager_add_full(gtk_recent_manager_get_default(), uri, &recent_data);
      g_free(uri);
    }
#endif

	return 0;
}

gint file_save_real(GtkWidget *textview, FileInfo *fi)
{
	FILE *fp;
	GtkTextIter start, end;
	gchar *str;
	gsize rbytes, wbytes;
	GError *err = NULL;
	
	GtkTextBuffer *textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	
	gtk_text_buffer_get_start_iter(textbuffer, &start);
	gtk_text_buffer_get_end_iter(textbuffer, &end);	
	str = gtk_text_buffer_get_text(textbuffer, &start, &end, TRUE);
	
	switch (fi->lineend) {
	case CR:
		convert_line_ending(&str, CR);
		break;
	case CR+LF:
		convert_line_ending(&str, CR+LF);
	}
	
	if (!fi->charset)
		fi->charset = g_strdup(get_default_charset());
	str = g_convert(str, -1, fi->charset, "UTF-8", &rbytes, &wbytes, &err);
	if (err) {
		switch (err->code) {
		case G_CONVERT_ERROR_ILLEGAL_SEQUENCE:
			run_dialog_message(gtk_widget_get_toplevel(textview),
				GTK_MESSAGE_ERROR, _("Can't convert codeset to '%s'"), fi->charset);
			break;
		default:
			run_dialog_message(gtk_widget_get_toplevel(textview),
				GTK_MESSAGE_ERROR, err->message);
		}
		g_error_free(err);
		return -1;
	}
	
	fp = fopen(fi->filename, "w");
	if (!fp) {
		run_dialog_message(gtk_widget_get_toplevel(textview),
			GTK_MESSAGE_ERROR, _("Can't open file to write"));
		return -1;
	}
	if (fputs(str, fp) == EOF) {
		run_dialog_message(gtk_widget_get_toplevel(textview),
			GTK_MESSAGE_ERROR, _("Can't write file"));
		return -1;
	}
	
	gtk_text_buffer_set_modified(textbuffer, FALSE);
	fclose(fp);
	g_free(str);
	
	return 0;
}
