#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-settings.h>
#include <stdlib.h>

#ifdef MOUSEPAD_SETTINGS_KEYFILE_BACKEND
/* Needed to use keyfile GSettings backend */
# define G_SETTINGS_ENABLE_BACKEND
# include <gio/gsettingsbackend.h>
#endif


struct MousepadSettings_
{
  GObject parent;
  GSettings *settings[MOUSEPAD_NUM_SCHEMAS];
};



struct MousepadSettingsClass_
{
  GObjectClass parent_class;
};



static void mousepad_settings_finalize (GObject *object);



G_DEFINE_TYPE (MousepadSettings, mousepad_settings, G_TYPE_OBJECT)



/* Global instance, accessed by mousepad_settings_get_default() */
static MousepadSettings *default_settings = NULL;



static const gchar *mousepad_schema_ids[MOUSEPAD_NUM_SCHEMAS] =
{
  "org.xfce.mousepad.preferences.view",
  "org.xfce.mousepad.preferences.window",
  "org.xfce.mousepad.state.search",
  "org.xfce.mousepad.state.window"
};



GType
mousepad_schema_get_type (void)
{
  static GType type = 0;
  if (G_UNLIKELY (type == 0))
    {
      static const GEnumValue values[] =
      {
        { MOUSEPAD_SCHEMA_VIEW_SETTINGS,   "MOUSEPAD_SCHEMA_VIEW_SETTINGS",   "view-settings" },
        { MOUSEPAD_SCHEMA_WINDOW_SETTINGS, "MOUSEPAD_SCHEMA_WINDOW_SETTINGS", "window-settings" },
        { MOUSEPAD_SCHEMA_SEARCH_STATE,    "MOUSEPAD_SCHEMA_SEARCH_STATE",    "search-state" },
        { MOUSEPAD_SCHEMA_WINDOW_STATE,    "MOUSEPAD_SCHEMA_WINDOW_STATE",    "window-state" },
        { 0, NULL, NULL }
      };
     type = g_enum_register_static ("MousepadSchema", values);
    }
  return type;
}



static void
mousepad_settings_class_init (MousepadSettingsClass *klass)
{
  GObjectClass *g_object_class;

  g_object_class = G_OBJECT_CLASS (klass);

  g_object_class->finalize = mousepad_settings_finalize;
}



static void
mousepad_settings_finalize (GObject *object)
{
  gint i;
  MousepadSettings *self = MOUSEPAD_SETTINGS (object);

  G_OBJECT_CLASS (mousepad_settings_parent_class)->finalize (object);

  g_settings_sync ();

  for (i = 0; i < MOUSEPAD_NUM_SCHEMAS; i++)
    {
      if (G_IS_SETTINGS (self->settings[i]))
        g_object_unref (self->settings[i]);
    }
}



static void
mousepad_settings_init (MousepadSettings *self)
{
  gint i;

#ifdef MOUSEPAD_SETTINGS_KEYFILE_BACKEND
  GSettingsBackend *backend;
  gchar *conf_file;

  /* Path inside user's config directory */
  conf_file = g_build_filename (g_get_user_config_dir (),
                                "Mousepad",
                                "settings.conf",
                                NULL);

  /* Create a keyfile backend */
  backend = g_keyfile_settings_backend_new (conf_file, "/", NULL);
  g_free (conf_file);

  for (i = 0; i < MOUSEPAD_NUM_SCHEMAS; i++)
    {
      self->settings[i] = g_object_new (G_TYPE_SETTINGS,
                                        "backend", backend,
                                        "schema-id", mousepad_schema_ids[i],
                                        NULL);
      /* TODO: need to cleanup backend reference? */
    }
#else
  for (i = 0; i < MOUSEPAD_NUM_SCHEMAS; i++)
    self->settings[i] = g_settings_new (mousepad_schema_ids[i]);
#endif
}



/* Called at exit to cleanup the MousepadSettings singleton */
static void
mousepad_settings_cleanup_default (void)
{
  /* cleanup the singleton settings instance */
  if (MOUSEPAD_IS_SETTINGS (default_settings))
    {
      g_object_unref (default_settings);
      default_settings = NULL;
    }
}



static void
mousepad_settings_update_gsettings_schema_dir (void)
{
  const gchar *old_value = g_getenv ("GSETTINGS_SCHEMA_DIR");
  /* Append to existing env. var. */
  if (old_value != NULL)
    {
      gchar *new_value;
#ifndef G_OS_WIN32
      const gchar *pathsep = ":";
#else
      const gchar *pathsep = ";";
#endif
      new_value = g_strconcat (old_value, pathsep, MOUSEPAD_GSETTINGS_SCHEMA_DIR, NULL);
      g_setenv ("GSETTINGS_SCHEMA_DIR", new_value, TRUE);
      g_free (new_value);
    }
  /* Create a new env. var. */
  else
    {
      g_setenv ("GSETTINGS_SCHEMA_DIR", MOUSEPAD_GSETTINGS_SCHEMA_DIR, FALSE);
    }
}



/* The first time it's called, it constructs the singleton instance that
 * is always returned and is automatically cleaned up during normal exit. */
MousepadSettings *
mousepad_settings_get_default (void)
{
  static gsize default_initialized = 0;

  if (g_once_init_enter (&default_initialized))
    {

      /* If we're installed in an unusual location, we still want to load
       * the schema so enforce this with the relevant environment variable. */
      mousepad_settings_update_gsettings_schema_dir ();

      default_settings = g_object_new (MOUSEPAD_TYPE_SETTINGS, NULL);

      /* Auto-cleanup at exit */
      atexit (mousepad_settings_cleanup_default);

      /* Never enter this block again */
      g_once_init_leave (&default_initialized, 1);
    }

  g_warn_if_fail (MOUSEPAD_IS_SETTINGS (default_settings));

  return default_settings;
}



GSettings *
mousepad_settings_get_from_schema (MousepadSettings *settings,
                                   MousepadSchema    schema)
{
  g_return_val_if_fail (MOUSEPAD_IS_SETTINGS (settings), NULL);
  g_return_val_if_fail (schema < MOUSEPAD_NUM_SCHEMAS, NULL);

  return settings->settings[schema];
}



void
mousepad_settings_bind (MousepadSchema     schema,
                        const gchar       *key,
                        gpointer           object,
                        const gchar       *prop,
                        GSettingsBindFlags flags)
{
  MousepadSettings *settings;
  g_return_if_fail (schema < MOUSEPAD_NUM_SCHEMAS);
  settings = mousepad_settings_get_default ();
  g_settings_bind (settings->settings[schema], key, object, prop, flags);
}



gulong
mousepad_settings_connect_changed (MousepadSchema     schema,
                                   const gchar       *key,
                                   GCallback          callback,
                                   gpointer           user_data,
                                   GSignalFlags       connect_flags)
{
  gulong            signal_id;
  gchar            *signal_name;
  MousepadSettings *settings;

  g_return_val_if_fail (schema < MOUSEPAD_NUM_SCHEMAS, 0);
  g_return_val_if_fail (callback != NULL, 0);

  if (key != NULL)
    signal_name = g_strdup_printf ("changed::%s", key);
  else
    signal_name = g_strdup ("changed");

  settings = mousepad_settings_get_default ();

  signal_id = g_signal_connect_data (settings->settings[schema],
                                     signal_name,
                                     callback,
                                     user_data,
                                     NULL,
                                     connect_flags);

  g_free (signal_name);

  return signal_id;
}



gboolean
mousepad_settings_get_boolean (MousepadSchema  schema,
                               const gchar    *key)
{
  MousepadSettings *settings;
  g_return_val_if_fail (schema < MOUSEPAD_NUM_SCHEMAS, FALSE);
  settings = mousepad_settings_get_default ();
  return g_settings_get_boolean (settings->settings[schema], key);
}



void
mousepad_settings_set_boolean (MousepadSchema  schema,
                               const gchar    *key,
                               gboolean        value)
{
  MousepadSettings *settings;
  g_return_if_fail (schema < MOUSEPAD_NUM_SCHEMAS);
  settings = mousepad_settings_get_default ();
  g_settings_set_boolean (settings->settings[schema], key, value);
}



gint
mousepad_settings_get_int (MousepadSchema  schema,
                           const gchar    *key)
{
  MousepadSettings *settings;
  g_return_val_if_fail (schema < MOUSEPAD_NUM_SCHEMAS, FALSE);
  settings = mousepad_settings_get_default ();
  return g_settings_get_int (settings->settings[schema], key);
}



void
mousepad_settings_set_int (MousepadSchema  schema,
                           const gchar    *key,
                           gint            value)
{
  MousepadSettings *settings;
  g_return_if_fail (schema < MOUSEPAD_NUM_SCHEMAS);
  settings = mousepad_settings_get_default ();
  g_settings_set_int (settings->settings[schema], key, value);
}



gchar *
mousepad_settings_get_string (MousepadSchema  schema,
                              const gchar    *key)
{
  MousepadSettings *settings;
  g_return_val_if_fail (schema < MOUSEPAD_NUM_SCHEMAS, FALSE);
  settings = mousepad_settings_get_default ();
  return g_settings_get_string (settings->settings[schema], key);
}



void
mousepad_settings_set_string (MousepadSchema  schema,
                              const gchar    *key,
                              const gchar    *value)
{
  MousepadSettings *settings;
  g_return_if_fail (schema < MOUSEPAD_NUM_SCHEMAS);
  settings = mousepad_settings_get_default ();
  g_settings_set_string (settings->settings[schema], key, value);
}
