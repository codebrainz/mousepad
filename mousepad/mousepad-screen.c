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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-file.h>
#include <mousepad/mousepad-view.h>
#include <mousepad/mousepad-screen.h>



static void            mousepad_screen_class_init               (MousepadScreenClass  *klass);
static void            mousepad_screen_init                     (MousepadScreen       *screen);
static void            mousepad_screen_finalize                 (GObject              *object);
static void            mousepad_screen_get_property             (GObject              *object,
                                                                 guint                 prop_id,
                                                                 GValue               *value,
                                                                 GParamSpec           *pspec);
static void            mousepad_screen_set_property             (GObject              *object,
                                                                 guint                 prop_id,
                                                                 const GValue         *value,
                                                                 GParamSpec           *pspec);
static void            mousepad_screen_set_font                 (MousepadScreen       *screen,
                                                                 const gchar          *font_name);
static void            mousepad_screen_set_word_wrap            (MousepadScreen       *screen,
                                                                 gboolean              word_wrap);
static void            mousepad_screen_modified_changed         (GtkTextBuffer        *buffer,
                                                                 MousepadScreen       *screen);
static void            mousepad_screen_has_selection            (GtkTextBuffer        *buffer,
                                                                 GParamSpec           *pspec,
                                                                 MousepadScreen       *screen);



enum
{
  PROP_0,
  PROP_AUTO_INDENT,
  PROP_FILENAME,
  PROP_FONT_NAME,
  PROP_LINE_NUMBERS,
  PROP_TITLE,
  PROP_WORD_WRAP,
};

enum
{
  SELECTION_CHANGED,
  MODIFIED_CHANGED,
  LAST_SIGNAL,
};

struct _MousepadScreenClass
{
  GtkScrolledWindowClass __parent__;
};

struct _MousepadScreen
{
  GtkScrolledWindow  __parent__;

  GtkWidget         *textview;

  /* absolute path of the file */
  gchar             *filename;

  /* name of the file used for the titles */
  gchar             *display_name;

  /* last document modified time */
  time_t             mtime;
};



static guint         screen_signals[LAST_SIGNAL];
static GObjectClass *mousepad_screen_parent_class;
static guint         untitled_counter = 0;



GtkWidget *
mousepad_screen_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_SCREEN, NULL);
}



GType
mousepad_screen_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_type_register_static_simple (GTK_TYPE_SCROLLED_WINDOW,
                                            I_("MousepadScreen"),
                                            sizeof (MousepadScreenClass),
                                            (GClassInitFunc) mousepad_screen_class_init,
                                            sizeof (MousepadScreen),
                                            (GInstanceInitFunc) mousepad_screen_init,
                                            0);
    }

  return type;
}



static void
mousepad_screen_class_init (MousepadScreenClass *klass)
{
  GObjectClass   *gobject_class;

  mousepad_screen_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_screen_finalize;
  gobject_class->get_property = mousepad_screen_get_property;
  gobject_class->set_property = mousepad_screen_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_AUTO_INDENT,
                                   g_param_spec_boolean ("auto-indent",
                                                         "auto-indent",
                                                         "auto-indent",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_FILENAME,
                                   g_param_spec_string ("filename",
                                                        "filename",
                                                        "filename",
                                                        NULL,
                                                        EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_FONT_NAME,
                                   g_param_spec_string ("font-name",
                                                        "font-name",
                                                        "font-name",
                                                        "Bitstream Vera Sans Mono 10",
                                                        EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_LINE_NUMBERS,
                                   g_param_spec_boolean ("line-numbers",
                                                         "line-numbers",
                                                         "line-numbers",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        "title",
                                                        "title",
                                                        NULL,
                                                        EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_WORD_WRAP,
                                   g_param_spec_boolean ("word-wrap",
                                                         "word-wrap",
                                                         "word-wrap",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  screen_signals[SELECTION_CHANGED] =
    g_signal_new (I_("selection-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  screen_signals[MODIFIED_CHANGED] =
    g_signal_new (I_("modified-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}



static void
mousepad_screen_init (MousepadScreen *screen)
{
  GtkTextBuffer *buffer;

  /* initialize the variables */
  screen->filename     = NULL;
  screen->display_name = g_strdup_printf ("%s %d", _("Untitled"), ++untitled_counter);
  screen->mtime        = 0;

  /* setup the scolled window */
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (screen),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (screen), NULL);
  gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (screen), NULL);

  /* setup the textview */
  screen->textview = g_object_new (MOUSEPAD_TYPE_VIEW, NULL);
  gtk_container_add (GTK_CONTAINER (screen), screen->textview);
  gtk_widget_show (screen->textview);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (screen->textview));
  g_signal_connect (G_OBJECT (buffer), "modified-changed",
                    G_CALLBACK (mousepad_screen_modified_changed), screen);
  g_signal_connect (G_OBJECT (buffer), "notify::has-selection",
                    G_CALLBACK (mousepad_screen_has_selection), screen);

}



static void
mousepad_screen_finalize (GObject *object)
{
  MousepadScreen *screen = MOUSEPAD_SCREEN (object);

  g_free (screen->filename);
  g_free (screen->display_name);

  (*G_OBJECT_CLASS (mousepad_screen_parent_class)->finalize) (object);
}



static void
mousepad_screen_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  MousepadScreen *screen = MOUSEPAD_SCREEN (object);

  switch (prop_id)
    {
      case PROP_FILENAME:
        g_value_set_static_string (value, mousepad_screen_get_filename (screen));
        break;

      case PROP_TITLE:
        g_value_set_static_string (value, mousepad_screen_get_title (screen, FALSE));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}



static void
mousepad_screen_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  MousepadScreen *screen = MOUSEPAD_SCREEN (object);

  switch (prop_id)
    {
      case PROP_FONT_NAME:
        mousepad_screen_set_font (screen, g_value_get_string (value));
        break;

      case PROP_LINE_NUMBERS:
        mousepad_view_set_show_line_numbers (MOUSEPAD_VIEW (screen->textview),
                                             g_value_get_boolean (value));
        break;

      case PROP_AUTO_INDENT:
        mousepad_view_set_auto_indent (MOUSEPAD_VIEW (screen->textview),
                                       g_value_get_boolean (value));
        break;

      case PROP_WORD_WRAP:
        mousepad_screen_set_word_wrap (screen, g_value_get_boolean (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}



static void
mousepad_screen_set_font (MousepadScreen *screen,
                          const gchar    *font_name)
{
  PangoFontDescription *font_desc;

  _mousepad_return_if_fail (MOUSEPAD_IS_SCREEN (screen));

  if (G_LIKELY (font_name))
    {
      font_desc = pango_font_description_from_string (font_name);
      gtk_widget_modify_font (GTK_WIDGET (screen->textview), font_desc);
      pango_font_description_free (font_desc);
    }
}



static void
mousepad_screen_set_word_wrap (MousepadScreen *screen,
                               gboolean        word_wrap)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_SCREEN (screen));

  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (screen->textview),
                               word_wrap ? GTK_WRAP_WORD : GTK_WRAP_NONE);
}



static void
mousepad_screen_modified_changed (GtkTextBuffer  *buffer,
                                  MousepadScreen *screen)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_SCREEN (screen));

  g_signal_emit (G_OBJECT (screen), screen_signals[MODIFIED_CHANGED], 0);
}



static void
mousepad_screen_has_selection (GtkTextBuffer  *buffer,
                               GParamSpec     *pspec,
                               MousepadScreen *screen)
{
  gboolean selected;

  _mousepad_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  _mousepad_return_if_fail (MOUSEPAD_IS_SCREEN (screen));

  g_object_get (G_OBJECT (buffer), "has-selection", &selected, NULL);

  g_signal_emit (G_OBJECT (screen), screen_signals[SELECTION_CHANGED], 0, selected);
}



void
mousepad_screen_set_filename (MousepadScreen *screen,
                              const gchar    *filename)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_SCREEN (screen));
  _mousepad_return_if_fail (filename != NULL);

  /* cleanup the old names */
  g_free (screen->filename);
  g_free (screen->display_name);

  /* create the new names */
  screen->filename = g_strdup (filename);
  screen->display_name = g_filename_display_basename (filename);

  /* tell the listeners */
  g_object_notify (G_OBJECT (screen), "title");
}



gboolean
mousepad_screen_open_file (MousepadScreen  *screen,
                           const gchar     *filename,
                           GError         **error)
{
  GtkTextBuffer *buffer;
  GtkTextIter    iter;
  gboolean       succeed = FALSE;
  gboolean       readonly;
  gint           new_mtime;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_SCREEN (screen), FALSE);
  _mousepad_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _mousepad_return_val_if_fail (filename != NULL, FALSE);

  /* get the buffer */
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (screen->textview));

  /* we're going to add the file content */
  gtk_text_buffer_begin_user_action (buffer);

  /* insert the file content */
  if (mousepad_file_read_to_buffer (filename,
                                    buffer,
                                    &new_mtime,
                                    &readonly,
                                    error))
    {
      /* set the new filename */
      mousepad_screen_set_filename (screen, filename);

      /* set the new mtime */
      screen->mtime = new_mtime;

      /* whether the textview is editable */
      gtk_text_view_set_editable (GTK_TEXT_VIEW (screen->textview), !readonly);

      /* move the cursors to the first place and pretend nothing happend */
      gtk_text_buffer_get_start_iter (buffer, &iter);
      gtk_text_buffer_place_cursor (buffer, &iter);
      gtk_text_buffer_set_modified (buffer, FALSE);
      gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (screen->textview), &iter, 0, FALSE, 0, 0);

      /* it worked out very well */
      succeed = TRUE;
    }

  /* and we're done */
  gtk_text_buffer_end_user_action (buffer);

  return succeed;
}



gboolean
mousepad_screen_save_file (MousepadScreen  *screen,
                           const gchar     *filename,
                           GError         **error)
{
  gchar         *content;
  gchar         *converted = NULL;
  GtkTextBuffer *buffer;
  GtkTextIter    start, end;
  gsize          bytes;
  gint           new_mtime;
  gboolean       succeed = FALSE;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_SCREEN (screen), FALSE);
  _mousepad_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _mousepad_return_val_if_fail (filename != NULL, FALSE);

  /* get the buffer */
  buffer = mousepad_screen_get_text_buffer (screen);

  /* get the textview content */
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  content = gtk_text_buffer_get_slice (buffer, &start, &end, TRUE);

  if (G_LIKELY (content != NULL))
    {
      /* TODO: fix the file encoding here, basic utf8 for testing */
      converted = g_convert (content,
                             strlen (content),
                             "UTF-8",
                             "UTF-8",
                             NULL,
                             &bytes,
                             error);

      /* cleanup */
      g_free (content);

      if (G_LIKELY (converted != NULL))
        {
          succeed = mousepad_file_save_data (filename, converted, bytes,
                                             &new_mtime, error);

          /* cleanup */
          g_free (converted);

          /* saving work w/o problems */
          if (G_LIKELY (succeed))
            {
              /* set the new mtime */
              screen->mtime = new_mtime;

              /* nothing happend */
              gtk_text_buffer_set_modified (buffer, FALSE);

              /* we were allowed to write */
              gtk_text_view_set_editable (GTK_TEXT_VIEW (screen->textview), TRUE);

              /* emit the modified-changed signal */
              mousepad_screen_modified_changed (NULL, screen);
            }
        }
    }

  return succeed;
}



const gchar *
mousepad_screen_get_title (MousepadScreen *screen,
                           gboolean        show_full_path)
{
  const gchar *title;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_SCREEN (screen), NULL);

  if (G_UNLIKELY (show_full_path && screen->filename))
    title = screen->filename;
  else
    title = screen->display_name;

  return title;
}



const gchar *
mousepad_screen_get_filename (MousepadScreen *screen)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_SCREEN (screen), NULL);

  return screen->filename;
}



gboolean
mousepad_screen_get_mtime (MousepadScreen *screen)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_SCREEN (screen), FALSE);

  return screen->mtime;
}



gboolean
mousepad_screen_get_modified (MousepadScreen *screen)
{
  GtkTextBuffer *buffer;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_SCREEN (screen), FALSE);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (screen->textview));

  return gtk_text_buffer_get_modified (buffer);
}



GtkTextView *
mousepad_screen_get_text_view (MousepadScreen  *screen)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_SCREEN (screen), NULL);

  return GTK_TEXT_VIEW (screen->textview);
}



GtkTextBuffer *
mousepad_screen_get_text_buffer (MousepadScreen  *screen)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_SCREEN (screen), NULL);

  return gtk_text_view_get_buffer (GTK_TEXT_VIEW (screen->textview));
}
