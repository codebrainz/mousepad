/* $Id$ */
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
#include <mousepad/mousepad-preferences.h>



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

  /* window preferences */
  PROP_WINDOW_HEIGHT,
  PROP_WINDOW_WIDTH,
  PROP_WINDOW_STATUSBAR_VISIBLE,

  /* hidden settings */
  PROP_MISC_ALWAYS_SHOW_TABS,
  PROP_MISC_CYCLE_TABS,
  PROP_MISC_PATH_IN_TITLE,
  PROP_MISC_RECENT_MENU_ITEMS,
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
  if (G_LIKELY (g_value_type_transformable (G_TYPE_STRING, G_TYPE_BOOLEAN) == FALSE))
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_BOOLEAN, transform_string_to_boolean);
  if (G_LIKELY (g_value_type_transformable (G_TYPE_STRING, G_TYPE_INT) == FALSE))
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_INT, transform_string_to_int);

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
                                   g_param_spec_int ("view-tab-width",
                                                     "ViewTabWidth",
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
                                                     1, G_MAXINT, 10,
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
      mousepad_preferences_store (preferences);
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
    {
      g_value_init (dst, pspec->value_type);
      g_param_value_set_default (pspec, dst);
    }

  if (g_param_values_cmp (pspec, value, dst) != 0)
    {
      g_value_copy (value, dst);

      mousepad_preferences_store (preferences);
    }
}



#ifndef NDEBUG
static void
mousepad_preferences_check_option_name (GParamSpec *pspec)
{
  const gchar *s;
  const gchar *name, *nick;
  gboolean     upper = TRUE;
  gchar       *option;
  gchar       *t;

  /* get property name and nick */
  name = g_param_spec_get_name (pspec);
  nick = g_param_spec_get_nick (pspec);

  if (G_UNLIKELY (nick == NULL))
    {
      g_warning ("The nick name of %s is NULL", name);
    }
  else
    {
      /* allocate string for option name */
      option = g_new (gchar, strlen (name) + 1);

      /* convert name */
      for (s = name, t = option; *s != '\0'; ++s)
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

      /* compare the strings */
      if (G_UNLIKELY (!option || strcmp (option, nick) != 0))
        g_warning ("The option name (%s) and nick name (%s) of property \"%s\" do not match", option, nick, name);

      /* cleanup */
      g_free (option);
    }
}
#endif



static void
mousepad_preferences_load (MousepadPreferences *preferences)
{
  const gchar  *string;
  GParamSpec  **specs;
  GParamSpec   *spec;
  XfceRc       *rc;
  GValue        dst = { 0, };
  GValue        src = { 0, };
  guint         nspecs;
  guint         n;

  /* try to open the config file */
  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "Mousepad/mousepadrc", TRUE);
  if (G_UNLIKELY (rc == NULL))
    {
      g_warning (_("Failed to load the preferences."));

      return;
    }

  /* freeze notification signals */
  g_object_freeze_notify (G_OBJECT (preferences));

  /* set group */
  xfce_rc_set_group (rc, "Configuration");

  /* get all the properties in the class */
  specs = g_object_class_list_properties (G_OBJECT_GET_CLASS (preferences), &nspecs);

  for (n = 0; n < nspecs; ++n)
    {
      spec = specs[n];

#ifndef NDEBUG
      /* check nick name with generated option name */
      mousepad_preferences_check_option_name (spec);
#endif

      /* read the entry */
      string = xfce_rc_read_entry (rc, spec->_nick, NULL);

      if (G_UNLIKELY (string == NULL))
        continue;

      /* create gvalue with the string as value */
      g_value_init (&src, G_TYPE_STRING);
      g_value_set_static_string (&src, string);

      if (spec->value_type == G_TYPE_STRING)
        {
          /* they have the same type, so set the property */
          g_object_set_property (G_OBJECT (preferences), spec->name, &src);
        }
      else if (g_value_type_transformable (G_TYPE_STRING, spec->value_type))
        {
          /* transform the type */
          g_value_init (&dst, spec->value_type);
          if (g_value_transform (&src, &dst))
            g_object_set_property (G_OBJECT (preferences), spec->name, &dst);
          g_value_unset (&dst);
        }
      else
        {
          g_warning ("Failed to load property \"%s\"", spec->name);
        }

      /* cleanup */
      g_value_unset (&src);
    }

  /* cleanup the specs */
  g_free (specs);

  /* close the rc file */
  xfce_rc_close (rc);

  /* allow notifications again */
  g_object_thaw_notify (G_OBJECT (preferences));
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
  MousepadPreferences *preferences = MOUSEPAD_PREFERENCES (user_data);
  const gchar         *string;
  GParamSpec         **specs;
  GParamSpec          *spec;
  XfceRc              *rc;
  GValue               dst = { 0, };
  GValue               src = { 0, };
  guint                nspecs;
  guint                n;

  /* open the config file */
  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "Mousepad/mousepadrc", FALSE);
  if (G_UNLIKELY (rc == NULL))
    {
      g_warning (_("Failed to store the preferences."));
      return FALSE;
    }

  /* set the group */
  xfce_rc_set_group (rc, "Configuration");

  /* get the list of properties in the class */
  specs = g_object_class_list_properties (G_OBJECT_GET_CLASS (preferences), &nspecs);

  for (n = 0; n < nspecs; ++n)
    {
      spec = specs[n];

      /* init a string value */
      g_value_init (&dst, G_TYPE_STRING);

      if (spec->value_type == G_TYPE_STRING)
        {
          /* set the string value */
          g_object_get_property (G_OBJECT (preferences), spec->name, &dst);
        }
      else
        {
          /* property contains another type, transform it first */
          g_value_init (&src, spec->value_type);
          g_object_get_property (G_OBJECT (preferences), spec->name, &src);
          g_value_transform (&src, &dst);
          g_value_unset (&src);
        }

      /* store the setting */
      string = g_value_get_string (&dst);
      if (G_LIKELY (string != NULL))
        xfce_rc_write_entry (rc, spec->_nick, string);

      /* cleanup */
      g_value_unset (&dst);
    }

  /* cleanup */
  g_free (specs);
  xfce_rc_close (rc);

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
