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

#ifndef __MOUSEPAD_SETTINGS_STORE_H__
#define __MOUSEPAD_SETTINGS_STORE_H__ 1

#include <gio/gio.h>

G_BEGIN_DECLS


#define MOUSEPAD_TYPE_SETTINGS_STORE            (mousepad_settings_store_get_type ())
#define MOUSEPAD_SETTINGS_STORE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_SETTINGS_STORE, MousepadSettingsStore))
#define MOUSEPAD_SETTINGS_STORE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_SETTINGS_STORE, MousepadSettingsStoreClass))
#define MOUSEPAD_IS_SETTINGS_STORE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_SETTINGS_STORE))
#define MOUSEPAD_IS_SETTINGS_STORE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MOUSEPAD_TYPE_SETTINGS_STORE))
#define MOUSEPAD_SETTINGS_STORE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_SETTINGS_STORE, MousepadSettingsStoreClass))


typedef struct MousepadSettingsStore_      MousepadSettingsStore;
typedef struct MousepadSettingsStoreClass_ MousepadSettingsStoreClass;


GType                  mousepad_settings_store_get_type        (void);

MousepadSettingsStore *mousepad_settings_store_new             (void);

const gchar           *mousepad_settings_store_lookup_key_name (MousepadSettingsStore *store,
                                                                const gchar           *path);

GSettings             *mousepad_settings_store_lookup_settings (MousepadSettingsStore *store,
                                                                const gchar           *path);

gboolean               mousepad_settings_store_lookup          (MousepadSettingsStore *store,
                                                                const gchar           *path,
                                                                const gchar          **key_name,
                                                                GSettings            **settings);

G_END_DECLS

#endif /* __MOUSEPAD_SETTINGS_STORE_H__ */
