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

#ifndef __MOUSEPAD_STYLESCHEME_ACTION_H__
#define __MOUSEPAD_STYLESCHEME_ACTION_H__ 1

#include <gtk/gtk.h>

#if GTK_CHECK_VERSION(3, 0, 0)
#include <gtksourceview/gtksource.h>
#else
#include <gtksourceview/gtksourcestylescheme.h>
#endif

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_STYLE_SCHEME_ACTION            (mousepad_style_scheme_action_get_type ())
#define MOUSEPAD_STYLE_SCHEME_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_STYLE_SCHEME_ACTION, MousepadStyleSchemeAction))
#define MOUSEPAD_STYLE_SCHEME_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_STYLE_SCHEME_ACTION, MousepadStyleSchemeActionClass))
#define MOUSEPAD_IS_STYLE_SCHEME_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_STYLE_SCHEME_ACTION))
#define MOUSEPAD_IS_STYLE_SCHEME_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MOUSEPAD_TYPE_STYLE_SCHEME_ACTION))
#define MOUSEPAD_STYLE_SCHEME_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_STYLE_SCHEME_ACTION, MousepadStyleSchemeActionClass))

typedef struct MousepadStyleSchemeAction_      MousepadStyleSchemeAction;
typedef struct MousepadStyleSchemeActionClass_ MousepadStyleSchemeActionClass;

GType                 mousepad_style_scheme_action_get_type         (void);

GtkAction            *mousepad_style_scheme_action_new              (GtkSourceStyleScheme      *scheme);

GtkSourceStyleScheme *mousepad_style_scheme_action_get_style_scheme (MousepadStyleSchemeAction *action);

G_END_DECLS

#endif /* __MOUSEPAD_STYLESCHEME_ACTION_H__ */
