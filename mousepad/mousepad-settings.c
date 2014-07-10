#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-settings.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef MOUSEPAD_SETTINGS_KEYFILE_BACKEND
/* Needed to use keyfile GSettings backend */
# define G_SETTINGS_ENABLE_BACKEND
# include <gio/gsettingsbackend.h>
#endif



typedef enum
{
  MOUSEPAD_SCHEMA_VIEW_SETTINGS,
  MOUSEPAD_SCHEMA_WINDOW_SETTINGS,
  MOUSEPAD_SCHEMA_SEARCH_STATE,
  MOUSEPAD_SCHEMA_WINDOW_STATE,
  MOUSEPAD_NUM_SCHEMAS
}
MousepadSchema;



static const gchar *
mousepad_schema_ids[MOUSEPAD_NUM_SCHEMAS] =
{
  "org.xfce.mousepad.preferences.view",
  "org.xfce.mousepad.preferences.window",
  "org.xfce.mousepad.state.search",
  "org.xfce.mousepad.state.window"
};



static GSettings *mousepad_settings[MOUSEPAD_NUM_SCHEMAS] = { NULL };
static gint       mousepad_settings_init_count            = 0;



G_LOCK_DEFINE (settings_lock);
#define MOUSEPAD_SETTINGS_LOCK()   G_LOCK (settings_lock)
#define MOUSEPAD_SETTINGS_UNLOCK() G_UNLOCK (settings_lock)



void
mousepad_settings_finalize (void)
{
  gint i;

  MOUSEPAD_SETTINGS_LOCK ();

  g_settings_sync ();

  mousepad_settings_init_count--;
  if (mousepad_settings_init_count > 0)
    {
      MOUSEPAD_SETTINGS_UNLOCK ();
      return;
    }
  MOUSEPAD_SETTINGS_UNLOCK ();

  for (i = 0; i < MOUSEPAD_NUM_SCHEMAS; i++)
    {
      if (G_IS_SETTINGS (mousepad_settings[i]))
        {
          g_object_unref (mousepad_settings[i]);
          mousepad_settings[i] = NULL;
        }
    }
}



static void
mousepad_settings_update_gsettings_schema_dir (void)
{
  const gchar *old_value;

  old_value = g_getenv ("GSETTINGS_SCHEMA_DIR");
  /* Append to existing env. var. if not in there yet */
  if (old_value != NULL &&
      strstr (old_value, MOUSEPAD_GSETTINGS_SCHEMA_DIR) != NULL)
    {
      gchar       *new_value;
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



void
mousepad_settings_init (void)
{

  MOUSEPAD_SETTINGS_LOCK ();

  if (mousepad_settings_init_count == 0)
    {
      gint i;

      /* If we're installed in an unusual location, we still want to load
       * the schema so enforce this with the relevant environment variable. */
      mousepad_settings_update_gsettings_schema_dir ();

#ifdef MOUSEPAD_SETTINGS_KEYFILE_BACKEND
{
      GSettingsBackend *backend;
      gchar            *conf_file;

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
          mousepad_settings[i] = g_object_new (G_TYPE_SETTINGS,
                                               "backend", backend,
                                               "schema-id", mousepad_schema_ids[i],
                                               NULL);
        }

      g_object_unref (backend);
}
#else
      for (i = 0; i < MOUSEPAD_NUM_SCHEMAS; i++)
        mousepad_settings[i] = g_settings_new (mousepad_schema_ids[i]);
#endif
    }

    mousepad_settings_init_count++;

    MOUSEPAD_SETTINGS_UNLOCK ();
}



/* checks that string starts and ends with alnum character and contains only
 * alnum, underscore or dash/hyphen characters */
static gboolean
mousepad_settings_check_path_part (const gchar *s,
                                   gint         len)
{
  gint i;

  if (! isalnum (s[0]) || ! isalnum (s[len-1]))
    return FALSE;

  for (i = 0; i < len ; i++)
    {
      if (! isalnum (s[i]) && s[i] != '-' && s[i] != '_')
        return FALSE;
    }

  return TRUE;
}




static gboolean
mousepad_settings_parse_path_names (const gchar  *path,
                                    const gchar **type,
                                    gint         *type_length,
                                    const gchar **schema,
                                    gint         *schema_length,
                                    const gchar **key,
                                    gint         *key_length,
                                    gboolean      validate_parts)
{
  const gchar *t=NULL, *s=NULL, *k=NULL, *p;
  gint         tl=0, sl=0, kl=0;

  if (G_UNLIKELY (! path || ! path[0]))
    return FALSE;

  p = path;
  while (*p)
    {
      if (*p == '/' || p == path /* first char but not a slash */)
        {
          if (t == NULL)
            {
              /* skip leading slash if exists */
              t = (*p == '/') ? ++p : p++;
              continue;
            }
          else if (s == NULL)
            {
              tl = p - t;
              s = ++p;
              continue;
            }
          else if (k == NULL)
            {
              sl = p - s;
              k = ++p;
              continue;
            }
        }
      ++p;
    }

  kl = p - k;
  /* remove trailing slash if it exists */
  if (k[kl - 1] == '/')
    kl--;

  /* sanity checking on what was actually parsed (or not) */
  if (!t || !s || !k || !tl || !sl || !kl ||
      (validate_parts &&
       (! mousepad_settings_check_path_part (t, tl) ||
        ! mousepad_settings_check_path_part (s, sl) ||
        ! mousepad_settings_check_path_part (k, kl))))
    {
        return FALSE;
    }

  /* return desired values to caller */
  if (type)          *type          = t;
  if (type_length)   *type_length   = tl;
  if (schema)        *schema        = s;
  if (schema_length) *schema_length = sl;
  if (key)           *key           = k;
  if (key_length)    *key_length    = kl;

  return TRUE;
}



static MousepadSchema
mousepad_settings_schema_from_names (const gchar *type,
                                     gint         type_len,
                                     const gchar *name,
                                     gint         name_len)
{
  if (type_len == 11 && strncmp ("preferences", type, type_len) == 0)
    {
      if (name_len == 4 && strncmp ("view", name, name_len) == 0)
        return MOUSEPAD_SCHEMA_VIEW_SETTINGS;
      else if (name_len == 6 && strncmp ("window", name, name_len) == 0)
        return MOUSEPAD_SCHEMA_WINDOW_SETTINGS;
    }
  else if (type_len == 5 && strncmp ("state", type, type_len) == 0)
    {
      if (name_len == 6 && strncmp ("search", name, name_len) == 0)
        return MOUSEPAD_SCHEMA_SEARCH_STATE;
      else if (name_len == 6 && strncmp ("window", name, name_len) == 0)
        return MOUSEPAD_SCHEMA_WINDOW_STATE;
    }
  return MOUSEPAD_NUM_SCHEMAS; /* not found */
}



static MousepadSchema
mousepad_settings_parse_path (const gchar  *path,
                              const gchar **key_name)
{
  const gchar *type_name, *schema_name;
  gint         type_len, schema_len, key_len;

  if (mousepad_settings_parse_path_names (path,
                                          &type_name,
                                          &type_len,
                                          &schema_name,
                                          &schema_len,
                                          key_name,
                                          &key_len,
                                          TRUE))
    {
      MousepadSchema  schema;

      schema = mousepad_settings_schema_from_names (type_name,
                                                    type_len,
                                                    schema_name,
                                                    schema_len);

      if (schema == MOUSEPAD_NUM_SCHEMAS && key_name != NULL)
        *key_name = NULL;

      return schema;
    }
  else if (key_name != NULL)
    {
      *key_name = NULL;
    }

  return MOUSEPAD_NUM_SCHEMAS;
}



gboolean
mousepad_setting_bind (const gchar       *path,
                       gpointer           object,
                       const gchar       *prop,
                       GSettingsBindFlags flags)
{
  gboolean       result = FALSE;
  const gchar   *key_name;
  MousepadSchema schema;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (object != NULL, FALSE);
  g_return_val_if_fail (prop != NULL, FALSE);


  schema = mousepad_settings_parse_path (path, &key_name);

  if (G_LIKELY (schema != MOUSEPAD_NUM_SCHEMAS))
    {
      MOUSEPAD_SETTINGS_LOCK ();
      g_settings_bind (mousepad_settings[schema], key_name, object, prop, flags);
      MOUSEPAD_SETTINGS_UNLOCK ();
      result = TRUE;
    }

  return result;
}



gulong
mousepad_setting_connect (const gchar  *path,
                           GCallback    callback,
                           gpointer     user_data,
                           GSignalFlags connect_flags)
{
  gulong         signal_id = 0;
  const gchar   *key_name;
  MousepadSchema schema;

  g_return_val_if_fail (path != NULL, 0);
  g_return_val_if_fail (callback != NULL, 0);

  schema = mousepad_settings_parse_path (path, &key_name);

  if (G_LIKELY (schema != MOUSEPAD_NUM_SCHEMAS))
    {
      gchar *signal_name;

      signal_name = g_strdup_printf ("changed::%s", key_name);

      MOUSEPAD_SETTINGS_LOCK ();
      signal_id = g_signal_connect_data (mousepad_settings[schema],
                                         signal_name,
                                         callback,
                                         user_data,
                                         NULL,
                                         connect_flags);
      MOUSEPAD_SETTINGS_UNLOCK ();

      g_free (signal_name);
    }

  return signal_id;
}



void
mousepad_setting_disconnect (const gchar *path,
                             gulong       handler_id)
{
  MousepadSchema schema;

  g_return_if_fail (path != NULL);
  g_return_if_fail (handler_id > 0);

  schema = mousepad_settings_parse_path (path, NULL);

  if (G_LIKELY (schema != MOUSEPAD_NUM_SCHEMAS))
    g_signal_handler_disconnect (mousepad_settings[schema], handler_id);
  else
    g_warn_if_reached ();
}



gboolean
mousepad_setting_get (const gchar *path,
                      const gchar *format_string,
                      ...)
{
  gboolean       result = FALSE;
  const gchar   *key_name;
  MousepadSchema schema;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (format_string != NULL, FALSE);

  schema = mousepad_settings_parse_path (path, &key_name);

  if (G_LIKELY (schema != MOUSEPAD_NUM_SCHEMAS))
    {

      GVariant *variant;
      va_list   ap;

      MOUSEPAD_SETTINGS_LOCK ();
      variant = g_settings_get_value (mousepad_settings[schema], key_name);
      MOUSEPAD_SETTINGS_UNLOCK ();

      g_variant_ref_sink (variant);

      va_start (ap, format_string);
      g_variant_get_va (variant, format_string, NULL, &ap);
      va_end (ap);

      g_variant_unref (variant);

      result = TRUE;
    }

  return result;
}



gboolean
mousepad_setting_set (const gchar *path,
                      const gchar *format_string,
                      ...)
{
  gboolean       result = FALSE;
  const gchar   *key_name;
  MousepadSchema schema;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (format_string != NULL, FALSE);

  schema = mousepad_settings_parse_path (path, &key_name);

  if (G_LIKELY (schema != MOUSEPAD_NUM_SCHEMAS))
    {
      GVariant *variant;
      va_list   ap;

      va_start (ap, format_string);
      variant = g_variant_new_va (format_string, NULL, &ap);
      va_end (ap);

      g_variant_ref_sink (variant);

      MOUSEPAD_SETTINGS_LOCK ();
      g_settings_set_value (mousepad_settings[schema], key_name, variant);
      MOUSEPAD_SETTINGS_UNLOCK ();

      g_variant_unref (variant);

      result = TRUE;
    }


  return result;
}



gboolean
mousepad_setting_get_boolean (const gchar *path)
{
  gboolean value = FALSE;
  gboolean result = mousepad_setting_get (path, "b", &value);
  g_warn_if_fail (result);
  return value;
}



void
mousepad_setting_set_boolean (const gchar *path,
                              gboolean     value)
{
  mousepad_setting_set (path, "b", value);
}



gint
mousepad_setting_get_int (const gchar *path)
{
  gint     value = 0;
  gboolean result = mousepad_setting_get (path, "i", &value);
  g_warn_if_fail (result);
  return value;
}



void
mousepad_setting_set_int (const gchar *path,
                          gint         value)
{
  mousepad_setting_set (path, "i", value);
}



gchar *
mousepad_setting_get_string (const gchar *path)
{
  gchar   *value = NULL;
  gboolean result = mousepad_setting_get (path, "s", &value);
  g_warn_if_fail (result);
  return value;
}



void
mousepad_setting_set_string (const gchar *path,
                             const gchar *value)
{
  mousepad_setting_set (path, "s", value);
}



gint
mousepad_setting_get_enum (const gchar *path)
{
  gint           result = 0;
  const gchar   *key_name;
  MousepadSchema schema;

  g_return_val_if_fail (path != NULL, FALSE);

  schema = mousepad_settings_parse_path (path, &key_name);

  if (G_LIKELY (schema != MOUSEPAD_NUM_SCHEMAS))
    {
      MOUSEPAD_SETTINGS_LOCK ();
      result = g_settings_get_enum (mousepad_settings[schema], key_name);
      MOUSEPAD_SETTINGS_UNLOCK ();
    }
  else
    g_warn_if_reached ();

  return result;
}



void
mousepad_setting_set_enum (const gchar *path,
                           gint         value)
{
  const gchar   *key_name;
  MousepadSchema schema;

  g_return_val_if_fail (path != NULL, FALSE);

  schema = mousepad_settings_parse_path (path, &key_name);

  if (G_LIKELY (schema != MOUSEPAD_NUM_SCHEMAS))
    {
      MOUSEPAD_SETTINGS_LOCK ();
      g_settings_set_enum (mousepad_settings[schema], key_name, value);
      MOUSEPAD_SETTINGS_UNLOCK ();
    }
  else
    g_warn_if_reached ();
}
