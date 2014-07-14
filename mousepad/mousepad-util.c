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
#include <mousepad/mousepad-util.h>



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
mousepad_util_image_button (const gchar *stock_id,
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
mousepad_util_entry_error (GtkWidget *widget,
                           gboolean   error)
{
  const GdkColor red   = {0, 0xffff, 0x6666, 0x6666};
  const GdkColor white = {0, 0xffff, 0xffff, 0xffff};
  gpointer       pointer;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  /* get the current error state */
  pointer = mousepad_object_get_data (G_OBJECT (widget), "error-state");

  /* only change the state when really needed to avoid multiple widget calls */
  if (GPOINTER_TO_INT (pointer) != error)
    {
      /* set the widget style */
      gtk_widget_modify_base (widget, GTK_STATE_NORMAL, error ? &red : NULL);
      gtk_widget_modify_text (widget, GTK_STATE_NORMAL, error ? &white : NULL);

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
  GtkWidget *vbox, *ebox, *hbox;
  GtkWidget *icon, *label, *line;

  /* remove the main vbox */
  g_object_ref (G_OBJECT (dialog->vbox));
  gtk_container_remove (GTK_CONTAINER (dialog), dialog->vbox);

  /* add a new vbox to the main window */
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (dialog), vbox);
  gtk_widget_show (vbox);

  /* event box for the background color */
  ebox = gtk_event_box_new ();
  gtk_box_pack_start (GTK_BOX (vbox), ebox, FALSE, FALSE, 0);
  gtk_widget_modify_bg (ebox, GTK_STATE_NORMAL, &ebox->style->base[GTK_STATE_NORMAL]);
  gtk_widget_show (ebox);

  /* create a hbox */
  hbox = gtk_hbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
  gtk_container_add (GTK_CONTAINER (ebox), hbox);
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
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  /* cleanup */
  g_free (full_title);

  /* add the separator between header and content */
  line = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (vbox), line, FALSE, FALSE, 0);
  gtk_widget_show (line);

  /* add the main dialog box to the new vbox */
  gtk_box_pack_start (GTK_BOX (vbox), GTK_DIALOG (dialog)->vbox, TRUE, TRUE, 0);
  g_object_unref (G_OBJECT (GTK_DIALOG (dialog)->vbox));
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



static gboolean
mousepad_util_search_iter (const GtkTextIter   *start,
                           const gchar         *string,
                           MousepadSearchFlags  flags,
                           GtkTextIter         *match_start,
                           GtkTextIter         *match_end,
                           const GtkTextIter   *limit)
{
  GtkTextIter  iter, begin;
  gunichar     iter_char, str_char;
  gboolean     succeed = FALSE;
  const gchar *needle = string;
  gboolean     match_case, search_backwards, whole_word;
  guint        needle_offset = 0;

  g_return_val_if_fail (start != NULL, FALSE);
  g_return_val_if_fail (string != NULL, FALSE);
  g_return_val_if_fail (limit != NULL, FALSE);

  /* search properties */
  match_case       = (flags & MOUSEPAD_SEARCH_FLAGS_MATCH_CASE) != 0;
  search_backwards = (flags & MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD) != 0;
  whole_word       = (flags & MOUSEPAD_SEARCH_FLAGS_WHOLE_WORD) != 0;

  /* set the start iter */
  iter = *start;

  /* walk from the start to the end iter */
  for (;;)
    {
      /* break when we reach the limit iter, for backwards searching it's
       * it has to be less than the limit which is 0 so that we can match
       * at the very start of the buffer. for forwards searching the limit
       * is the index after the last char so break when we are at or past
       * the limit. */
      if ((search_backwards && gtk_text_iter_compare (&iter, limit) < 0) ||
          (!search_backwards && gtk_text_iter_compare (&iter, limit) >= 0))
        {
          break;
        }

      /* get the unichar characters */
      iter_char = gtk_text_iter_get_char (&iter);
      str_char  = g_utf8_get_char (needle);

      /* skip unknown characters */
      if (G_UNLIKELY (iter_char == 0xFFFC))
        goto continue_searching;

      /* lower case searching */
      if (!match_case)
        {
          /* convert the characters to lower case */
          iter_char = g_unichar_tolower (iter_char);
          str_char  = g_unichar_tolower (str_char);
        }

      /* compare the two characters */
      if (iter_char == str_char)
        {
          /* first character matched, set the begin iter */
          if (needle_offset == 0)
            begin = iter;

          /* get the next character and increase the offset counter */
          needle = g_utf8_next_char (needle);
          needle_offset++;

          /* we hit the end of the search string, so we had a full match */
          if (G_UNLIKELY (*needle == '\0'))
            {
              if (G_LIKELY (!search_backwards))
                {
                  /* set the end iter after the character (for selection) */
                  gtk_text_iter_forward_char (&iter);

                  /* check if we match a whole word */
                  if (whole_word && !(mousepad_util_iter_starts_word (&begin) && mousepad_util_iter_ends_word (&iter)))
                    goto reset_and_continue_searching;
                }
              else
                {
                  /* set the start iter after the character (for selection) */
                  gtk_text_iter_forward_char (&begin);

                  /* check if we match a whole word */
                  if (whole_word && !(mousepad_util_iter_starts_word (&iter) && mousepad_util_iter_ends_word (&begin)))
                    goto reset_and_continue_searching;
                }

              /* set the start and end iters */
              *match_start = begin;
              *match_end   = iter;

              /* return true and stop searching */
              succeed = TRUE;

              break;
            }
        }
      else if (needle_offset > 0)
        {
          /* label */
          reset_and_continue_searching:

          /* reset the needle */
          needle = string;
          needle_offset = 0;

          /* reset the iter */
          iter = begin;
        }

      /* label */
      continue_searching:

      /* jump to next iter in the buffer */
      if ((!search_backwards && !gtk_text_iter_forward_char (&iter))
          || (search_backwards && !gtk_text_iter_backward_char (&iter)))
        break;
    }

  return succeed;
}



static void
mousepad_util_search_get_iters (GtkTextBuffer       *buffer,
                                MousepadSearchFlags  flags,
                                GtkTextIter         *start,
                                GtkTextIter         *end,
                                GtkTextIter         *iter)
{
  GtkTextIter sel_start, sel_end, tmp;

  /* get selection bounds */
  gtk_text_buffer_get_selection_bounds (buffer, &sel_start, &sel_end);

  if (flags & MOUSEPAD_SEARCH_FLAGS_AREA_DOCUMENT)
    {
      /* get document bounds */
      gtk_text_buffer_get_bounds (buffer, start, end);

      /* set the start iter */
      if (flags & MOUSEPAD_SEARCH_FLAGS_ITER_AREA_START)
        *iter = *start;
      else if (flags & MOUSEPAD_SEARCH_FLAGS_ITER_AREA_END)
        *iter = *end;
      else
        goto set_selection_iter;
    }
  else if (flags & MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION)
    {
      /* set area iters */
      *start = sel_start;
      *end = sel_end;

      set_selection_iter:

      /* set the start iter */
      if (flags & (MOUSEPAD_SEARCH_FLAGS_ITER_AREA_START | MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START))
        *iter = sel_start;
      else if (flags & (MOUSEPAD_SEARCH_FLAGS_ITER_AREA_END | MOUSEPAD_SEARCH_FLAGS_ITER_SEL_END))
        *iter = sel_end;
      else
        g_assert_not_reached ();
    }
  else
    {
      /* this should never happen */
      g_assert_not_reached ();
    }

  /* invert the start and end iter on backwards searching */
  if (flags & MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD)
    {
      tmp = *start;
      *start = *end;
      *end = tmp;
      /* when searching backwards, we need to start before the selection */
      gtk_text_iter_backward_char (iter);
    }
}



gint
mousepad_util_highlight (GtkTextBuffer       *buffer,
                         GtkTextTag          *tag,
                         const gchar         *string,
                         MousepadSearchFlags  flags)
{
  GtkTextIter start, iter, end;
  GtkTextIter match_start, match_end;
  GtkTextIter cache_start, cache_end;
  gboolean    found, cached = FALSE;
  gint        counter = 0;

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), -1);
  g_return_val_if_fail (GTK_IS_TEXT_TAG (tag), -1);
  g_return_val_if_fail (string == NULL || g_utf8_validate (string, -1, NULL), -1);
  g_return_val_if_fail ((flags & MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD) == 0, -1);

  /* get the buffer bounds */
  gtk_text_buffer_get_bounds (buffer, &start, &end);

  /* remove all the highlight tags */
  gtk_text_buffer_remove_tag (buffer, tag, &start, &end);

  /* quit if there is nothing to highlight */
  if (string == NULL || *string == '\0' || flags & MOUSEPAD_SEARCH_FLAGS_ACTION_CLEANUP)
    return 0;

  /* get the search iters */
  mousepad_util_search_get_iters (buffer, flags, &start, &end, &iter);

  /* initialize cache iters */
  cache_start = cache_end = iter;

  /* highlight all the occurences of the strings */
  do
    {
      /* search for the next occurence of the string */
      found = mousepad_util_search_iter (&iter, string, flags, &match_start, &match_end, &end);

      if (G_LIKELY (found))
        {
          /* try to extend the cache */
          if (gtk_text_iter_equal (&cache_end, &match_start))
            {
              /* enable caching */
              cached = TRUE;
            }
          else
            {
              if (cached)
                {
                  /* highlight the cached occurences */
                  gtk_text_buffer_apply_tag (buffer, tag, &cache_start, &cache_end);

                  /* cache is flushed */
                  cached = FALSE;
                }

              /* highlight the matched occurence */
              gtk_text_buffer_apply_tag (buffer, tag, &match_start, &match_end);

              /* set the new cache start iter */
              cache_start = match_start;
            }

          /* set the end iters */
          iter = cache_end = match_end;

          /* increase the counter */
          counter++;
        }
      else if (G_UNLIKELY (cached))
        {
          /* flush the cached iters */
          gtk_text_buffer_apply_tag (buffer, tag, &cache_start, &cache_end);
        }
    }
  while (G_LIKELY (found));

  return counter;
}



gint
mousepad_util_search (GtkTextBuffer       *buffer,
                      const gchar         *string,
                      const gchar         *replace,
                      MousepadSearchFlags  flags)
{
  gchar       *reversed = NULL;
  gint         counter = 0;
  gboolean     found, search_again = FALSE;
  gboolean     search_backwards, wrap_around;
  GtkTextIter  start, end, iter;
  GtkTextIter  match_start, match_end;
  GtkTextMark *mark_start, *mark_iter, *mark_end, *mark_replace;

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), -1);
  g_return_val_if_fail (string && g_utf8_validate (string, -1, NULL), -1);
  g_return_val_if_fail (replace == NULL || g_utf8_validate (replace, -1, NULL), -1);
  g_return_val_if_fail ((flags & MOUSEPAD_SEARCH_FLAGS_ACTION_HIGHTLIGHT) == 0, -1);

  /* freeze buffer notifications */
  g_object_freeze_notify (G_OBJECT (buffer));

  /* get the search iters */
  mousepad_util_search_get_iters (buffer, flags, &start, &end, &iter);

  /* store the initial iters in marks */
  mark_start = gtk_text_buffer_create_mark (buffer, NULL, &start, TRUE);
  mark_iter  = gtk_text_buffer_create_mark (buffer, NULL, &iter, TRUE);
  mark_end   = gtk_text_buffer_create_mark (buffer, NULL, &end, TRUE);

  /* some to make the code easier to read */
  search_backwards = ((flags & MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD) != 0);
  wrap_around = ((flags & MOUSEPAD_SEARCH_FLAGS_WRAP_AROUND) != 0 && !gtk_text_iter_equal (&start, &iter));

  /* if we're not really searching anything, reset the cursor */
  if (string == NULL || *string == '\0')
    goto reset_cursor;

  if (search_backwards)
    {
      /* reverse the search string */
      reversed = g_utf8_strreverse (string, -1);

      /* set the new search string */
      string = reversed;
    }

  do
    {
      /* search the string */
      found = mousepad_util_search_iter (&iter, string, flags, &match_start, &match_end, &end);

      /* don't search again unless changed below */
      search_again = FALSE;

      if (found)
        {
          /* handle the action */
          if (flags & MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT)
            {
              /* select the match */
              if (! search_backwards)
                gtk_text_buffer_select_range (buffer, &match_start, &match_end);
              else
                gtk_text_buffer_select_range (buffer, &match_end, &match_start);
            }
          else if (flags & MOUSEPAD_SEARCH_FLAGS_ACTION_REPLACE)
            {
              /* create text mark */
              mark_replace = gtk_text_buffer_create_mark (buffer, NULL, &match_start, search_backwards);

              /* delete the match */
              gtk_text_buffer_delete (buffer, &match_start, &match_end);

              /* restore the iter */
              gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark_replace);

              /* insert the replacement */
              if (G_LIKELY (replace))
                gtk_text_buffer_insert (buffer, &iter, replace, -1);

              /* remove the mark */
              gtk_text_buffer_delete_mark (buffer, mark_replace);

              /* restore the begin and end iters */
              gtk_text_buffer_get_iter_at_mark (buffer, &start, mark_start);
              gtk_text_buffer_get_iter_at_mark (buffer, &end, mark_end);

              /* when we don't replace all matches, select the next one */
              if ((flags & MOUSEPAD_SEARCH_FLAGS_ENTIRE_AREA) == 0)
                flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT;

              /* search again */
              search_again = TRUE;
            }
          else if (flags & MOUSEPAD_SEARCH_FLAGS_ACTION_NONE)
            {
              /* keep searching when requested */
              if (flags & MOUSEPAD_SEARCH_FLAGS_ENTIRE_AREA)
                search_again = TRUE;

              /* move iter */
              iter = match_end;
            }
          else
            {
              /* no valid action was defined */
              g_assert_not_reached ();
            }

          /* increase the counter */
          counter++;
        }
      else if (wrap_around)
        {
          /* get the start iter (note that the start and iter were already
           * reversed for backwards searching) */
          gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark_start);

          /* get end iter */
          gtk_text_buffer_get_iter_at_mark (buffer, &end, mark_iter);

          /* we wrapped, don't try again */
          wrap_around = FALSE;

          /* search again */
          search_again = TRUE;
        }
      else
        {
          reset_cursor:

          /* get the initial start mark */
          gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark_iter);

          /* reset the cursor */
          gtk_text_buffer_place_cursor (buffer, &iter);
        }
    }
  while (search_again);

  /* make sure the selection is restored */
  if (flags & MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION)
    gtk_text_buffer_select_range (buffer, &start, &end);

  /* cleanup */
  g_free (reversed);

  /* cleanup marks */
  gtk_text_buffer_delete_mark (buffer, mark_start);
  gtk_text_buffer_delete_mark (buffer, mark_iter);
  gtk_text_buffer_delete_mark (buffer, mark_end);

  /* thawn buffer notifications */
  g_object_thaw_notify (G_OBJECT (buffer));

  return counter;
}



/* get the related action of the widget or walk up the parents to find one.
 * useful for example to get the related action of a tool item from its child. */
GtkAction *
mousepad_util_find_related_action (GtkWidget *widget)
{
  GtkAction *action;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  do
    {
      if (GTK_IS_ACTIVATABLE (widget))
        action = gtk_activatable_get_related_action (GTK_ACTIVATABLE (widget));

      if (G_UNLIKELY (action == NULL))
        {
          if (gtk_widget_is_toplevel (widget))
            break;
          widget = gtk_widget_get_parent (widget);
        }
    }
  while (action == NULL);

  return action;
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
