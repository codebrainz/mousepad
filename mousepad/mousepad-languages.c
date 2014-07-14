#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <mousepad/mousepad-languages.h>
#include <mousepad/mousepad-util.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksourcelanguagemanager.h>



static inline void
mousepad_action_set_language (GtkAction *action,
                              GtkSourceLanguage *language)
{
  g_return_if_fail (GTK_IS_ACTION (action));

  if (GTK_IS_SOURCE_LANGUAGE (language))
    {
      g_object_set_data_full (G_OBJECT (action),
                              "mousepad-language",
                              g_object_ref (language),
                              g_object_unref);
    }
  else
    g_object_set_data (G_OBJECT (action), "mousepad-language", NULL);
}



static inline GtkSourceLanguage *
mousepad_action_get_language (GtkAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  return g_object_get_data (G_OBJECT (action), "mousepad-language");
}



static inline void
mousepad_action_group_set_active_language (GtkActionGroup    *group,
                                           GtkSourceLanguage *language)
{
  g_return_if_fail (GTK_IS_ACTION_GROUP (group));

  if (GTK_IS_SOURCE_LANGUAGE (language))
    {
      g_object_set_data_full (G_OBJECT (group),
                              "mousepad-active-language",
                              g_object_ref (language),
                              g_object_unref);
    }
  else
    g_object_set_data (G_OBJECT (group), "mousepad-active-language", NULL);
}



static inline GtkSourceLanguage *
mousepad_action_group_get_active_language (GtkActionGroup *group)
{
  g_return_val_if_fail (GTK_IS_ACTION_GROUP (group), NULL);

  return g_object_get_data (G_OBJECT (group), "mousepad-active-language");
}



static GIcon *
mousepad_languages_get_icon_for_mime_type (const gchar *mime_type)
{
  gchar *content_type;
  GIcon *icon = NULL;

  content_type = g_content_type_from_mime_type (mime_type);
  if (content_type != NULL)
    {
      icon = g_content_type_get_icon (content_type);
      g_free (content_type);
    }

  return icon;
}



static GIcon *
mousepad_languages_get_icon_for_language (GtkSourceLanguage *language)
{
  gchar **mime_types;
  GIcon  *mime_icon = NULL;

  g_return_val_if_fail (GTK_IS_SOURCE_LANGUAGE (language), NULL);

  mime_types = gtk_source_language_get_mime_types (language);
  if (G_LIKELY (mime_types != NULL && g_strv_length (mime_types)) > 0)
    mime_icon = mousepad_languages_get_icon_for_mime_type (mime_types[0]);
  g_strfreev (mime_types);

  return mime_icon;
}



static void
mousepad_languages_active_language_changed (GtkActionGroup  *group,
                                            GtkToggleAction *action)
{
  GtkSourceLanguage *language;

  if (gtk_toggle_action_get_active (action))
    {
      language = mousepad_action_get_language (GTK_ACTION (action));
      mousepad_action_group_set_active_language (group, language);
      g_signal_emit_by_name (group, "language-changed", language);
    }
}



static GtkRadioAction *
mousepad_languages_create_action (GtkActionGroup    *group,
                                  GtkSourceLanguage *language,
                                  GSList           **radio_action_group,
                                  GtkAccelGroup     *accel_group)
{
  GtkRadioAction *action;
  GQuark          value_quark;
  GIcon          *icon = NULL;

  g_return_val_if_fail (GTK_IS_ACTION_GROUP (group), NULL);
  g_return_val_if_fail (language == NULL || GTK_IS_SOURCE_LANGUAGE (language), NULL);
  g_return_val_if_fail (radio_action_group != NULL, NULL);

  if (G_UNLIKELY (language == NULL))
    {
      /* NULL/'none' language is treated as plain text */
      value_quark = g_quark_from_static_string ("mousepad-language-none");
      action = gtk_radio_action_new ("mousepad-language-none", _("Plain Text"), _("No language"), NULL, value_quark);
      gtk_action_set_accel_group (GTK_ACTION (action), accel_group);
      mousepad_action_set_language (GTK_ACTION (action), NULL);
      icon = mousepad_languages_get_icon_for_mime_type ("text/plain");
    }
  else
    {
      const gchar *language_id, *name;
      gchar       *label, *tooltip;

      language_id = gtk_source_language_get_id (language);
      value_quark = g_quark_from_string (language_id);

      /* the proper name of the language for user display */
      name = gtk_source_language_get_name (language);

      /* a label for the action in the action group */
      label = g_strdup_printf ("mousepad-language-%s", language_id);

      /* a tooltip to the section and name, ex. 'Scripts/Python' */
      tooltip = g_strdup_printf ("%s/%s",
                                 gtk_source_language_get_section (language),
                                 gtk_source_language_get_name (language));

      /* a unique id of the language, for the radio action's value */
      value_quark = g_quark_from_string (language_id);

      /* create the new action */
      action = gtk_radio_action_new (label, name, tooltip, NULL, value_quark);
      g_free (label);
      g_free (tooltip);

      gtk_action_set_accel_group (GTK_ACTION (action), accel_group);

      /* update the action's related language */
      mousepad_action_set_language (GTK_ACTION (action), language);

      /* try and get an icon for the language's first mime-type if possible */
      icon = mousepad_languages_get_icon_for_language (language);
    }

  /* set an icon for the action if we have one */
  if (G_IS_ICON (icon))
    {
      gtk_action_set_gicon (GTK_ACTION (action), icon);
      g_object_unref (icon);
    }

  /* watch for the 'none' language activation */
  g_signal_connect_object (action,
                           "activate",
                           G_CALLBACK (mousepad_languages_active_language_changed),
                           group,
                           G_CONNECT_SWAPPED);

  /* add it to the provided radio action group */
  gtk_radio_action_set_group (action, *radio_action_group);
  *radio_action_group = gtk_radio_action_get_group (action);

  gtk_action_group_add_action_with_accel (group, GTK_ACTION (action), NULL);
  g_object_unref (action);

  return action;
}



GtkActionGroup *
mousepad_languages_action_group_new (void)
{
  GtkSourceLanguageManager *manager;
  const gchar       *const *language_ids;
  const gchar       *const *language_id_ptr;
  GSList                   *radio_action_group = NULL;
  GtkActionGroup           *group;
  GtkAccelGroup            *accel_group;
  static gsize              installed_signal = 0;

  /* one-time, add our 'language-changed' signal to the action group class */
  if (g_once_init_enter (&installed_signal))
    {
      g_signal_new ("language-changed",
                    GTK_TYPE_ACTION_GROUP,
                    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    0, NULL, NULL,
                    g_cclosure_marshal_VOID__OBJECT,
                    G_TYPE_NONE, 1,
                    GTK_TYPE_SOURCE_LANGUAGE);
      g_once_init_leave (&installed_signal, 1);
    }

  accel_group = gtk_accel_group_new ();
  group = gtk_action_group_new ("mousepad-languages");

  /* add an action for the 'none' language */
  mousepad_languages_create_action (group, NULL, &radio_action_group, accel_group);

  /* the default is the 'none' language */
  mousepad_action_group_set_active_language (group, NULL);

  manager = gtk_source_language_manager_get_default ();
  language_ids = gtk_source_language_manager_get_language_ids (manager);

  for (language_id_ptr = language_ids; language_id_ptr && *language_id_ptr; language_id_ptr++)
    {
      GtkSourceLanguage *language;

      /* lookup the language for the id */
      language = gtk_source_language_manager_get_language (manager, *language_id_ptr);

      /* create an action for the language */
      mousepad_languages_create_action (group, language, &radio_action_group, accel_group);
    }

  g_object_unref (accel_group);

  return group;
}



static GtkWidget *
mousepad_languages_create_submenu (GtkActionGroup *group,
                                   const gchar    *section)
{
  GtkWidget                *menu;
  GtkWidget                *item;
  GSList                   *language_ids, *iter;

  menu = gtk_menu_new ();

  language_ids = mousepad_languages_get_sorted_for_section (section);

  for (iter = language_ids; iter != NULL; iter = g_slist_next (iter))
    {
      GtkAction         *action;
      GtkSourceLanguage *language = iter->data;

      action = mousepad_languages_get_action (group, language);
      item = gtk_action_create_menu_item (action);

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
    }
  g_slist_free (language_ids);

  return menu;
}



GtkWidget *
mousepad_languages_create_menu (GtkActionGroup *group)
{
  GtkWidget *menu;
  GtkWidget *item;
  GSList    *sections, *iter;
  GtkAction *action;

  menu = gtk_menu_new ();

  /* add the 'none' language first */
  action = gtk_action_group_get_action (group, "mousepad-language-none");
  item = gtk_action_create_menu_item (action);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* separator the 'none' language from the section submenus */
  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  sections = mousepad_languages_get_sorted_sections ();
  for (iter = sections; iter != NULL; iter = g_slist_next (iter))
    {
      const gchar *section = iter->data;
      GtkWidget   *submenu;

      /* create a menu item for the section */
      item = gtk_menu_item_new_with_label (section);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      /* create a submenu for the section and it to the item */
      submenu = mousepad_languages_create_submenu (group, section);
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);

    }
  g_slist_free (sections);

  return menu;
}



static GtkSourceLanguage *
mousepad_languages_get_radio_action_group_active (GSList *actions)
{
  GSList *iter;

  for (iter = actions; iter != NULL; iter = g_slist_next (iter))
    {
      if (gtk_toggle_action_get_active (iter->data))
        return mousepad_action_get_language (iter->data);
    }

  return NULL;
}



GtkSourceLanguage *
mousepad_languages_get_active (GtkActionGroup *group)
{
  GList             *actions, *action_iter;
  GtkSourceLanguage *language = NULL;

  /* scan for the first radio action in case there's other stuff in the group */
  actions = gtk_action_group_list_actions (group);
  for (action_iter = actions; action_iter != NULL; action_iter = g_list_next (action_iter))
    {
      /* make sure it's one of our language radio actions */
      if (GTK_IS_RADIO_ACTION (action_iter->data) &&
          GTK_IS_SOURCE_LANGUAGE (mousepad_action_get_language (action_iter->data)))
        {
          GSList *radio_actions;
          /* get the active language from its group */
          radio_actions = gtk_radio_action_get_group (action_iter->data);
          language = mousepad_languages_get_radio_action_group_active (radio_actions);
          break;
        }
    }
  g_list_free (actions);

  return language;
}



void
mousepad_languages_set_active (GtkActionGroup    *group,
                               GtkSourceLanguage *language)
{
  GtkAction *action;

  g_return_if_fail (GTK_IS_ACTION_GROUP (group));

  action = mousepad_languages_get_action (group, language);
  if (G_UNLIKELY (action == NULL))
    {
      g_critical ("failed to find action for language '%s'",
                  language ? gtk_source_language_get_id (language) : "none");
      return;
    }

  /* the "language-changed" signal is emitted by the action groups handler
   * for actions activating when we do this */
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
}



GtkAction *
mousepad_languages_get_action (GtkActionGroup    *group,
                               GtkSourceLanguage *language)
{
  gchar       *action_name;
  GtkAction   *action;
  const gchar *language_id;

  g_return_val_if_fail (GTK_IS_ACTION_GROUP (group), NULL);

  if (GTK_IS_SOURCE_LANGUAGE (language))
    language_id = gtk_source_language_get_id (language);
  else
    language_id = "none";

  action_name = g_strdup_printf ("mousepad-language-%s", language_id);
  action = gtk_action_group_get_action (group, action_name);
  g_free (action_name);

  return action;
}



GtkSourceLanguage *
mousepad_languages_get_action_language (GtkAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  return mousepad_action_get_language (action);
}


static gint
mousepad_util_languages_name_compare (gconstpointer a,
                                      gconstpointer b)
{
  const gchar *name_a, *name_b;

  if (G_UNLIKELY (!GTK_IS_SOURCE_LANGUAGE (a)))
    return -(a != b);
  if (G_UNLIKELY (!GTK_IS_SOURCE_LANGUAGE (b)))
    return a != b;

  name_a = gtk_source_language_get_name (GTK_SOURCE_LANGUAGE (a));
  name_b = gtk_source_language_get_name (GTK_SOURCE_LANGUAGE (b));

  return g_utf8_collate (name_a, name_b);
}



GSList *
mousepad_languages_get_sorted_sections (void)
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
      if (G_LIKELY (GTK_IS_SOURCE_LANGUAGE (language)))
        {
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



GSList *
mousepad_languages_get_sorted_for_section (const gchar *section)
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
      if (G_LIKELY (GTK_IS_SOURCE_LANGUAGE (language)))
        {
          /* only get languages in the specified section */
          if (g_strcmp0 (gtk_source_language_get_section (language), section) == 0)
            list = g_slist_prepend (list, language);
        }
      languages++;
    }

  return g_slist_sort(list, (GCompareFunc)mousepad_util_languages_name_compare);
}
