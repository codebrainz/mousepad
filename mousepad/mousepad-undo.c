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
#include <mousepad/mousepad-undo.h>



/* maximum number of steps in the undo manager */
#define MAX_UNDO_STEPS 75



typedef struct _MousepadUndoInfo   MousepadUndoInfo;
typedef enum   _MousepadUndoAction MousepadUndoAction;

enum
{
  CAN_UNDO,
  CAN_REDO,
  LAST_SIGNAL
};

enum _MousepadUndoAction
{
  INSERT,
  DELETE,
};

struct _MousepadUndoClass
{
  GObjectClass __parent__;
};

struct _MousepadUndo
{
  GObject __parent__;

  /* the text buffer we're monitoring */
  GtkTextBuffer      *buffer;

  /* whether the undo monitor is locked */
  guint               locked;

  /* list of undo steps */
  GList              *steps;

  /* position in the steps list */
  gint                steps_position;

  /* the buffer of the active step */
  GString            *step_buffer;

  /* the info of the active step */
  MousepadUndoAction  step_action;
  gint                step_start;
  gint                step_end;
  guint               step_isspaces : 1;

  /* whether we can undo and redo */
  guint               can_undo : 1;
  guint               can_redo : 1;
};

struct _MousepadUndoInfo
{
  /* the action of the undo step */
  MousepadUndoAction  action;

  /* the deleted or inserted string */
  gchar              *string;

  /* the start and end position in the buffer */
  gint                start;
  gint                end;
};



static void  mousepad_undo_class_init       (MousepadUndoClass  *klass);
static void  mousepad_undo_init             (MousepadUndo       *undo);
static void  mousepad_undo_finalize         (GObject            *object);
static void  mousepad_undo_free_step        (MousepadUndoInfo   *info,
                                             MousepadUndo       *undo);
static void  mousepad_undo_preform_step     (MousepadUndo       *undo,
                                             gboolean            undo_step);
static void  mousepad_undo_new_step         (MousepadUndo       *undo);
static void  mousepad_undo_insert_text      (GtkTextBuffer      *buffer,
                                             GtkTextIter        *pos,
                                             const gchar        *text,
                                             gint                length,
                                             MousepadUndo       *undo);
static void  mousepad_undo_delete_range     (GtkTextBuffer      *buffer,
                                             GtkTextIter        *start_iter,
                                             GtkTextIter        *end_iter,
                                             MousepadUndo       *undo);



static GObjectClass *mousepad_undo_parent_class;
static guint         undo_signals[LAST_SIGNAL];



GType
mousepad_undo_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_type_register_static_simple (G_TYPE_OBJECT,
                                            I_("MousepadUndo"),
                                            sizeof (MousepadUndoClass),
                                            (GClassInitFunc) mousepad_undo_class_init,
                                            sizeof (MousepadUndo),
                                            (GInstanceInitFunc) mousepad_undo_init,
                                            0);
    }

  return type;
}



static void
mousepad_undo_class_init (MousepadUndoClass *klass)
{
  GObjectClass *gobject_class;

  mousepad_undo_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_undo_finalize;

  undo_signals[CAN_UNDO] =
    g_signal_new (I_("can-undo"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  undo_signals[CAN_REDO] =
    g_signal_new (I_("can-redo"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}



static void
mousepad_undo_init (MousepadUndo *undo)
{
  /* reset some variabled */
  undo->locked = 0;
  undo->steps = NULL;
  undo->steps_position = 0;

  /* we can't undo or redo */
  undo->can_undo = FALSE;
  undo->can_redo = FALSE;

  /* allocate the string buffer (we prealloc 15 characters to avoid multiple reallocations) */
  undo->step_buffer = g_string_sized_new (15);
}



static void
mousepad_undo_finalize (GObject *object)
{
  MousepadUndo *undo = MOUSEPAD_UNDO (object);
  GList        *li;

  /* cleanup the undo steps */
  for (li = undo->steps; li != NULL; li = li->next)
    mousepad_undo_free_step (li->data, undo);

  /* free the list */
  g_list_free (undo->steps);

  /* cleanup the monitor step */
  g_string_free (undo->step_buffer, TRUE);

  /* release the undo reference from the buffer */
  g_object_unref (G_OBJECT (undo->buffer));

  (*G_OBJECT_CLASS (mousepad_undo_parent_class)->finalize) (object);
}



static void
mousepad_undo_free_step (MousepadUndoInfo *info,
                         MousepadUndo     *undo)
{
  /* remove from the list */
  undo->steps = g_list_remove (undo->steps, info);

  /* free the string */
  g_free (info->string);

  /* free the slice */
  g_slice_free (MousepadUndoInfo, info);
}



static void
mousepad_undo_preform_step (MousepadUndo *undo,
                            gboolean      undo_step)
{
  MousepadUndoInfo   *info;
  MousepadUndoAction  action;
  GtkTextIter         start_iter, end_iter;

  _mousepad_return_if_fail (MOUSEPAD_IS_UNDO (undo));

  /* prevent undo updates */
  mousepad_undo_lock (undo);

  /* flush the undo buffer */
  mousepad_undo_new_step (undo);
  undo->step_start = undo->step_end = 0;

  /* decrease the position counter if we're going to undo */
  if (undo_step)
    undo->steps_position--;

  /* get the step we're going to undo */
  info = g_list_nth_data (undo->steps, undo->steps_position);

  if (G_LIKELY (info))
    {
      /* get the action */
      action = info->action;

      /* swap the action if we're going to undo */
      if (!undo_step)
        action = (action == INSERT ? DELETE : INSERT);

      /* get the start iter position */
      gtk_text_buffer_get_iter_at_offset (undo->buffer, &start_iter, info->start);

      switch (action)
        {
          case INSERT:
            /* get the end iter */
            gtk_text_buffer_get_iter_at_offset (undo->buffer, &end_iter, info->end);

            /* delete the inserted text */
            gtk_text_buffer_delete (undo->buffer, &start_iter, &end_iter);

            break;

          case DELETE:
            /* insert the deleted text */
            gtk_text_buffer_insert (undo->buffer, &start_iter, info->string, -1);

            break;

          default:
            _mousepad_assert_not_reached ();
        }

      /* set the cursor, we scroll to the cursor in mousepad-document */
      gtk_text_buffer_place_cursor (undo->buffer, &start_iter);

      /* increase the position counter if we did a redo */
      if (!undo_step)
        undo->steps_position++;
    }

  /* set the can_undo boolean */
  undo->can_undo = (undo->steps_position > 0);
  undo->can_redo = (undo->steps_position < g_list_length (undo->steps));

  /* emit the can-undo and can-redo signals */
  g_signal_emit (G_OBJECT (undo), undo_signals[CAN_UNDO], 0, undo->can_undo);
  g_signal_emit (G_OBJECT (undo), undo_signals[CAN_REDO], 0, undo->can_redo);

  /* remove our lock */
  mousepad_undo_unlock (undo);
}



static void
mousepad_undo_new_step (MousepadUndo *undo)
{
  MousepadUndoInfo *info, *existing;
  gint              i, length;

  /* leave when there is nothing todo */
  if (undo->step_start == 0 && undo->step_end == 0)
    return;

  length = g_list_length (undo->steps);

  /* when we're not at the end of the list, the user did some redo steps. so we actually
   * reverted the steps after the current position. if we want to do proper undoing after
   * appending the current step, we have to append the redo-ed steps in reversed order
   * with an inverted action.
   *
   * there is also a lazy way to avoid weird undo's/redo's: remove all the steps after the
   * current list position. this was the old behaviour of Mousepad. */

  if (undo->steps_position != length)
    for (i = length - 1; i >= undo->steps_position; i--)
      {
        /* get the existing step */
        existing = g_list_nth_data (undo->steps, i);

        /* allocate a new slice */
        info = g_slice_new0 (MousepadUndoInfo);

        /* copy the data from the existing step */
        info->string = g_strdup (existing->string);
        info->start  = existing->start;
        info->end    = existing->end;

        /* set the inverted action */
        info->action = (existing->action == INSERT ? DELETE : INSERT);

        /* append to the steps list */
        undo->steps = g_list_append (undo->steps, info);
      }

  /* allocate the slice */
  info = g_slice_new0 (MousepadUndoInfo);

  /* set the info */
  info->string = g_strdup (undo->step_buffer->str);
  info->action = undo->step_action;
  info->start  = undo->step_start;
  info->end    = undo->step_end;

  /* append to the steps list */
  undo->steps = g_list_append (undo->steps, info);

  /* erase the buffer */
  undo->step_buffer = g_string_erase (undo->step_buffer, 0, -1);
  undo->step_start = undo->step_end = 0;

  /* check the list length */
  for (i = g_list_length (undo->steps); i > MAX_UNDO_STEPS; i--)
    {
      /* get the first item in the list */
      info = g_list_first (undo->steps)->data;

      /* cleanup the data and remove from the list */
      mousepad_undo_free_step (info, undo);
    }

  /* set the new list position (end of the list) */
  undo->steps_position = i;

  /* we're at the end of the list, so we can't redo */
  undo->can_redo = FALSE;
}



static void
mousepad_undo_handle_step (const gchar        *text,
                           gint                start,
                           gint                end,
                           MousepadUndoAction  action,
                           MousepadUndo       *undo)
{
  gint     length;
  guchar   c;
  gboolean char_isspace = FALSE;
  gboolean char_isnewline = FALSE;
  gboolean create_new_step = FALSE;
  gboolean clear_step_afterwards = FALSE;

  /* length of the text */
  length = ABS (end - start);

  /* only do this if there is 1 character typed / deleted */
  if (length == 1)
    {
      /* get the character */
      c = g_utf8_get_char (text);

      /* check if the charater is a space or a new line */
      char_isspace = (c == ' ' || c == '\t');
      char_isnewline = (c == '\n');
    }

  /* create a new step if we jump to a new line or the string contains only spaces, but the
   * new char is not a space */
  if (char_isnewline || (undo->step_isspaces && !char_isspace))
    {
      create_new_step = clear_step_afterwards = TRUE;
      goto new_step;
    }
  /* if the buffer does not start with a space, but the new char does, we flush the buffer
   * afterwards */
  else if (!undo->step_isspaces && char_isspace)
    clear_step_afterwards = TRUE;

  /* check if we can append (insert action) */
  if (undo->step_action == action && action == INSERT && undo->step_end == start)
    {
      /* append the inserted string */
      undo->step_buffer = g_string_append_len (undo->step_buffer, text, length);

      /* update the end position */
      undo->step_end = end;
    }
  /* check if we can prepend (delete action) */
  else if (undo->step_action == action && action == DELETE && undo->step_start == end)
    {
      /* prepend the deleted text */
      undo->step_buffer = g_string_prepend_len (undo->step_buffer, text, length);

      /* update the start position */
      undo->step_start = start;
    }
  /* create a new step */
  else
    {
      create_new_step = clear_step_afterwards = TRUE;
    }

new_step:
  if (clear_step_afterwards)
    mousepad_undo_new_step (undo);

  /* only start a new step when the char was not a space */
  if (create_new_step)
    {
      /* set the new info */
      undo->step_buffer   = g_string_append_len (undo->step_buffer, text, ABS (start - end));
      undo->step_action   = action;
      undo->step_start    = start;
      undo->step_end      = end;
      undo->step_isspaces = char_isspace;

      /* flush again when we had a new line */
      if (char_isnewline)
        mousepad_undo_new_step (undo);
    }

  /* check if we need to send the undo signal */
  if (undo->can_undo != TRUE)
    {
      undo->can_undo = TRUE;

      /* emit the can-undo signal */
      g_signal_emit (G_OBJECT (undo), undo_signals[CAN_UNDO], 0, undo->can_undo);
    }
}



static void
mousepad_undo_insert_text (GtkTextBuffer *buffer,
                           GtkTextIter   *pos,
                           const gchar   *text,
                           gint           length,
                           MousepadUndo  *undo)
{
  gint start, end;

  /* quit when the undo manager is locked */
  if (undo->locked)
    return;

  /* get the start and end position */
  start = gtk_text_iter_get_offset (pos);
  end = start + length;

  /* append the changes */
  mousepad_undo_handle_step (text, start, end, INSERT, undo);
}



static void
mousepad_undo_delete_range (GtkTextBuffer *buffer,
                            GtkTextIter   *start_iter,
                            GtkTextIter   *end_iter,
                            MousepadUndo  *undo)
{
  gint   start, end;
  gchar *text;

  /* quit when the undo manager is locked */
  if (undo->locked)
    return;

  /* get the start and end position */
  start = gtk_text_iter_get_offset (start_iter);
  end = gtk_text_iter_get_offset (end_iter);

  /* get the deleted string */
  text = gtk_text_buffer_get_slice (buffer, start_iter, end_iter, FALSE);

  /* append the changes */
  mousepad_undo_handle_step (text, start, end, DELETE, undo);

  /* cleanup */
  g_free (text);
}



MousepadUndo *
mousepad_undo_new (GtkTextBuffer *buffer)
{
  MousepadUndo *undo;

  _mousepad_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);

  /* create the undo object */
  undo = g_object_new (MOUSEPAD_TYPE_UNDO, NULL);

  /* set the buffer (we also take a reference) */
  undo->buffer = g_object_ref (G_OBJECT (buffer));

  /* connect signals to the buffer so we can monitor it */
  g_signal_connect (G_OBJECT (buffer), "insert-text", G_CALLBACK (mousepad_undo_insert_text), undo);
  g_signal_connect (G_OBJECT (buffer), "delete-range", G_CALLBACK (mousepad_undo_delete_range), undo);

  return undo;
}



void
mousepad_undo_lock (MousepadUndo *undo)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_UNDO (undo));

  /* increase the lock count */
  undo->locked++;
}



void
mousepad_undo_unlock (MousepadUndo *undo)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_UNDO (undo));
  _mousepad_return_if_fail (undo->locked > 0);

  /* decrease the lock count */
  undo->locked--;
}



void
mousepad_undo_do_undo (MousepadUndo *undo)
{
  /* run the undo step */
  mousepad_undo_preform_step (undo, TRUE);
}



void
mousepad_undo_do_redo (MousepadUndo *undo)
{
  /* run the redo step */
  mousepad_undo_preform_step (undo, FALSE);
}



gboolean
mousepad_undo_can_undo (MousepadUndo *undo)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_UNDO (undo), FALSE);

  return undo->can_undo;
}



gboolean
mousepad_undo_can_redo (MousepadUndo *undo)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_UNDO (undo), FALSE);

  return undo->can_redo;
}



void
mousepad_undo_populate_popup (GtkTextView  *textview,
                              GtkMenu      *menu,
                              MousepadUndo *undo)
{
  GtkWidget *item;
  gboolean   editable;

  _mousepad_return_if_fail (MOUSEPAD_IS_UNDO (undo));

  /* whether the textview is editable */
  editable = gtk_text_view_get_editable (textview);

  /* separator */
  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* redo item */
  item = gtk_image_menu_item_new_from_stock (GTK_STOCK_REDO, NULL);
  gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
  gtk_widget_set_sensitive (item, editable && mousepad_undo_can_redo (undo));
  g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (mousepad_undo_do_redo), undo);
  gtk_widget_show (item);

  /* undo item */
  item = gtk_image_menu_item_new_from_stock (GTK_STOCK_UNDO, NULL);
  gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
  gtk_widget_set_sensitive (item, editable && mousepad_undo_can_undo (undo));
  g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (mousepad_undo_do_undo), undo);
  gtk_widget_show (item);
}
