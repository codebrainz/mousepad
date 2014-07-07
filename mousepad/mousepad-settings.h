#ifndef MOUSEPAD_SETTINGS_H_
#define MOUSEPAD_SETTINGS_H_ 1

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum
{
  MOUSEPAD_SCHEMA_VIEW_SETTINGS,
  MOUSEPAD_SCHEMA_WINDOW_SETTINGS,
  MOUSEPAD_SCHEMA_SEARCH_STATE,
  MOUSEPAD_SCHEMA_WINDOW_STATE,
  MOUSEPAD_NUM_SCHEMAS
}
MousepadSchema;

GType mousepad_schema_get_type (void);
#define MOUSEPAD_TYPE_SCHEMA (mousepad_schema_get_type ())

#define MOUSEPAD_TYPE_SETTINGS            (mousepad_settings_get_type ())
#define MOUSEPAD_SETTINGS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_SETTINGS, MousepadSettings))
#define MOUSEPAD_SETTINGS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_SETTINGS, MousepadSettingsClass))
#define MOUSEPAD_IS_SETTINGS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_SETTINGS))
#define MOUSEPAD_IS_SETTINGS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MOUSEPAD_TYPE_SETTINGS))
#define MOUSEPAD_SETTINGS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_SETTINGS, MousepadSettingsClass))

typedef struct MousepadSettings_      MousepadSettings;
typedef struct MousepadSettingsClass_ MousepadSettingsClass;

GType             mousepad_settings_get_type        (void);

MousepadSettings *mousepad_settings_get_default     (void);

GSettings        *mousepad_settings_get_from_schema (MousepadSettings  *settings,
                                                     MousepadSchema     schema);

void              mousepad_settings_bind            (MousepadSchema     schema,
                                                     const gchar       *key,
                                                     gpointer           object,
                                                     const gchar       *prop,
                                                     GSettingsBindFlags flags);

gboolean          mousepad_settings_get_boolean     (MousepadSchema     schema,
                                                     const gchar       *key);

void              mousepad_settings_set_boolean     (MousepadSchema     schema,
                                                     const gchar       *key,
                                                     gboolean           value);

gint              mousepad_settings_get_int         (MousepadSchema     schema,
                                                     const gchar       *key);

void              mousepad_settings_set_int         (MousepadSchema     schema,
                                                     const gchar       *key,
                                                     gint               value);

gchar            *mousepad_settings_get_string      (MousepadSchema     schema,
                                                     const gchar       *key);

void              mousepad_settings_set_string      (MousepadSchema     schema,
                                                     const gchar       *key,
                                                     const gchar       *value);

/* Setting names */
#define MOUSEPAD_SETTING_AUTO_INDENT            "auto-indent"
#define MOUSEPAD_SETTING_FONT_NAME              "font-name"
#define MOUSEPAD_SETTING_SHOW_WHITESPACE        "show-whitespace"
#define MOUSEPAD_SETTING_SHOW_LINE_ENDINGS      "show-line-endings"
#define MOUSEPAD_SETTING_HIGHLIGHT_CURRENT_LINE "highlight-current-line"
#define MOUSEPAD_SETTING_INDENT_ON_TAB          "indent-on-tab"
#define MOUSEPAD_SETTING_INDENT_WIDTH           "indent-width"
#define MOUSEPAD_SETTING_INSERT_SPACES          "insert-spaces"
#define MOUSEPAD_SETTING_RIGHT_MARGIN_POSITION  "right-margin-position"
#define MOUSEPAD_SETTING_SHOW_LINE_MARKS        "show-line-marks"
#define MOUSEPAD_SETTING_SHOW_LINE_NUMBERS      "show-line-numbers"
#define MOUSEPAD_SETTING_SHOW_RIGHT_MARGIN      "show-right-margin"
#define MOUSEPAD_SETTING_SMART_HOME_END         "smart-home-end"
#define MOUSEPAD_SETTING_TAB_WIDTH              "tab-width"
#define MOUSEPAD_SETTING_WORD_WRAP              "word-wrap"
#define MOUSEPAD_SETTING_COLOR_SCHEME           "color-scheme"
#define MOUSEPAD_SETTING_STATUSBAR_VISIBLE      "statusbar-visible"
#define MOUSEPAD_SETTING_ALWAYS_SHOW_TABS       "always-show-tabs"
#define MOUSEPAD_SETTING_CYCLE_TABS             "cycle-tabs"
#define MOUSEPAD_SETTING_DEFAULT_TAB_SIZES      "default-tab-sizes"
#define MOUSEPAD_SETTING_PATH_IN_TITLE          "path-in-title"
#define MOUSEPAD_SETTING_RECENT_MENU_ITEMS      "recent-menu-items"
#define MOUSEPAD_SETTING_REMEMBER_GEOMETRY      "remember-geometry"

/* State setting names */
#define MOUSEPAD_STATE_SEARCH_DIRECTION            "direction"
#define MOUSEPAD_STATE_SEARCH_MATCH_CASE           "match-case"
#define MOUSEPAD_STATE_SEARCH_MATCH_WHOLE_WORD     "match-whole-word"
#define MOUSEPAD_STATE_SEARCH_REPLACE_ALL          "replace-all"
#define MOUSEPAD_STATE_SEARCH_REPLACE_ALL_LOCATION "replace-all-location"
#define MOUSEPAD_STATE_WINDOW_HEIGHT               "height"
#define MOUSEPAD_STATE_WINDOW_WIDTH                "width"

G_END_DECLS

#endif /* MOUSEPAD_SETTINGS_H_ */
