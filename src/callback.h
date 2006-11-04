/*
 *  callback.h
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

#ifndef _CALLBACK_H
#define _CALLBACK_H

void cb_file_new(StructData *sd);
void cb_file_new_window(StructData *sd);
void cb_file_open(StructData *sd);
#if GTK_CHECK_VERSION(2,10,0)
void cb_file_open_recent(StructData *sd, GtkRecentChooser *chooser);
#endif
gint cb_file_save(StructData *sd);
gint cb_file_save_as(StructData *sd);
gint cb_file_print(StructData *sd);
void cb_file_quit(StructData *sd);
void cb_edit_undo(StructData *sd);
void cb_edit_redo(StructData *sd);
void cb_edit_cut(StructData *sd);
void cb_edit_copy(StructData *sd);
void cb_edit_paste(StructData *sd);
void cb_edit_delete(StructData *sd);
void cb_edit_select_all(StructData *sd);
void cb_search_find(StructData *sd);
void cb_search_find_next(StructData *sd);
void cb_search_find_prev(StructData *sd);
void cb_search_replace(StructData *sd);
void cb_search_jump_to(StructData *sd);
void cb_option_font(StructData *sd);
void cb_option_word_wrap(StructData *sd, guint action, GtkWidget *widget);
void cb_option_line_numbers(StructData *sd, guint action, GtkWidget *widget);
void cb_option_auto_indent(StructData *sd, guint action, GtkWidget *widget);
void cb_help_about(StructData *sd);
void cb_xfprint_closed (GPid pid, gint status, gpointer sd);

#endif /* _CALLBACK_H */
