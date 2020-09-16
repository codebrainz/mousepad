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
#include <mousepad/mousepad-util.h>
#include <gtksourceview/gtksource.h>



static gboolean
mousepad_util_iter_word_characters (const GtkTextIter *iter)
{
  gunichar c;

  /* get the characters */
  c = gtk_text_iter_get_char (iter);

  /* character we'd like to see in a word */
  if (g_unichar_isalnum (c) || c == '_')
    return TRUE;

  return FALSE;
}



gboolean
mousepad_util_iter_starts_word (const GtkTextIter *iter)
{
  GtkTextIter prev;

  /* normal gtk word start */
  if (!gtk_text_iter_starts_word (iter))
    return FALSE;

  /* init iter for previous char */
  prev = *iter;

  /* return true when we could not step backwards (start of buffer) */
  if (!gtk_text_iter_backward_char (&prev))
    return TRUE;

  /* check if the previous char also belongs to the word */
  if (mousepad_util_iter_word_characters (&prev))
    return FALSE;

  return TRUE;
}



gboolean
mousepad_util_iter_ends_word (const GtkTextIter *iter)
{
  if (!gtk_text_iter_ends_word (iter))
    return FALSE;

  /* check if it's a character we'd like to see in a word */
  if (mousepad_util_iter_word_characters (iter))
    return FALSE;

  return TRUE;
}



gboolean
mousepad_util_iter_inside_word (const GtkTextIter *iter)
{
  GtkTextIter prev;

  /* not inside a word when at beginning or end of a word */
  if (mousepad_util_iter_starts_word (iter) || mousepad_util_iter_ends_word (iter))
    return FALSE;

  /* normal gtk function */
  if (gtk_text_iter_inside_word (iter))
    return TRUE;

  /* check if the character is a word char */
  if (!mousepad_util_iter_word_characters (iter))
    return FALSE;

  /* initialize previous iter */
  prev = *iter;

  /* get one character backwards */
  if (!gtk_text_iter_backward_char (&prev))
    return FALSE;

  return mousepad_util_iter_word_characters (&prev);
}



gboolean
mousepad_util_iter_forward_word_end (GtkTextIter *iter)
{
  if (mousepad_util_iter_ends_word (iter))
    return TRUE;

  while (gtk_text_iter_forward_char (iter))
    if (mousepad_util_iter_ends_word (iter))
      return TRUE;

  return mousepad_util_iter_ends_word (iter);
}



gboolean
mousepad_util_iter_backward_word_start (GtkTextIter *iter)
{
  /* return true if the iter already starts a word */
  if (mousepad_util_iter_starts_word (iter))
    return TRUE;

  /* move backwards until we find a word start */
  while (gtk_text_iter_backward_char (iter))
    if (mousepad_util_iter_starts_word (iter))
      return TRUE;

  /* while stops when we hit the first char in the buffer */
  return mousepad_util_iter_starts_word (iter);
}



gboolean
mousepad_util_iter_forward_text_start (GtkTextIter *iter)
{
  g_return_val_if_fail (!mousepad_util_iter_inside_word (iter), FALSE);

  /* keep until we hit text or a line end */
  while (g_unichar_isspace (gtk_text_iter_get_char (iter)))
    if (gtk_text_iter_ends_line (iter) || !gtk_text_iter_forward_char (iter))
      break;

  return TRUE;
}



gboolean
mousepad_util_iter_backward_text_start (GtkTextIter *iter)
{
  GtkTextIter prev = *iter;

  g_return_val_if_fail (!mousepad_util_iter_inside_word (iter), FALSE);

  while (!gtk_text_iter_starts_line (&prev) && gtk_text_iter_backward_char (&prev))
    {
      if (g_unichar_isspace (gtk_text_iter_get_char (&prev)))
        *iter = prev;
      else
        break;
    }

  return TRUE;
}



gchar *
mousepad_util_config_name (const gchar *name)
{
  const gchar *s;
  gchar       *config, *t;
  gboolean     upper = TRUE;

  /* allocate string */
  config = g_new (gchar, strlen (name) + 1);

  /* convert name */
  for (s = name, t = config; *s != '\0'; ++s)
    {
      if (*s == '-')
        {
          upper = TRUE;
        }
      else if (upper)
        {
          *t++ = g_ascii_toupper (*s);
          upper = FALSE;
        }
      else
        {
          *t++ = g_ascii_tolower (*s);
        }
    }

  /* zerro terminate string */
  *t = '\0';

  return config;
}



gchar *
mousepad_util_key_name (const gchar *name)
{
  const gchar *s;
  gchar       *key, *t;

  /* allocate string (max of 9 extra - chars) */
  key = g_new (gchar, strlen (name) + 10);

  /* convert name */
  for (s = name, t = key; *s != '\0'; ++s)
    {
      if (g_ascii_isupper (*s) && s != name)
        *t++ = '-';

      *t++ = g_ascii_tolower (*s);
    }

  /* zerro terminate string */
  *t = '\0';

  return key;
}



gchar *
mousepad_util_utf8_strcapital (const gchar *str)
{
  gunichar     c;
  const gchar *p;
  gchar       *buf;
  GString     *result;
  gboolean     upper = TRUE;

  g_return_val_if_fail (g_utf8_validate (str, -1, NULL), NULL);

  /* create a new string */
  result = g_string_sized_new (strlen (str));

  /* walk though the string */
  for (p = str; *p != '\0'; p = g_utf8_next_char (p))
    {
      /* get the unicode char */
      c = g_utf8_get_char (p);

      /* only change the case of alpha chars */
      if (g_unichar_isalpha (c))
        {
          /* check case */
          if (upper ? g_unichar_isupper (c) : g_unichar_islower (c))
            {
              /* currect case is already correct */
              g_string_append_unichar (result, c);
            }
          else
            {
              /* convert the case of the char and append it */
              buf = upper ? g_utf8_strup (p, 1) : g_utf8_strdown (p, 1);
              g_string_append (result, buf);
              g_free (buf);
            }

          /* next char must be lowercase */
          upper = FALSE;
        }
      else
        {
          /* append the char */
          g_string_append_unichar (result, c);

          /* next alpha char uppercase after a space */
          upper = g_unichar_isspace (c);
        }
    }

  /* return the result */
  return g_string_free (result, FALSE);
}



gchar *
mousepad_util_utf8_stropposite (const gchar *str)
{
  gunichar     c;
  const gchar *p;
  gchar       *buf;
  GString     *result;

  g_return_val_if_fail (g_utf8_validate (str, -1, NULL), NULL);

  /* create a new string */
  result = g_string_sized_new (strlen (str));

  /* walk though the string */
  for (p = str; *p != '\0'; p = g_utf8_next_char (p))
    {
      /* get the unicode char */
      c = g_utf8_get_char (p);

      /* only change the case of alpha chars */
      if (g_unichar_isalpha (c))
        {
          /* get the opposite case of the char */
          if (g_unichar_isupper (c))
            buf = g_utf8_strdown (p, 1);
          else
            buf = g_utf8_strup (p, 1);

          /* append to the buffer */
          g_string_append (result, buf);
          g_free (buf);
        }
      else
        {
          /* append the char */
          g_string_append_unichar (result, c);
        }
    }

  /* return the result */
  return g_string_free (result, FALSE);
}



gchar *
mousepad_util_escape_underscores (const gchar *str)
{
  GString     *result;
  const gchar *s;

  /* allocate a new string */
  result = g_string_sized_new (strlen (str));

  /* escape all underscores */
  for (s = str; *s != '\0'; ++s)
    {
      if (G_UNLIKELY (*s == '_'))
        g_string_append (result, "__");
      else
        g_string_append_c (result, *s);
    }

  return g_string_free (result, FALSE);
}



GtkWidget *
mousepad_util_image_button (const gchar *icon_name,
                            const gchar *label)
{
  GtkWidget *button, *image;

  image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (image);

  button = gtk_button_new_with_mnemonic (label);
  gtk_button_set_image (GTK_BUTTON (button), image);
  gtk_widget_show (button);

  return button;
}



void
mousepad_util_entry_error (GtkWidget *widget,
                           gboolean   error)
{
  gpointer pointer;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  /* get the current error state */
  pointer = mousepad_object_get_data (G_OBJECT (widget), "error-state");

  /* only change the state when really needed to avoid multiple widget calls */
  if (GPOINTER_TO_INT (pointer) != error)
    {
      /* set the widget style */
      /* see http://gtk.10911.n7.nabble.com/set-custom-entry-background-td88472.html#a88489 */
      if (error)
        gtk_style_context_add_class (gtk_widget_get_style_context (widget), GTK_STYLE_CLASS_ERROR);
      else
        gtk_style_context_remove_class (gtk_widget_get_style_context (widget), GTK_STYLE_CLASS_ERROR);

      /* set the new state */
      mousepad_object_set_data (G_OBJECT (widget), "error-state", GINT_TO_POINTER (error));
    }
}



void
mousepad_util_dialog_header (GtkDialog   *dialog,
                             const gchar *title,
                             const gchar *subtitle,
                             const gchar *iconname)
{
  gchar     *full_title;
  GtkWidget *vbox, *hbox;
  GtkWidget *icon, *label, *line;
  GtkWidget *dialog_vbox;

  /* remove the main vbox */
  dialog_vbox = gtk_bin_get_child (GTK_BIN (dialog));
  g_object_ref (G_OBJECT (dialog_vbox));
  gtk_container_remove (GTK_CONTAINER (dialog), dialog_vbox);

  /* add a new vbox to the main window */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (dialog), vbox);
  gtk_widget_show (vbox);

  /* create a hbox */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);
  gtk_widget_show (hbox);

  /* title icon */
  icon = gtk_image_new_from_icon_name (iconname, GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);
  gtk_widget_show (icon);

  /* create the title */
  full_title = g_strdup_printf ("<b><big>%s</big></b>\n%s", title, subtitle);

  /* title label */
  label = gtk_label_new (full_title);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.5);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  /* cleanup */
  g_free (full_title);

  /* add the separator between header and content */
  line = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start (GTK_BOX (vbox), line, FALSE, FALSE, 0);
  gtk_widget_show (line);

  /* add the main dialog box to the new vbox */
  gtk_box_pack_start (GTK_BOX (vbox), dialog_vbox, TRUE, TRUE, 0);
  g_object_unref (G_OBJECT (dialog_vbox));
}


gint
mousepad_util_get_real_line_offset (const GtkTextIter *iter,
                                    gint               tab_size)
{
  gint        offset = 0;
  GtkTextIter needle = *iter;

  /* move the needle to the start of the line */
  gtk_text_iter_set_line_offset (&needle, 0);

  /* forward the needle until we hit the iter */
  while (!gtk_text_iter_equal (&needle, iter))
    {
      /* append the real tab offset or 1 */
      if (gtk_text_iter_get_char (&needle) == '\t')
        offset += (tab_size - (offset % tab_size));
      else
        offset++;

      /* next char */
      gtk_text_iter_forward_char (&needle);
    }

  return offset;
}



gboolean
mousepad_util_forward_iter_to_text (GtkTextIter       *iter,
                                    const GtkTextIter *limit)
{
  gunichar c;

  do
    {
      /* get the iter character */
      c = gtk_text_iter_get_char (iter);

      /* break if the character is not a space */
      if (!g_unichar_isspace (c) || c == '\n' || c == '\r')
        break;

      /* break when we reached the limit iter */
      if (limit && gtk_text_iter_equal (iter, limit))
        return FALSE;
    }
  while (gtk_text_iter_forward_char (iter));

  return TRUE;
}



gchar *
mousepad_util_get_save_location (const gchar *relpath,
                                 gboolean     create_parents)
{
  gchar *filename, *dirname;

  g_return_val_if_fail (g_get_user_config_dir () != NULL, NULL);

  /* create the full filename */
  filename = g_build_filename (g_get_user_config_dir (), relpath, NULL);

  /* test if the file exists */
  if (G_UNLIKELY (g_file_test (filename, G_FILE_TEST_EXISTS) == FALSE))
    {
      if (create_parents)
        {
          /* get the directory name */
          dirname = g_path_get_dirname (filename);

          /* make the directory with parents */
          if (g_mkdir_with_parents (dirname, 0700) == -1)
            {
              /* show warning to the user */
              g_critical (_("Unable to create base directory \"%s\". "
                            "Saving to file \"%s\" will be aborted."), dirname, filename);

              /* don't return a filename, to avoid problems */
              g_free (filename);
              filename = NULL;
            }

          /* cleanup */
          g_free (dirname);
        }
      else
        {
          /* cleanup */
          g_free (filename);
          filename = NULL;
        }
    }

  return filename;
}



void
mousepad_util_save_key_file (GKeyFile    *keyfile,
                             const gchar *filename)
{
  gchar  *contents;
  gsize   length;
  GError *error = NULL;

  /* get the contents of the key file */
  contents = g_key_file_to_data (keyfile, &length, &error);

  if (G_LIKELY (error == NULL))
    {
      /* write the contents to the file */
      if (G_UNLIKELY (g_file_set_contents (filename, contents, length, &error) == FALSE))
        goto print_error;
    }
  else
    {
      print_error:

      /* print error */
      g_critical (_("Failed to store the preferences to \"%s\": %s"), filename, error->message);

      /* cleanup */
      g_error_free (error);
    }

  /* cleanup */
  g_free (contents);
}



GType
mousepad_util_search_flags_get_type (void)
{
  static GType type = G_TYPE_NONE;

  if (G_UNLIKELY (type == G_TYPE_NONE))
    {
      /* use empty values table */
      static const GFlagsValue values[] =
      {
          { 0, NULL, NULL }
      };

      /* register the type */
      type = g_flags_register_static (I_("MousepadSearchFlags"), values);
      }

  return type;
}



gint
mousepad_util_search (GtkSourceSearchContext *search_context,
                      const gchar            *string,
                      const gchar            *replace,
                      MousepadSearchFlags     flags)
{
  GtkSourceSearchSettings *search_settings;
  GtkTextBuffer           *buffer, *selection_buffer = NULL;
  GtkTextIter              start, end, iter, bstart, fend, biter, fiter;
  gchar                   *selected_text;
  gint                     counter = 0;
  gboolean                 found;

  g_return_val_if_fail (GTK_SOURCE_IS_SEARCH_CONTEXT (search_context), -1);
  g_return_val_if_fail (string != NULL && g_utf8_validate (string, -1, NULL), -1);
  g_return_val_if_fail (replace == NULL || g_utf8_validate (replace, -1, NULL), -1);

  /* freeze buffer notifications */
  buffer = GTK_TEXT_BUFFER (gtk_source_search_context_get_buffer (search_context));
  g_object_freeze_notify (G_OBJECT (buffer));

  /* get the search iters */
  if (flags & MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START)
    gtk_text_buffer_get_selection_bounds (buffer, &iter, NULL);
  else
    gtk_text_buffer_get_selection_bounds (buffer, NULL, &iter);

  /* substitute a virtual selection buffer to the real buffer and work inside */
  if (flags & MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION)
    {
      selection_buffer = GTK_TEXT_BUFFER (gtk_source_buffer_new (NULL));
      gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
      selected_text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
      gtk_text_buffer_set_text (selection_buffer, selected_text, -1);
      search_context = gtk_source_search_context_new (GTK_SOURCE_BUFFER (selection_buffer), NULL);
      gtk_text_buffer_get_start_iter (selection_buffer, &iter);
    }

  /* set the search context settings */
  search_settings = gtk_source_search_context_get_settings (search_context);
  gtk_source_search_settings_set_search_text (search_settings, string);
  gtk_source_search_settings_set_case_sensitive (search_settings,
                                                 flags & MOUSEPAD_SEARCH_FLAGS_MATCH_CASE);
  gtk_source_search_settings_set_at_word_boundaries (search_settings,
                                                     flags & MOUSEPAD_SEARCH_FLAGS_WHOLE_WORD);
  gtk_source_search_settings_set_wrap_around (search_settings,
                                              flags & MOUSEPAD_SEARCH_FLAGS_WRAP_AROUND);
  gtk_source_search_settings_set_regex_enabled (search_settings,
                                                flags & MOUSEPAD_SEARCH_FLAGS_ENABLE_REGEX);

  /* search the string */
  if (flags & MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD)
    found = gtk_source_search_context_backward2 (search_context, &iter, &start, &end, NULL);
  else
    found = gtk_source_search_context_forward2 (search_context, &iter, &start, &end, NULL);

  /* set the counter, ensuring the buffer is fully scanned if needed (searching in both
   * directions leads faster to a full scan) */
  if (! (flags & MOUSEPAD_SEARCH_FLAGS_ENTIRE_AREA) || *string == '\0')
    counter = found;
  else
    {
      counter = gtk_source_search_context_get_occurrences_count (search_context);
      if (counter == -1)
        {
          gtk_source_search_settings_set_wrap_around (search_settings, TRUE);
          if (! found)
            {
              start = iter;
              end = iter;
            }
          biter = start;
          fiter = end;
          do
            {
              gtk_source_search_context_backward2 (search_context, &biter, &bstart, NULL, NULL);
              gtk_source_search_context_forward2 (search_context, &fiter, NULL, &fend, NULL);
              counter = gtk_source_search_context_get_occurrences_count (search_context);
              biter = bstart;
              fiter = fend;
            }
          while (counter == -1 && ! gtk_text_iter_equal (&biter, &start)
                               && ! gtk_text_iter_equal (&fiter, &end));
          gtk_source_search_settings_set_wrap_around (search_settings,
                                                      flags & MOUSEPAD_SEARCH_FLAGS_WRAP_AROUND);
        }
    }

  /* handle the action */
  if (found && (flags & MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT)
      && ! (flags & MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION))
    gtk_text_buffer_select_range (buffer, &start, &end);
  else if (flags & MOUSEPAD_SEARCH_FLAGS_ACTION_REPLACE)
    {
      if (found && ! (flags & MOUSEPAD_SEARCH_FLAGS_ENTIRE_AREA))
        {
          /* replace selected occurrence */
          gtk_source_search_context_replace2 (search_context, &start, &end, replace, -1, NULL);

          /* select next occurrence */
          flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT;
          flags &= ~MOUSEPAD_SEARCH_FLAGS_ACTION_REPLACE;
          counter = mousepad_util_search (search_context, string, NULL, flags);
        }
      else if (counter > 0 && (flags & MOUSEPAD_SEARCH_FLAGS_ENTIRE_AREA))
        {
          gtk_source_search_context_replace_all (search_context, replace, -1, NULL);
          if (flags & MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION)
            {
              /* replace all occurrences in the virtual selection buffer */
              gtk_source_search_context_replace_all (search_context, replace, -1, NULL);
              gtk_text_buffer_get_bounds (selection_buffer, &start, &end);
              selected_text = gtk_text_buffer_get_text (selection_buffer, &start, &end, FALSE);

              /* replace selection in the real buffer by the text in the virtual selection buffer */
              gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
              gtk_text_buffer_begin_user_action (buffer);
              gtk_text_buffer_delete (buffer, &start, &end);
              gtk_text_buffer_insert (buffer, &start, selected_text, -1);
              gtk_text_buffer_end_user_action (buffer);
            }
        }
    }
  else if (! (flags & MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION))
    gtk_text_buffer_place_cursor (buffer, &iter);

  /* thawn buffer notifications */
  g_object_thaw_notify (G_OBJECT (buffer));

  /* cleanup */
  if (flags & MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION)
    {
      g_object_unref (G_OBJECT (selection_buffer));
      g_object_unref (G_OBJECT (search_context));
      g_free (selected_text);
    }

  return counter;
}



GIcon *
mousepad_util_icon_for_mime_type (const gchar *mime_type)
{
  gchar *content_type;
  GIcon *icon = NULL;

  g_return_val_if_fail (mime_type != NULL, NULL);

  content_type = g_content_type_from_mime_type (mime_type);
  if (content_type != NULL)
    icon = g_content_type_get_icon (content_type);

  return icon;
}



static void
mousepad_util_container_foreach_counter (GtkWidget *widget,
                                         guint     *n_children)
{
  *n_children = *n_children + 1;
}



gboolean
mousepad_util_container_has_children (GtkContainer *container)
{
  guint n_children = 0;

  g_return_val_if_fail (GTK_IS_CONTAINER (container), FALSE);

  gtk_container_foreach (container,
                         (GtkCallback) mousepad_util_container_foreach_counter,
                         &n_children);

  return (n_children > 0);
}


void
mousepad_util_container_clear (GtkContainer *container)
{
  GList *list, *iter;

  g_return_if_fail (GTK_IS_CONTAINER (container));

  list = gtk_container_get_children (container);

  for (iter = list; iter != NULL; iter = g_list_next (iter))
    gtk_container_remove (container, iter->data);

  g_list_free (list);
}



void
mousepad_util_container_move_children (GtkContainer *source,
                                       GtkContainer *destination)
{
  GList *list, *iter;

  list = gtk_container_get_children (source);

  for (iter = list; iter != NULL; iter = g_list_next (iter))
    {
      GtkWidget *tmp = g_object_ref (iter->data);

      gtk_container_remove (source, tmp);
      gtk_container_add (destination, tmp);
      g_object_unref (tmp);
    }

  g_list_free (list);
}



static gint
mousepad_util_style_schemes_name_compare (gconstpointer a,
                                                  gconstpointer b)
{
  const gchar *name_a, *name_b;

  if (G_UNLIKELY (!GTK_SOURCE_IS_STYLE_SCHEME (a)))
    return -(a != b);
  if (G_UNLIKELY (!GTK_SOURCE_IS_STYLE_SCHEME (b)))
    return a != b;

  name_a = gtk_source_style_scheme_get_name (GTK_SOURCE_STYLE_SCHEME (a));
  name_b = gtk_source_style_scheme_get_name (GTK_SOURCE_STYLE_SCHEME (b));

  return g_utf8_collate (name_a, name_b);
}



static GSList *
mousepad_util_get_style_schemes (void)
{
  GSList               *list = NULL;
  const gchar * const  *schemes;
  GtkSourceStyleScheme *scheme;

  schemes = gtk_source_style_scheme_manager_get_scheme_ids (
              gtk_source_style_scheme_manager_get_default ());

  while (*schemes)
    {
      scheme = gtk_source_style_scheme_manager_get_scheme (
                gtk_source_style_scheme_manager_get_default (), *schemes);
      list = g_slist_prepend (list, scheme);
      schemes++;
    }

  return list;
}



GSList *
mousepad_util_style_schemes_get_sorted (void)
{
  return g_slist_sort (mousepad_util_get_style_schemes (),
                       mousepad_util_style_schemes_name_compare);
}



GSList *
mousepad_util_get_sorted_section_names (void)
{
  GSList                   *list = NULL;
  const gchar *const       *languages;
  GtkSourceLanguage        *language;
  GtkSourceLanguageManager *manager;

  manager = gtk_source_language_manager_get_default ();
  languages = gtk_source_language_manager_get_language_ids (manager);

  while (*languages)
    {
      language = gtk_source_language_manager_get_language (manager, *languages);
      if (G_LIKELY (GTK_SOURCE_IS_LANGUAGE (language)))
        {
          /* ignore hidden languages */
          if(gtk_source_language_get_hidden(language))
            {
              languages++;
              continue;
            }

          /* ensure no duplicates in list */
          if (!g_slist_find_custom (list,
                                    gtk_source_language_get_section (language),
                                    (GCompareFunc)g_strcmp0))
            {
              list = g_slist_prepend (list, (gchar *)gtk_source_language_get_section (language));
            }
        }
      languages++;
    }

  return g_slist_sort (list, (GCompareFunc) g_utf8_collate);
}



static gint
mousepad_util_languages_name_compare (gconstpointer a,
                                              gconstpointer b)
{
  const gchar *name_a, *name_b;

  if (G_UNLIKELY (!GTK_SOURCE_IS_LANGUAGE (a)))
    return -(a != b);
  if (G_UNLIKELY (!GTK_SOURCE_IS_LANGUAGE (b)))
    return a != b;

  name_a = gtk_source_language_get_name (GTK_SOURCE_LANGUAGE (a));
  name_b = gtk_source_language_get_name (GTK_SOURCE_LANGUAGE (b));

  return g_utf8_collate (name_a, name_b);
}



GSList *
mousepad_util_get_sorted_languages_for_section (const gchar *section)
{
  GSList                   *list = NULL;
  const gchar *const       *languages;
  GtkSourceLanguage        *language;
  GtkSourceLanguageManager *manager;

  g_return_val_if_fail (section != NULL, NULL);

  manager = gtk_source_language_manager_get_default ();
  languages = gtk_source_language_manager_get_language_ids (manager);

  while (*languages)
    {
      language = gtk_source_language_manager_get_language (manager, *languages);
      if (G_LIKELY (GTK_SOURCE_IS_LANGUAGE (language)))
        {
          /* ignore hidden languages */
          if(gtk_source_language_get_hidden(language))
            {
              languages++;
              continue;
            }

          /* only get languages in the specified section */
          if (g_strcmp0 (gtk_source_language_get_section (language), section) == 0)
            list = g_slist_prepend (list, language);
        }
      languages++;
    }

  return g_slist_sort(list, (GCompareFunc) mousepad_util_languages_name_compare);
}
