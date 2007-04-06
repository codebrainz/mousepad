/* $Id$ */
/*
 * Copyright (c) 2007 Nick Schermer <nick@xfce.org>
 * Copyright (c) 2007 Benedikt Meurer <benny@xfce.org>
 *
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib-object.h>

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-preferences.h>



enum
{
  PROP_0,
  PROP_FONT_NAME,
  PROP_LAST_MATCH_CASE,
  PROP_LAST_STATUSBAR_VISIBLE,
  PROP_LAST_WINDOW_HEIGHT,
  PROP_LAST_WINDOW_WIDTH,
  PROP_LAST_WRAP_AROUND,
  PROP_WORD_WRAP,
  PROP_MISC_ALWAYS_SHOW_TABS,
  PROP_MISC_CYCLE_TABS,
  PROP_MISC_SHOW_FULL_PATH_IN_TITLE,
  PROP_MISC_RECENT_MENU_LIMIT,
  PROP_MISC_REMEMBER_GEOMETRY,
  N_PROPERTIES,
};

static void     transform_string_to_boolean             (const GValue             *src,
                                                         GValue                   *dst);
static void     transform_string_to_int                 (const GValue             *src,
                                                         GValue                   *dst);
static void     mousepad_preferences_class_init         (MousepadPreferencesClass *klass);
static void     mousepad_preferences_init               (MousepadPreferences      *preferences);
static void     mousepad_preferences_finalize           (GObject                  *object);
static void     mousepad_preferences_get_property       (GObject                  *object,
                                                         guint                     prop_id,
                                                         GValue                   *value,
                                                         GParamSpec               *pspec);
static void     mousepad_preferences_set_property       (GObject                  *object,
                                                         guint                     prop_id,
                                                         const GValue             *value,
                                                         GParamSpec               *pspec);
static void     mousepad_preferences_queue_store        (MousepadPreferences      *preferences);
static gboolean mousepad_preferences_load               (MousepadPreferences      *preferences);
static gboolean mousepad_preferences_store_idle         (gpointer                  user_data);
static void     mousepad_preferences_store_idle_destroy (gpointer                  user_data);

struct _MousepadPreferencesClass
{
  GObjectClass __parent__;
};

struct _MousepadPreferences
{
  GObject  __parent__;

  /* all the properties stored in the Object */
  GValue   values[N_PROPERTIES];

  /* idle save timer id */
  gint     store_idle_id;
};



static GObjectClass *mousepad_preferences_parent_class;


/**
 * transform_string_to_boolean:
 * @const GValue : String #GValue.
 * @GValue       : Return location for a #GValue boolean.
 *
 * Converts a string into a boolean. This is used when
 * converting the XfceRc values.
 **/
static void
transform_string_to_boolean (const GValue *src,
                             GValue       *dst)
{
  g_value_set_boolean (dst, strcmp (g_value_get_string (src), "FALSE") != 0);
}



/**
 * transform_string_to_int:
 * @const GValue : String #GValue.
 * @GValue       : Return location for a #GValue integer.
 *
 * Converts a string into a number.
 **/
static void
transform_string_to_int (const GValue *src,
                         GValue       *dst)
{
  g_value_set_int (dst, (gint) strtol (g_value_get_string (src), NULL, 10));
}



GType
mousepad_preferences_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_type_register_static_simple (G_TYPE_OBJECT,
                                            I_("MousepadPreferences"),
                                            sizeof (MousepadPreferencesClass),
                                            (GClassInitFunc) mousepad_preferences_class_init,
                                            sizeof (MousepadPreferences),
                                            (GInstanceInitFunc) mousepad_preferences_init,
                                            0);
    }

  return type;
}



static void
mousepad_preferences_class_init (MousepadPreferencesClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  mousepad_preferences_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_preferences_finalize;
  gobject_class->get_property = mousepad_preferences_get_property;
  gobject_class->set_property = mousepad_preferences_set_property;

  /* register transformation functions */
  if (!g_value_type_transformable (G_TYPE_STRING, G_TYPE_BOOLEAN))
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_BOOLEAN, transform_string_to_boolean);
  if (!g_value_type_transformable (G_TYPE_STRING, G_TYPE_INT))
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_INT, transform_string_to_int);

  /**
   * MousepadPreferences:font-name
   *
   * The font name and size used in the text view. If this value is
   * %NULL, the system font will be used.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FONT_NAME,
                                   g_param_spec_string ("font-name",
                                                        "font-name",
                                                        "font-name",
                                                        NULL,
                                                        MOUSEPAD_PARAM_READWRITE));

  /**
   * MousepadPreferences:last-match-case
   *
   * Whether to enable match case in the search bar.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_MATCH_CASE,
                                   g_param_spec_boolean ("last-match-case",
                                                         "last-match-case",
                                                         "last-match-case",
                                                         FALSE,
                                                         MOUSEPAD_PARAM_READWRITE));

  /**
   * MousepadPreferences:last-statusbar-visible
   *
   * Whether to display the statusbar in the Mousepad window.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_STATUSBAR_VISIBLE,
                                   g_param_spec_boolean ("last-statusbar-visible",
                                                         "last-statusbar-visible",
                                                         "last-statusbar-visible",
                                                         TRUE,
                                                         MOUSEPAD_PARAM_READWRITE));

  /**
   * MousepadPreferences:last-window-height
   *
   * The last known height of a #MousepadWindow, which will be used as
   * default height for newly created windows.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_WINDOW_HEIGHT,
                                   g_param_spec_int ("last-window-height",
                                                     "last-window-height",
                                                     "last-window-height",
                                                     1, G_MAXINT, 480,
                                                     MOUSEPAD_PARAM_READWRITE));

  /**
   * MousepadPreferences:last-window-width
   *
   * The last known width of a #MousepadWindow, which will be used as
   * default width for newly created windows.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_WINDOW_WIDTH,
                                   g_param_spec_int ("last-window-width",
                                                     "last-window-width",
                                                     "last-window-width",
                                                     1, G_MAXINT, 640,
                                                     MOUSEPAD_PARAM_READWRITE));

  /**
   * MousepadPreferences:last-wrap-around
   *
   * Whether to enable wrap around in the search bar.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_WRAP_AROUND,
                                   g_param_spec_boolean ("last-wrap-around",
                                                         "last-wrap-around",
                                                         "last-wrap-around",
                                                         TRUE,
                                                         MOUSEPAD_PARAM_READWRITE));

  /**
   * MousepadPreferences:word-wrap
   *
   * Whether word wrapping is enabled.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_WORD_WRAP,
                                   g_param_spec_boolean ("word-wrap",
                                                         "word-wrap",
                                                         "word-wrap",
                                                         FALSE,
                                                         MOUSEPAD_PARAM_READWRITE));

  /**
   * MousepadPreferences:misc-always-show-tabs
   *
   * Whether tabs are always visible. By default the tabs are hidden
   * when there is only 1 screen opened.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_ALWAYS_SHOW_TABS,
                                   g_param_spec_boolean ("misc-always-show-tabs",
                                                         "misc-always-show-tabs",
                                                         "misc-always-show-tabs",
                                                         FALSE,
                                                         MOUSEPAD_PARAM_READWRITE));

  /**
   * MousepadPreferences:misc-cycle-tabs
   *
   * Whether to cycle tabs with the back and forward buttons in the go menu.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_CYCLE_TABS,
                                   g_param_spec_boolean ("misc-cycle-tabs",
                                                         "misc-cycle-tabs",
                                                         "misc-cycle-tabs",
                                                         FALSE,
                                                         MOUSEPAD_PARAM_READWRITE));

  /**
   * MousepadPreferences:misc-show-full-path-in-title
   *
   * Whether to show the full file name path in the window title.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_SHOW_FULL_PATH_IN_TITLE,
                                   g_param_spec_boolean ("misc-show-full-path-in-title",
                                                         "misc-show-full-path-in-title",
                                                         "misc-show-full-path-in-title",
                                                         FALSE,
                                                         MOUSEPAD_PARAM_READWRITE));

  /**
   * MousepadPreferences:misc-recent-menu-limit
   *
   * The number of items in the recent menu,
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_RECENT_MENU_LIMIT,
                                   g_param_spec_int ("misc-recent-menu-limit",
                                                     "misc-recent-menu-limit",
                                                     "misc-recent-menu-limit",
                                                     1, G_MAXINT, 10,
                                                     MOUSEPAD_PARAM_READWRITE));

  /**
   * MousepadPreferences:misc-remember-geometry
   *
   * Whether Mousepad should remember the size of windows and apply this
   * to newly created windows. If %TRUE the width and height are
   * saved to "last-window-width" and "last-window-height". If
   * %FALSE the user may specify the start size in "last-window-with"
   * and "last-window-height".
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_REMEMBER_GEOMETRY,
                                   g_param_spec_boolean ("misc-remember-geometry",
                                                         "misc-remember-geometry",
                                                         "misc-remember-geometry",
                                                         TRUE,
                                                         MOUSEPAD_PARAM_READWRITE));
}



static void
mousepad_preferences_init (MousepadPreferences *preferences)
{
  /* load the settings */
  mousepad_preferences_load (preferences);
}



static void
mousepad_preferences_finalize (GObject *object)
{
  MousepadPreferences *preferences = MOUSEPAD_PREFERENCES (object);
  guint                n;

  /* flush preferences */
  if (G_UNLIKELY (preferences->store_idle_id != 0))
    {
      mousepad_preferences_store_idle (preferences);
      g_source_remove (preferences->store_idle_id);
    }
  /* release the property values */
  for (n = 1; n < N_PROPERTIES; ++n)
    if (G_IS_VALUE (preferences->values + n))
      g_value_unset (preferences->values + n);

  (*G_OBJECT_CLASS (mousepad_preferences_parent_class)->finalize) (object);
}



static void
mousepad_preferences_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  MousepadPreferences *preferences = MOUSEPAD_PREFERENCES (object);
  GValue              *src;

  src = preferences->values + prop_id;
  if (G_IS_VALUE (src))
    g_value_copy (src, value);
  else
    g_param_value_set_default (pspec, value);
}



static void
mousepad_preferences_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  MousepadPreferences *preferences = MOUSEPAD_PREFERENCES (object);
  GValue              *dst;

  dst = preferences->values + prop_id;
  if (G_UNLIKELY (!G_IS_VALUE (dst)))
    g_value_init (dst, pspec->value_type);

  if (g_param_values_cmp (pspec, value, dst) != 0)
    {
      g_value_copy (value, dst);
      mousepad_preferences_queue_store (preferences);
    }
}



static void
mousepad_preferences_queue_store (MousepadPreferences *preferences)
{
  if (preferences->store_idle_id == 0)
    {
      preferences->store_idle_id = g_idle_add_full (G_PRIORITY_LOW, mousepad_preferences_store_idle,
                                                    preferences, mousepad_preferences_store_idle_destroy);
    }
}



static gchar*
property_name_to_option_name (const gchar *property_name)
{
  const gchar *s;
  gboolean     upper = TRUE;
  gchar       *option;
  gchar       *t;

  option = g_new (gchar, strlen (property_name) + 1);
  for (s = property_name, t = option; *s != '\0'; ++s)
    {
      if (*s == '-')
        {
          upper = TRUE;
        }
      else if (upper)
        {
          *t++ = g_ascii_toupper (*s);
          upper = FALSE;
        }
      else
        {
          *t++ = *s;
        }
    }
  *t = '\0';

  return option;
}



static gboolean
mousepad_preferences_load (MousepadPreferences *preferences)
{
  const gchar  *string;
  GParamSpec  **specs;
  GParamSpec   *spec;
  XfceRc       *rc;
  GValue        dst = { 0, };
  GValue        src = { 0, };
  gchar        *option;
  guint         nspecs;
  guint         n;

  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "Mousepad/mousepadrc", TRUE);
  if (G_UNLIKELY (rc == NULL))
    {
      g_warning ("Failed to load the preferences. Unable to open \"~/.config/Mousepad/mousepadrc\"");
      return FALSE;
    }

  g_object_freeze_notify (G_OBJECT (preferences));

  xfce_rc_set_group (rc, "Configuration");

  specs = g_object_class_list_properties (G_OBJECT_GET_CLASS (preferences), &nspecs);

  for (n = 0; n < nspecs; ++n)
    {
      spec = specs[n];

      option = property_name_to_option_name (spec->name);
      string = xfce_rc_read_entry (rc, option, NULL);
      g_free (option);

      if (G_UNLIKELY (string == NULL))
        continue;

      g_value_init (&src, G_TYPE_STRING);
      g_value_set_static_string (&src, string);

      if (spec->value_type == G_TYPE_STRING)
        {
          g_object_set_property (G_OBJECT (preferences), spec->name, &src);
        }
      else if (g_value_type_transformable (G_TYPE_STRING, spec->value_type))
        {
          g_value_init (&dst, spec->value_type);
          if (g_value_transform (&src, &dst))
            g_object_set_property (G_OBJECT (preferences), spec->name, &dst);
          g_value_unset (&dst);
        }
      else
        {
          g_warning ("Failed to load property \"%s\"", spec->name);
        }

      g_value_unset (&src);
    }
  g_free (specs);

  xfce_rc_close (rc);

  g_object_thaw_notify (G_OBJECT (preferences));

  return FALSE;
}



static gboolean
mousepad_preferences_store_idle (gpointer user_data)
{
  MousepadPreferences *preferences = MOUSEPAD_PREFERENCES (user_data);
  const gchar         *string;
  GParamSpec         **specs;
  GParamSpec          *spec;
  XfceRc              *rc;
  GValue               dst = { 0, };
  GValue               src = { 0, };
  gchar               *option;
  guint                nspecs;
  guint                n;

  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "Mousepad/mousepadrc", FALSE);
  if (G_UNLIKELY (rc == NULL))
    {
      g_warning ("Failed to store the preferences. Unable to open \"~/.config/Mousepad/mousepadrc\"");
      return FALSE;
    }

  xfce_rc_set_group (rc, "Configuration");

  specs = g_object_class_list_properties (G_OBJECT_GET_CLASS (preferences), &nspecs);

  for (n = 0; n < nspecs; ++n)
    {
      spec = specs[n];

      g_value_init (&dst, G_TYPE_STRING);

      if (spec->value_type == G_TYPE_STRING)
        {
          g_object_get_property (G_OBJECT (preferences), spec->name, &dst);
        }
      else
        {
          g_value_init (&src, spec->value_type);
          g_object_get_property (G_OBJECT (preferences), spec->name, &src);
          g_value_transform (&src, &dst);
          g_value_unset (&src);
        }

      /* determine the option name for the spec */
      option = property_name_to_option_name (spec->name);

      /* store the setting */
      string = g_value_get_string (&dst);
      if (G_LIKELY (string != NULL))
        xfce_rc_write_entry (rc, option, string);

      /* cleanup */
      g_value_unset (&dst);
      g_free (option);
    }

  /* cleanup */
  xfce_rc_close (rc);
  g_free (specs);

  return FALSE;
}



static void
mousepad_preferences_store_idle_destroy (gpointer user_data)
{
  MOUSEPAD_PREFERENCES (user_data)->store_idle_id = 0;
}



/**
 * mousepad_preferences_get:
 *
 * Queries the global #MousepadPreferences instance, which is shared
 * by all modules. The function automatically takes a reference
 * for the caller, so you'll need to call g_object_unref() when
 * you're done with it.
 *
 * Return value: the global #MousepadPreferences instance.
 **/
MousepadPreferences*
mousepad_preferences_get (void)
{
  static MousepadPreferences *preferences = NULL;

  if (G_UNLIKELY (preferences == NULL))
    {
      preferences = g_object_new (MOUSEPAD_TYPE_PREFERENCES, NULL);
      g_object_add_weak_pointer (G_OBJECT (preferences),
                                 (gpointer) &preferences);
    }
  else
    {
      g_object_ref (G_OBJECT (preferences));
    }

  return preferences;
}
