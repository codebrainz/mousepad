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

#ifndef __MOUSEPAD_STATUSBAR_H__
#define __MOUSEPAD_STATUSBAR_H__

#include <gtk/gtk.h>

#include <gtksourceview/gtksource.h>


G_BEGIN_DECLS

#define MOUSEPAD_TYPE_STATUSBAR            (mousepad_statusbar_get_type ())
#define MOUSEPAD_STATUSBAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_STATUSBAR, MousepadStatusbar))
#define MOUSEPAD_STATUSBAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_STATUSBAR, MousepadStatusbarClass))
#define MOUSEPAD_IS_STATUSBAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_STATUSBAR))
#define MOUSEPAD_IS_STATUSBAR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_STATUSBAR))
#define MOUSEPAD_STATUSBAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_STATUSBAR, MousepadStatusbarClass))

typedef struct _MousepadStatusbarClass MousepadStatusbarClass;
typedef struct _MousepadStatusbar      MousepadStatusbar;

GType       mousepad_statusbar_get_type             (void) G_GNUC_CONST;

GtkWidget  *mousepad_statusbar_new                  (void);

void        mousepad_statusbar_set_cursor_position  (MousepadStatusbar *statusbar,
                                                     gint               line,
                                                     gint               column,
                                                     gint               selection);

void        mousepad_statusbar_set_overwrite        (MousepadStatusbar *statusbar,
                                                     gboolean           overwrite);

void        mousepad_statusbar_set_language         (MousepadStatusbar *statusbar,
                                                     GtkSourceLanguage *language);

void        mousepad_statusbar_push_tooltip         (MousepadStatusbar *statusbar,
                                                     const gchar       *tooltip);

void        mousepad_statusbar_pop_tooltip          (MousepadStatusbar *statusbar);

G_END_DECLS

#endif /* !__MOUSEPAD_STATUSBAR_H__ */
