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
#include <mousepad/mousepad-language-action.h>
#include <mousepad/mousepad-util.h>



enum
{
  PROP_0,
  PROP_LANGUAGE,
  NUM_PROPERTIES
};



struct MousepadLanguageAction_
{
  GtkRadioAction     parent;
  GtkSourceLanguage *language;
};



struct MousepadLanguageActionClass_
{
  GtkRadioActionClass parent_class;
};



static void mousepad_language_action_finalize        (GObject                *object);
static void mousepad_language_action_set_property    (GObject                *object,
                                                      guint                   prop_id,
                                                      const GValue           *value,
                                                      GParamSpec             *pspec);
static void mousepad_language_action_get_property    (GObject                *object,
                                                      guint                   prop_id,
                                                      GValue                 *value,
                                                      GParamSpec             *pspec);
static void mousepad_language_action_set_language    (MousepadLanguageAction *action,
                                                      GtkSourceLanguage      *language);


G_DEFINE_TYPE (MousepadLanguageAction, mousepad_language_action, GTK_TYPE_RADIO_ACTION)



static void
mousepad_language_action_class_init (MousepadLanguageActionClass *klass)
{
  GObjectClass *g_object_class;

  g_object_class = G_OBJECT_CLASS (klass);

  g_object_class->finalize = mousepad_language_action_finalize;
  g_object_class->set_property = mousepad_language_action_set_property;
  g_object_class->get_property = mousepad_language_action_get_property;

  g_object_class_install_property (
    g_object_class,
    PROP_LANGUAGE,
    g_param_spec_object (
      "language",
      "Language",
      "The GtkSourceLanguage associated with the action",
      GTK_SOURCE_TYPE_LANGUAGE,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

}



static void
mousepad_language_action_finalize (GObject *object)
{
  MousepadLanguageAction *self;

  g_return_if_fail (MOUSEPAD_IS_LANGUAGE_ACTION (object));

  self = MOUSEPAD_LANGUAGE_ACTION (object);

  if (GTK_SOURCE_IS_LANGUAGE (self->language))
    g_object_unref (self->language);

  G_OBJECT_CLASS (mousepad_language_action_parent_class)->finalize (object);
}



static void
mousepad_language_action_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  MousepadLanguageAction *self = MOUSEPAD_LANGUAGE_ACTION (object);

  switch (prop_id)
    {
    case PROP_LANGUAGE:
      mousepad_language_action_set_language (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
mousepad_language_action_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  MousepadLanguageAction *self = MOUSEPAD_LANGUAGE_ACTION (object);

  switch (prop_id)
    {
    case PROP_LANGUAGE:
      g_value_set_object (value, mousepad_language_action_get_language (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
mousepad_language_action_init (MousepadLanguageAction *self)
{
  self->language = NULL;
}



GtkAction *
mousepad_language_action_new (GtkSourceLanguage *language)
{
  gchar       *name;
  const gchar *language_id;
  GtkAction   *action;

  if (GTK_SOURCE_IS_LANGUAGE (language))
    language_id = gtk_source_language_get_id (language);
  else
    language_id = "none";

  name = g_strdup_printf ("mousepad-language-%s", language_id);
  action = g_object_new (MOUSEPAD_TYPE_LANGUAGE_ACTION,
                         "name", name,
                         "language", language, NULL);
  g_free (name);

  return action;
}



GtkSourceLanguage *
mousepad_language_action_get_language (MousepadLanguageAction *self)
{
  g_return_val_if_fail (MOUSEPAD_IS_LANGUAGE_ACTION (self), NULL);

  return self->language;
}



static GIcon *
mousepad_language_action_get_icon (MousepadLanguageAction *self)
{
  gchar **mime_types;
  GIcon  *mime_icon = NULL;

  g_return_val_if_fail (MOUSEPAD_IS_LANGUAGE_ACTION (self), NULL);

  if (self->language != NULL)
    {
      mime_types = gtk_source_language_get_mime_types (self->language);
      if (G_LIKELY (mime_types != NULL && g_strv_length (mime_types)) > 0)
        mime_icon = mousepad_util_icon_for_mime_type (mime_types[0]);
      g_strfreev (mime_types);
    }
  else
    mime_icon = mousepad_util_icon_for_mime_type ("text/plain");

  return mime_icon;
}



static void
mousepad_language_action_set_language (MousepadLanguageAction *self,
                                       GtkSourceLanguage      *language)
{
  GQuark  value_quark;
  GIcon  *icon = NULL;

  g_return_if_fail (MOUSEPAD_IS_LANGUAGE_ACTION (self));
  g_return_if_fail (language == NULL || GTK_SOURCE_IS_LANGUAGE (language));

  if (G_UNLIKELY (self->language != NULL))
    g_object_unref (self->language);

  if (G_UNLIKELY (language == NULL))
    {
      self->language = NULL;
      value_quark = g_quark_from_static_string ("none");
      gtk_action_set_label (GTK_ACTION (self), _("Plain Text"));
      gtk_action_set_tooltip (GTK_ACTION (self), _("No filetype"));
    }
  else
    {
      const gchar *language_id, *name, *section;
      gchar       *tooltip;

      self->language = g_object_ref (language);
      language_id = gtk_source_language_get_id (language);
      value_quark = g_quark_from_string (language_id);
      name = gtk_source_language_get_name (language);
      section = gtk_source_language_get_section (language);
      tooltip = g_strdup_printf ("%s/%s", section, name);
      gtk_action_set_label (GTK_ACTION (self), name);
      gtk_action_set_tooltip (GTK_ACTION (self), tooltip);
      g_free (tooltip);
    }

  g_object_set (G_OBJECT (self), "value", value_quark, NULL);

  icon = mousepad_language_action_get_icon (self);
  if (G_IS_ICON (icon))
    {
      gtk_action_set_gicon (GTK_ACTION (self), icon);
      g_object_unref (icon);
    }

  g_object_notify (G_OBJECT (self), "language");
}
