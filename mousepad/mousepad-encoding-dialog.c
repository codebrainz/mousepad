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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-encoding-dialog.h>
#include <mousepad/mousepad-preferences.h>
#include <mousepad/mousepad-util.h>



static void     mousepad_encoding_dialog_class_init             (MousepadEncodingDialogClass *klass);
static void     mousepad_encoding_dialog_init                   (MousepadEncodingDialog      *dialog);
static void     mousepad_encoding_dialog_finalize               (GObject                     *object);
static gboolean mousepad_encoding_dialog_test_encodings_idle    (gpointer                     user_data);
static void     mousepad_encoding_dialog_test_encodings_destroy (gpointer                     user_data);
static void     mousepad_encoding_dialog_test_encodings         (MousepadEncodingDialog      *dialog);
static void     mousepad_encoding_dialog_read_file              (MousepadEncodingDialog      *dialog,
                                                                 const gchar                 *encoding);
static void     mousepad_encoding_dialog_button_toggled         (GtkWidget                   *button,
                                                                 MousepadEncodingDialog      *dialog);
static void     mousepad_encoding_dialog_combo_changed          (GtkComboBox                 *combo,
                                                                 MousepadEncodingDialog      *dialog);



enum
{
  COLUMN_LABEL,
  COLUMN_ID,
  COLUMN_UNPRINTABLE,
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
  guint          encoding_id;

  /* encoding test position */
  gint           encoding_n;

  /* ok button */
  GtkWidget     *button_ok;

  /* error label and box */
  GtkWidget     *error_box;
  GtkWidget     *error_label;

  /* progressbar */
  GtkWidget     *progress;

  /* the three radio button */
  GtkWidget     *radio_utf8;
  GtkWidget     *radio_system;
  GtkWidget     *radio_other;

  /* other encodings combo box */
  GtkListStore  *store;
  GtkWidget     *combo;
};



static const gchar *encodings[] =
{
"ISO-8859-1", "ISO-8859-2", "ISO-8859-3", "ISO-8859-4", "ISO-8859-5", "ISO-8859-6",
"ISO-8859-7", "ISO-8859-8", "ISO-8859-9", "ISO-8859-10", "ISO-8859-11", "ISO-8859-13",
"ISO-8859-14", "ISO-8859-15", "ISO-8859-16",

"ARMSCII-8",
"ASCII",
"BIG5",
"BIG5-HKSCS",
"BIG5-HKSCS:1999",
"BIG5-HKSCS:2001",
"C99", "IBM850", "CP862", "CP866", "CP874", "CP932", "CP936", "CP949", "CP950",
"CP1133", "CP1250", "CP1251", "CP1252", "CP1253", "CP1254", "CP1255", "CP1256", "CP1257", "CP1258",
"EUC-CN",
"EUC-JP",
"EUC-KR",
"EUC-TW",
"GB18030",
"GBK",
"Georgian-Academy",
"Georgian-PS",
"HP-ROMAN8",
"HZ",
"ISO-2022-CN",
"ISO-2022-CN-EXT",
"ISO-2022-JP",
"ISO-2022-JP-1",
"ISO-2022-JP-2",
"ISO-2022-KR",



"JAVA",
"JOHAB",
"KOI8-R",
"KOI8-RU",
"KOI8-T",
"KOI8-U",
"MacArabic",
"MacCentralEurope",
"MacCroatian",
"MacGreek",
"MacHebrew",
"MacIceland",
"Macintosh",
"MacMacRoman",
"MacRomania",
"MacThai",
"MacTurkish",
"MacUkraine",
"MacCyrillic",
"MuleLao-1",
"NEXTSTEP",
"PT154",
"SHIFT_JIS",
"TCVN",
"TIS-620,",
"UCS-2",
"UCS-2-INTERNAL",
"UCS-2BE",
"UCS-2LE",
"UCS-4",
"UCS-4-INTERNAL",
"UCS-4BE",
"UCS-4LE",
"UTF-16",
"UTF-16BE",
"UTF-16LE",
"UTF-32",
"UTF-32BE",
"UTF-32LE",
"UTF-7",
"UTF-8",
"VISCII"
};



G_DEFINE_TYPE (MousepadEncodingDialog, mousepad_encoding_dialog, GTK_TYPE_DIALOG);



static void
mousepad_encoding_dialog_class_init (MousepadEncodingDialogClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_encoding_dialog_finalize;
}



static void
mousepad_encoding_dialog_init (MousepadEncodingDialog *dialog)
{
  const gchar     *system_charset;
  gchar           *system_label;
  GtkWidget       *vbox, *hbox, *icon;
  GtkCellRenderer *cell;

  /* set some dialog properties */
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 550, 350);

  /* add buttons */
  gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
  dialog->button_ok = gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_OK, GTK_RESPONSE_OK);

  /* create the header */
  mousepad_util_dialog_header (GTK_DIALOG (dialog), _("The document was not UTF-8 valid"),
                               _("Please select an encoding below."), GTK_STOCK_FILE);

  /* dialog vbox */
  vbox = gtk_vbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  hbox = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  /* encoding radio buttons */
  dialog->radio_utf8 = gtk_radio_button_new_with_label (NULL, _("Default (UTF-8)"));
  g_signal_connect (G_OBJECT (dialog->radio_utf8), "toggled", G_CALLBACK (mousepad_encoding_dialog_button_toggled), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->radio_utf8, FALSE, FALSE, 0);
  gtk_widget_show (dialog->radio_utf8);

  g_get_charset (&system_charset);
  system_label = g_strdup_printf ("%s (%s)", _("System"), system_charset);
  dialog->radio_system = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (dialog->radio_utf8), system_label);
  g_signal_connect (G_OBJECT (dialog->radio_system), "toggled", G_CALLBACK (mousepad_encoding_dialog_button_toggled), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->radio_system, FALSE, FALSE, 0);
  gtk_widget_show (dialog->radio_system);
  g_free (system_label);

  dialog->radio_other = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (dialog->radio_system), _("Other:"));
  g_signal_connect (G_OBJECT (dialog->radio_other), "toggled", G_CALLBACK (mousepad_encoding_dialog_button_toggled), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->radio_other, FALSE, FALSE, 0);

  /* create store */
  dialog->store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT);

  /* combobox with other charsets */
  dialog->combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (dialog->store));
  gtk_box_pack_start (GTK_BOX (hbox), dialog->combo, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (dialog->combo), "changed", G_CALLBACK (mousepad_encoding_dialog_combo_changed), dialog);

  /* text renderer for 1st column */
  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (dialog->combo), cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (dialog->combo), cell, "text", COLUMN_LABEL, NULL);

  /* progress bar */
  dialog->progress = gtk_progress_bar_new ();
  gtk_box_pack_start (GTK_BOX (hbox), dialog->progress, TRUE, TRUE, 0);
  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (dialog->progress), "Testing encodings...");
  gtk_widget_show (dialog->progress);

  /* error box */
  dialog->error_box = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), dialog->error_box, FALSE, FALSE, 0);

  /* error icon */
  icon = gtk_image_new_from_stock (GTK_STOCK_DIALOG_ERROR, GTK_ICON_SIZE_BUTTON);
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
  mousepad_view_set_line_numbers (dialog->document->textview, FALSE);
  mousepad_document_set_word_wrap (dialog->document, FALSE);
  gtk_widget_show (GTK_WIDGET (dialog->document));

  /* lock undo manager forever */
  mousepad_undo_lock (dialog->document->undo);
}



static void
mousepad_encoding_dialog_finalize (GObject *object)
{
  MousepadEncodingDialog *dialog = MOUSEPAD_ENCODING_DIALOG (object);

  /* stop running timeout */
  if (G_UNLIKELY (dialog->encoding_id))
    g_source_remove (dialog->encoding_id);

  /* clear and release store */
  gtk_list_store_clear (dialog->store);
  g_object_unref (G_OBJECT (dialog->store));

  (*G_OBJECT_CLASS (mousepad_encoding_dialog_parent_class)->finalize) (object);
}



static gboolean
mousepad_encoding_dialog_test_encodings_idle (gpointer user_data)
{
  MousepadEncodingDialog *dialog = MOUSEPAD_ENCODING_DIALOG (user_data);
  gdouble                 fraction;
  gint                    unprintable, value;
  const gchar            *encoding;
  GtkTreeIter             iter, needle;

  GDK_THREADS_ENTER ();

  /* calculate the status */
  fraction = (dialog->encoding_n + 1.00) / G_N_ELEMENTS (encodings);

  /* set progress bar */
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (dialog->progress), fraction);

  /* get the encoding */
  encoding = encodings[dialog->encoding_n];

  /* set an encoding */
  unprintable = mousepad_file_test_encoding (dialog->document->file, encoding, NULL);

  /* add the encoding to the combo box is the test succeed */
  if (unprintable != -1)
    {
      /* get the first model iter, if there is one */
      if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (dialog->store), &needle))
        {
          for (;;)
            {
              /* get the column value */
              gtk_tree_model_get (GTK_TREE_MODEL (dialog->store), &needle, COLUMN_UNPRINTABLE, &value, -1);

              /* insert before the item with a higher number of unprintable characters */
              if (value > unprintable)
                {
                  gtk_list_store_insert_before (dialog->store, &iter, &needle);

                  break;
                }

              /* leave when we reached the end of the tree */
              if (gtk_tree_model_iter_next (GTK_TREE_MODEL (dialog->store), &needle) == FALSE)
                goto append_to_list;
            }
        }
      else
        {
          append_to_list:

          /* append to the list */
          gtk_list_store_append (dialog->store, &iter);
        }

      /* set the column data */
      gtk_list_store_set (dialog->store, &iter,
                          COLUMN_LABEL, encoding,
                          COLUMN_ID, dialog->encoding_n,
                          COLUMN_UNPRINTABLE, unprintable,
                          -1);
    }

  /* advance offset */
  dialog->encoding_n++;

  /* show the widgets when we're done */
  if (dialog->encoding_n == G_N_ELEMENTS (encodings))
    {
      /* select the first item */
      gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->combo), 0);

      /* hide progress bar */
      gtk_widget_hide (dialog->progress);

      /* show the radio button and combo box */
      gtk_widget_show (dialog->radio_other);
      gtk_widget_show (dialog->combo);
    }

  GDK_THREADS_LEAVE ();

  return (dialog->encoding_n < G_N_ELEMENTS (encodings));
}



static void
mousepad_encoding_dialog_test_encodings_destroy (gpointer user_data)
{
  MOUSEPAD_ENCODING_DIALOG (user_data)->encoding_id = 0;
}



static void
mousepad_encoding_dialog_test_encodings (MousepadEncodingDialog *dialog)
{
  if (G_LIKELY (dialog->encoding_id == 0))
    {
      /* reset counter */
      dialog->encoding_n = 0;

      /* start new idle function */
      dialog->encoding_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, mousepad_encoding_dialog_test_encodings_idle,
                                             dialog, mousepad_encoding_dialog_test_encodings_destroy);
    }
}



static void
mousepad_encoding_dialog_read_file (MousepadEncodingDialog *dialog,
                                    const gchar            *encoding)
{
  GtkTextIter  start, end;
  GError      *error = NULL;
  gchar       *message;
  gboolean     succeed;

  /* clear buffer */
  gtk_text_buffer_get_bounds (dialog->document->buffer, &start, &end);
  gtk_text_buffer_delete (dialog->document->buffer, &start, &end);

  /* set encoding */
  mousepad_file_set_encoding (dialog->document->file, encoding);

  /* try to open the file */
  succeed = mousepad_file_open (dialog->document->file, &error);

  /* set sensitivity of the ok button */
  gtk_widget_set_sensitive (dialog->button_ok, succeed);

  if (succeed)
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
  const gchar *system_charset;

  /* ignore inactive buttons */
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    {
      /* set sensitivity of the other combobox */
      gtk_widget_set_sensitive (dialog->combo, (button == dialog->radio_other));

      if (button == dialog->radio_utf8)
        {
          /* open the file */
          mousepad_encoding_dialog_read_file (dialog, NULL);
        }
      else if (button == dialog->radio_system)
        {
          /* get the system charset */
          g_get_charset (&system_charset);

          /* open the file */
          mousepad_encoding_dialog_read_file (dialog, system_charset);
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
  if (GTK_WIDGET_SENSITIVE (combo) && gtk_combo_box_get_active_iter (combo, &iter))
    {
      /* get the id */
      gtk_tree_model_get (GTK_TREE_MODEL (dialog->store), &iter, COLUMN_ID, &id, -1);

      /* open the file with other encoding */
      mousepad_encoding_dialog_read_file (dialog, encodings[id]);
    }
}



GtkWidget *
mousepad_encoding_dialog_new (GtkWindow    *parent,
                              MousepadFile *file)
{
  MousepadEncodingDialog *dialog;

  _mousepad_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);
  _mousepad_return_val_if_fail (MOUSEPAD_IS_FILE (file), NULL);

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



const gchar *
mousepad_encoding_dialog_get_encoding (MousepadEncodingDialog *dialog)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_ENCODING_DIALOG (dialog), NULL);

  return mousepad_file_get_encoding (dialog->document->file);
}
