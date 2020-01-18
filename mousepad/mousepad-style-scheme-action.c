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
#include <mousepad/mousepad-style-scheme-action.h>

#include <glib/gi18n.h>

#include <gtksourceview/gtksource.h>



enum
{
  PROP_0,
  PROP_STYLE_SCHEME,
  NUM_PROPERTIES
};



struct MousepadStyleSchemeAction_
{
  GtkRadioAction        parent;
  GtkSourceStyleScheme *scheme;
};



struct MousepadStyleSchemeActionClass_
{
  GtkRadioActionClass parent_class;
};



static void mousepad_style_scheme_action_finalize         (GObject      *object);
static void mousepad_style_scheme_action_set_property     (GObject      *object,
                                                           guint         prop_id,
                                                           const GValue *value,
                                                           GParamSpec   *pspec);
static void mousepad_style_scheme_action_get_property     (GObject      *object,
                                                           guint         prop_id,
                                                           GValue       *value,
                                                           GParamSpec   *pspec);
static void mousepad_style_scheme_action_set_style_scheme (MousepadStyleSchemeAction *action,
                                                           GtkSourceStyleScheme      *scheme);



G_DEFINE_TYPE (MousepadStyleSchemeAction, mousepad_style_scheme_action, GTK_TYPE_RADIO_ACTION)



static void
mousepad_style_scheme_action_class_init (MousepadStyleSchemeActionClass *klass)
{
  GObjectClass *g_object_class;

  g_object_class = G_OBJECT_CLASS (klass);

  g_object_class->finalize = mousepad_style_scheme_action_finalize;
  g_object_class->set_property = mousepad_style_scheme_action_set_property;
  g_object_class->get_property = mousepad_style_scheme_action_get_property;

  g_object_class_install_property (
    g_object_class,
    PROP_STYLE_SCHEME,
    g_param_spec_object ("style-scheme",
                         "StyleScheme",
                         "The style scheme related to the action",
                         GTK_SOURCE_TYPE_STYLE_SCHEME,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
}



static void
mousepad_style_scheme_action_finalize (GObject *object)
{
  MousepadStyleSchemeAction *self;

  g_return_if_fail (MOUSEPAD_IS_STYLE_SCHEME_ACTION (object));

  self = MOUSEPAD_STYLE_SCHEME_ACTION (object);

  if (GTK_SOURCE_IS_STYLE_SCHEME (self->scheme))
    g_object_unref (self->scheme);

  G_OBJECT_CLASS (mousepad_style_scheme_action_parent_class)->finalize (object);
}



static void
mousepad_style_scheme_action_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  MousepadStyleSchemeAction *self = MOUSEPAD_STYLE_SCHEME_ACTION (object);

  switch (prop_id)
    {
    case PROP_STYLE_SCHEME:
      mousepad_style_scheme_action_set_style_scheme (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
mousepad_style_scheme_action_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  MousepadStyleSchemeAction *self = MOUSEPAD_STYLE_SCHEME_ACTION (object);

  switch (prop_id)
    {
    case PROP_STYLE_SCHEME:
      g_value_set_object (value, mousepad_style_scheme_action_get_style_scheme (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
mousepad_style_scheme_action_init (MousepadStyleSchemeAction *self)
{
  self->scheme = NULL;
}



GtkAction *
mousepad_style_scheme_action_new (GtkSourceStyleScheme *scheme)
{
  const gchar *scheme_id;
  gchar       *name;
  GtkAction   *action;

  if (GTK_SOURCE_IS_STYLE_SCHEME (scheme))
    scheme_id = gtk_source_style_scheme_get_id (scheme);
  else
    scheme_id = "none";

  name = g_strdup_printf ("mousepad-style-scheme-%s", scheme_id);
  action = g_object_new (MOUSEPAD_TYPE_STYLE_SCHEME_ACTION,
                         "name", name,
                         "style-scheme", scheme, NULL);
  g_free (name);

  return action;
}



static void
mousepad_style_scheme_action_set_style_scheme (MousepadStyleSchemeAction *self,
                                               GtkSourceStyleScheme      *scheme)
{
  GQuark value_quark;

  if (G_UNLIKELY (self->scheme != NULL))
    g_object_unref (self->scheme);

  if (G_UNLIKELY (scheme == NULL))
    {
      self->scheme = NULL;
      gtk_action_set_label (GTK_ACTION (self), _("None"));
      gtk_action_set_tooltip (GTK_ACTION (self), _("No style scheme"));
      value_quark = g_quark_from_static_string ("none");
    }
  else
    {
      gchar       *tooltip, *author;
      gchar      **authors;
      const gchar *scheme_id;

      self->scheme = g_object_ref (scheme);

      authors = (gchar**) gtk_source_style_scheme_get_authors (scheme);
      author = g_strjoinv (", ", authors);

      tooltip = g_strdup_printf (_("%s | Authors: %s | Filename: %s"),
                                 gtk_source_style_scheme_get_description (scheme),
                                 author,
                                 gtk_source_style_scheme_get_filename (scheme));

      gtk_action_set_label (GTK_ACTION (self), gtk_source_style_scheme_get_name (scheme));
      gtk_action_set_tooltip (GTK_ACTION (self), tooltip);

      g_free (author);
      g_free (tooltip);

      scheme_id = gtk_source_style_scheme_get_id (scheme);
      value_quark = g_quark_from_string (scheme_id);
    }

  g_object_set (G_OBJECT (self), "value", value_quark, NULL);

  g_object_notify (G_OBJECT (self), "style-scheme");
}



GtkSourceStyleScheme *
mousepad_style_scheme_action_get_style_scheme (MousepadStyleSchemeAction *self)
{
  g_return_val_if_fail (MOUSEPAD_IS_STYLE_SCHEME_ACTION (self), NULL);

  return self->scheme;
}
