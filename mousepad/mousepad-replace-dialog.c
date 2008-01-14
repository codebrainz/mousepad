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
#include <mousepad/mousepad-replace-dialog.h>
#include <mousepad/mousepad-dialogs.h>
#include <mousepad/mousepad-preferences.h>
#include <mousepad/mousepad-util.h>
#include <mousepad/mousepad-marshal.h>



static void                 mousepad_replace_dialog_class_init              (MousepadReplaceDialogClass *klass);
static void                 mousepad_replace_dialog_init                    (MousepadReplaceDialog      *dialog);
static void                 mousepad_replace_dialog_unrealize               (GtkWidget                  *widget);
static void                 mousepad_replace_dialog_finalize                (GObject                    *object);
static void                 mousepad_replace_dialog_response                (GtkWidget                  *widget,
                                                                             gint                        response_id);
static void                 mousepad_replace_dialog_changed                 (MousepadReplaceDialog      *dialog);
static void                 mousepad_replace_dialog_case_sensitive_toggled  (GtkToggleButton            *button,
                                                                             MousepadReplaceDialog      *dialog);
static void                 mousepad_replace_dialog_whole_word_toggled      (GtkToggleButton            *button,
                                                                             MousepadReplaceDialog      *dialog);
static void                 mousepad_replace_dialog_replace_all_toggled     (GtkToggleButton            *button,
                                                                             MousepadReplaceDialog      *dialog);
static void                 mousepad_replace_dialog_search_location_changed (GtkComboBox                *combo,
                                                                             MousepadReplaceDialog      *dialog);
static void                 mousepad_replace_dialog_direction_changed       (GtkComboBox                *combo,
                                                                             MousepadReplaceDialog      *dialog);
static void                 mousepad_replace_dialog_history_combo_box       (GtkComboBox                *combo_box);
static void                 mousepad_replace_dialog_history_insert_text     (const gchar                *text);



struct _MousepadReplaceDialogClass
{
  GtkDialogClass __parent__;
};

struct _MousepadReplaceDialog
{
  GtkDialog __parent__;

  /* mousepad preferences */
  MousepadPreferences *preferences;

  /* dialog widgets */
  GtkWidget           *search_entry;
  GtkWidget           *replace_entry;
  GtkWidget           *find_button;
  GtkWidget           *replace_button;
  GtkWidget           *search_location_combo;
  GtkWidget           *hits_label;

  /* search flags */
  guint                search_direction;
  guint                match_case : 1;
  guint                match_whole_word : 1;
  guint                replace_all : 1;
  guint                replace_all_location;
};

enum
{
  IN_SELECTION = 0,
  IN_DOCUMENT,
  IN_ALL_DOCUMENTS
};

enum
{
  DIRECTION_UP = 0,
  DIRECTION_DOWN,
  DIRECTION_BOTH
};

enum
{
  SEARCH,
  LAST_SIGNAL
};



static GSList *history_list = NULL;
static guint   dialog_signals[LAST_SIGNAL];



G_DEFINE_TYPE (MousepadReplaceDialog, mousepad_replace_dialog, GTK_TYPE_DIALOG);


static void
mousepad_replace_dialog_class_init (MousepadReplaceDialogClass *klass)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *gtkwidget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_replace_dialog_finalize;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->unrealize = mousepad_replace_dialog_unrealize;

  dialog_signals[SEARCH] =
    g_signal_new (I_("search"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  _mousepad_marshal_INT__FLAGS_STRING_STRING,
                  G_TYPE_INT, 3,
                  MOUSEPAD_TYPE_SEARCH_FLAGS,
                  G_TYPE_STRING, G_TYPE_STRING);
}



static void
mousepad_replace_dialog_init (MousepadReplaceDialog *dialog)
{
  GtkWidget    *vbox, *hbox, *combo, *label, *check;
  GtkSizeGroup *size_group;
  gboolean      match_whole_word, match_case;
  gint          search_direction, replace_all_location;

  /* initialize some variables */
  dialog->replace_all = FALSE;

  /* get the mousepad preferences */
  dialog->preferences = mousepad_preferences_get ();

  /* read the preferences */
  g_object_get (G_OBJECT (dialog->preferences),
                "search-match-whole-word", &match_whole_word,
                "search-match-case", &match_case,
                "search-direction", &search_direction,
                "search-replace-all-location", &replace_all_location,
                NULL);

  /* set some flags */
  dialog->match_whole_word = match_whole_word;
  dialog->match_case = match_case;
  dialog->search_direction = search_direction;
  dialog->replace_all_location = replace_all_location;

  /* set dialog properties */
  gtk_window_set_title (GTK_WINDOW (dialog), _("Replace"));
  gtk_window_set_default_size (GTK_WINDOW (dialog), 400, -1);
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
  g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (mousepad_replace_dialog_response), NULL);

  /* dialog buttons */
  dialog->find_button = gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_FIND, MOUSEPAD_RESPONSE_FIND);
  dialog->replace_button = mousepad_util_image_button (GTK_STOCK_FIND_AND_REPLACE, _("_Replace"));
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), dialog->replace_button, MOUSEPAD_RESPONSE_REPLACE);
  gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CLOSE, MOUSEPAD_RESPONSE_CLOSE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_FIND);

  /* create main vertical box */
  vbox = g_object_new (GTK_TYPE_VBOX, "border-width", 6, "spacing", 4, NULL);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  /* horizontal box for search string */
  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  /* create a size group */
  size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  label = gtk_label_new_with_mnemonic (_("_Search for:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_size_group_add_widget (size_group, label);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_widget_show (label);

  combo = gtk_combo_box_entry_new_text ();
  mousepad_replace_dialog_history_combo_box (GTK_COMBO_BOX (combo));
  gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL(label), combo);
  gtk_widget_show (combo);

  /* store as an entry widget */
  dialog->search_entry = gtk_bin_get_child (GTK_BIN (combo));
  g_signal_connect_swapped (G_OBJECT (dialog->search_entry), "changed", G_CALLBACK (mousepad_replace_dialog_changed), dialog);

  /* horizontal box for replace string */
  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new_with_mnemonic (_("Replace _with:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_size_group_add_widget (size_group, label);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_widget_show (label);

  combo = gtk_combo_box_entry_new_text ();
  mousepad_replace_dialog_history_combo_box (GTK_COMBO_BOX (combo));
  gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, TRUE, 0);
  gtk_widget_show (combo);

  /* store as an entry widget */
  dialog->replace_entry = gtk_bin_get_child (GTK_BIN (combo));

  /* search direction */
  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL(label), combo);
  gtk_widget_show (hbox);

  label = gtk_label_new_with_mnemonic (_("Search _direction:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_size_group_add_widget (size_group, label);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_widget_show (label);

  combo = gtk_combo_box_new_text ();
  gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL(label), combo);
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Up"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Down"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Both"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), search_direction);
  g_signal_connect (G_OBJECT (combo), "changed", G_CALLBACK (mousepad_replace_dialog_direction_changed), dialog);
  gtk_widget_show (combo);

  /* release size group */
  g_object_unref (G_OBJECT (size_group));

  /* case sensitive */
  check = gtk_check_button_new_with_mnemonic (_("Case sensi_tive"));
  gtk_box_pack_start (GTK_BOX (vbox), check, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), match_case);
  g_signal_connect (G_OBJECT (check), "toggled", G_CALLBACK (mousepad_replace_dialog_case_sensitive_toggled), dialog);
  gtk_widget_show (check);

  /* match whole word */
  check = gtk_check_button_new_with_mnemonic (_("_Match whole word"));
  gtk_box_pack_start (GTK_BOX (vbox), check, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), match_whole_word);
  g_signal_connect (G_OBJECT (check), "toggled", G_CALLBACK (mousepad_replace_dialog_whole_word_toggled), dialog);
  gtk_widget_show (check);

  /* horizontal box for the replace all options */
  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  check = gtk_check_button_new_with_mnemonic (_("Replace _all in:"));
  gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (check), "toggled", G_CALLBACK (mousepad_replace_dialog_replace_all_toggled), dialog);
  gtk_widget_show (check);

  combo = dialog->search_location_combo = gtk_combo_box_new_text ();
  gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Selection"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Document"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("All Documents"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), replace_all_location);
  gtk_widget_set_sensitive (combo, FALSE);
  g_signal_connect (G_OBJECT (combo), "changed", G_CALLBACK (mousepad_replace_dialog_search_location_changed), dialog);
  gtk_widget_show (combo);

  label = dialog->hits_label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);
}



static void
mousepad_replace_dialog_unrealize (GtkWidget *widget)
{
  MousepadReplaceDialog *dialog = MOUSEPAD_REPLACE_DIALOG (widget);
  const gchar           *text;

  _mousepad_return_if_fail (GTK_IS_ENTRY (dialog->replace_entry));
  _mousepad_return_if_fail (GTK_IS_ENTRY (dialog->search_entry));

  text = gtk_entry_get_text (GTK_ENTRY (dialog->search_entry));
  mousepad_replace_dialog_history_insert_text (text);

  text = gtk_entry_get_text (GTK_ENTRY (dialog->replace_entry));
  mousepad_replace_dialog_history_insert_text (text);

  (*GTK_WIDGET_CLASS (mousepad_replace_dialog_parent_class)->unrealize) (widget);
}



static void
mousepad_replace_dialog_finalize (GObject *object)
{
  MousepadReplaceDialog *dialog = MOUSEPAD_REPLACE_DIALOG (object);

  /* release the preferences */
  g_object_unref (G_OBJECT (dialog->preferences));

  (*G_OBJECT_CLASS (mousepad_replace_dialog_parent_class)->finalize) (object);
}



static void
mousepad_replace_dialog_response (GtkWidget *widget,
                                  gint       response_id)
{
  MousepadSearchFlags    flags;
  MousepadReplaceDialog *dialog = MOUSEPAD_REPLACE_DIALOG (widget);
  gint                   matches;
  const gchar           *search_str, *replace_str;
  gchar                 *message;

  /* close dialog */
  if (response_id == MOUSEPAD_RESPONSE_CLOSE)
    goto destroy_dialog;

  /* search direction */
  if (dialog->search_direction == DIRECTION_UP
      && dialog->replace_all == FALSE)
    flags = MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD;
  else
    flags = MOUSEPAD_SEARCH_FLAGS_DIR_FORWARD;

  /* case sensitive searching */
  if (dialog->match_case)
    flags |= MOUSEPAD_SEARCH_FLAGS_MATCH_CASE;

  /* only match whole words */
  if (dialog->match_whole_word)
    flags |= MOUSEPAD_SEARCH_FLAGS_WHOLE_WORD;

  /* wrap around */
  if (dialog->search_direction == DIRECTION_BOTH
      && dialog->replace_all == FALSE)
    flags |= MOUSEPAD_SEARCH_FLAGS_WRAP_AROUND;

  /* search area */
  if (dialog->replace_all
      && dialog->replace_all_location == IN_SELECTION)
    flags |= MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION;
  else
    flags |= MOUSEPAD_SEARCH_FLAGS_AREA_DOCUMENT;

  /* start position */
  if (response_id == MOUSEPAD_RESPONSE_CHECK_ENTRY)
    {
      /* no visible actions */
      flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_NONE;

      if (dialog->replace_all)
        goto replace_flags;
      else
        goto search_flags;
    }
  else if (response_id == MOUSEPAD_RESPONSE_FIND)
    {
      /* select the first match */
      flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT;

      search_flags:

      /* start at the 'end' of the selection */
      if (flags & MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD)
        flags |= MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START;
      else
        flags |= MOUSEPAD_SEARCH_FLAGS_ITER_SEL_END;
    }
  else if (response_id == MOUSEPAD_RESPONSE_REPLACE)
    {
      /* replace matches */
      flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_REPLACE;

      if (dialog->replace_all)
        {
          replace_flags:

          /* replace all from the beginning of the document */
          flags |= MOUSEPAD_SEARCH_FLAGS_ITER_AREA_START
                   | MOUSEPAD_SEARCH_FLAGS_ENTIRE_AREA;

          /* search all opened documents (flag used in mousepad-window.c) */
          if (dialog->replace_all_location == IN_ALL_DOCUMENTS)
            flags |= MOUSEPAD_SEARCH_FLAGS_ALL_DOCUMENTS;
        }
      else
        {
          /* start at the 'beginning' of the selection */
          if (flags & MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD)
            flags |= MOUSEPAD_SEARCH_FLAGS_ITER_SEL_END;
          else
            flags |= MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START;
        }
    }
  else
    {
      destroy_dialog:

      /* destroy the window */
      gtk_widget_destroy (widget);

      /* leave */
      return;
    }

  /* get strings */
  search_str = gtk_entry_get_text (GTK_ENTRY (dialog->search_entry));
  replace_str = gtk_entry_get_text (GTK_ENTRY (dialog->replace_entry));

  /* emit the signal */
  g_signal_emit (G_OBJECT (dialog), dialog_signals[SEARCH], 0, flags, search_str, replace_str, &matches);

  /* reset counter */
  if (response_id == MOUSEPAD_RESPONSE_REPLACE && dialog->replace_all)
    matches = 0;

  /* update entry color */
  mousepad_util_entry_error (dialog->search_entry, matches == 0);

  /* update counter */
  if (dialog->replace_all)
    {
      message = g_strdup_printf (ngettext ("%d occurence", "%d occurences", matches), matches);
      gtk_label_set_markup (GTK_LABEL (dialog->hits_label), message);
      g_free (message);
    }
}


static void
mousepad_replace_dialog_changed (MousepadReplaceDialog *dialog)
{
  const gchar *text;
  gboolean     sensitive;

  /* get the search entry text */
  text = gtk_entry_get_text (GTK_ENTRY (dialog->search_entry));

  if (text != NULL && *text != '\0')
    {
      /* do an invisible search to give the user some visible feedback */
      gtk_dialog_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_CHECK_ENTRY);

      /* buttons are sensitive */
      sensitive = TRUE;
    }
  else
    {
      /* not text, means no error */
      mousepad_util_entry_error (dialog->search_entry, FALSE);

      /* reset occurences label */
      gtk_label_set_text (GTK_LABEL (dialog->hits_label), NULL);

      /* buttons are not sensitive */
      sensitive = FALSE;
    }

  /* set the sensitivity */
  gtk_widget_set_sensitive (dialog->find_button, sensitive);
  gtk_widget_set_sensitive (dialog->replace_button, sensitive);
}



static void
mousepad_replace_dialog_case_sensitive_toggled (GtkToggleButton       *button,
                                                MousepadReplaceDialog *dialog)
{
  /* get the toggle button state */
  dialog->match_case = gtk_toggle_button_get_active (button);

  /* save the setting */
  g_object_set (G_OBJECT (dialog->preferences), "search-match-case", dialog->match_case, NULL);

  /* update dialog */
  mousepad_replace_dialog_changed (dialog);
}



static void
mousepad_replace_dialog_whole_word_toggled (GtkToggleButton       *button,
                                            MousepadReplaceDialog *dialog)
{
  /* get the toggle button state */
  dialog->match_whole_word = gtk_toggle_button_get_active (button);

  /* save the setting */
  g_object_set (G_OBJECT (dialog->preferences), "search-match-whole-word", dialog->match_whole_word, NULL);

  /* update dialog */
  mousepad_replace_dialog_changed (dialog);
}



static void
mousepad_replace_dialog_replace_all_toggled (GtkToggleButton       *button,
                                             MousepadReplaceDialog *dialog)
{
  gboolean active;

  /* get the toggle button state */
  active = gtk_toggle_button_get_active (button);

  /* save flags */
  dialog->replace_all = active;

  /* set the sensitivity of some dialog widgets */
  gtk_widget_set_sensitive (dialog->search_location_combo, active);

  /* reset occurences label */
  gtk_label_set_text (GTK_LABEL (dialog->hits_label), NULL);

  /* set new label of the replace button */
  gtk_button_set_label (GTK_BUTTON (dialog->replace_button),
                        active ? _("_Replace All") : _("_Replace"));

  /* update dialog */
  mousepad_replace_dialog_changed (dialog);
}



static void
mousepad_replace_dialog_search_location_changed (GtkComboBox           *combo,
                                                 MousepadReplaceDialog *dialog)
{
  /* get the selected action */
  dialog->replace_all_location = gtk_combo_box_get_active (combo);

  /* save setting */
  g_object_set (G_OBJECT (dialog->preferences), "search-replace-all-location", dialog->replace_all_location, NULL);

  /* update dialog */
  mousepad_replace_dialog_changed (dialog);
}



static void
mousepad_replace_dialog_direction_changed (GtkComboBox           *combo,
                                           MousepadReplaceDialog *dialog)
{
  /* get the selected item */
  dialog->search_direction = gtk_combo_box_get_active (combo);

  /* save the last direction */
  g_object_set (G_OBJECT (dialog->preferences), "search-direction", dialog->search_direction, NULL);

  /* update dialog */
  mousepad_replace_dialog_changed (dialog);
}



/**
 * History functions
 **/
static void
mousepad_replace_dialog_history_combo_box (GtkComboBox *combo_box)
{
  GSList *li;

  _mousepad_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  /* append the items from the history to the combobox */
  for (li = history_list; li != NULL; li = li->next)
    gtk_combo_box_append_text (combo_box, li->data);
}



static void
mousepad_replace_dialog_history_insert_text (const gchar *text)
{
  GSList *li;

  /* quit if the box is empty */
  if (text == NULL || *text == '\0')
    return;

  /* check if the string is already in the history */
  for (li = history_list; li != NULL; li = li->next)
    if (strcmp (li->data, text) == 0)
      return;

  /* prepend the string */
  history_list = g_slist_prepend (history_list, g_strdup (text));
}



GtkWidget *
mousepad_replace_dialog_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_REPLACE_DIALOG, NULL);
}



void
mousepad_replace_dialog_history_clean (void)
{
  GSList *li;

  if (history_list)
    {
      /* remove all the entries */
      for (li = history_list; li != NULL; li = li->next)
        {
          /* cleanup the string */
          g_free (li->data);

          /* remove the item from the list */
          history_list = g_slist_delete_link (history_list, li);
        }

      /* cleanup the list */
      g_slist_free (history_list);
    }
}



void
mousepad_replace_dialog_page_switched (MousepadReplaceDialog *dialog)
{
  mousepad_replace_dialog_changed (dialog);
}
