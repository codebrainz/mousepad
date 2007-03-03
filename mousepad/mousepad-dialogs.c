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
#include <mousepad/mousepad-dialogs.h>
#include <mousepad/mousepad-file.h>



static GtkWidget *
mousepad_dialogs_image_button (const gchar *stock_id,
                               const gchar *label)
{
  GtkWidget *button, *image;

  image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (image);

  button = gtk_button_new_with_mnemonic (label);
  gtk_button_set_image (GTK_BUTTON (button), image);
  gtk_widget_show (button);

  return button;
}



void
mousepad_dialogs_show_about (GtkWindow *parent)
{
  static const gchar *authors[] =
  {
    "Erik Harrison <erikharrison@xfce.org>",
    "Nick Schermer <nick@xfce.org>",
    "Benedikt Meurer <benny@xfce.org>",
    NULL,
  };

  /* show the dialog */
  gtk_about_dialog_set_email_hook (exo_url_about_dialog_hook, NULL, NULL);
  gtk_about_dialog_set_url_hook (exo_url_about_dialog_hook, NULL, NULL);
  gtk_show_about_dialog (parent,
                         "authors", authors,
                         "comments", _("Mousepad is a fast text editor for the Xfce Desktop Environment."),
                         "copyright", _("Copyright \302\251 2004-2007 Xfce Development Team"),
                         "destroy-with-parent", TRUE,
                         "license", XFCE_LICENSE_GPL,
                         "logo-icon-name", PACKAGE_NAME,
                         "name", PACKAGE_NAME,
                         "version", PACKAGE_VERSION,
                         "translator-credits", _("translator-credits"),
                         "website", "http://www.xfce.org/projects/mousepad",
                         NULL);
}



void
mousepad_dialogs_show_error (GtkWindow    *parent,
                             const GError *error,
                             const gchar  *message)
{
  GtkWidget *dialog;

  /* create the warning dialog */
  dialog = gtk_message_dialog_new (parent,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   "%s.", message);

  /* set secondary text if an error is provided */
  if (G_LIKELY (error != NULL))
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s.", error->message);

  /* display the dialog */
  gtk_dialog_run (GTK_DIALOG (dialog));

  /* cleanup */
  gtk_widget_destroy (dialog);
}



gint
mousepad_dialogs_jump_to (GtkWindow *parent,
                          gint       current_line,
                          gint       last_line)
{
  GtkWidget     *dialog;
  GtkWidget     *hbox;
  GtkWidget     *label;
  GtkWidget     *button;
  GtkAdjustment *adjustment;
  gint           line_number = 0;

  /* build the dialog */
  dialog = gtk_dialog_new_with_buttons (_("Jump To"),
                                        parent,
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
                                        GTK_STOCK_CANCEL, MOUSEPAD_RESPONSE_CANCEL,
                                        GTK_STOCK_JUMP_TO, MOUSEPAD_RESPONSE_JUMP_TO,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
  gtk_widget_show (hbox);

  label = gtk_label_new_with_mnemonic (_("_Line number:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  /* the spin button adjustment */
  adjustment = (GtkAdjustment *) gtk_adjustment_new (current_line, 1, last_line, 1, 10, 0);

  /* the spin button */
  button = gtk_spin_button_new (adjustment, 1, 0);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (button), TRUE);
  gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (button), TRUE);
  gtk_entry_set_width_chars (GTK_ENTRY (button), 8);
  gtk_entry_set_activates_default (GTK_ENTRY (button), TRUE);
  gtk_label_set_mnemonic_widget (GTK_LABEL(label), button);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  /* run the dialog */
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == MOUSEPAD_RESPONSE_JUMP_TO)
    line_number = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (button));

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  return line_number;
}



gboolean
mousepad_dialogs_clear_recent (GtkWindow *parent)
{
  GtkWidget *dialog;
  gboolean   succeed = FALSE;

  dialog = gtk_message_dialog_new (parent,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("Are you sure you want to clear\nthe Recent History?"));

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("This will only remove the items from the "
                                              "history owned by Mousepad."));

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          GTK_STOCK_CANCEL, MOUSEPAD_RESPONSE_CANCEL,
                          GTK_STOCK_CLEAR, MOUSEPAD_RESPONSE_CLEAR,
                          NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_CANCEL);

  /* popup the dialog */
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == MOUSEPAD_RESPONSE_CLEAR)
    succeed = TRUE;

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  return succeed;
}



gint
mousepad_dialogs_save_changes (GtkWindow *parent)
{
  GtkWidget *dialog;
  gint       response;

  /* create the question dialog */
  dialog = gtk_message_dialog_new (parent,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("Do you want to save changes to this\n"
                                     "document before closing?"));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("If you don't save, your changes will be lost."));

  gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
                                mousepad_dialogs_image_button (GTK_STOCK_DELETE, _("_Don't Save")),
                                MOUSEPAD_RESPONSE_DONT_SAVE);
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          GTK_STOCK_CANCEL, MOUSEPAD_RESPONSE_CANCEL,
                          GTK_STOCK_SAVE, MOUSEPAD_RESPONSE_SAVE,
                          NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_SAVE);

  /* run the dialog and wait for a response */
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  return response;
}



static GtkFileChooserConfirmation
mousepad_dialogs_save_as_callback (GtkFileChooser *dialog)
{
  gchar                      *filename;
  GError                     *error = NULL;
  GtkFileChooserConfirmation  result;

  /* get the filename */
  filename = gtk_file_chooser_get_filename (dialog);

  if (mousepad_file_is_writable (filename, &error))
    {
      /* show the normal confirmation dialog */
      result = GTK_FILE_CHOOSER_CONFIRMATION_CONFIRM;
    }
  else
    {
      /* tell the user he cannot write to this file */
      mousepad_dialogs_show_error (GTK_WINDOW (dialog), error, _("Permission denied"));
      g_error_free (error);

      /* the user doesn't have permission to write to the file */
      result = GTK_FILE_CHOOSER_CONFIRMATION_SELECT_AGAIN;
    }

  /* cleanup */
  g_free (filename);

  return result;
}



gchar *
mousepad_dialogs_save_as (GtkWindow   *parent,
                          const gchar *filename)
{
  gchar     *new_filename = NULL;
  GtkWidget *dialog;

  dialog = gtk_file_chooser_dialog_new (_("Save As"),
                                        parent,
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_SAVE, GTK_RESPONSE_OK,
                                        NULL);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), TRUE);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  /* we check if the user is allowed to write to the file */
  g_signal_connect (G_OBJECT (dialog), "confirm-overwrite",
                    G_CALLBACK (mousepad_dialogs_save_as_callback), NULL);

  /* set the current filename */
  if (filename != NULL)
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), filename);

  /* run the dialog */
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    new_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  return new_filename;
}



gint
mousepad_dialogs_ask_overwrite (GtkWindow   *parent,
                                const gchar *filename)
{
  GtkWidget *dialog;
  gint       response;

  dialog = gtk_message_dialog_new (parent,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("The file has been externally modified. Are you sure "
                                     "you want to save the file?"));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("If you save the file, the external changes "
                                              "to \"%s\" will be lost."), filename);

  gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, MOUSEPAD_RESPONSE_CANCEL);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
                                mousepad_dialogs_image_button (GTK_STOCK_SAVE, _("_Overwrite")),
                                MOUSEPAD_RESPONSE_OVERWRITE);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
                                mousepad_dialogs_image_button (GTK_STOCK_REFRESH, _("_Reload")),
                                MOUSEPAD_RESPONSE_RELOAD);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_CANCEL);

  /* run the dialog */
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  return response;
}



gint
mousepad_dialogs_ask_reload (GtkWindow *parent)
{
  GtkWidget *dialog;
  gint       response;

  dialog = gtk_message_dialog_new (parent,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("Do you want to save your changes before reloading?"));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("If you reload the file, you changes will be lost."));

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          GTK_STOCK_CANCEL, MOUSEPAD_RESPONSE_CANCEL,
                          GTK_STOCK_SAVE_AS, MOUSEPAD_RESPONSE_SAVE_AS,
                          NULL);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
                                mousepad_dialogs_image_button (GTK_STOCK_REFRESH, _("_Reload")),
                                MOUSEPAD_RESPONSE_RELOAD);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_CANCEL);

  /* run the dialog */
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  return response;
}
