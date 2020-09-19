/*
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

#ifndef __MOUSEPAD_WINDOW_H__
#define __MOUSEPAD_WINDOW_H__

#include <mousepad/mousepad-document.h>

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_WINDOW            (mousepad_window_get_type ())
#define MOUSEPAD_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_WINDOW, MousepadWindow))
#define MOUSEPAD_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_WINDOW, MousepadWindowClass))
#define MOUSEPAD_IS_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_WINDOW))
#define MOUSEPAD_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_WINDOW))
#define MOUSEPAD_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_WINDOW, MousepadWindowClass))

enum
{
  TARGET_TEXT_URI_LIST,
  TARGET_GTK_NOTEBOOK_TAB
};

static const GtkTargetEntry drop_targets[] =
{
  { (gchar *) "text/uri-list", 0, TARGET_TEXT_URI_LIST },
  { (gchar *) "GTK_NOTEBOOK_TAB", GTK_TARGET_SAME_APP, TARGET_GTK_NOTEBOOK_TAB }
};

typedef struct _MousepadWindowClass MousepadWindowClass;
typedef struct _MousepadWindow      MousepadWindow;

GType           mousepad_window_get_type           (void) G_GNUC_CONST;

GtkWidget      *mousepad_window_new                (void);

void            mousepad_window_add                (MousepadWindow   *window,
                                                    MousepadDocument *document);

gboolean        mousepad_window_open_files         (MousepadWindow   *window,
                                                    const gchar      *working_directory,
                                                    gchar           **filenames);

void            mousepad_window_show_preferences   (MousepadWindow   *window);

void            mousepad_window_create_menubar     (MousepadWindow   *window);

GtkWidget      *mousepad_window_get_languages_menu (MousepadWindow   *window);

G_END_DECLS

#endif /* !__MOUSEPAD_WINDOW_H__ */
