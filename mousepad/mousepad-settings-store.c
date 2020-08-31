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

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-settings-store.h>



#ifdef MOUSEPAD_SETTINGS_KEYFILE_BACKEND
/* Needed to use keyfile GSettings backend */
# define G_SETTINGS_ENABLE_BACKEND
# include <gio/gsettingsbackend.h>
#endif

#define MOUSEPAD_SCHEMA_ID "org.xfce.mousepad"
/* length of "/org/xfce/mousepad" */
#define MOUSEPAD_PREFIX_PATH_LENGTH 18


struct MousepadSettingsStore_
{
  GObject     parent;
  GSettings  *root;
  GHashTable *keys;
};



struct MousepadSettingsStoreClass_
{
  GObjectClass parent_class;
};



typedef struct
{
  const gchar *name;
  GSettings   *settings;
}
MousepadSettingKey;



static void mousepad_settings_store_finalize (GObject *object);



G_DEFINE_TYPE (MousepadSettingsStore, mousepad_settings_store, G_TYPE_OBJECT)



static MousepadSettingKey *
mousepad_setting_key_new (const gchar *key_name,
                          GSettings   *settings)
{
  MousepadSettingKey *key;

  key = g_slice_new0 (MousepadSettingKey);
  key->name = g_intern_string (key_name);
  key->settings = g_object_ref (settings);

  return key;
}



static void
mousepad_setting_key_free (MousepadSettingKey *key)
{
  if (G_LIKELY (key != NULL))
    {
      g_object_unref (key->settings);
      g_slice_free (MousepadSettingKey, key);
    }
}



static void
mousepad_settings_store_update_env (void)
{
  const gchar *old_value = g_getenv ("GSETTINGS_SCHEMA_DIR");
  gchar       *new_value = NULL;

  /* append to path */
  if (old_value != NULL)
    {
      gchar **paths = g_strsplit (old_value, G_SEARCHPATH_SEPARATOR_S, 0);
      gchar **paths2;
      gsize   len = g_strv_length (paths);

      paths2 = g_realloc (paths, len + 2);
      if (paths2 != NULL)
        {
          paths = paths2;
          paths[len++] = g_strdup (MOUSEPAD_GSETTINGS_SCHEMA_DIR);
          paths[len] = NULL;
          new_value = g_strjoinv (G_SEARCHPATH_SEPARATOR_S, paths);
        }
      g_strfreev (paths);
    }

  /* set new path */
  if (new_value == NULL)
    new_value = g_strdup (MOUSEPAD_GSETTINGS_SCHEMA_DIR);

  g_setenv ("GSETTINGS_SCHEMA_DIR", new_value, TRUE);
  g_free (new_value);
}



static void
mousepad_settings_store_class_init (MousepadSettingsStoreClass *klass)
{
  GObjectClass *g_object_class;

  g_object_class = G_OBJECT_CLASS (klass);

  g_object_class->finalize = mousepad_settings_store_finalize;

  mousepad_settings_store_update_env ();
}



static void
mousepad_settings_store_finalize (GObject *object)
{
  MousepadSettingsStore *self;

  g_return_if_fail (MOUSEPAD_IS_SETTINGS_STORE (object));

  self = MOUSEPAD_SETTINGS_STORE (object);

  g_hash_table_destroy (self->keys);

  g_object_unref (self->root);

  G_OBJECT_CLASS (mousepad_settings_store_parent_class)->finalize (object);
}



static void
mousepad_settings_store_add_key (MousepadSettingsStore *self,
                                 const gchar           *path,
                                 const gchar           *key_name,
                                 GSettings             *settings)
{
  MousepadSettingKey *key;

  key = mousepad_setting_key_new (key_name, settings);

  g_hash_table_insert (self->keys, (gpointer) g_intern_string (path), key);
}



#if GLIB_CHECK_VERSION (2, 46, 0)

static void
mousepad_settings_store_add_settings (MousepadSettingsStore *self,
                                      const gchar           *schema_id,
                                      GSettingsSchemaSource *source,
                                      GSettings             *settings)
{
  GSettingsSchema *schema;
  GSettings       *child_settings;
  gchar          **keys, **keyp, **children, **childp;
  gchar           *key_path, *child_schema_id;
  const gchar     *path, *key_name, *child_name;

  /* loop through keys in schema and store mapping of their path to GSettings */
  schema = g_settings_schema_source_lookup (source, schema_id, FALSE);
  keys = g_settings_schema_list_keys (schema);
  /* skip "/org/xfce/mousepad" */
  path = g_settings_schema_get_path (schema) + MOUSEPAD_PREFIX_PATH_LENGTH;
  for (keyp = keys; keyp && *keyp; keyp++)
    {
      key_name = *keyp;
      key_path = g_strdup_printf ("%s%s", path, key_name);
      mousepad_settings_store_add_key (self, key_path, key_name, settings);
      g_free (key_path);
    }
  g_strfreev (keys);

  /* loop through child schemas and add them too */
  children = g_settings_schema_list_children (schema);
  for (childp = children; childp && *childp; childp++)
    {
      child_name = *childp;
      child_settings = g_settings_get_child (settings, child_name);
      child_schema_id = g_strdup_printf ("%s.%s", schema_id, child_name);
      mousepad_settings_store_add_settings (self, child_schema_id, source, child_settings);
      g_object_unref (child_settings);
      g_free (child_schema_id);
    }
  g_strfreev (children);
}

#else

#if G_GNUC_CHECK_VERSION (4, 3)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

static void
mousepad_settings_store_add_settings (MousepadSettingsStore *self,
                                      const gchar           *path,
                                      GSettings             *settings)
{
  gchar **keys, **keyp;
  gchar **children, **childp;

  /* loop through keys in schema and store mapping of their path to GSettings */
  keys = g_settings_list_keys (settings);
  for (keyp = keys; keyp && *keyp; keyp++)
    {
      const gchar *key_name = *keyp;
      gchar       *key_path = g_strdup_printf ("%s/%s", path, key_name);
      mousepad_settings_store_add_key (self, key_path, key_name, settings);
      g_free (key_path);
    }
  g_strfreev (keys);

  /* loop through child schemas and add them too */
  children = g_settings_list_children (settings);
  for (childp = children; childp && *childp; childp++)
    {
      const gchar *child_name = *childp;
      GSettings   *child_settings = g_settings_get_child (settings, child_name);
      gchar       *child_path = g_strdup_printf ("%s/%s", path, child_name);
      mousepad_settings_store_add_settings (self, child_path, child_settings);
      g_object_unref (child_settings);
      g_free (child_path);
    }
  g_strfreev (children);
}

#if G_GNUC_CHECK_VERSION (4, 3)
# pragma GCC diagnostic pop
#endif

#endif



static void
mousepad_settings_store_init (MousepadSettingsStore *self)
{
#if GLIB_CHECK_VERSION (2, 46, 0)
  GSettingsSchemaSource *source;
#endif

#ifdef MOUSEPAD_SETTINGS_KEYFILE_BACKEND
  GSettingsBackend *backend;
  gchar            *conf_file;
  conf_file = g_build_filename (g_get_user_config_dir (),
                                "Mousepad",
                                "settings.conf",
                                NULL);
  backend = g_keyfile_settings_backend_new (conf_file, "/", NULL);
  g_free (conf_file);
  self->root = g_settings_new_with_backend (MOUSEPAD_SCHEMA_ID, backend);
  g_object_unref (backend);
#else
  self->root = g_settings_new (MOUSEPAD_SCHEMA_ID);
#endif

  self->keys = g_hash_table_new_full (g_str_hash,
                                      g_str_equal,
                                      NULL,
                                      (GDestroyNotify) mousepad_setting_key_free);

#if GLIB_CHECK_VERSION (2, 46, 0)
  source = g_settings_schema_source_get_default ();
  mousepad_settings_store_add_settings (self, MOUSEPAD_SCHEMA_ID, source, self->root);
#else
  mousepad_settings_store_add_settings (self, "", self->root);
#endif
}



MousepadSettingsStore *
mousepad_settings_store_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_SETTINGS_STORE, NULL);
}



const gchar *
mousepad_settings_store_lookup_key_name (MousepadSettingsStore *self,
                                         const gchar           *path)
{
  const gchar *key_name = NULL;

  if (! mousepad_settings_store_lookup (self, path, &key_name, NULL))
    return NULL;

  return key_name;
}



GSettings *
mousepad_settings_store_lookup_settings (MousepadSettingsStore *self,
                                         const gchar           *path)
{
  GSettings *settings = NULL;

  if (! mousepad_settings_store_lookup (self, path, NULL, &settings))
    return NULL;

  return settings;
}



gboolean
mousepad_settings_store_lookup (MousepadSettingsStore *self,
                                const gchar           *path,
                                const gchar          **key_name,
                                GSettings            **settings)
{
  MousepadSettingKey *key;

  g_return_val_if_fail (MOUSEPAD_IS_SETTINGS_STORE (self), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  if (key_name == NULL && settings == NULL)
    return g_hash_table_contains (self->keys, path);

  key = g_hash_table_lookup (self->keys, path);

  if (G_UNLIKELY (key == NULL))
    return FALSE;

  if (key_name != NULL)
    *key_name = key->name;

  if (settings != NULL)
    *settings = key->settings;

  return TRUE;
}
