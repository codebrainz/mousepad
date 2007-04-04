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
#include <mousepad/mousepad-statusbar.h>

static void              mousepad_statusbar_class_init                (MousepadStatusbarClass *klass);
static void              mousepad_statusbar_init                      (MousepadStatusbar      *statusbar);
static void              mousepad_statusbar_finalize                  (GObject                *object);

struct _MousepadStatusbarClass
{
  GtkHBoxClass __parent__;
};

struct _MousepadStatusbar
{
  GtkHBox             __parent__;

  /* the three statusbar labels */
  GtkWidget          *label;
  GtkWidget          *position;
  GtkWidget          *overwrite;
};



static GObjectClass *mousepad_statusbar_parent_class;



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
      type = g_type_register_static_simple (GTK_TYPE_HBOX,
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
  GObjectClass *gobject_class;

  mousepad_statusbar_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_statusbar_finalize;
}



static void
mousepad_statusbar_init (MousepadStatusbar *statusbar)
{
  GtkWidget *frame;

  /* spacing between the 3 frames */
  gtk_box_set_spacing (GTK_BOX (statusbar), 3);

  /* tooltip entry */
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (statusbar), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  statusbar->label = gtk_label_new (NULL);
  gtk_label_set_single_line_mode (GTK_LABEL (statusbar->label), TRUE);
  gtk_misc_set_alignment (GTK_MISC (statusbar->label), 0.0, 0.5);
  gtk_misc_set_padding (GTK_MISC (statusbar->label), 2, 2);
  gtk_label_set_ellipsize (GTK_LABEL (statusbar->label), PANGO_ELLIPSIZE_END);
  gtk_container_add (GTK_CONTAINER (frame), statusbar->label);
  gtk_widget_show (statusbar->label);

  /* line and column numbers */
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (statusbar), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  statusbar->position = gtk_label_new (NULL);
  gtk_container_add (GTK_CONTAINER (frame), statusbar->position);
  gtk_misc_set_padding (GTK_MISC (statusbar->position), 2, 2);
  gtk_widget_show (statusbar->position);

  /* overwrite */
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (statusbar), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  statusbar->overwrite = gtk_label_new (NULL);
  gtk_container_add (GTK_CONTAINER (frame), statusbar->overwrite);
  gtk_misc_set_padding (GTK_MISC (statusbar->overwrite), 2, 2);
  gtk_widget_show (statusbar->overwrite);
}



static void
mousepad_statusbar_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (mousepad_statusbar_parent_class)->finalize) (object);
}



void
mousepad_statusbar_set_text (MousepadStatusbar *statusbar,
                             const gchar       *text)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_STATUSBAR (statusbar));

  /* set the label text */
  gtk_label_set_text (GTK_LABEL (statusbar->label), text);
}



void
mousepad_statusbar_set_cursor_position (MousepadStatusbar *statusbar,
                                        gint               line,
                                        gint               column)
{
  gchar *string;

  _mousepad_return_if_fail (MOUSEPAD_IS_STATUSBAR (statusbar));

  string = g_strdup_printf (_("Line %d Col %d"), line, column);
  gtk_label_set_text (GTK_LABEL (statusbar->position), string);
  g_free (string);
}



void
mousepad_statusbar_set_overwrite (MousepadStatusbar *statusbar,
                                  gboolean           overwrite)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_STATUSBAR (statusbar));

  gtk_label_set_text (GTK_LABEL (statusbar->overwrite), overwrite ? _("OVR") : _("INS"));
}
