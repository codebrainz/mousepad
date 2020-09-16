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
#include <mousepad/mousepad-close-button.h>
#include <mousepad/mousepad-settings.h>
#include <mousepad/mousepad-util.h>
#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-marshal.h>
#include <mousepad/mousepad-view.h>
#include <mousepad/mousepad-window.h>

#include <gtksourceview/gtksource.h>

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif

#include <time.h>



static void      mousepad_document_finalize                (GObject                *object);
static void      mousepad_document_notify_cursor_position  (GtkTextBuffer          *buffer,
                                                            GParamSpec             *pspec,
                                                            MousepadDocument       *document);
static void      mousepad_document_notify_has_selection    (GtkTextBuffer          *buffer,
                                                            GParamSpec             *pspec,
                                                            MousepadDocument       *document);
static void      mousepad_document_notify_overwrite        (GtkTextView            *textview,
                                                            GParamSpec             *pspec,
                                                            MousepadDocument       *document);
static void      mousepad_document_notify_language         (GtkSourceBuffer        *buffer,
                                                            GParamSpec             *pspec,
                                                            MousepadDocument       *document);
static void      mousepad_document_drag_data_received      (GtkWidget              *widget,
                                                            GdkDragContext         *context,
                                                            gint                    x,
                                                            gint                    y,
                                                            GtkSelectionData       *selection_data,
                                                            guint                   info,
                                                            guint                   drag_time,
                                                            MousepadDocument       *document);
static void      mousepad_document_filename_changed        (MousepadDocument       *document,
                                                            const gchar            *filename);
static void      mousepad_document_label_color             (MousepadDocument       *document);
static void      mousepad_document_tab_button_clicked      (GtkWidget              *widget,
                                                            MousepadDocument       *document);


enum
{
  CLOSE_TAB,
  CURSOR_CHANGED,
  SELECTION_CHANGED,
  OVERWRITE_CHANGED,
  LANGUAGE_CHANGED,
  LAST_SIGNAL
};

struct _MousepadDocumentClass
{
  GtkScrolledWindowClass __parent__;
};

struct _MousepadDocumentPrivate
{
  GtkScrolledWindow __parent__;

  /* the tab label, ebox and CSS provider */
  GtkWidget           *ebox;
  GtkWidget           *label;
  GtkCssProvider      *css_provider;

  /* utf-8 valid document names */
  gchar               *utf8_filename;
  gchar               *utf8_basename;
};



static guint document_signals[LAST_SIGNAL];



MousepadDocument *
mousepad_document_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_DOCUMENT, NULL);
}



G_DEFINE_TYPE_WITH_PRIVATE (MousepadDocument, mousepad_document, GTK_TYPE_SCROLLED_WINDOW)



static void
mousepad_document_class_init (MousepadDocumentClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_document_finalize;

  document_signals[CLOSE_TAB] =
    g_signal_new (I_("close-tab"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  document_signals[CURSOR_CHANGED] =
    g_signal_new (I_("cursor-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  _mousepad_marshal_VOID__INT_INT_INT,
                  G_TYPE_NONE, 3, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);

  document_signals[SELECTION_CHANGED] =
    g_signal_new (I_("selection-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__INT,
                  G_TYPE_NONE, 1, G_TYPE_INT);

  document_signals[OVERWRITE_CHANGED] =
    g_signal_new (I_("overwrite-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  document_signals[LANGUAGE_CHANGED] =
    g_signal_new (I_("language-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GTK_SOURCE_TYPE_LANGUAGE);
}



static void
mousepad_document_init (MousepadDocument *document)
{
  GtkTargetList        *target_list;

  /* private structure */
  document->priv = mousepad_document_get_instance_private (document);

  /* initialize the variables */
  document->priv->utf8_filename = NULL;
  document->priv->utf8_basename = NULL;
  document->priv->label = NULL;
  document->priv->css_provider = gtk_css_provider_new ();

  /* setup the scolled window */
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (document),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (document), GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (document), NULL);
  gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (document), NULL);

  /* create a textbuffer and associated search context */
  document->buffer = GTK_TEXT_BUFFER (gtk_source_buffer_new (NULL));
  document->search_context = gtk_source_search_context_new (GTK_SOURCE_BUFFER (document->buffer), NULL);
  gtk_source_search_context_set_highlight (document->search_context, FALSE);

  /* initialize the file */
  document->file = mousepad_file_new (document->buffer);

  /* connect signals to the file */
  g_signal_connect_swapped (G_OBJECT (document->file), "filename-changed", G_CALLBACK (mousepad_document_filename_changed), document);

  /* create the highlight tag */
  document->tag = gtk_text_buffer_create_tag (document->buffer, NULL, "background", "#ffff78", NULL);

  /* setup the textview */
  document->textview = g_object_new (MOUSEPAD_TYPE_VIEW, "buffer", document->buffer, NULL);
  gtk_container_add (GTK_CONTAINER (document), GTK_WIDGET (document->textview));
  gtk_widget_show (GTK_WIDGET (document->textview));

  /* also allow dropping of uris and tabs in the textview */
  target_list = gtk_drag_dest_get_target_list (GTK_WIDGET (document->textview));
  gtk_target_list_add_table (target_list, drop_targets, G_N_ELEMENTS (drop_targets));

  /* attach signals to the text view and buffer */
  g_signal_connect (G_OBJECT (document->buffer), "notify::cursor-position", G_CALLBACK (mousepad_document_notify_cursor_position), document);
  g_signal_connect (G_OBJECT (document->buffer), "notify::has-selection", G_CALLBACK (mousepad_document_notify_has_selection), document);
  g_signal_connect_swapped (G_OBJECT (document->buffer), "modified-changed", G_CALLBACK (mousepad_document_label_color), document);
  g_signal_connect_swapped (G_OBJECT (document->file), "readonly-changed", G_CALLBACK (mousepad_document_label_color), document);
  g_signal_connect (G_OBJECT (document->textview), "notify::overwrite", G_CALLBACK (mousepad_document_notify_overwrite), document);
  g_signal_connect (G_OBJECT (document->textview), "drag-data-received", G_CALLBACK (mousepad_document_drag_data_received), document);
  g_signal_connect (G_OBJECT (document->buffer), "notify::language", G_CALLBACK (mousepad_document_notify_language), document);
}



static void
mousepad_document_finalize (GObject *object)
{
  MousepadDocument *document = MOUSEPAD_DOCUMENT (object);

  /* cleanup */
  g_free (document->priv->utf8_filename);
  g_free (document->priv->utf8_basename);
  g_object_unref (document->priv->css_provider);

  /* release the file */
  g_object_unref (G_OBJECT (document->file));

  /* release the buffer reference and the search context reference */
  g_object_unref (G_OBJECT (document->buffer));
  g_object_unref (G_OBJECT (document->search_context));

  (*G_OBJECT_CLASS (mousepad_document_parent_class)->finalize) (object);
}



static void
mousepad_document_notify_cursor_position (GtkTextBuffer    *buffer,
                                          GParamSpec       *pspec,
                                          MousepadDocument *document)
{
  GtkTextIter iter;
  gint        line, column, selection;
  gint        tab_size;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* get the current iter position */
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, gtk_text_buffer_get_insert (buffer));

  /* get the current line number */
  line = gtk_text_iter_get_line (&iter) + 1;

  /* get the tab size */
  tab_size = MOUSEPAD_SETTING_GET_INT (TAB_WIDTH);

  /* get the column */
  column = mousepad_util_get_real_line_offset (&iter, tab_size);

  /* get length of the selection */
  selection = mousepad_view_get_selection_length (document->textview, NULL);

  /* emit the signal */
  g_signal_emit (G_OBJECT (document), document_signals[CURSOR_CHANGED], 0, line, column, selection);
}



static void
mousepad_document_notify_has_selection (GtkTextBuffer    *buffer,
                                        GParamSpec       *pspec,
                                        MousepadDocument *document)
{
  gint     selection;
  gboolean is_column_selection;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* get length of the selection */
  selection = mousepad_view_get_selection_length (document->textview, &is_column_selection);

  /* don't send large numbers */
  if (selection > 1)
    selection = 1;

  /* if it's a column selection with content */
  if (selection == 1 && is_column_selection)
    selection = 2;

  /* emit the signal */
  g_signal_emit (G_OBJECT (document), document_signals[SELECTION_CHANGED], 0, selection);
}



static void
mousepad_document_notify_overwrite (GtkTextView      *textview,
                                    GParamSpec       *pspec,
                                    MousepadDocument *document)
{
  gboolean overwrite;

  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (GTK_IS_TEXT_VIEW (textview));

  /* whether overwrite is enabled */
  overwrite = gtk_text_view_get_overwrite (textview);

  /* emit the signal */
  g_signal_emit (G_OBJECT (document), document_signals[OVERWRITE_CHANGED], 0, overwrite);
}



static void
mousepad_document_notify_language (GtkSourceBuffer  *buffer,
                                   GParamSpec       *pspec,
                                   MousepadDocument *document)
{
  GtkSourceLanguage *language;

  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

  /* the new language */
  language = gtk_source_buffer_get_language (buffer);

  /* emit the signal */
  g_signal_emit (G_OBJECT (document), document_signals[LANGUAGE_CHANGED], 0, language);
}



static void
mousepad_document_drag_data_received (GtkWidget        *widget,
                                      GdkDragContext   *context,
                                      gint              x,
                                      gint              y,
                                      GtkSelectionData *selection_data,
                                      guint             info,
                                      guint             drag_time,
                                      MousepadDocument *document)
{
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* emit the drag-data-received signal from the document when a tab or uri has been dropped */
  if (info == TARGET_TEXT_URI_LIST || info == TARGET_GTK_NOTEBOOK_TAB)
    g_signal_emit_by_name (G_OBJECT (document), "drag-data-received", context, x, y, selection_data, info, drag_time);
}



static void
mousepad_document_filename_changed (MousepadDocument *document,
                                    const gchar      *filename)
{
  gchar *utf8_filename, *utf8_basename;

  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (filename != NULL);

  /* convert the title into a utf-8 valid version for display */
  utf8_filename = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);

  if (G_LIKELY (utf8_filename))
    {
      /* create the display name */
      utf8_basename = g_filename_display_basename (utf8_filename);

      /* remove the old names */
      g_free (document->priv->utf8_filename);
      g_free (document->priv->utf8_basename);

      /* set the new names */
      document->priv->utf8_filename = utf8_filename;
      document->priv->utf8_basename = utf8_basename;

      /* update the tab label and tooltip */
      if (G_UNLIKELY (document->priv->label))
        {
          /* set the tab label */
          gtk_label_set_text (GTK_LABEL (document->priv->label), utf8_basename);

          /* set the tab tooltip */
          gtk_widget_set_tooltip_text (document->priv->ebox, utf8_filename);

          /* update label color */
          mousepad_document_label_color (document);
        }
    }
}



static void
mousepad_document_label_color (MousepadDocument *document)
{
  GtkStyleContext *context;
  const gchar     *css_string;

  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (document->buffer));
  g_return_if_fail (MOUSEPAD_IS_FILE (document->file));

  if (document->priv->label)
    {
      context = gtk_widget_get_style_context (document->priv->label);

      /* label color */
      if (gtk_text_buffer_get_modified (document->buffer))
        css_string = "label { color: red; }";
      else if (mousepad_file_get_read_only (document->file))
        css_string = "label { color: green; }";
      else
        {
          /* fallback to default color */
          gtk_style_context_remove_provider (context, GTK_STYLE_PROVIDER (document->priv->css_provider));
          return;
        }

      /* update color */
      gtk_css_provider_load_from_data (document->priv->css_provider, css_string, -1, NULL);
      gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (document->priv->css_provider),
                                      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
}



void
mousepad_document_set_overwrite (MousepadDocument *document,
                                 gboolean          overwrite)
{
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  gtk_text_view_set_overwrite (GTK_TEXT_VIEW (document->textview), overwrite);
}



void
mousepad_document_focus_textview (MousepadDocument *document)
{
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* focus the textview */
  gtk_widget_grab_focus (GTK_WIDGET (document->textview));
}



void
mousepad_document_send_signals (MousepadDocument *document)
{
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* re-send the cursor changed signal */
  mousepad_document_notify_cursor_position (document->buffer, NULL, document);

  /* re-send the overwrite signal */
  mousepad_document_notify_overwrite (GTK_TEXT_VIEW (document->textview), NULL, document);

  /* re-send the selection status */
  mousepad_document_notify_has_selection (document->buffer, NULL, document);

  /* re-send the language signal */
  mousepad_document_notify_language (GTK_SOURCE_BUFFER (document->buffer), NULL, document);
}



GtkWidget *
mousepad_document_get_tab_label (MousepadDocument *document)
{
  GtkWidget  *hbox, *button;

  /* create the box */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_show (hbox);

  /* the ebox */
  document->priv->ebox = g_object_new (GTK_TYPE_EVENT_BOX, "border-width", 2,
                                       "visible-window", FALSE, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), document->priv->ebox, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text (document->priv->ebox, document->priv->utf8_filename);
  gtk_widget_show (document->priv->ebox);

  /* create the label */
  document->priv->label = gtk_label_new (mousepad_document_get_basename (document));
  gtk_container_add (GTK_CONTAINER (document->priv->ebox), document->priv->label);
  gtk_widget_show (document->priv->label);

  /* set label color */
  mousepad_document_label_color (document);

  /* create the button */
  button = mousepad_close_button_new ();
  gtk_widget_show (button);

  /* pack button, add signal and tooltip */
  gtk_widget_set_tooltip_text (button, _("Close this tab"));
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (mousepad_document_tab_button_clicked), document);

  return hbox;
}



static void
mousepad_document_tab_button_clicked (GtkWidget        *widget,
                                      MousepadDocument *document)
{
  g_signal_emit (G_OBJECT (document), document_signals[CLOSE_TAB], 0);
}



const gchar *
mousepad_document_get_basename (MousepadDocument *document)
{
  static gint untitled_counter = 0;

  g_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), NULL);

  /* check if there is a filename set */
  if (document->priv->utf8_basename == NULL)
    {
      /* create an unique untitled document name */
      document->priv->utf8_basename = g_strdup_printf ("%s %d", _("Untitled"), ++untitled_counter);
    }

  return document->priv->utf8_basename;
}



const gchar *
mousepad_document_get_filename (MousepadDocument *document)
{
  g_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), NULL);

  return document->priv->utf8_filename;
}
