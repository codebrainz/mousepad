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
#include <mousepad/mousepad-settings.h>
#include <mousepad/mousepad-settings-store.h>

#include <ctype.h>



static MousepadSettingsStore *settings_store = NULL;
static gint settings_init_count = 0;



void
mousepad_settings_finalize (void)
{
  g_settings_sync ();

  settings_init_count--;
  if (settings_init_count > 0)
    return;

  if (MOUSEPAD_IS_SETTINGS_STORE (settings_store))
    {
      g_object_unref (settings_store);
      settings_store = NULL;
    }
}



void
mousepad_settings_init (void)
{

  if (settings_init_count == 0)
    {
      if (! MOUSEPAD_IS_SETTINGS_STORE (settings_store))
        settings_store = mousepad_settings_store_new ();
    }

  settings_init_count++;
}



gboolean
mousepad_setting_bind (const gchar       *path,
                       gpointer           object,
                       const gchar       *prop,
                       GSettingsBindFlags flags)
{
  gboolean     result = FALSE;
  const gchar *key_name = NULL;
  GSettings   *settings = NULL;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
  g_return_val_if_fail (prop != NULL, FALSE);

  if (mousepad_settings_store_lookup (settings_store, path, &key_name, &settings))
    {
      g_settings_bind (settings, key_name, object, prop, flags);
      return TRUE;
    }

  return result;
}



gulong
mousepad_setting_connect (const gchar   *path,
                           GCallback     callback,
                           gpointer      user_data,
                           GConnectFlags connect_flags)
{
  gulong       signal_id = 0;
  const gchar *key_name = NULL;
  GSettings   *settings = NULL;

  g_return_val_if_fail (path != NULL, 0);
  g_return_val_if_fail (callback != NULL, 0);

  if (mousepad_settings_store_lookup (settings_store, path, &key_name, &settings))
    {
      gchar *signal_name;

      signal_name = g_strdup_printf ("changed::%s", key_name);

      signal_id = g_signal_connect_data (settings,
                                         signal_name,
                                         callback,
                                         user_data,
                                         NULL,
                                         connect_flags);

      g_free (signal_name);
    }

  return signal_id;
}



gulong
mousepad_setting_connect_object (const gchar  *path,
                                 GCallback     callback,
                                 gpointer      gobject,
                                 GConnectFlags connect_flags)
{
  gulong       signal_id = 0;
  const gchar *key_name = NULL;
  GSettings   *settings = NULL;

  g_return_val_if_fail (path != NULL, 0);
  g_return_val_if_fail (callback != NULL, 0);
  g_return_val_if_fail (G_IS_OBJECT (gobject), 0);

  if (mousepad_settings_store_lookup (settings_store, path, &key_name, &settings))
    {
      gchar *signal_name;

      signal_name = g_strdup_printf ("changed::%s", key_name);

      signal_id = g_signal_connect_object (settings,
                                           signal_name,
                                           callback,
                                           gobject,
                                           connect_flags);

      g_free (signal_name);
    }

  return signal_id;
}



void
mousepad_setting_disconnect (const gchar *path,
                             gulong       handler_id)
{
  GSettings *settings;

  g_return_if_fail (path != NULL);
  g_return_if_fail (handler_id > 0);

  settings = mousepad_settings_store_lookup_settings (settings_store, path);

  if (G_IS_SETTINGS (settings))
    g_signal_handler_disconnect (settings, handler_id);
  else
    g_warn_if_reached ();
}



gboolean
mousepad_setting_get (const gchar *path,
                      const gchar *format_string,
                      ...)
{
  gboolean     result = FALSE;
  const gchar *key_name = NULL;
  GSettings   *settings = NULL;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (format_string != NULL, FALSE);

  if (mousepad_settings_store_lookup (settings_store, path, &key_name, &settings))
    {
      GVariant *variant;
      va_list   ap;

      variant = g_settings_get_value (settings, key_name);

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
  gboolean     result = FALSE;
  const gchar *key_name = NULL;
  GSettings   *settings = NULL;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (format_string != NULL, FALSE);

  if (mousepad_settings_store_lookup (settings_store, path, &key_name, &settings))
    {
      GVariant *variant;
      va_list   ap;

      va_start (ap, format_string);
      variant = g_variant_new_va (format_string, NULL, &ap);
      va_end (ap);

      g_variant_ref_sink (variant);

      g_settings_set_value (settings, key_name, variant);

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
  mousepad_setting_set (path, "s", value != NULL ? value : "");
}



gint
mousepad_setting_get_enum (const gchar *path)
{
  gint         result = 0;
  const gchar *key_name = NULL;
  GSettings   *settings = NULL;

  g_return_val_if_fail (path != NULL, FALSE);

  if (mousepad_settings_store_lookup (settings_store, path, &key_name, &settings))
    result = g_settings_get_enum (settings, key_name);
  else
    g_warn_if_reached ();

  return result;
}



void
mousepad_setting_set_enum (const gchar *path,
                           gint         value)
{
  const gchar *key_name = NULL;
  GSettings   *settings = NULL;

  g_return_if_fail (path != NULL);

  if (mousepad_settings_store_lookup (settings_store, path, &key_name, &settings))
    g_settings_set_enum (settings, key_name, value);
  else
    g_warn_if_reached ();
}
