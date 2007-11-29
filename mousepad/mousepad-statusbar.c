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

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-statusbar.h>
#include <mousepad/mousepad-window.h>
#include <mousepad/mousepad-util.h>



static void              mousepad_statusbar_class_init                (MousepadStatusbarClass *klass);
static void              mousepad_statusbar_init                      (MousepadStatusbar      *statusbar);
static gboolean          mousepad_statusbar_overwrite_clicked         (GtkWidget              *widget,
                                                                       GdkEventButton         *event,
                                                                       MousepadStatusbar      *statusbar);



enum
{
  ENABLE_OVERWRITE,
  LAST_SIGNAL,
};

struct _MousepadStatusbarClass
{
  GtkStatusbarClass __parent__;
};

struct _MousepadStatusbar
{
  GtkStatusbar        __parent__;

  /* whether overwrite is enabled */
  guint               overwrite_enabled : 1;

  /* extra labels in the statusbar */
  GtkWidget          *position;
  GtkWidget          *overwrite;
};



static guint statusbar_signals[LAST_SIGNAL];



GtkWidget *
mousepad_statusbar_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_STATUSBAR, NULL);
}



GType
mousepad_statusbar_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_type_register_static_simple (GTK_TYPE_STATUSBAR,
                                            I_("MousepadStatusbar"),
                                            sizeof (MousepadStatusbarClass),
                                            (GClassInitFunc) mousepad_statusbar_class_init,
                                            sizeof (MousepadStatusbar),
                                            (GInstanceInitFunc) mousepad_statusbar_init,
                                            0);
    }

  return type;
}



static void
mousepad_statusbar_class_init (MousepadStatusbarClass *klass)
{
  GObjectClass   *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);

  statusbar_signals[ENABLE_OVERWRITE] =
    g_signal_new (I_("enable-overwrite"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_NO_HOOKS,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}



static void
mousepad_statusbar_init (MousepadStatusbar *statusbar)
{
  GtkWidget    *ebox, *box, *separator;
  GtkStatusbar *bar = GTK_STATUSBAR (statusbar);

  /* init statusbar */
  gtk_statusbar_set_has_resize_grip (bar, TRUE);

  /* create a new horizontal box */
  box = gtk_hbox_new (FALSE, 8);
  gtk_widget_show (box);

  /* reorder the gtk statusbar */
  g_object_ref (G_OBJECT (bar->label));
  gtk_container_remove (GTK_CONTAINER (bar->frame), bar->label);
  gtk_container_add (GTK_CONTAINER (bar->frame), box);
  gtk_box_pack_start (GTK_BOX (box), bar->label, TRUE, TRUE, 0);
  g_object_unref (G_OBJECT (bar->label));

  /* separator */
  separator = gtk_vseparator_new ();
  gtk_box_pack_start (GTK_BOX (box), separator, FALSE, FALSE, 0);
  gtk_widget_show (separator);

  /* line and column numbers */
  statusbar->position = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (box), statusbar->position, FALSE, TRUE, 0);
  gtk_widget_show (statusbar->position);

  /* separator */
  separator = gtk_vseparator_new ();
  gtk_box_pack_start (GTK_BOX (box), separator, FALSE, FALSE, 0);
  gtk_widget_show (separator);

  /* overwrite event box */
  ebox = gtk_event_box_new ();
  gtk_box_pack_start (GTK_BOX (box), ebox, FALSE, TRUE, 0);
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (ebox), FALSE);
  mousepad_util_set_tooltip (ebox, _("Toggle the overwrite mode"));
  g_signal_connect (G_OBJECT (ebox), "button-press-event", G_CALLBACK (mousepad_statusbar_overwrite_clicked), statusbar);
  gtk_widget_show (ebox);

  /* overwrite label */
  statusbar->overwrite = gtk_label_new (_("OVR"));
  gtk_container_add (GTK_CONTAINER (ebox), statusbar->overwrite);
  gtk_widget_show (statusbar->overwrite);
}



static gboolean
mousepad_statusbar_overwrite_clicked (GtkWidget         *widget,
                                      GdkEventButton    *event,
                                      MousepadStatusbar *statusbar)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_STATUSBAR (statusbar), FALSE);

  /* only respond on the left button click */
  if (event->type != GDK_BUTTON_PRESS || event->button != 1)
    return FALSE;

  /* swap the overwrite mode */
  statusbar->overwrite_enabled = !statusbar->overwrite_enabled;

  /* send the signal */
  g_signal_emit (G_OBJECT (statusbar), statusbar_signals[ENABLE_OVERWRITE], 0, statusbar->overwrite_enabled);

  return TRUE;
}



void
mousepad_statusbar_set_cursor_position (MousepadStatusbar *statusbar,
                                        gint               line,
                                        gint               column,
                                        gint               selection)
{
  gchar string[64];

  _mousepad_return_if_fail (MOUSEPAD_IS_STATUSBAR (statusbar));

  /* create printable string */
  if (G_UNLIKELY (selection > 0))
    g_snprintf (string, sizeof (string), _("Line: %d Column: %d Selection: %d"), line, column, selection);
  else
    g_snprintf (string, sizeof (string), _("Line: %d Column: %d"), line, column);

  /* set label */
  gtk_label_set_text (GTK_LABEL (statusbar->position), string);
}



void
mousepad_statusbar_set_overwrite (MousepadStatusbar *statusbar,
                                  gboolean           overwrite)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_STATUSBAR (statusbar));

  gtk_widget_set_sensitive (statusbar->overwrite, overwrite);

  statusbar->overwrite_enabled = overwrite;
}
