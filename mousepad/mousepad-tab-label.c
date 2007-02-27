/* $Id$ */
/*
 * Copyright (c) 2007 Nick Schermer <nick@xfce.org>
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

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-preferences.h>
#include <mousepad/mousepad-tab-label.h>

static void              mousepad_tab_label_class_init                   (MousepadTabLabelClass  *klass);
static void              mousepad_tab_label_init                         (MousepadTabLabel       *window);
static void              mousepad_tab_label_finalize                     (GObject                *object);
static void              mousepad_tab_label_get_property                 (GObject                *object,
                                                                          guint                   prop_id,
                                                                          GValue                 *value,
                                                                          GParamSpec             *pspec);
static void              mousepad_tab_label_set_property                 (GObject                *object,
                                                                          guint                   prop_id,
                                                                          const GValue           *value,
                                                                          GParamSpec             *pspec);
static void              mousepad_tab_label_close_tab                    (GtkWidget              *widget,
                                                                          MousepadTabLabel       *label);


enum
{
  PROP_0,
  PROP_TITLE,
  PROP_TOOLTIP,
};

enum
{
  CLOSE_TAB,
  LAST_SIGNAL,
};

struct _MousepadTabLabelClass
{
  GtkHBoxClass __parent__;

  /* signals */
  void (*close_tab)   (MousepadTabLabel *label);
};

struct _MousepadTabLabel
{
  GtkHBox              __parent__;

  MousepadPreferences *preferences;

  GtkTooltips         *tooltips;
  GtkWidget           *ebox;
  GtkWidget           *label;
};


static GObjectClass *mousepad_tab_label_parent_class;
static guint         label_signals[LAST_SIGNAL];



GtkWidget *
mousepad_tab_label_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_TAB_LABEL, NULL);
}



GType
mousepad_tab_label_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_type_register_static_simple (GTK_TYPE_HBOX,
                                            I_("MousepadTabLabel"),
                                            sizeof (MousepadTabLabelClass),
                                            (GClassInitFunc) mousepad_tab_label_class_init,
                                            sizeof (MousepadTabLabel),
                                            (GInstanceInitFunc) mousepad_tab_label_init,
                                            0);
    }

  return type;
}



static void
mousepad_tab_label_class_init (MousepadTabLabelClass *klass)
{
  GObjectClass *gobject_class;

  mousepad_tab_label_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_tab_label_finalize;
  gobject_class->get_property = mousepad_tab_label_get_property;
  gobject_class->set_property = mousepad_tab_label_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        "title",
                                                        "title",
                                                        NULL,
                                                        EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_TOOLTIP,
                                   g_param_spec_string ("tooltip",
                                                        "tooltip",
                                                        "tooltip",
                                                        NULL,
                                                        EXO_PARAM_READWRITE));

  label_signals[CLOSE_TAB] =
    g_signal_new ("close-tab",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MousepadTabLabelClass, close_tab),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}



static void
mousepad_tab_label_init (MousepadTabLabel *label)
{
  GtkWidget *button;
  GtkWidget *image;

  label->preferences = mousepad_preferences_get ();

  label->tooltips = gtk_tooltips_new ();
  g_object_ref (G_OBJECT (label->tooltips));
  gtk_object_sink (GTK_OBJECT (label->tooltips));

  gtk_widget_push_composite_child ();

  label->ebox = g_object_new (GTK_TYPE_EVENT_BOX, "border-width", 2, NULL);
  GTK_WIDGET_SET_FLAGS (label->ebox, GTK_NO_WINDOW);
  gtk_box_pack_start (GTK_BOX (label), label->ebox, TRUE, TRUE, 0);
  gtk_widget_show (label->ebox);

  label->label = g_object_new (GTK_TYPE_LABEL,
                                "selectable", FALSE,
                                "xalign", 0.0, NULL);
  gtk_container_add (GTK_CONTAINER (label->ebox), label->label);
  gtk_widget_show (label->label);

  button = gtk_button_new ();
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_container_set_border_width (GTK_CONTAINER (button), 0);
  gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);
  GTK_WIDGET_UNSET_FLAGS (button, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_tooltips_set_tip (label->tooltips, button, _("Close this tab"), NULL);
  gtk_box_pack_start (GTK_BOX (label), button, FALSE, FALSE, 0);
  exo_binding_new (G_OBJECT (label->preferences), "misc-tab-close-buttons", G_OBJECT (button), "visible");

  image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);
  gtk_widget_show (image);

  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (mousepad_tab_label_close_tab), label);

  gtk_widget_pop_composite_child ();
}



static void
mousepad_tab_label_finalize (GObject *object)
{
  MousepadTabLabel *label = MOUSEPAD_TAB_LABEL (object);

  g_object_unref (G_OBJECT (label->preferences));
  g_object_unref (G_OBJECT (label->tooltips));

  (*G_OBJECT_CLASS (mousepad_tab_label_parent_class)->finalize) (object);
}



static void
mousepad_tab_label_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  MousepadTabLabel *label = MOUSEPAD_TAB_LABEL (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_object_get_property (G_OBJECT (label->label), "label", value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
mousepad_tab_label_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  MousepadTabLabel *label = MOUSEPAD_TAB_LABEL (object);
  const gchar      *title;

  switch (prop_id)
    {
      case PROP_TITLE:
        title = g_value_get_string (value);
        gtk_label_set_text (GTK_LABEL (label->label), title);
        break;

      case PROP_TOOLTIP:
        title = g_value_get_string (value);
        gtk_tooltips_set_tip (label->tooltips, label->ebox, title, NULL);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}



static void
mousepad_tab_label_close_tab (GtkWidget        *widget,
                              MousepadTabLabel *label)
{
  g_signal_emit (G_OBJECT (label), label_signals[CLOSE_TAB], 0);
}
