/*
 *  font.h
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

#ifndef _FONT_H
#define _FONT_H

void set_text_font_by_name(GtkWidget *widget, gchar *fontname);
gchar *get_font_name_from_widget(GtkWidget *widget);  /* MUST BE FREE */
gchar *get_font_name_by_selector(GtkWidget *window, gchar *current_fontname);
void change_text_font_by_selector(GtkWidget *widget);

#endif /* _FONT_H */
