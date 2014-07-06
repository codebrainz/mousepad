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

G_END_DECLS

#endif /* MOUSEPAD_SETTINGS_H_ */
