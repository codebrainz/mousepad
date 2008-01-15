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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-file.h>


#define MAX_ENCODING_CHECK_CHARS       (10000)
#define MOUSEPAD_GET_LINE_END_CHARS(a) ((a) == MOUSEPAD_LINE_END_DOS ? "\r\n\0" : ((a) == MOUSEPAD_LINE_END_MAC ? "\r\0" : "\n\0"))


enum
{
  /* EXTERNALLY_MODIFIED, */
  FILENAME_CHANGED,
  LAST_SIGNAL
};

struct _MousepadFileClass
{
  GObjectClass __parent__;
};

struct _MousepadFile
{
  GObject             __parent__;

  /* the text buffer this file belongs to */
  GtkTextBuffer      *buffer;

  /* filename */
  gchar              *filename;

  /* encoding of the file */
  gchar              *encoding;

  /* line ending of the file */
  MousepadLineEnding  line_ending;

  /* our last modification time */
  gint                mtime;

  /* if file is read-only */
  guint               readonly : 1;
};



static void  mousepad_file_class_init       (MousepadFileClass  *klass);
static void  mousepad_file_init             (MousepadFile       *file);
static void  mousepad_file_finalize         (GObject            *object);



static guint file_signals[LAST_SIGNAL];



G_DEFINE_TYPE (MousepadFile, mousepad_file, G_TYPE_OBJECT);



static void
mousepad_file_class_init (MousepadFileClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_file_finalize;

#if 0
  /* TODO implement this signal */
  file_signals[EXTERNALLY_MODIFIED] =
    g_signal_new (I_("externally-modified"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
#endif

  file_signals[FILENAME_CHANGED] =
    g_signal_new (I_("filename-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING);
}



static void
mousepad_file_init (MousepadFile *file)
{
  /* initialize */
  file->filename    = NULL;
  file->encoding    = NULL;
  file->line_ending = MOUSEPAD_LINE_END_NONE;
  file->readonly    = TRUE;
  file->mtime       = 0;
}



static void
mousepad_file_finalize (GObject *object)
{
  MousepadFile *file = MOUSEPAD_FILE (object);

  /* cleanup */
  g_free (file->filename);
  g_free (file->encoding);

  /* release the reference from the buffer */
  g_object_unref (G_OBJECT (file->buffer));

  (*G_OBJECT_CLASS (mousepad_file_parent_class)->finalize) (object);
}



MousepadFile *
mousepad_file_new (GtkTextBuffer *buffer)
{
  MousepadFile *file;

  _mousepad_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);

  file = g_object_new (MOUSEPAD_TYPE_FILE, NULL);

  /* set the buffer */
  file->buffer = g_object_ref (G_OBJECT (buffer));

  return file;
}



void
mousepad_file_set_filename (MousepadFile *file,
                            const gchar  *filename)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_FILE (file));

  /* free the old filename */
  g_free (file->filename);

  /* set the filename */
  file->filename = g_strdup (filename);

  /* send a signal that the name has been changed */
  g_signal_emit (G_OBJECT (file), file_signals[FILENAME_CHANGED], 0, file->filename);
}



const gchar *
mousepad_file_get_filename (MousepadFile *file)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_FILE (file), NULL);

  return file->filename;
}



gchar *
mousepad_file_get_uri (MousepadFile *file)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_FILE (file), NULL);

  return g_filename_to_uri (file->filename, NULL, NULL);
}



void
mousepad_file_set_encoding (MousepadFile *file,
                            const gchar  *encoding)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_FILE (file));

  /* cleanup */
  g_free (file->encoding);

  /* set new encoding */
  file->encoding = g_strdup (encoding);
}



const gchar *
mousepad_file_get_encoding (MousepadFile *file)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_FILE (file), NULL);

  return file->encoding ? file->encoding : "UTF-8";
}



gboolean
mousepad_file_get_read_only (MousepadFile *file)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_FILE (file), FALSE);

  return file->filename ? file->readonly : FALSE;
}



void
mousepad_file_set_line_ending (MousepadFile       *file,
                               MousepadLineEnding  line_ending)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_FILE (file));

  file->line_ending = line_ending;
}



MousepadLineEnding
mousepad_file_get_line_ending (MousepadFile *file)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_FILE (file), MOUSEPAD_LINE_END_NONE);

  return file->line_ending;
}



gboolean
mousepad_file_open (MousepadFile  *file,
                    GError       **error)
{
  GIOChannel  *channel;
  GIOStatus    status;
  gsize        length, terminator_pos;
  gboolean     succeed = FALSE;
  gchar       *string;
  const gchar *line_term;
  GtkTextIter  start, end;
  struct stat  statb;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_FILE (file), FALSE);
  _mousepad_return_val_if_fail (GTK_IS_TEXT_BUFFER (file->buffer), FALSE);
  _mousepad_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _mousepad_return_val_if_fail (file->filename != NULL, FALSE);

  /* check if the file exists, if not, it's a filename from the command line */
  if (g_file_test (file->filename, G_FILE_TEST_EXISTS) == FALSE)
    {
      file->readonly = FALSE;

      return TRUE;
    }

  /* open the channel */
  channel = g_io_channel_new_file (file->filename, "r", error);

  if (G_LIKELY (channel))
    {
      /* freeze notifications */
      g_object_freeze_notify (G_OBJECT (file->buffer));

      /* set the encoding of the channel */
      if (file->encoding != NULL)
        {
          /* set the encoding */
          status = g_io_channel_set_encoding (channel, file->encoding, error);

          if (G_UNLIKELY (status != G_IO_STATUS_NORMAL))
            goto failed;
        }

      /* get the iter at the beginning of the document */
      gtk_text_buffer_get_start_iter (file->buffer, &end);

      /* read the content of the file */
      for (;;)
        {
          /* read the line */
          status = g_io_channel_read_line (channel, &string, &length, &terminator_pos, error);

          /* leave on problems */
          if (G_UNLIKELY (status != G_IO_STATUS_NORMAL && status != G_IO_STATUS_EOF))
            goto failed;

          if (G_LIKELY (string))
            {
              /* detect the line ending of the file */
              if (length != terminator_pos
                  && file->line_ending == MOUSEPAD_LINE_END_NONE)
                {
                  /* get the characters */
                  line_term = string + terminator_pos;

                  if (strcmp (line_term, "\n") == 0)
                    file->line_ending = MOUSEPAD_LINE_END_UNIX;
                  else if (strcmp (line_term, "\r") == 0)
                    file->line_ending = MOUSEPAD_LINE_END_MAC;
                  else if (strcmp (line_term, "\r\n") == 0)
                    file->line_ending = MOUSEPAD_LINE_END_DOS;
                  else
                    g_warning (_("Unknown line ending detected (%s)"), line_term);
                }

              /* make sure the string utf-8 valid */
              if (G_UNLIKELY (g_utf8_validate (string, length, NULL) == FALSE))
                {
                  /* set an error */
                  g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_FAILED,
                               _("Converted string is not UTF-8 valid"));

                  /* free string */
                  g_free (string);

                  goto failed;
                }

              /* insert the string in the buffer */
              gtk_text_buffer_insert (file->buffer, &end, string, length);

              /* cleanup */
              g_free (string);
            }

          /* break when we've reached the end of the file */
          if (G_UNLIKELY (status == G_IO_STATUS_EOF))
            break;
        }

      /* done */
      succeed = TRUE;

      /* get the start iter */
      gtk_text_buffer_get_start_iter (file->buffer, &start);

      /* set the cursor to the beginning of the document */
      gtk_text_buffer_place_cursor (file->buffer, &start);

      /* get file status */
      if (G_LIKELY (g_lstat (file->filename, &statb) == 0));
        {
          /* store the readonly mode */
          file->readonly = !((statb.st_mode & S_IWUSR) != 0);

          /* store the file modification time */
          file->mtime = statb.st_mtime;
        }

      failed:

      /* empty the buffer if we did not succeed */
      if (G_UNLIKELY (succeed == FALSE))
        {
          gtk_text_buffer_get_bounds (file->buffer, &start, &end);
          gtk_text_buffer_delete (file->buffer, &start, &end);
        }

      /* this does not count as a modified buffer */
      gtk_text_buffer_set_modified (file->buffer, FALSE);

      /* thawn notifications */
      g_object_thaw_notify (G_OBJECT (file->buffer));

      /* close the channel and flush the write buffer */
      g_io_channel_shutdown (channel, TRUE, NULL);

      /* release the channel */
      g_io_channel_unref (channel);
    }

  return succeed;
}



gboolean
mousepad_file_save (MousepadFile  *file,
                    GError       **error)
{
  GIOChannel  *channel;
  GIOStatus    status;
  gboolean     succeed = FALSE;
  gchar       *slice;
  const gchar *line_term;
  GtkTextIter  start, end;
  struct stat  statb;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_FILE (file), FALSE);
  _mousepad_return_val_if_fail (GTK_IS_TEXT_BUFFER (file->buffer), FALSE);
  _mousepad_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _mousepad_return_val_if_fail (file->filename != NULL, FALSE);

  /* open the channel. the file is created or truncated */
  channel = g_io_channel_new_file (file->filename, "w", error);

  if (G_LIKELY (channel))
    {
      /* set the encoding of the channel */
      if (file->encoding != NULL)
        {
          /* set the encoding */
          status = g_io_channel_set_encoding (channel, file->encoding, error);

          if (G_UNLIKELY (status != G_IO_STATUS_NORMAL))
            goto failed;
        }

      /* get the line ending characters we're going to use */
      line_term = MOUSEPAD_GET_LINE_END_CHARS (file->line_ending);

      /* get the start iter of the buffer */
      gtk_text_buffer_get_start_iter (file->buffer, &start);

      /* walk the buffer */
      do
        {
          /* set the end iter */
          end = start;

          /* insert the text if this line is not empty */
          if (gtk_text_iter_ends_line (&start) == FALSE)
            {
              /* move to the end of the line */
              gtk_text_iter_forward_to_line_end (&end);

              /* get the test slice */
              slice = gtk_text_buffer_get_slice (file->buffer, &start, &end, TRUE);

              /* write the file content */
              status = g_io_channel_write_chars (channel, slice, -1, NULL, error);

              /* cleanup */
              g_free (slice);

              /* leave when an error occured */
              if (G_UNLIKELY (status != G_IO_STATUS_NORMAL))
                goto failed;
            }

          /* insert a new line if we haven't reached the end of the buffer */
          if (gtk_text_iter_is_end (&end) == FALSE)
            {
              /* insert the new line */
              status = g_io_channel_write_chars (channel, line_term, -1, NULL, error);

              /* leave when an error occured */
              if (G_UNLIKELY (status != G_IO_STATUS_NORMAL))
                goto failed;
            }
        }
      while (gtk_text_iter_forward_line (&start));

      /* everything seems to be ok */
      succeed = TRUE;

      /* set the new modification time */
      if (G_LIKELY (g_lstat (file->filename, &statb) == 0))
        file->mtime = statb.st_mtime;

      /* everything has been saved */
      gtk_text_buffer_set_modified (file->buffer, FALSE);

      /* we saved succesfully */
      file->readonly = FALSE;

      failed:

      /* close the channel and flush the write buffer */
      g_io_channel_shutdown (channel, TRUE, error);

      /* release the channel */
      g_io_channel_unref (channel);
    }

  return succeed;
}



gboolean
mousepad_file_reload (MousepadFile  *file,
                      GError       **error)
{
  GtkTextIter start, end;
  gboolean    succeed = FALSE;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_FILE (file), FALSE);
  _mousepad_return_val_if_fail (GTK_IS_TEXT_BUFFER (file->buffer), FALSE);
  _mousepad_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _mousepad_return_val_if_fail (file->filename != NULL, FALSE);

  /* simple test if the file has not been removed */
  if (G_UNLIKELY (g_file_test (file->filename, G_FILE_TEST_EXISTS) == FALSE))
    {
      /* set an error */
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT,
                   _("The file \"%s\" you tried to reload does not exist anymore"), file->filename);

      return FALSE;
    }

  /* clear the buffer */
  gtk_text_buffer_get_bounds (file->buffer, &start, &end);
  gtk_text_buffer_delete (file->buffer, &start, &end);

  /* reload the file */
  succeed = mousepad_file_open (file, error);

  return succeed;
}



gboolean
mousepad_file_get_externally_modified (MousepadFile  *file,
                                       GError       **error)
{
  struct stat statb;
  gboolean    modified = TRUE;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_FILE (file), FALSE);
  _mousepad_return_val_if_fail (file->filename != NULL, FALSE);
  _mousepad_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (G_LIKELY (g_lstat (file->filename, &statb) == 0))
    {
      /* check if our modification time differs from the current one */
      modified = (file->mtime > 0 && statb.st_mtime != file->mtime);
    }
  else if (error != NULL)
    {
      /* failed to stat the file */
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   _("Failed to read the status of \"%s\""), file->filename);
    }

  return modified;
}



gint
mousepad_file_test_encoding (MousepadFile  *file,
                             const gchar   *encoding,
                             GError       **error)
{
  GIOChannel  *channel;
  GIOStatus    status;
  gunichar     c;
  gint         unprintable = -1;
  gint         counter = 0;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_FILE (file), -1);
  _mousepad_return_val_if_fail (GTK_IS_TEXT_BUFFER (file->buffer), -1);
  _mousepad_return_val_if_fail (error == NULL || *error == NULL, -1);
  _mousepad_return_val_if_fail (file->filename != NULL, -1);
  _mousepad_return_val_if_fail (encoding != NULL, -1);

  /* check if the file exists */
  if (g_file_test (file->filename, G_FILE_TEST_EXISTS) == FALSE)
    return -1;

  /* open the channel */
  channel = g_io_channel_new_file (file->filename, "r", error);

  if (G_LIKELY (channel))
    {
      /* set the encoding of the channel */
      status = g_io_channel_set_encoding (channel, encoding, error);

      if (G_UNLIKELY (status != G_IO_STATUS_NORMAL))
        goto failed;

      /* set the counter */
      unprintable = 0;

      /* read the characters in the file and break on large files */
      while (G_UNLIKELY (counter < MAX_ENCODING_CHECK_CHARS))
        {
          /* read a character */
          status = g_io_channel_read_unichar (channel, &c, error);

          /* leave on problems */
          if (G_UNLIKELY (status != G_IO_STATUS_NORMAL && status != G_IO_STATUS_EOF))
            {
              /* reset counter */
              unprintable = -1;

              goto failed;
            }

          /* check if the character is unprintable, but not a soft hyphen */
          if (G_UNLIKELY (g_unichar_isprint (c) == FALSE
                          && g_unichar_break_type (c) != G_UNICODE_BREAK_AFTER))
            unprintable++;

          /* break when we've reached the end of the file */
          if (G_UNLIKELY (status == G_IO_STATUS_EOF))
            break;

          /* increase counter */
          counter++;
        }

      failed:

      /* close the channel and flush the write buffer */
      g_io_channel_shutdown (channel, TRUE, NULL);

      /* release the channel */
      g_io_channel_unref (channel);
    }

  return unprintable;
}
