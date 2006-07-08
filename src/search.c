/*
 *  search.c
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

#include "mousepad.h"
#include "gtksourceiter.h"

gboolean document_search_real(StructData *sd, gint direction)
{
	GtkTextIter iter, match_start, match_end;
	gboolean res;
	GtkSourceSearchFlags search_flags = GTK_SOURCE_SEARCH_VISIBLE_ONLY | GTK_SOURCE_SEARCH_TEXT_ONLY;	
	GtkTextBuffer *textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(sd->mainwin->textview));
	
	if (!sd->search.string_find)
		return FALSE;
	
	if (!sd->search.match_case)
		search_flags = search_flags | GTK_SOURCE_SEARCH_CASE_INSENSITIVE;
	
	gtk_text_buffer_get_iter_at_mark(textbuffer, &iter, gtk_text_buffer_get_insert(textbuffer));
	if (direction < 0) {
		res = gtk_source_iter_backward_search(
			&iter, sd->search.string_find, search_flags, &match_start, &match_end, NULL);
		if (gtk_text_iter_equal(&iter, &match_end)) {
			res = gtk_source_iter_backward_search(
				&match_start, sd->search.string_find, search_flags, &match_start, &match_end, NULL);
		}
	} else {
		res = gtk_source_iter_forward_search(
			&iter, sd->search.string_find, search_flags, &match_start, &match_end, NULL);
	}
	/* TODO: both gtk_(text/source)_iter_backward_search works not fine for multi-byte */
	
	/* wrap */
	/* TODO: define limit NULL -> proper value */
	if (!res) {
		if (direction < 0) {
			gtk_text_buffer_get_end_iter(textbuffer, &iter);
			res = gtk_source_iter_backward_search(
				&iter, sd->search.string_find, search_flags, &match_start, &match_end, NULL);
		} else {
			gtk_text_buffer_get_start_iter(textbuffer, &iter);
			res = gtk_source_iter_forward_search(
				&iter, sd->search.string_find, search_flags, &match_start, &match_end, NULL);
		}
	}
	
	if (res) {
		gtk_text_buffer_place_cursor(textbuffer, &match_start);
		gtk_text_buffer_move_mark_by_name(textbuffer, "insert", &match_end);
		gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(sd->mainwin->textview), &match_start, 0.1, FALSE, 0.5, 0.5);
	} 
	else if (direction == 0)
		run_dialog_message(gtk_widget_get_toplevel(sd->mainwin->textview), GTK_MESSAGE_WARNING,
			_("Search string not found"));
	
	return res;
}

static gint document_replace_real(StructData *sd)
{
	GtkTextIter iter, match_start, match_end;
	GtkTextMark *mark_init = NULL;
	gboolean res;
	gint num = 0;
	GtkWidget *q_dialog = NULL;
	GtkSourceSearchFlags search_flags = GTK_SOURCE_SEARCH_VISIBLE_ONLY | GTK_SOURCE_SEARCH_TEXT_ONLY;	
	GtkTextBuffer *textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(sd->mainwin->textview));
	
	if (!sd->search.match_case)
		search_flags = search_flags | GTK_SOURCE_SEARCH_CASE_INSENSITIVE;
	
	if (sd->search.replace_all) {
		gtk_text_buffer_get_iter_at_mark(textbuffer, &iter, gtk_text_buffer_get_insert(textbuffer));
		mark_init = gtk_text_buffer_create_mark(textbuffer, NULL, &iter, FALSE);
		gtk_text_buffer_get_start_iter(textbuffer, &iter);
	}
	
	do {
		if (sd->search.replace_all) {
			res = gtk_source_iter_forward_search(
				&iter, sd->search.string_find, search_flags, &match_start, &match_end, NULL);
			if (res) {
				gtk_text_buffer_place_cursor(textbuffer, &match_start);
				gtk_text_buffer_move_mark_by_name(textbuffer, "insert", &match_end);
				gtk_text_buffer_get_iter_at_mark(
					textbuffer, &iter, gtk_text_buffer_get_insert(textbuffer));
			}
		}
		else
			res = document_search_real(sd, 0);
		
		if (res) {
			if (!sd->search.replace_all) {
				if (num == 0 && q_dialog == NULL)
					q_dialog = create_dialog_message_question(
						gtk_widget_get_toplevel(sd->mainwin->textview), _("Replace?"));
				switch (gtk_dialog_run(GTK_DIALOG(q_dialog))) {
				case GTK_RESPONSE_CANCEL:
					res = 0;
					if (num == 0)
						num = -1;
					continue;
				case GTK_RESPONSE_NO:
					continue;
				}
			}
			gtk_text_buffer_delete_selection(textbuffer, TRUE, TRUE);
			undo_set_sequency(TRUE);
			gtk_text_buffer_insert_at_cursor(textbuffer, sd->search.string_replace, strlen(sd->search.string_replace));
			num++;
			gtk_text_buffer_get_iter_at_mark(
				textbuffer, &iter, gtk_text_buffer_get_insert(textbuffer));
			if (sd->search.replace_all)
				undo_set_sequency(TRUE);
		}
	} while (res);
	
	if (q_dialog)
		gtk_widget_destroy(q_dialog);
	if (sd->search.replace_all) {
		gtk_text_buffer_get_iter_at_mark(textbuffer, &iter, mark_init);
		gtk_text_buffer_place_cursor(textbuffer, &iter);
		run_dialog_message(gtk_widget_get_toplevel(sd->mainwin->textview), GTK_MESSAGE_INFO,
			_("%d strings replaced"), num);
		undo_set_sequency(FALSE);
	}
	
	return num;
}


static gint entry_len;

static void toggle_sensitivity(GtkWidget *w, gint pos1, gint pos2, gint *pos3)
{
	if (pos3) {
		if (!entry_len)
			gtk_dialog_set_response_sensitive(GTK_DIALOG(gtk_widget_get_toplevel(w)),
				GTK_RESPONSE_OK, TRUE);
		entry_len += pos2;
/*		entry_len = entry_len + pos2; */
	} else {
		entry_len = entry_len + pos1 - pos2;
		if (!entry_len)
			gtk_dialog_set_response_sensitive(GTK_DIALOG(gtk_widget_get_toplevel(w)),
				GTK_RESPONSE_OK, FALSE);
	}
}

static void toggle_check_case(GtkToggleButton *tog, StructData *sd)
{
	sd->search.match_case = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(tog));
}

static void toggle_check_all(GtkToggleButton *tog, StructData *sd)
{
	sd->search.replace_all = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(tog));
}

gint run_dialog_find(StructData *sd)
{
	GtkWidget *dialog;
	GtkWidget *table;
	GtkWidget *label_find;
	GtkWidget *entry_find = NULL;
	GtkWidget *check_case;
	gint res;

	dialog = gtk_dialog_new_with_buttons(_("Find"),
	           GTK_WINDOW(gtk_widget_get_toplevel(sd->mainwin->textview)),
	           GTK_DIALOG_DESTROY_WITH_PARENT,
	           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	           GTK_STOCK_FIND, GTK_RESPONSE_OK,
	           NULL);
	
	table = gtk_table_new(2, 2, FALSE);
	 gtk_table_set_row_spacings(GTK_TABLE(table), 8);
	 gtk_table_set_col_spacings(GTK_TABLE(table), 8);
	 gtk_container_set_border_width(GTK_CONTAINER(table), 8);
	 gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, FALSE, 0);
	label_find = gtk_label_new_with_mnemonic(_("Fi_nd what: "));
	 gtk_misc_set_alignment(GTK_MISC(label_find), 0, 0.5);
	 gtk_table_attach_defaults(GTK_TABLE(table), label_find, 0, 1, 0, 1);
	entry_find = gtk_entry_new();
	 gtk_table_attach_defaults(GTK_TABLE(table), entry_find, 1, 2, 0, 1);
	 gtk_label_set_mnemonic_widget(GTK_LABEL(label_find), entry_find);
	 gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
		GTK_RESPONSE_OK, FALSE);
	 entry_len = 0;
	 g_signal_connect(G_OBJECT(entry_find), "insert-text",
		G_CALLBACK(toggle_sensitivity), NULL);
	 g_signal_connect(G_OBJECT(entry_find), "delete-text",
		G_CALLBACK(toggle_sensitivity), NULL);
	 if (sd->search.string_find) 
		 gtk_entry_set_text(GTK_ENTRY(entry_find), sd->search.string_find);

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
	gtk_entry_set_activates_default(GTK_ENTRY(entry_find), TRUE);
	
	check_case = gtk_check_button_new_with_mnemonic(_("_Match case"));
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_case), sd->search.match_case);
	 g_signal_connect(GTK_OBJECT(check_case), "toggled", G_CALLBACK(toggle_check_case), sd);
	 gtk_table_attach_defaults (GTK_TABLE(table), check_case, 0, 2, 1, 2);

	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_widget_show_all(table);	
	
	res = gtk_dialog_run(GTK_DIALOG(dialog));
	if (res == GTK_RESPONSE_OK) {
		g_free(sd->search.string_find);
		sd->search.string_find = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry_find)));
	}
	
	gtk_widget_destroy(dialog);
	
	if (res == GTK_RESPONSE_OK) {
		if (strlen(sd->search.string_find)) {
			document_search_real(sd, 0);
		}
	}
	
	return res;
}

gint run_dialog_replace(StructData *sd)
{
	GtkWidget *dialog;
	GtkWidget *table;
	GtkWidget *label_replace, *label_find;
	GtkWidget *entry_find, *entry_replace = NULL;
	GtkWidget *check_case, *check_all;
	gint res;
	
	dialog = gtk_dialog_new_with_buttons(_("Replace"),
	  GTK_WINDOW(gtk_widget_get_toplevel(sd->mainwin->textview)),
	  GTK_DIALOG_DESTROY_WITH_PARENT,
	  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	  GTK_STOCK_FIND_AND_REPLACE, GTK_RESPONSE_OK,
	  NULL);
	
	table = gtk_table_new(3, 2, FALSE);
	 gtk_table_set_row_spacings(GTK_TABLE(table), 8);
	 gtk_table_set_col_spacings(GTK_TABLE(table), 8);
	 gtk_container_set_border_width(GTK_CONTAINER(table), 8);
	 gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, FALSE, 0);
	label_find = gtk_label_new_with_mnemonic(_("Fi_nd what: "));
	 gtk_misc_set_alignment(GTK_MISC(label_find), 0, 0.5);
	 gtk_table_attach_defaults(GTK_TABLE(table), label_find, 0, 1, 0, 1);
	entry_find = gtk_entry_new();
	 gtk_table_attach_defaults(GTK_TABLE(table), entry_find, 1, 2, 0, 1);
	 gtk_label_set_mnemonic_widget(GTK_LABEL(label_find), entry_find);
	 gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
		GTK_RESPONSE_OK, FALSE);
	 entry_len = 0;
	 g_signal_connect(G_OBJECT(entry_find), "insert-text",
		G_CALLBACK(toggle_sensitivity), NULL);
	 g_signal_connect(G_OBJECT(entry_find), "delete-text",
		G_CALLBACK(toggle_sensitivity), NULL);
	 if (sd->search.string_find) 
		 gtk_entry_set_text(GTK_ENTRY(entry_find), sd->search.string_find);
	label_replace = gtk_label_new_with_mnemonic(_("Re_place with: "));
	 gtk_misc_set_alignment(GTK_MISC(label_replace), 0, 0.5);
	 gtk_table_attach_defaults(GTK_TABLE(table), label_replace, 0, 1, 1, 2);
	entry_replace = gtk_entry_new();
	 gtk_table_attach_defaults(GTK_TABLE(table), entry_replace, 1, 2, 1, 2);
	 gtk_label_set_mnemonic_widget(GTK_LABEL(label_replace), entry_replace);
	 if (sd->search.string_replace)
		 gtk_entry_set_text(GTK_ENTRY(entry_replace), sd->search.string_replace);

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
	gtk_entry_set_activates_default(GTK_ENTRY(entry_replace), TRUE);

	check_case = gtk_check_button_new_with_mnemonic(_("_Match case"));
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_case), sd->search.match_case);
	 g_signal_connect(GTK_OBJECT(check_case), "toggled", G_CALLBACK(toggle_check_case), sd);
	 gtk_table_attach_defaults (GTK_TABLE(table), check_case, 0, 2, 2, 3);
	check_all = gtk_check_button_new_with_mnemonic(_("Replace _all at once"));
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_all), sd->search.replace_all);
	 g_signal_connect(GTK_OBJECT(check_all), "toggled", G_CALLBACK(toggle_check_all), sd);
	 gtk_table_attach_defaults(GTK_TABLE(table), check_all, 0, 2, 3, 4);

	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_widget_show_all(table);	
	
	res = gtk_dialog_run(GTK_DIALOG(dialog));
	if (res == GTK_RESPONSE_OK) {
		g_free(sd->search.string_find);
		sd->search.string_find = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry_find)));
		g_free(sd->search.string_replace);
		sd->search.string_replace = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry_replace)));
	}
	
	gtk_widget_destroy(dialog);
	
	if (res == GTK_RESPONSE_OK) {
		if (strlen(sd->search.string_find)) {
			document_replace_real(sd);
		}
	}
	
	return res;
}

void run_dialog_jump_to(StructData *sd)
{
	GtkWidget *dialog;
	GtkWidget *button;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *spinner;
	GtkAdjustment *spinner_adj;
	GtkTextIter iter;
	gint num, max_num;
	
	GtkTextBuffer *textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(sd->mainwin->textview));
	
	gtk_text_buffer_get_iter_at_mark(textbuffer, &iter,
		gtk_text_buffer_get_insert(textbuffer));
	num = gtk_text_iter_get_line(&iter) + 1;
	gtk_text_buffer_get_end_iter(textbuffer, &iter);
	max_num = gtk_text_iter_get_line(&iter) + 1;
	
	dialog = gtk_dialog_new_with_buttons(_("Jump To"),
		GTK_WINDOW(gtk_widget_get_toplevel(sd->mainwin->textview)),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		NULL);
	button = gedit_button_new_with_stock_image(_("_Jump"), GTK_STOCK_JUMP_TO);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_dialog_add_action_widget(GTK_DIALOG(dialog), button, GTK_RESPONSE_OK);
	hbox = gtk_hbox_new(TRUE, 0);
	 gtk_container_set_border_width (GTK_CONTAINER(hbox), 8);
	 gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, FALSE, FALSE, 0);
	label = gtk_label_new_with_mnemonic(_("_Line number: "));
	 gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	spinner_adj = (GtkAdjustment *) gtk_adjustment_new(num, 1, max_num, 1, 1, 0);
	spinner = gtk_spin_button_new(spinner_adj, 1, 0);
	 gtk_entry_set_width_chars(GTK_ENTRY(spinner), 8);
	 gtk_box_pack_start(GTK_BOX(hbox), spinner, FALSE, FALSE, 0);
	 gtk_label_set_mnemonic_widget(GTK_LABEL(label), spinner);
	 gtk_entry_set_activates_default(GTK_ENTRY(spinner), TRUE);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_widget_show_all(dialog);
	
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
		gtk_text_buffer_get_iter_at_line(textbuffer, &iter,
			gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner)) - 1);
		gtk_text_buffer_place_cursor(textbuffer, &iter);
		gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(sd->mainwin->textview), &iter, 0.25, FALSE, 0.5, 0.5);
	}
	
	gtk_widget_destroy (dialog);
}
