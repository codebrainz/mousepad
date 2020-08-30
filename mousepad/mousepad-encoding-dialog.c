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
#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-encoding.h>
#include <mousepad/mousepad-encoding-dialog.h>
#include <mousepad/mousepad-util.h>

#include <glib/gstdio.h>

#include <gtksourceview/gtksource.h>



static void     mousepad_encoding_dialog_finalize               (GObject                     *object);
static void     mousepad_encoding_dialog_response               (GtkDialog                   *dialog,
                                                                 gint                         response_id);
static gboolean mousepad_encoding_dialog_test_encodings_idle    (gpointer                     user_data);
static void     mousepad_encoding_dialog_test_encodings_destroy (gpointer                     user_data);
static void     mousepad_encoding_dialog_test_encodings         (MousepadEncodingDialog      *dialog);
static void     mousepad_encoding_dialog_cancel_test_encodings  (GtkWidget                   *button,
                                                                 MousepadEncodingDialog      *dialog);
static void     mousepad_encoding_dialog_read_file              (MousepadEncodingDialog      *dialog,
                                                                 MousepadEncoding encoding);
static void     mousepad_encoding_dialog_button_toggled         (GtkWidget                   *button,
                                                                 MousepadEncodingDialog      *dialog);
static void     mousepad_encoding_dialog_combo_changed          (GtkComboBox                 *combo,
                                                                 MousepadEncodingDialog      *dialog);



enum
{
  COLUMN_LABEL,
  COLUMN_ID,
  N_COLUMNS
};

struct _MousepadEncodingDialogClass
{
  GtkDialogClass __parent__;
};

struct _MousepadEncodingDialog
{
  GtkDialog __parent__;

  /* the file */
  MousepadDocument *document;

  /* encoding test idle id */
  guint          timer_id;

  /* boolean to cancel the testing loop */
  guint          cancel_testing : 1;

  /* dialog widget */
  GtkWidget     *button_ok;
  GtkWidget     *button_cancel;
  GtkWidget     *error_box;
  GtkWidget     *error_label;
  GtkWidget     *progress_bar;

  /* the three radio button */
  GtkWidget     *radio_utf8;
  GtkWidget     *radio_system;
  GtkWidget     *radio_other;

  /* other encodings combo box */
  GtkListStore  *store;
  GtkWidget     *combo;
};



G_DEFINE_TYPE (MousepadEncodingDialog, mousepad_encoding_dialog, GTK_TYPE_DIALOG)



static void
mousepad_encoding_dialog_class_init (MousepadEncodingDialogClass *klass)
{
  GObjectClass   *gobject_class;
  GtkDialogClass *gtkdialog_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_encoding_dialog_finalize;

  gtkdialog_class = GTK_DIALOG_CLASS (klass);
  gtkdialog_class->response = mousepad_encoding_dialog_response;
}



static void
mousepad_encoding_dialog_init (MousepadEncodingDialog *dialog)
{
  const gchar     *system_charset;
  gchar           *system_label;
  GtkWidget       *area, *vbox, *hbox, *icon;
  GtkCellRenderer *cell;

  /* set some dialog properties */
  gtk_window_set_default_size (GTK_WINDOW (dialog), 550, 350);

  /* add buttons */
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
  dialog->button_ok = gtk_dialog_add_button (GTK_DIALOG (dialog), _("_OK"), GTK_RESPONSE_OK);

  /* create the header */
  mousepad_util_dialog_header (GTK_DIALOG (dialog), _("The document was not UTF-8 valid"),
                               _("Please select an encoding below."), "text-x-generic");

  /* dialog vbox */
  area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (area), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  /* encoding radio buttons */
  dialog->radio_utf8 = gtk_radio_button_new_with_label (NULL, _("Default (UTF-8)"));
  g_signal_connect (G_OBJECT (dialog->radio_utf8), "toggled",
                    G_CALLBACK (mousepad_encoding_dialog_button_toggled), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->radio_utf8, FALSE, FALSE, 0);
  gtk_widget_show (dialog->radio_utf8);

  g_get_charset (&system_charset);
  system_label = g_strdup_printf ("%s (%s)", _("System"), system_charset);
  dialog->radio_system = gtk_radio_button_new_with_label_from_widget (
                           GTK_RADIO_BUTTON (dialog->radio_utf8), system_label);
  g_signal_connect (G_OBJECT (dialog->radio_system), "toggled",
                    G_CALLBACK (mousepad_encoding_dialog_button_toggled), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->radio_system, FALSE, FALSE, 0);
  gtk_widget_show (dialog->radio_system);
  g_free (system_label);

  dialog->radio_other = gtk_radio_button_new_with_label_from_widget (
                          GTK_RADIO_BUTTON (dialog->radio_system), _("Other:"));
  g_signal_connect (G_OBJECT (dialog->radio_other), "toggled",
                    G_CALLBACK (mousepad_encoding_dialog_button_toggled), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->radio_other, FALSE, FALSE, 0);

  /* create store */
  dialog->store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_INT);

  /* combobox with other charsets */
  dialog->combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (dialog->store));
  gtk_box_pack_start (GTK_BOX (hbox), dialog->combo, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (dialog->combo), "changed",
                    G_CALLBACK (mousepad_encoding_dialog_combo_changed), dialog);

  /* text renderer for 1st column */
  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (dialog->combo), cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (dialog->combo),
                                  cell, "text", COLUMN_LABEL, NULL);

  /* progress bar */
  dialog->progress_bar = gtk_progress_bar_new ();
  gtk_box_pack_start (GTK_BOX (hbox), dialog->progress_bar, TRUE, TRUE, 0);
  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (dialog->progress_bar),
                             _("Checking encodings..."));
  gtk_widget_show (dialog->progress_bar);

  /* cancel button */
  dialog->button_cancel = gtk_button_new_with_mnemonic (_("_Cancel"));
  gtk_box_pack_start (GTK_BOX (hbox), dialog->button_cancel, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (dialog->button_cancel), "clicked",
                    G_CALLBACK (mousepad_encoding_dialog_cancel_test_encodings), dialog);
  gtk_widget_show (dialog->button_cancel);

  /* error box */
  dialog->error_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (vbox), dialog->error_box, FALSE, FALSE, 0);

  /* error icon */
  icon = gtk_image_new_from_icon_name ("dialog-error", GTK_ICON_SIZE_BUTTON);
  gtk_box_pack_start (GTK_BOX (dialog->error_box), icon, FALSE, FALSE, 0);
  gtk_widget_show (icon);

  /* error label */
  dialog->error_label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (dialog->error_box), dialog->error_label, FALSE, FALSE, 0);
  gtk_label_set_use_markup (GTK_LABEL (dialog->error_label), TRUE);
  gtk_widget_show (dialog->error_label);

  /* create text view */
  dialog->document = mousepad_document_new ();
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (dialog->document), TRUE, TRUE, 0);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (dialog->document->textview), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (dialog->document->textview), FALSE);
  g_settings_unbind (G_OBJECT (dialog->document->textview), "show-line-numbers");
  gtk_source_view_set_show_line_numbers (GTK_SOURCE_VIEW (dialog->document->textview), FALSE);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (dialog->document->textview), GTK_WRAP_NONE);
  gtk_widget_show (GTK_WIDGET (dialog->document));
}



static void
mousepad_encoding_dialog_finalize (GObject *object)
{
  MousepadEncodingDialog *dialog = MOUSEPAD_ENCODING_DIALOG (object);

  /* stop running timeout */
  if (G_UNLIKELY (dialog->timer_id))
    g_source_remove (dialog->timer_id);

  /* clear and release store */
  gtk_list_store_clear (dialog->store);
  g_object_unref (G_OBJECT (dialog->store));

  (*G_OBJECT_CLASS (mousepad_encoding_dialog_parent_class)->finalize) (object);
}



static void
mousepad_encoding_dialog_response (GtkDialog *dialog,
                                   gint       response_id)
{
  /* make sure we cancel encoding testing asap */
  MOUSEPAD_ENCODING_DIALOG (dialog)->cancel_testing = TRUE;
}



static gboolean
mousepad_encoding_dialog_test_encodings_idle (gpointer user_data)
{
  MousepadEncodingDialog *dialog = MOUSEPAD_ENCODING_DIALOG (user_data);
  const gchar            *filename;
  GMappedFile            *mapped_file;
  GError                 *error = NULL;
  const gchar            *contents;
  gsize                   length, written;
  guint                   i, n;
  gchar                  *encoded;

  /* get the filename */
  filename = mousepad_file_get_filename (dialog->document->file);

  /* check if the file exists */
  if (filename && g_file_test (filename, G_FILE_TEST_EXISTS))
    {
      /* try to open the file */
      mapped_file = g_mapped_file_new (filename, FALSE, &error);

      if (G_LIKELY (mapped_file))
        {
          /* get the mapped file contents and length */
          contents = g_mapped_file_get_contents (mapped_file);
          length = g_mapped_file_get_length (mapped_file);

          if (G_LIKELY (contents && length > 0))
            {
              /* test all the encodings */
              for (i = 0, n = 0; i < n_encoding_infos && !dialog->cancel_testing; i++)
                {
                  /* set progress bar fraction */
                  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (dialog->progress_bar), (i + 1.00) / n_encoding_infos);

                  /* try to convert the content */
                  encoded = g_convert (contents, length, "UTF-8", encoding_infos[i].charset, NULL, &written, NULL);

                  if (G_LIKELY (encoded))
                    {
                      /* glib uses a faster validator when the string is nul-terminated */
                      if (G_LIKELY (written > 0 && encoded[written] == '\0'))
                        written = -1;

                      /* validate the encoded content */
                      if (G_LIKELY (g_utf8_validate (encoded, written, NULL)))
                        {
                          /* insert in the store */
                          gtk_list_store_insert_with_values (dialog->store, NULL, n++,
                                                             COLUMN_LABEL, encoding_infos[i].charset,
                                                             COLUMN_ID, encoding_infos[i].encoding, -1);
                        }

                      /* cleanup */
                      g_free (encoded);
                    }

                  /* iterate the main loop to update the gui */
                  while (gtk_events_pending ())
                    gtk_main_iteration ();
                }
            }

          /* close the mapped file */
          g_mapped_file_unref (mapped_file);
        }
    }

  /* hide progress bar and cancel button */
  gtk_widget_hide (dialog->progress_bar);
  gtk_widget_hide (dialog->button_cancel);

  /* show the radio button and combo box */
  gtk_widget_show (dialog->radio_other);
  gtk_widget_show (dialog->combo);

  /* select the first item */
  gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->combo), 0);

  return FALSE;
}



static void
mousepad_encoding_dialog_test_encodings_destroy (gpointer user_data)
{
  MOUSEPAD_ENCODING_DIALOG (user_data)->timer_id = 0;
}



static void
mousepad_encoding_dialog_test_encodings (MousepadEncodingDialog *dialog)
{
  if (G_LIKELY (dialog->timer_id == 0))
    {
      /* reset boolean */
      dialog->cancel_testing = FALSE;

      /* start a new idle function */
      dialog->timer_id = g_idle_add_full (G_PRIORITY_LOW, mousepad_encoding_dialog_test_encodings_idle,
                                          dialog, mousepad_encoding_dialog_test_encodings_destroy);
    }
}



static void
mousepad_encoding_dialog_cancel_test_encodings (GtkWidget              *button,
                                                MousepadEncodingDialog *dialog)
{
  /* cancel the testing loop */
  dialog->cancel_testing = TRUE;
}



static void
mousepad_encoding_dialog_read_file (MousepadEncodingDialog *dialog,
                                    MousepadEncoding        encoding)
{
  GtkTextIter  start, end;
  GError      *error = NULL;
  gchar       *message;
  gint         result;

  /* clear buffer */
  gtk_text_buffer_get_bounds (dialog->document->buffer, &start, &end);
  gtk_text_buffer_delete (dialog->document->buffer, &start, &end);

  /* set encoding */
  mousepad_file_set_encoding (dialog->document->file, encoding);

  /* try to open the file */
  result = mousepad_file_open (dialog->document->file, NULL, &error);

  /* set sensitivity of the ok button */
  gtk_widget_set_sensitive (dialog->button_ok, result == 0);

  if (result == 0)
    {
      /* no error, hide the box */
      gtk_widget_hide (dialog->error_box);
    }
  else
    {
      /* format message */
      message = g_strdup_printf ("<b>%s.</b>", error->message);

      /* set the error label */
      gtk_label_set_markup (GTK_LABEL (dialog->error_label), message);

      /* cleanup */
      g_free (message);

      /* show the error box */
      gtk_widget_show (dialog->error_box);

      /* clear the error */
      g_error_free (error);
    }
}



static void
mousepad_encoding_dialog_button_toggled (GtkWidget              *button,
                                         MousepadEncodingDialog *dialog)
{
  /* ignore inactive buttons */
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    {
      /* set sensitivity of the other combobox */
      gtk_widget_set_sensitive (dialog->combo, (button == dialog->radio_other));

      if (button == dialog->radio_utf8)
        {
          /* open the file */
          mousepad_encoding_dialog_read_file (dialog, MOUSEPAD_ENCODING_UTF_8);
        }
      else if (button == dialog->radio_system)
        {
          /* open the file */
          mousepad_encoding_dialog_read_file (dialog, mousepad_encoding_user ());
        }
      else
        {
          /* poke function */
          mousepad_encoding_dialog_combo_changed (GTK_COMBO_BOX (dialog->combo), dialog);
        }
    }
}



static void
mousepad_encoding_dialog_combo_changed (GtkComboBox            *combo,
                                        MousepadEncodingDialog *dialog)
{
  GtkTreeIter iter;
  gint        id;

  /* get the selected item */
  if (gtk_widget_get_sensitive (GTK_WIDGET (combo)) && gtk_combo_box_get_active_iter (combo, &iter))
    {
      /* get the id */
      gtk_tree_model_get (GTK_TREE_MODEL (dialog->store), &iter, COLUMN_ID, &id, -1);

      /* open the file with other encoding */
      mousepad_encoding_dialog_read_file (dialog, id);
    }
}



GtkWidget *
mousepad_encoding_dialog_new (GtkWindow    *parent,
                              MousepadFile *file)
{
  MousepadEncodingDialog *dialog;

  g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);
  g_return_val_if_fail (MOUSEPAD_IS_FILE (file), NULL);

  /* create the dialog */
  dialog = g_object_new (MOUSEPAD_TYPE_ENCODING_DIALOG, NULL);

  /* set parent window */
  gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

  /* set the filename */
  mousepad_file_set_filename (dialog->document->file, mousepad_file_get_filename (file));

  /* start with the system encoding */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->radio_system), TRUE);

  /* queue idle function */
  mousepad_encoding_dialog_test_encodings (dialog);

  return GTK_WIDGET (dialog);
}



MousepadEncoding
mousepad_encoding_dialog_get_encoding (MousepadEncodingDialog *dialog)
{
  g_return_val_if_fail (MOUSEPAD_IS_ENCODING_DIALOG (dialog), MOUSEPAD_ENCODING_NONE);

  return mousepad_file_get_encoding (dialog->document->file);
}



const gchar *
mousepad_encoding_dialog_get_encoding_custom (MousepadEncodingDialog *dialog)
{
  g_return_val_if_fail (MOUSEPAD_IS_ENCODING_DIALOG (dialog), NULL);

  return NULL;
}
