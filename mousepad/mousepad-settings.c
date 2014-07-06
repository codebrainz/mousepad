#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-settings.h>
#include <stdlib.h>

/* Needed to use keyfile GSettings backend */
# define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>



struct MousepadSettings_
{
  GSettings parent;
};



struct MousepadSettingsClass_
{
  GSettingsClass parent_class;
};



static void mousepad_settings_finalize (GObject *object);



G_DEFINE_TYPE (MousepadSettings, mousepad_settings, G_TYPE_SETTINGS)



/* Global GSettings subclass instance, accessed by mousepad_settings_get_default() */
static MousepadSettings *default_settings = NULL;



static void
mousepad_settings_class_init (MousepadSettingsClass *klass)
{
  GObjectClass *g_object_class;

  g_object_class = G_OBJECT_CLASS (klass);

  g_object_class->finalize = mousepad_settings_finalize;
}



static void
mousepad_settings_finalize (GObject *object)
{/*
  MousepadSettings *self;

  g_return_if_fail (MOUSEPAD_IS_SETTINGS (object));

  self = MOUSEPAD_SETTINGS (object);
*/
  G_OBJECT_CLASS (mousepad_settings_parent_class)->finalize (object);
}



static void
mousepad_settings_init (MousepadSettings *self)
{
}



/* Called at exit to cleanup the MousepadSettings singleton */
static void
mousepad_settings_cleanup_default (void)
{
  /* last-ditch attempt to save settings to disk */
  MOUSEPAD_GSETTINGS_SYNC ();

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
      GSettingsBackend *backend;

      /* If we're installed in an unusual location, we still want to load
       * the schema so enforce this with the relevant environment variable. */
      mousepad_settings_update_gsettings_schema_dir ();

#ifndef MOUSEPAD_GSETTINGS_USE_DBUS
      gchar *conf_file;

      /* Path inside user's config directory */
      conf_file = g_build_filename (g_get_user_config_dir (),
                                    "mousepad",
                                    "settings.conf",
                                    NULL);

      /* Always use the keyfile backend */
      backend = g_keyfile_settings_backend_new (conf_file, "/", NULL);
      g_free (conf_file);
#else
      backend = g_settings_backend_get_default ();
#endif

      /* Construct the singleton instance */
      default_settings = g_object_new (
        MOUSEPAD_TYPE_SETTINGS,
        "backend", backend /* give ref to settings object */,
        "schema-id", "org.xfce.Mousepad",
        NULL);

      /* Auto-cleanup at exit */
      atexit (mousepad_settings_cleanup_default);

      /* Never enter this block again */
      g_once_init_leave (&default_initialized, 1);
    }

  g_warn_if_fail (MOUSEPAD_IS_SETTINGS (default_settings));

  return default_settings;
}
