/*
 *  dialog.h
 *  This file is part of Mousepad
 *
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

#ifndef _DIALOG_H
#define _DIALOG_H

void run_dialog_message(GtkWidget *window, GtkMessageType type, gchar *message, ...);
GtkWidget *create_dialog_message_question(GtkWidget *window, gchar *message, ...);
gint run_dialog_message_question(GtkWidget *window, gchar *message, ...);
void run_dialog_about(GtkWidget *window, const gchar *name, const gchar *version,
	const gchar *description, const gchar *copyright, gchar *iconpath);

#endif /* _DIALOG_H */
