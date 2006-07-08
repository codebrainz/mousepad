/*
 *  window.h
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

#ifndef _WINDOW_H
#define _WINDOW_H

MainWindow *create_main_window(StructData *sd);
gchar *get_current_file_basename(gchar *filename);
void set_main_window_title(StructData *sd);
#if 0
void set_main_window_title_with_asterisk(gboolean flag);
void set_main_window_title_toggle_asterisk(void);
#endif /* 0 */

#endif /* _WINDOW_H */
