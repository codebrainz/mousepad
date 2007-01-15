/*
 *  undo.h
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

#ifndef _UNDO_H
#define _UNDO_H

void undo_reset_step_modif(void);
void undo_init(GtkWidget *textview, GtkTextBuffer *buffer, GtkWidget *menubar);
gint undo_block_signal(GtkTextBuffer *buffer);
gint undo_unblock_signal(GtkTextBuffer *buffer);
gint undo_disconnect_signal(GtkTextBuffer *buffer);
void undo_set_sequency(gboolean seq);
void undo_undo(GtkTextBuffer *buffer);
void undo_redo(GtkTextBuffer *buffer);

#endif /* _UNDO_H */
