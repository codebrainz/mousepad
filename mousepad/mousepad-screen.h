/* $Id$ */
/*
 * Copyright (c) 2007 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __MOUSEPAD_SCREEN_H__
#define __MOUSEPAD_SCREEN_H__

G_BEGIN_DECLS

#define MOUSEPAD_SCROLL_MARGIN 0.02

#define MOUSEPAD_TYPE_SCREEN            (mousepad_screen_get_type ())
#define MOUSEPAD_SCREEN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_SCREEN, MousepadScreen))
#define MOUSEPAD_SCREEN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_SCREEN, MousepadScreenClass))
#define MOUSEPAD_IS_SCREEN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_SCREEN))
#define MOUSEPAD_IS_SCREEN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPADL_TYPE_SCREEN))
#define MOUSEPAD_SCREEN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_SCREEN, MousepadScreenClass))

typedef struct _MousepadScreenClass MousepadScreenClass;
typedef struct _MousepadScreen      MousepadScreen;

GType           mousepad_screen_get_type                 (void) G_GNUC_CONST;

GtkWidget      *mousepad_screen_new                      (void);

void            mousepad_screen_set_filename             (MousepadScreen  *screen,
                                                          const gchar     *filename);

gboolean        mousepad_screen_open_file                (MousepadScreen  *screen,
                                                          const gchar     *filename,
                                                          GError         **error);

gboolean        mousepad_screen_save_file                (MousepadScreen  *screen,
                                                          const gchar     *filename,
                                                          GError         **error);

gboolean        mousepad_screen_reload                   (MousepadScreen  *screen,
                                                          GError         **error);

const gchar    *mousepad_screen_get_title                (MousepadScreen  *screen,
                                                          gboolean         show_full_path);

const gchar    *mousepad_screen_get_filename             (MousepadScreen  *screen);

gboolean        mousepad_screen_get_mtime                (MousepadScreen  *screen);

GtkTextView    *mousepad_screen_get_text_view            (MousepadScreen  *screen);

GtkTextBuffer  *mousepad_screen_get_text_buffer          (MousepadScreen  *screen);

gboolean        mousepad_screen_get_modified             (MousepadScreen  *screen);

G_END_DECLS

#endif /* !__MOUSEPAD_SCREEN_H__ */
