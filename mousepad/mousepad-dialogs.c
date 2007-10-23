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
#include <mousepad/mousepad-dialogs.h>
#include <mousepad/mousepad-util.h>



void
mousepad_dialogs_show_about (GtkWindow *parent)
{
  static const gchar *authors[] =
  {
    "Nick Schermer <nick@xfce.org>",
    "Erik Harrison <erikharrison@xfce.org>",
    NULL,
  };

  /* show the dialog */
  gtk_show_about_dialog (parent,
                         "authors", authors,
                         "comments", _("Mousepad is a fast text editor for the Xfce Desktop Environment."),
                         "destroy-with-parent", TRUE,
                         "license", XFCE_LICENSE_GPL,
                         "logo-icon-name", PACKAGE_NAME,
                         "name", PACKAGE_NAME,
                         "version", PACKAGE_VERSION,
                         "translator-credits", _("translator-credits"),
                         "website", "http://www.xfce.org/",
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
mousepad_dialogs_other_tab_size (GtkWindow *parent,
                                 gint      active_size)
{
  GtkWidget *dialog;
  GtkWidget *scale;

  /* build dialog */
  dialog = gtk_dialog_new_with_buttons (_("Select Tab Size"),
                                        parent,
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
                                        GTK_STOCK_CANCEL, MOUSEPAD_RESPONSE_CANCEL,
                                        GTK_STOCK_OK, MOUSEPAD_RESPONSE_OK,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_OK);

  /* create scale widget */
  scale = gtk_hscale_new_with_range (1, 32, 1);
  gtk_range_set_value (GTK_RANGE (scale), active_size);
  gtk_scale_set_digits (GTK_SCALE (scale), 0);
  gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
  gtk_scale_set_value_pos (GTK_SCALE (scale), GTK_POS_TOP);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), scale, TRUE, TRUE, 0);
  gtk_widget_show (scale);

  /* run the dialog */
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == MOUSEPAD_RESPONSE_OK)
    active_size = gtk_range_get_value (GTK_RANGE (scale));

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  return active_size;
}



gint
mousepad_dialogs_go_to_line (GtkWindow *parent,
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
  dialog = gtk_dialog_new_with_buttons (_("Go To Line"),
                                        parent,
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
                                        GTK_STOCK_CANCEL, MOUSEPAD_RESPONSE_CANCEL,
                                        GTK_STOCK_JUMP_TO, MOUSEPAD_RESPONSE_JUMP_TO,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_JUMP_TO);
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
  GtkWidget *image;
  gboolean   succeed = FALSE;

  /* create the question dialog */
  dialog = gtk_message_dialog_new (parent, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_OTHER, GTK_BUTTONS_NONE,
                                   _("Remove all entries from the documents history?"));
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          GTK_STOCK_CANCEL, MOUSEPAD_RESPONSE_CANCEL,
                          GTK_STOCK_CLEAR, MOUSEPAD_RESPONSE_CLEAR,
                          NULL);
  gtk_window_set_title (GTK_WINDOW (dialog), _("Clear Documents History"));
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_CANCEL);
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("Clearing the documents history will permanently "
                                              "remove all currently listed entries."));

  /* the dialog icon */
  image = gtk_image_new_from_stock (GTK_STOCK_CLEAR, GTK_ICON_SIZE_DIALOG);
  gtk_message_dialog_set_image (GTK_MESSAGE_DIALOG (dialog), image);
  gtk_widget_show (image);

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
  GtkWidget *image;
  gint       response;

  /* the dialog icon */
  image = gtk_image_new_from_stock (GTK_STOCK_SAVE, GTK_ICON_SIZE_DIALOG);
  gtk_widget_show (image);

  /* create the question dialog */
  dialog = gtk_message_dialog_new (parent, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_OTHER, GTK_BUTTONS_NONE,
                                   _("Do you want to save the changes before closing?"));

  gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
                                mousepad_util_image_button (GTK_STOCK_DELETE, _("_Don't Save")),
                                MOUSEPAD_RESPONSE_DONT_SAVE);

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          GTK_STOCK_CANCEL, MOUSEPAD_RESPONSE_CANCEL,
                          GTK_STOCK_SAVE, MOUSEPAD_RESPONSE_SAVE,
                          NULL);

  gtk_window_set_title (GTK_WINDOW (dialog), _("Save Changes"));
  gtk_message_dialog_set_image (GTK_MESSAGE_DIALOG (dialog), image);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_SAVE);

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("If you don't save the document, all the changes will be lost."));

  /* run the dialog and wait for a response */
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  return response;
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
                                mousepad_util_image_button (GTK_STOCK_SAVE, _("_Overwrite")),
                                MOUSEPAD_RESPONSE_OVERWRITE);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
                                mousepad_util_image_button (GTK_STOCK_REFRESH, _("_Reload")),
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
                                mousepad_util_image_button (GTK_STOCK_REFRESH, _("_Reload")),
                                MOUSEPAD_RESPONSE_RELOAD);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_CANCEL);

  /* run the dialog */
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  return response;
}
