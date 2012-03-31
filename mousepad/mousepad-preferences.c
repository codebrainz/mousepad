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
#include <mousepad/mousepad-util.h>
#include <mousepad/mousepad-preferences.h>

#define GROUP_NAME "Configuration"



enum
{
  PROP_0,

  /* search preferences */
  PROP_SEARCH_DIRECTION,
  PROP_SEARCH_MATCH_CASE,
  PROP_SEARCH_MATCH_WHOLE_WORD,
  PROP_SEARCH_REPLACE_ALL_LOCATION,

  /* textview preferences */
  PROP_VIEW_AUTO_INDENT,
  PROP_VIEW_FONT_NAME,
  PROP_VIEW_LINE_NUMBERS,
  PROP_VIEW_TAB_WIDTH,
  PROP_VIEW_TABS_AS_SPACES,
  PROP_VIEW_WORD_WRAP,
  PROP_VIEW_COLOR_SCHEME,

  /* window preferences */
  PROP_WINDOW_HEIGHT,
  PROP_WINDOW_WIDTH,
  PROP_WINDOW_STATUSBAR_VISIBLE,

  /* hidden settings */
  PROP_MISC_ALWAYS_SHOW_TABS,
  PROP_MISC_CYCLE_TABS,
  PROP_MISC_DEFAULT_TAB_SIZES,
  PROP_MISC_PATH_IN_TITLE,
  PROP_MISC_RECENT_MENU_ITEMS,
  PROP_MISC_REMEMBER_GEOMETRY,
  N_PROPERTIES
};



static void     mousepad_preferences_finalize           (GObject                  *object);
static void     mousepad_preferences_get_property       (GObject                  *object,
                                                         guint                     prop_id,
                                                         GValue                   *value,
                                                         GParamSpec               *pspec);
static void     mousepad_preferences_set_property       (GObject                  *object,
                                                         guint                     prop_id,
                                                         const GValue             *value,
                                                         GParamSpec               *pspec);
#ifndef NDEBUG
static void     mousepad_preferences_check_option_name  (GParamSpec               *pspec);
#endif
static void     mousepad_preferences_load               (MousepadPreferences      *preferences);
static void     mousepad_preferences_store              (MousepadPreferences      *preferences);
static gboolean mousepad_preferences_store_idle         (gpointer                  user_data);
static void     mousepad_preferences_store_idle_destroy (gpointer                  user_data);

struct _MousepadPreferencesClass
{
  GObjectClass __parent__;
};

struct _MousepadPreferences
{
  GObject  __parent__;

  /* all the properties stored in the object */
  GValue   values[N_PROPERTIES];

  /* idle save timer id */
  gint     store_idle_id;
};



G_DEFINE_TYPE (MousepadPreferences, mousepad_preferences, G_TYPE_OBJECT);



static void
mousepad_preferences_class_init (MousepadPreferencesClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_preferences_finalize;
  gobject_class->get_property = mousepad_preferences_get_property;
  gobject_class->set_property = mousepad_preferences_set_property;

  /**
   * Search Preferences
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SEARCH_DIRECTION,
                                   g_param_spec_int ("search-direction",
                                                     "SearchDirection",
                                                     NULL,
                                                     0, 2, 1,
                                                     MOUSEPAD_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SEARCH_MATCH_CASE,
                                   g_param_spec_boolean ("search-match-case",
                                                         "SearchMatchCase",
                                                         NULL,
                                                         FALSE,
                                                         MOUSEPAD_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SEARCH_MATCH_WHOLE_WORD,
                                   g_param_spec_boolean ("search-match-whole-word",
                                                         "SearchMatchWholeWord",
                                                         NULL,
                                                         FALSE,
                                                         MOUSEPAD_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SEARCH_REPLACE_ALL_LOCATION,
                                   g_param_spec_int ("search-replace-all-location",
                                                     "SearchReplaceAllLocation",
                                                     NULL,
                                                     0, 2, 1,
                                                     MOUSEPAD_PARAM_READWRITE));

  /**
   * Textview Preferences
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_VIEW_AUTO_INDENT,
                                   g_param_spec_boolean ("view-auto-indent",
                                                         "ViewAutoIndent",
                                                         NULL,
                                                         FALSE,
                                                         MOUSEPAD_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_VIEW_FONT_NAME,
                                   g_param_spec_string ("view-font-name",
                                                        "ViewFontName",
                                                        NULL,
                                                        NULL,
                                                        MOUSEPAD_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_VIEW_LINE_NUMBERS,
                                   g_param_spec_boolean ("view-line-numbers",
                                                         "ViewLineNumbers",
                                                         NULL,
                                                         FALSE,
                                                         MOUSEPAD_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_VIEW_TAB_WIDTH,
                                   g_param_spec_int ("view-tab-size",
                                                     "ViewTabSize",
                                                     NULL,
                                                     1, 32, 8,
                                                     MOUSEPAD_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_VIEW_TABS_AS_SPACES,
                                   g_param_spec_boolean ("view-insert-spaces",
                                                         "ViewInsertSpaces",
                                                         NULL,
                                                         FALSE,
                                                         MOUSEPAD_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_VIEW_WORD_WRAP,
                                   g_param_spec_boolean ("view-word-wrap",
                                                         "ViewWordWrap",
                                                         NULL,
                                                         FALSE,
                                                         MOUSEPAD_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_VIEW_COLOR_SCHEME,
                                   g_param_spec_string ("view-color-scheme",
                                                        "ViewColorScheme",
                                                        NULL,
                                                        "none",
                                                        MOUSEPAD_PARAM_READWRITE));


  /**
   * Window Preferences
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_WINDOW_HEIGHT,
                                   g_param_spec_int ("window-height",
                                                     "WindowHeight",
                                                     NULL,
                                                     1, G_MAXINT, 480,
                                                     MOUSEPAD_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_WINDOW_WIDTH,
                                   g_param_spec_int ("window-width",
                                                     "WindowWidth",
                                                     NULL,
                                                     1, G_MAXINT, 640,
                                                     MOUSEPAD_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_WINDOW_STATUSBAR_VISIBLE,
                                   g_param_spec_boolean ("window-statusbar-visible",
                                                         "WindowStatusbarVisible",
                                                         NULL,
                                                         TRUE,
                                                         MOUSEPAD_PARAM_READWRITE));


  /**
   * Hidden Preferences
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_ALWAYS_SHOW_TABS,
                                   g_param_spec_boolean ("misc-always-show-tabs",
                                                         "MiscAlwaysShowTabs",
                                                         NULL,
                                                         FALSE,
                                                         MOUSEPAD_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_MISC_CYCLE_TABS,
                                   g_param_spec_boolean ("misc-cycle-tabs",
                                                         "MiscCycleTabs",
                                                         NULL,
                                                         FALSE,
                                                         MOUSEPAD_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_MISC_DEFAULT_TAB_SIZES,
                                   g_param_spec_string ("misc-default-tab-sizes",
                                                        "MiscDefaultTabSizes",
                                                        NULL,
                                                        "2,3,4,8",
                                                        MOUSEPAD_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_MISC_PATH_IN_TITLE,
                                   g_param_spec_boolean ("misc-path-in-title",
                                                         "MiscPathInTitle",
                                                         NULL,
                                                         FALSE,
                                                         MOUSEPAD_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_MISC_RECENT_MENU_ITEMS,
                                   g_param_spec_int ("misc-recent-menu-items",
                                                     "MiscRecentMenuItems",
                                                     NULL,
                                                     1, 100, 10,
                                                     MOUSEPAD_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_MISC_REMEMBER_GEOMETRY,
                                   g_param_spec_boolean ("misc-remember-geometry",
                                                         "MiscRememberGeometry",
                                                         NULL,
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
      g_source_remove (preferences->store_idle_id);
      mousepad_preferences_store (preferences);
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
  GValue              *src = preferences->values + prop_id;

  if (G_LIKELY (G_IS_VALUE (src)))
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
  GValue              *dst = preferences->values + prop_id;

  /* initialize value if needed */
  if (G_UNLIKELY (G_IS_VALUE (dst) == FALSE))
    {
      g_value_init (dst, pspec->value_type);
      g_param_value_set_default (pspec, dst);
    }

  /* compare the values */
  if (g_param_values_cmp (pspec, value, dst) != 0)
    {
      /* copy the new value */
      g_value_copy (value, dst);

      /* queue a store */
      mousepad_preferences_store (preferences);
    }
}



#ifndef NDEBUG
static void
mousepad_preferences_check_option_name (GParamSpec *pspec)
{
  const gchar *name, *nick;
  gchar       *option;

  /* get property name and nick */
  name = g_param_spec_get_name (pspec);
  nick = g_param_spec_get_nick (pspec);

  if (G_UNLIKELY (nick == NULL))
    {
      g_warning ("The nick name of %s is NULL", name);
    }
  else
    {
      /* get option name */
      option = mousepad_util_config_name (name);

      /* compare the strings */
      if (G_UNLIKELY (!option || strcmp (option, nick) != 0))
        g_warning ("The option name (%s) and nick name (%s) of property %s do not match", option, nick, name);

      /* cleanup */
      g_free (option);
    }
}
#endif



static void
mousepad_preferences_load (MousepadPreferences *preferences)
{
  gchar          *string;
  GParamSpec    **pspecs;
  GParamSpec     *pspec;
  GParamSpecInt  *ispec;
  GKeyFile       *keyfile;
  gchar          *filename;
  GValue         *dst;
  guint           nspecs;
  guint           n;
  gint            integer;

  /* get the save location, leave when there if no file found */
  filename = mousepad_util_get_save_location (MOUSEPAD_RC_RELPATH, FALSE);
  if (G_UNLIKELY (filename == NULL))
    return;

  /* create a new key file */
  keyfile = g_key_file_new ();

  /* open the config file */
  if (G_LIKELY (g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, NULL)))
    {
      /* freeze notification signals */
      g_object_freeze_notify (G_OBJECT (preferences));

      /* get all the properties in the class */
      pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (preferences), &nspecs);

      for (n = 0; n < nspecs; n++)
        {
          /* get the pspec */
          pspec = pspecs[n];

          /* get the pspec destination value and initialize it */
          dst = preferences->values + (n + 1);
          g_value_init (dst, pspec->value_type);

#ifndef NDEBUG
          /* check nick name with generated option name */
          mousepad_preferences_check_option_name (pspec);
#endif

          /* read the propert value from the key file */
          string = g_key_file_get_value (keyfile, GROUP_NAME, g_param_spec_get_nick (pspec), NULL);
          if (G_UNLIKELY (string == NULL || *string == '\0'))
            goto setdefault;

          if (pspec->value_type == G_TYPE_STRING)
            {
              /* set string as the value */
              g_value_take_string (dst, string);

              /* don't free the string */
              string = NULL;
            }
          else if (pspec->value_type == G_TYPE_INT)
            {
              /* get integer spec */
              ispec = G_PARAM_SPEC_INT (pspec);

              /* get the value and clamp it */
              integer = CLAMP (atoi (string), ispec->minimum, ispec->maximum);

              /* set the integer */
              g_value_set_int (dst, integer);
            }
          else if (pspec->value_type == G_TYPE_BOOLEAN)
            {
              /* set the boolean */
              g_value_set_boolean (dst, (strcmp (string, "false") != 0));
            }
          else
            {
              /* print warning */
              g_warning ("Failed to load property \"%s\"", pspec->name);

              setdefault:

              /* set default */
              g_param_value_set_default (pspec, dst);
            }

          /* cleanup */
          g_free (string);
        }

      /* cleanup the specs */
      g_free (pspecs);

      /* allow notifications again */
      g_object_thaw_notify (G_OBJECT (preferences));
    }

  /* free the key file */
  g_key_file_free (keyfile);

  /* cleanup filename */
  g_free (filename);
}



static void
mousepad_preferences_store (MousepadPreferences *preferences)
{
  if (preferences->store_idle_id == 0)
    {
      preferences->store_idle_id = g_idle_add_full (G_PRIORITY_LOW, mousepad_preferences_store_idle,
                                                    preferences, mousepad_preferences_store_idle_destroy);
    }
}



static gboolean
mousepad_preferences_store_idle (gpointer user_data)
{
  MousepadPreferences  *preferences = MOUSEPAD_PREFERENCES (user_data);
  GParamSpec          **pspecs;
  GParamSpec           *pspec;
  GKeyFile             *keyfile;
  gchar                *filename;
  GValue               *value;
  guint                 nspecs;
  guint                 n;
  const gchar          *nick;

  /* get the config filename */
  filename = mousepad_util_get_save_location (MOUSEPAD_RC_RELPATH, TRUE);

  /* leave when no filename is returned */
  if (G_UNLIKELY (filename == NULL))
    return FALSE;

  /* create an empty key file */
  keyfile = g_key_file_new ();

  /* try to load the file contents, no worries if this fails */
  g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, NULL);

  /* get the list of properties in the class */
  pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (preferences), &nspecs);

  for (n = 0; n < nspecs; n++)
    {
      /* get the pspec */
      pspec = pspecs[n];

      /* get the value */
      value = preferences->values + (n + 1);

      /* continue if the value is not initialized */
      if (G_UNLIKELY (G_IS_VALUE (value) == FALSE))
        continue;

      /* get nick name */
      nick = g_param_spec_get_nick (pspec);

      if (pspec->value_type == G_TYPE_STRING)
        {
          /* store the string */
          if (g_value_get_string (value) != NULL)
            g_key_file_set_value (keyfile, GROUP_NAME, nick, g_value_get_string (value));
        }
      else if (pspec->value_type == G_TYPE_INT)
        {
          /* store the interger */
          g_key_file_set_integer (keyfile, GROUP_NAME, nick, g_value_get_int (value));
        }
      else if (pspec->value_type == G_TYPE_BOOLEAN)
        {
          /* store the boolean */
          g_key_file_set_boolean (keyfile, GROUP_NAME, nick, g_value_get_boolean (value));
        }
      else
        {
          g_warning ("Failed to save property \"%s\"", pspec->name);
        }
    }

  /* cleanup the specs */
  g_free (pspecs);

  /* save the keyfile */
  mousepad_util_save_key_file (keyfile, filename);

  /* close the key file */
  g_key_file_free (keyfile);

  /* cleanup */
  g_free (filename);

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
      g_object_add_weak_pointer (G_OBJECT (preferences), (gpointer) &preferences);
    }
  else
    {
      g_object_ref (G_OBJECT (preferences));
    }

  return preferences;
}
