#ifndef MOUSEPAD_SETTINGS_STORE_H_
#define MOUSEPAD_SETTINGS_STORE_H_ 1

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

#endif /* MOUSEPAD_SETTINGS_STORE_H_ */
