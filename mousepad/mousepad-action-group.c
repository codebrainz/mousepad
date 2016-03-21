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
#include <mousepad/mousepad-action-group.h>
#include <mousepad/mousepad-gtkcompat.h>
#include <mousepad/mousepad-language-action.h>
#include <mousepad/mousepad-style-scheme-action.h>
#include <mousepad/mousepad-settings.h>

#include <gtksourceview/gtksourcelanguagemanager.h>
#include <gtksourceview/gtksourcestyleschememanager.h>



enum
{
  PROP_0,
  PROP_ACTIVE_LANGUAGE,
  PROP_ACTIVE_STYLE_SCHEME,
  NUM_PROPERTIES
};



struct MousepadActionGroup_
{
  GtkActionGroup        parent;
  GtkSourceLanguage    *active_language;
  GtkSourceStyleScheme *active_scheme;
  gboolean              locked;
};



struct MousepadActionGroupClass_
{
  GtkActionGroupClass parent_class;
};



static void       mousepad_action_group_finalize                         (GObject                *object);
static void       mousepad_action_group_set_property                     (GObject                *object,
                                                                          guint                   prop_id,
                                                                          const GValue           *value,
                                                                          GParamSpec             *pspec);
static void       mousepad_action_group_get_property                     (GObject                *object,
                                                                          guint                   prop_id,
                                                                          GValue                 *value,
                                                                          GParamSpec             *pspec);
static GtkWidget *mousepad_action_group_create_language_submenu          (MousepadActionGroup    *self,
                                                                          const gchar            *section);
static void       mousepad_action_group_language_action_activate         (MousepadActionGroup    *self,
                                                                          MousepadLanguageAction *action);
static void       mousepad_action_group_add_language_actions             (MousepadActionGroup    *self);
static GtkAction *mousepad_action_group_get_language_action              (MousepadActionGroup    *group,
                                                                          GtkSourceLanguage      *language);
static gint       mousepad_action_group_languages_name_compare           (gconstpointer           a,
                                                                          gconstpointer           b);
static GSList    *mousepad_action_group_get_sorted_languages_for_section (const gchar            *section);
static GSList    *mousepad_action_group_get_sorted_section_names         (void);
static void       mousepad_action_group_add_style_scheme_actions         (MousepadActionGroup    *self);
static GtkAction *mousepad_action_group_get_style_scheme_action          (MousepadActionGroup    *self,
                                                                          GtkSourceStyleScheme   *scheme);
static gint       mousepad_action_group_style_schemes_name_compare       (gconstpointer           a,
                                                                          gconstpointer           b);
static GSList    *mousepad_action_group_get_style_schemes                (void);
static GSList    *mousepad_action_group_style_schemes_get_sorted         (void);
static void       mousepad_action_group_color_scheme_setting_changed     (MousepadActionGroup    *self,
                                                                          gchar                  *key,
                                                                          GSettings              *settings);



G_DEFINE_TYPE (MousepadActionGroup, mousepad_action_group, GTK_TYPE_ACTION_GROUP)



static void
mousepad_action_group_class_init (MousepadActionGroupClass *klass)
{
  GObjectClass *g_object_class;

  g_object_class = G_OBJECT_CLASS (klass);

  g_object_class->finalize = mousepad_action_group_finalize;
  g_object_class->set_property = mousepad_action_group_set_property;
  g_object_class->get_property = mousepad_action_group_get_property;

  g_object_class_install_property (
    g_object_class,
    PROP_ACTIVE_LANGUAGE,
    g_param_spec_object ("active-language",
                         "ActiveLanguage",
                         "The currently active language action",
                         MOUSEPAD_TYPE_LANGUAGE_ACTION,
                         G_PARAM_READWRITE));

  g_object_class_install_property (
    g_object_class,
    PROP_ACTIVE_STYLE_SCHEME,
    g_param_spec_object ("active-style-scheme",
                         "ActiveStyleScheme",
                         "The currently active style scheme action",
                         MOUSEPAD_TYPE_STYLE_SCHEME_ACTION,
                         G_PARAM_READWRITE));

  g_signal_new ("user-set-language",
                G_TYPE_FROM_CLASS (g_object_class),
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL,
                g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE, 0,
                GTK_SOURCE_TYPE_LANGUAGE);
}



static void
mousepad_action_group_finalize (GObject *object)
{
  MousepadActionGroup *self;

  g_return_if_fail (MOUSEPAD_IS_ACTION_GROUP (object));

  self = MOUSEPAD_ACTION_GROUP (object);

  if (GTK_SOURCE_IS_LANGUAGE (self->active_language))
    g_object_unref (self->active_language);

  if (GTK_SOURCE_IS_STYLE_SCHEME (self->active_scheme))
    g_object_unref (self->active_scheme);

  G_OBJECT_CLASS (mousepad_action_group_parent_class)->finalize (object);
}



static void
mousepad_action_group_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  MousepadActionGroup *self = MOUSEPAD_ACTION_GROUP (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_LANGUAGE:
      mousepad_action_group_set_active_language (self, g_value_get_object (value));
      break;
    case PROP_ACTIVE_STYLE_SCHEME:
      mousepad_action_group_set_active_style_scheme (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
mousepad_action_group_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  MousepadActionGroup *self = MOUSEPAD_ACTION_GROUP (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_LANGUAGE:
      g_value_set_object (value, mousepad_action_group_get_active_language (self));
      break;
    case PROP_ACTIVE_STYLE_SCHEME:
      g_value_set_object (value, mousepad_action_group_get_active_style_scheme (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
mousepad_action_group_init (MousepadActionGroup *self)
{
  gchar                *scheme_id;
  GtkSourceStyleScheme *scheme;

  self->active_language = NULL;
  mousepad_action_group_add_language_actions (self);
  mousepad_action_group_set_active_language (self, NULL);

  self->active_scheme = NULL;
  mousepad_action_group_add_style_scheme_actions (self);

  /* set the initial style scheme from the setting */
  scheme_id = MOUSEPAD_SETTING_GET_STRING (COLOR_SCHEME);
  scheme = gtk_source_style_scheme_manager_get_scheme (
              gtk_source_style_scheme_manager_get_default (), scheme_id);
  g_free (scheme_id);
  mousepad_action_group_set_active_style_scheme (self, scheme);

  /* update the colour scheme when the setting changes */
  MOUSEPAD_SETTING_CONNECT_OBJECT (COLOR_SCHEME,
                                   G_CALLBACK (mousepad_action_group_color_scheme_setting_changed),
                                   self,
                                   G_CONNECT_SWAPPED);
}



GtkActionGroup *
mousepad_action_group_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_ACTION_GROUP, "name", "MousepadWindow", NULL);
}



/**
 * Property accessors
 **/



void
mousepad_action_group_set_active_language (MousepadActionGroup *self,
                                           GtkSourceLanguage   *language)
{
  GtkAction *action;

  g_return_if_fail (MOUSEPAD_IS_ACTION_GROUP (self));

  if (GTK_SOURCE_IS_LANGUAGE (self->active_language))
    g_object_unref (self->active_language);

  if (GTK_SOURCE_IS_LANGUAGE (language))
    self->active_language = g_object_ref (language);
  else
    self->active_language = NULL;

  action = mousepad_action_group_get_language_action (self, language);

  /* prevent recursion since we watch the action's 'activate' signal */
  self->locked = TRUE;
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
  self->locked = FALSE;

  g_object_notify (G_OBJECT (self), "active-language");
}



GtkSourceLanguage *
mousepad_action_group_get_active_language (MousepadActionGroup *self)
{
  g_return_val_if_fail (MOUSEPAD_IS_ACTION_GROUP (self), NULL);

  return self->active_language;
}



void
mousepad_action_group_set_active_style_scheme (MousepadActionGroup  *self,
                                               GtkSourceStyleScheme *scheme)
{
  GtkAction   *action;
  g_return_if_fail (MOUSEPAD_IS_ACTION_GROUP (self));

  if (GTK_SOURCE_IS_STYLE_SCHEME (self->active_scheme))
    g_object_unref (self->active_scheme);

  if (GTK_SOURCE_IS_STYLE_SCHEME (scheme))
    self->active_scheme = g_object_ref (scheme);
  else
    self->active_scheme = NULL;

  action = mousepad_action_group_get_style_scheme_action (self, scheme);

  /* prevent recursion since we watch the action's 'activate' signal */
  self->locked = TRUE;
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
  self->locked = FALSE;

  g_object_notify (G_OBJECT (self), "active-style-scheme");
}



GtkSourceStyleScheme *
mousepad_action_group_get_active_style_scheme (MousepadActionGroup *self)
{
  g_return_val_if_fail (MOUSEPAD_IS_ACTION_GROUP (self), NULL);

  return self->active_scheme;
}



/**
 * GUI proxy creation
 **/



static GtkWidget *
mousepad_action_group_create_language_submenu (MousepadActionGroup *self,
                                               const gchar         *section)
{
  GtkWidget *menu, *item;
  GSList    *language_ids, *iter;

  menu = gtk_menu_new ();

  language_ids = mousepad_action_group_get_sorted_languages_for_section (section);

  for (iter = language_ids; iter != NULL; iter = g_slist_next (iter))
    {
      GtkAction         *action;
      GtkSourceLanguage *language = iter->data;

      action = mousepad_action_group_get_language_action (self, language);
      item = gtk_action_create_menu_item (action);

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
    }

  g_slist_free (language_ids);

  return menu;
}



GtkWidget *
mousepad_action_group_create_language_menu (MousepadActionGroup *self)
{
  GtkWidget *menu;
  GtkWidget *item;
  GSList    *sections, *iter;
  GtkAction *action;

  menu = gtk_menu_new ();

  /* add the 'none' language first */
  action = gtk_action_group_get_action (GTK_ACTION_GROUP (self), "mousepad-language-none");
  item = gtk_action_create_menu_item (action);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* separate the 'none' language from the section submenus */
  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  sections = mousepad_action_group_get_sorted_section_names ();

  for (iter = sections; iter != NULL; iter = g_slist_next (iter))
    {
      const gchar *section = iter->data;
      GtkWidget   *submenu;

      /* create a menu item for the section */
      item = gtk_menu_item_new_with_label (section);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      /* create a submenu for the section and it to the item */
      submenu = mousepad_action_group_create_language_submenu (self, section);
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);

    }

  g_slist_free (sections);

  return menu;
}



GtkWidget *
mousepad_action_group_create_style_scheme_menu (MousepadActionGroup *self)
{
  GtkWidget *menu;
  GtkWidget *item;
  GtkAction *action;
  GSList    *schemes, *iter;

  menu = gtk_menu_new ();

  action = mousepad_action_group_get_style_scheme_action (self, NULL);
  item = gtk_action_create_menu_item (action);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  schemes = mousepad_action_group_style_schemes_get_sorted ();

  for (iter = schemes; iter != NULL; iter = g_slist_next (iter))
    {
      GtkSourceStyleScheme *scheme = iter->data;

      action = mousepad_action_group_get_style_scheme_action (self, scheme);
      item = gtk_action_create_menu_item (action);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
    }

  g_slist_free (schemes);

  return menu;
}



/**
 * Signal handlers
 **/



static void
mousepad_action_group_language_action_activate (MousepadActionGroup    *self,
                                                MousepadLanguageAction *action)
{
  /* only update the active action if we're not already in the process of
   * setting it and the sender action is actually active */
  if (! self->locked &&
      gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
    {
      GtkSourceLanguage *language;

      language = mousepad_language_action_get_language (action);
      mousepad_action_group_set_active_language (self, language);

      /* notify interested parties that the user explicitly set the filetype */
      g_signal_emit_by_name (self, "user-set-language", language);
    }
}



static void
mousepad_action_group_style_scheme_action_activate (MousepadActionGroup       *self,
                                                    MousepadStyleSchemeAction *action)
{
  const gchar *scheme_name = NULL;

  /* only update the active action if we're not already in the process of
   * setting it and the sender action is actually active */
  if (! self->locked &&
      gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
    {
      GtkSourceStyleScheme *scheme;

      scheme = mousepad_style_scheme_action_get_style_scheme (action);

      /* update the setting when the active action is changed */
      if (scheme != NULL)
        scheme_name = gtk_source_style_scheme_get_id(scheme);

      self->locked = TRUE;
      MOUSEPAD_SETTING_SET_STRING (COLOR_SCHEME, scheme_name);
      self->locked = FALSE;
    }
}



static void
mousepad_action_group_color_scheme_setting_changed (MousepadActionGroup *self,
                                                    gchar               *key,
                                                    GSettings           *settings)
{
  GtkSourceStyleScheme *scheme;
  gchar                *scheme_id;

  /* prevent recursion since the active style scheme setter updates the setting */
  if (self->locked)
    return;

  /* set the style scheme from the setting */
  scheme_id = MOUSEPAD_SETTING_GET_STRING (COLOR_SCHEME);
  scheme = gtk_source_style_scheme_manager_get_scheme (
              gtk_source_style_scheme_manager_get_default (), scheme_id);
  g_free (scheme_id);

  mousepad_action_group_set_active_style_scheme (self, scheme);
}



/**
 * Helper functions
 **/



static void
mousepad_action_group_add_language_actions (MousepadActionGroup *self)
{
  GtkSourceLanguageManager *manager;
  const gchar       *const *lang_ids;
  const gchar       *const *lang_id_ptr;
  GSList                   *group = NULL;
  GtkAccelGroup            *accel_group;
  GtkAction                *action;

  accel_group = gtk_accel_group_new ();

  /* add an action for the 'none' (non-)language */
  action = mousepad_language_action_new (NULL);
  gtk_radio_action_set_group (GTK_RADIO_ACTION (action), group);
  group = gtk_radio_action_get_group (GTK_RADIO_ACTION (action));
  gtk_action_set_accel_group (action, accel_group);
  gtk_action_group_add_action_with_accel (GTK_ACTION_GROUP (self), action, NULL);
  g_signal_connect_object (action, "activate", G_CALLBACK (mousepad_action_group_language_action_activate), self, G_CONNECT_SWAPPED);

  manager = gtk_source_language_manager_get_default ();
  lang_ids = gtk_source_language_manager_get_language_ids (manager);

  for (lang_id_ptr = lang_ids; lang_id_ptr && *lang_id_ptr; lang_id_ptr++)
    {
      GtkSourceLanguage *language;

      /* add an action for each GSV language */
      language = gtk_source_language_manager_get_language (manager, *lang_id_ptr);
      action = mousepad_language_action_new (language);
      gtk_radio_action_set_group (GTK_RADIO_ACTION (action), group);
      group = gtk_radio_action_get_group (GTK_RADIO_ACTION (action));
      gtk_action_set_accel_group (action, accel_group);
      gtk_action_group_add_action_with_accel (GTK_ACTION_GROUP (self), action, NULL);
      g_signal_connect_object (action, "activate", G_CALLBACK (mousepad_action_group_language_action_activate), self, G_CONNECT_SWAPPED);
    }

  g_object_unref (accel_group);
}



GtkAction *
mousepad_action_group_get_language_action (MousepadActionGroup *self,
                                           GtkSourceLanguage   *language)
{
  const gchar *language_id;
  gchar       *action_name;
  GtkAction   *action;

  g_return_val_if_fail (MOUSEPAD_IS_ACTION_GROUP (self), NULL);

  if (GTK_SOURCE_IS_LANGUAGE (language))
    language_id = gtk_source_language_get_id (language);
  else
    language_id = "none";

  action_name = g_strdup_printf ("mousepad-language-%s", language_id);
  action = gtk_action_group_get_action (GTK_ACTION_GROUP (self), action_name);
  g_free (action_name);

  return action;
}



static gint
mousepad_action_group_languages_name_compare (gconstpointer a,
                                              gconstpointer b)
{
  const gchar *name_a, *name_b;

  if (G_UNLIKELY (!GTK_SOURCE_IS_LANGUAGE (a)))
    return -(a != b);
  if (G_UNLIKELY (!GTK_SOURCE_IS_LANGUAGE (b)))
    return a != b;

  name_a = gtk_source_language_get_name (GTK_SOURCE_LANGUAGE (a));
  name_b = gtk_source_language_get_name (GTK_SOURCE_LANGUAGE (b));

  return g_utf8_collate (name_a, name_b);
}



static GSList *
mousepad_action_group_get_sorted_section_names (void)
{
  GSList                   *list = NULL;
  const gchar *const       *languages;
  GtkSourceLanguage        *language;
  GtkSourceLanguageManager *manager;

  manager = gtk_source_language_manager_get_default ();
  languages = gtk_source_language_manager_get_language_ids (manager);

  while (*languages)
    {
      language = gtk_source_language_manager_get_language (manager, *languages);
      if (G_LIKELY (GTK_SOURCE_IS_LANGUAGE (language)))
        {
          /* ignore hidden languages */
          if(gtk_source_language_get_hidden(language))
            {
              languages++;
              continue;
            }

          /* ensure no duplicates in list */
          if (!g_slist_find_custom (list,
                                    gtk_source_language_get_section (language),
                                    (GCompareFunc)g_strcmp0))
            {
              list = g_slist_prepend (list, (gchar *)gtk_source_language_get_section (language));
            }
        }
      languages++;
    }

  return g_slist_sort (list, (GCompareFunc) g_utf8_collate);
}



static GSList *
mousepad_action_group_get_sorted_languages_for_section (const gchar *section)
{
  GSList                   *list = NULL;
  const gchar *const       *languages;
  GtkSourceLanguage        *language;
  GtkSourceLanguageManager *manager;

  g_return_val_if_fail (section != NULL, NULL);

  manager = gtk_source_language_manager_get_default ();
  languages = gtk_source_language_manager_get_language_ids (manager);

  while (*languages)
    {
      language = gtk_source_language_manager_get_language (manager, *languages);
      if (G_LIKELY (GTK_SOURCE_IS_LANGUAGE (language)))
        {
          /* ignore hidden languages */
          if(gtk_source_language_get_hidden(language))
            {
              languages++;
              continue;
            }

          /* only get languages in the specified section */
          if (g_strcmp0 (gtk_source_language_get_section (language), section) == 0)
            list = g_slist_prepend (list, language);
        }
      languages++;
    }

  return g_slist_sort(list, (GCompareFunc) mousepad_action_group_languages_name_compare);
}



static void
mousepad_action_group_add_style_scheme_actions (MousepadActionGroup *self)
{
  GSList        *schemes, *iter;
  GSList        *group = NULL;
  GtkAccelGroup *accel_group;
  GtkAction     *action;

  accel_group = gtk_accel_group_new ();

  /* add an action for the 'none' (non-)scheme */
  action = mousepad_style_scheme_action_new (NULL);
  gtk_radio_action_set_group (GTK_RADIO_ACTION (action), group);
  group = gtk_radio_action_get_group (GTK_RADIO_ACTION (action));
  gtk_action_set_accel_group (action, accel_group);
  gtk_action_group_add_action_with_accel (GTK_ACTION_GROUP (self), action, NULL);
  g_signal_connect_object (action, "activate", G_CALLBACK (mousepad_action_group_style_scheme_action_activate), self, G_CONNECT_SWAPPED);

  schemes = mousepad_action_group_get_style_schemes ();

  for (iter = schemes; iter != NULL; iter = g_slist_next (iter))
    {
      GtkSourceStyleScheme *scheme = iter->data;

      /* add an action for each GSV style scheme */
      action = mousepad_style_scheme_action_new (scheme);
      gtk_radio_action_set_group (GTK_RADIO_ACTION (action), group);
      group = gtk_radio_action_get_group (GTK_RADIO_ACTION (action));
      gtk_action_set_accel_group (action, accel_group);
      gtk_action_group_add_action_with_accel (GTK_ACTION_GROUP (self), action, NULL);
      g_signal_connect_object (action, "activate", G_CALLBACK (mousepad_action_group_style_scheme_action_activate), self, G_CONNECT_SWAPPED);
    }

  g_slist_free (schemes);
  g_object_unref (accel_group);
}



static GtkAction *
mousepad_action_group_get_style_scheme_action (MousepadActionGroup  *self,
                                               GtkSourceStyleScheme *scheme)
{
  gchar       *name;
  const gchar *scheme_id;
  GtkAction   *action;

  if (GTK_SOURCE_IS_STYLE_SCHEME (scheme))
    scheme_id = gtk_source_style_scheme_get_id (scheme);
  else
    scheme_id = "none";

  name = g_strdup_printf ("mousepad-style-scheme-%s", scheme_id);
  action = gtk_action_group_get_action (GTK_ACTION_GROUP (self), name);
  g_free (name);

  return action;
}



static gint
mousepad_action_group_style_schemes_name_compare (gconstpointer a,
                                                  gconstpointer b)
{
  const gchar *name_a, *name_b;

  if (G_UNLIKELY (!GTK_SOURCE_IS_STYLE_SCHEME (a)))
    return -(a != b);
  if (G_UNLIKELY (!GTK_SOURCE_IS_STYLE_SCHEME (b)))
    return a != b;

  name_a = gtk_source_style_scheme_get_name (GTK_SOURCE_STYLE_SCHEME (a));
  name_b = gtk_source_style_scheme_get_name (GTK_SOURCE_STYLE_SCHEME (b));

  return g_utf8_collate (name_a, name_b);
}



static GSList *
mousepad_action_group_get_style_schemes (void)
{
  GSList               *list = NULL;
  const gchar * const  *schemes;
  GtkSourceStyleScheme *scheme;

  schemes = gtk_source_style_scheme_manager_get_scheme_ids (
              gtk_source_style_scheme_manager_get_default ());

  while (*schemes)
    {
      scheme = gtk_source_style_scheme_manager_get_scheme (
                gtk_source_style_scheme_manager_get_default (), *schemes);
      list = g_slist_prepend (list, scheme);
      schemes++;
    }

  return list;
}



static GSList *
mousepad_action_group_style_schemes_get_sorted (void)
{
  return g_slist_sort (mousepad_action_group_get_style_schemes (),
                       mousepad_action_group_style_schemes_name_compare);
}
