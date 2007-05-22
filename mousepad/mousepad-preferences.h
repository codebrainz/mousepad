/* $Id$ */
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

#ifndef __MOUSEPAD_PREFERENCIES_H__
#define __MOUSEPAD_PREFERENCIES_H__

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_PREFERENCES             (mousepad_preferences_get_type ())
#define MOUSEPAD_PREFERENCES(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_PREFERENCES, MousepadPreferences))
#define MOUSEPAD_PREFERENCES_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_PREFERENCES, MousepadPreferencesClass))
#define MOUSEPAD_IS_PREFERENCES(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_PREFERENCES))
#define MOUSEPAD_IS_PREFERENCES_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOUSEPAD_TYPE_PREFERENCES))
#define MOUSEPAD_PREFERENCES_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_PREFERENCES, MousepadPreferencesClass))

typedef struct _MousepadPreferencesClass MousepadPreferencesClass;
typedef struct _MousepadPreferences      MousepadPreferences;

GType                mousepad_preferences_get_type (void) G_GNUC_CONST;

MousepadPreferences *mousepad_preferences_get      (void);

G_END_DECLS

#endif /* !__MOUSEPAD_PREFERENCIES_H__ */
