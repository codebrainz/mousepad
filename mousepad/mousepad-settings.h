#ifndef MOUSEPAD_SETTINGS_H_
#define MOUSEPAD_SETTINGS_H_ 1

#include <glib-object.h>

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_SETTINGS            (mousepad_settings_get_type ())
#define MOUSEPAD_SETTINGS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_SETTINGS, MousepadSettings))
#define MOUSEPAD_SETTINGS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_SETTINGS, MousepadSettingsClass))
#define MOUSEPAD_IS_SETTINGS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_SETTINGS))
#define MOUSEPAD_IS_SETTINGS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MOUSEPAD_TYPE_SETTINGS))
#define MOUSEPAD_SETTINGS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_SETTINGS, MousepadSettingsClass))

typedef struct MousepadSettings_      MousepadSettings;
typedef struct MousepadSettingsClass_ MousepadSettingsClass;

GType             mousepad_settings_get_type (void);

MousepadSettings *mousepad_settings_get_default (void);

/* convenience wrappers for using the singleton settings */

#define mousepad_settings_bind(key, object, prop, flags) \
  g_settings_bind (G_SETTINGS (mousepad_settings_get_default ()), (key), (object), (prop), (flags))

#define mousepad_settings_get_boolean(key) \
  g_settings_get_boolean (G_SETTINGS (mousepad_settings_get_default ()), (key))

#define mousepad_settings_set_boolean(key, value) \
  g_settings_set_boolean (G_SETTINGS (mousepad_settings_get_default ()), (key), (value))

#define mousepad_settings_get_int(key) \
  g_settings_get_int (G_SETTINGS (mousepad_settings_get_default ()), (key))

#define mousepad_settings_set_int(key, value) \
  g_settings_set_int (G_SETTINGS (mousepad_settings_get_default ()), (key), (value))

#define mousepad_settings_get_string(key) \
  g_settings_get_string (G_SETTINGS (mousepad_settings_get_default ()), (key))

#define mousepad_settings_set_string(key, value) \
  g_settings_set_string (G_SETTINGS (mousepad_settings_get_default ()), (key), (value))

G_END_DECLS

#endif /* MOUSEPAD_SETTINGS_H_ */
