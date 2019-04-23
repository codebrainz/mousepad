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

#ifndef __MOUSEPAD_ACTION_GROUP_H__
#define __MOUSEPAD_ACTION_GROUP_H__ 1

#include <gtk/gtk.h>
#if GTK_CHECK_VERSION(3, 0, 0)
#include <gtksourceview/gtksource.h>
#else
#include <gtksourceview/gtksourcelanguage.h>
#include <gtksourceview/gtksourcestylescheme.h>
#endif

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_ACTION_GROUP            (mousepad_action_group_get_type ())
#define MOUSEPAD_ACTION_GROUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_ACTION_GROUP, MousepadActionGroup))
#define MOUSEPAD_ACTION_GROUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_ACTION_GROUP, MousepadActionGroupClass))
#define MOUSEPAD_IS_ACTION_GROUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_ACTION_GROUP))
#define MOUSEPAD_IS_ACTION_GROUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MOUSEPAD_TYPE_ACTION_GROUP))
#define MOUSEPAD_ACTION_GROUP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_ACTION_GROUP, MousepadActionGroupClass))

typedef struct MousepadActionGroup_      MousepadActionGroup;
typedef struct MousepadActionGroupClass_ MousepadActionGroupClass;

GType                 mousepad_action_group_get_type                 (void);

GtkActionGroup       *mousepad_action_group_new                      (void);

void                  mousepad_action_group_set_active_language      (MousepadActionGroup *group,
                                                                      GtkSourceLanguage   *language);

GtkSourceLanguage    *mousepad_action_group_get_active_language      (MousepadActionGroup *group);

GtkWidget            *mousepad_action_group_create_language_menu     (MousepadActionGroup *group);

void                  mousepad_action_group_set_active_style_scheme  (MousepadActionGroup  *group,
                                                                      GtkSourceStyleScheme *scheme);

GtkSourceStyleScheme *mousepad_action_group_get_active_style_scheme  (MousepadActionGroup *group);

GtkWidget            *mousepad_action_group_create_style_scheme_menu (MousepadActionGroup *group);

G_END_DECLS

#endif /* __MOUSEPAD_ACTION_GROUP_H__ */
