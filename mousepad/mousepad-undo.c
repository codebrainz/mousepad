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
#include <mousepad/mousepad-undo.h>


/* global */
#define MOUSEPAD_UNDO_MAX_STEPS         (100) /* maximum number of undo steps */
#define MOUSEPAD_UNDO_BUFFER_SIZE       (40)  /* buffer size */

/* points system */
#define MOUSEPAD_UNDO_POINTS            (30)  /* maximum points in a step */
#define MOUSEPAD_UNDO_POINTS_CHAR       (1)   /* points for a character */
#define MOUSEPAD_UNDO_POINTS_WORD_BREAK (10)  /* points for a word break */
#define MOUSEPAD_UNDO_POINTS_NEW_LINE   (25)  /* points for a new line */



typedef struct _MousepadUndoCache  MousepadUndoCache;
typedef struct _MousepadUndoStep   MousepadUndoStep;
typedef enum   _MousepadUndoAction MousepadUndoAction;



static void mousepad_undo_finalize                 (GObject            *object);
static void mousepad_undo_emit_signals             (MousepadUndo       *undo);
static void mousepad_undo_step_free                (MousepadUndoStep   *step);
static void mousepad_undo_step                     (MousepadUndo       *undo,
                                                    gboolean            redo);
static void mousepad_undo_cache_reset              (MousepadUndo       *undo);
static void mousepad_undo_clear_oldest_step        (MousepadUndo       *undo);
static void mousepad_undo_cache_to_step            (MousepadUndo       *undo);
static void mousepad_undo_needle_reset             (MousepadUndo       *undo);
static void mousepad_undo_buffer_changed           (MousepadUndo       *undo,
                                                    MousepadUndoAction  action,
                                                    const gchar        *text,
                                                    gint                length,
                                                    gint                start_offset,
                                                    gint                end_offset);
static void mousepad_undo_buffer_insert            (GtkTextBuffer      *buffer,
                                                    GtkTextIter        *pos,
                                                    const gchar        *text,
                                                    gint                length,
                                                    MousepadUndo       *undo);
static void mousepad_undo_buffer_delete            (GtkTextBuffer      *buffer,
                                                    GtkTextIter        *start_iter,
                                                    GtkTextIter        *end_iter,
                                                    MousepadUndo       *undo);
static void mousepad_undo_buffer_begin_user_action (GtkTextBuffer      *buffer,
                                                    MousepadUndo       *undo);


enum
{
  CAN_UNDO,
  CAN_REDO,
  LAST_SIGNAL
};

struct _MousepadUndoClass
{
  GObjectClass __parent__;
};

enum _MousepadUndoAction
{
  INSERT, /* insert action */
  DELETE, /* delete action */
};

struct _MousepadUndo
{
  GObject __parent__;

  /* the text buffer we're monitoring */
  GtkTextBuffer      *buffer;

  /* whether the undo manager is locked */
  gint                locked;

  /* whether multiple changes are merged */
  gint                grouping;

  /* whether we can undo or redo */
  guint               can_undo : 1;
  guint               can_redo : 1;

  /* list containing the steps */
  GList              *steps;

  /* number of steps */
  gint                n_steps;

  /* steps list pointer when undoing */
  GList              *needle;

  /* element in the last when saving */
  GList              *saved;

  /* string holding the deleted characters */
  GString            *cache;

  /* start and end positions of the cache */
  gint                cache_start;
  gint                cache_end;

  /* if the last character in the cache is a space */
  guint               cache_is_space : 1;

  /* if the changes in the cache are part of a group */
  guint               cache_in_group : 1;

  /* current action in the cache */
  MousepadUndoAction  cache_action;

  /* number of points assigned to the cache */
  gint                cache_points;
};

struct _MousepadUndoStep
{
  /* step action */
  MousepadUndoAction  action;

  /* deleted string */
  gchar              *data;

  /* start and end positions */
  gint                start;
  gint                end;

  /* whether this step is part of a group */
  guint               in_group : 1;
};



static guint undo_signals[LAST_SIGNAL];



G_DEFINE_TYPE (MousepadUndo, mousepad_undo, G_TYPE_OBJECT);


static void
mousepad_undo_class_init (MousepadUndoClass *klass)
{
  GObjectClass *gobject_class;

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
  /* initialize */
  undo->locked = 0;
  undo->grouping = 0;
  undo->n_steps = 0;
  undo->can_undo = FALSE;
  undo->can_redo = FALSE;
  undo->steps = NULL;
  undo->needle = NULL;
  undo->saved = NULL;

  /* initialize the cache */
  undo->cache = NULL;
  mousepad_undo_cache_reset (undo);
}



static void
mousepad_undo_finalize (GObject *object)
{
  MousepadUndo *undo = MOUSEPAD_UNDO (object);

  /* lock to avoid updates */
  mousepad_undo_lock (undo);

  /* clear the undo manager */
  mousepad_undo_clear (undo);

  /* release the undo reference from the buffer */
  g_object_unref (G_OBJECT (undo->buffer));

  (*G_OBJECT_CLASS (mousepad_undo_parent_class)->finalize) (object);
}



static void
mousepad_undo_emit_signals (MousepadUndo *undo)
{
  gboolean can_undo, can_redo;

  /* detect if we can undo or redo */
  can_undo = (undo->needle != NULL || undo->cache_start != undo->cache_end);
  can_redo = (undo->needle == NULL || g_list_previous (undo->needle) != NULL);

  /* emit signals if needed */
  if (undo->can_undo != can_undo)
    {
      undo->can_undo = can_undo;
      g_signal_emit (G_OBJECT (undo), undo_signals[CAN_UNDO], 0, can_undo);
    }

  if (undo->can_redo != can_redo)
    {
      undo->can_redo = can_redo;
      g_signal_emit (G_OBJECT (undo), undo_signals[CAN_REDO], 0, can_redo);
    }
}



static void
mousepad_undo_step_free (MousepadUndoStep *step)
{
  /* free the string */
  g_free (step->data);

  /* free the slice */
  g_slice_free (MousepadUndoStep, step);
}



static void
mousepad_undo_step (MousepadUndo *undo,
                    gboolean      redo)
{
  MousepadUndoStep   *step;
  MousepadUndoAction  action;
  GtkTextIter         start_iter, end_iter;
  GList              *li;

  /* lock */
  mousepad_undo_lock (undo);

  /* flush the cache */
  mousepad_undo_cache_to_step (undo);

  /* set the previous element for redoing */
  if (redo)
    {
      if (undo->needle)
        undo->needle = g_list_previous (undo->needle);
      else
        undo->needle = g_list_last (undo->steps);
    }

  /* freeze buffer notifications */
  g_object_freeze_notify (G_OBJECT (undo->buffer));

  for (li = undo->needle; li != NULL; li = (redo ? li->prev : li->next))
    {
      /* get the step */
      step = li->data;

      /* get the action */
      action = step->action;

      /* invert the action if needed */
      if (redo)
        action = (action == INSERT ? DELETE : INSERT);

      /* get the start iter */
      gtk_text_buffer_get_iter_at_offset (undo->buffer, &start_iter, step->start);

      switch (action)
        {
          case INSERT:
            /* debug check */
            mousepad_return_if_fail (step->data == NULL);

            /* get the end iter */
            gtk_text_buffer_get_iter_at_offset (undo->buffer, &end_iter, step->end);

            /* set the deleted for redo */
            step->data = gtk_text_buffer_get_slice (undo->buffer, &start_iter, &end_iter, TRUE);

            /* delete the inserted text */
            gtk_text_buffer_delete (undo->buffer, &start_iter, &end_iter);
            break;

          case DELETE:
            /* debug check */
            mousepad_return_if_fail (step->data != NULL);

            /* insert the deleted text */
            gtk_text_buffer_insert (undo->buffer, &start_iter, step->data, -1);

            /* and cleanup */
            g_free (step->data);
            step->data = NULL;
            break;

          default:
            mousepad_assert_not_reached ();
            break;
        }

      /* get the previous item when we redo */
      if (redo)
        {
          if (g_list_previous (li) != NULL)
            step = g_list_previous (li)->data;
          else
            step = NULL;
        }

      /* break when the step is not part of a group */
      if (step == NULL || step->in_group == FALSE)
        break;
    }

  /* thawn buffer notifications */
  g_object_thaw_notify (G_OBJECT (undo->buffer));

  /* set the needle */
  if (redo)
    undo->needle = li;
  else
    undo->needle = g_list_next (li);

  /* check if we've somehow reached the save point */
  gtk_text_buffer_set_modified (undo->buffer, undo->needle != undo->saved);

  /* emit undo and redo signals */
  mousepad_undo_emit_signals (undo);

  /* unlock */
  mousepad_undo_unlock (undo);
}



static void
mousepad_undo_clear_oldest_step (MousepadUndo *undo)
{
  GList            *li, *lprev;
  MousepadUndoStep *step;
  gint              to_remove;

  mousepad_return_if_fail (undo->n_steps > MOUSEPAD_UNDO_MAX_STEPS);

  /* number of steps to remove */
  to_remove = undo->n_steps - MOUSEPAD_UNDO_MAX_STEPS;

  /* get end of steps list and remove the entire group */
  for (li = g_list_last (undo->steps); li != NULL; li = lprev)
    {
      step = li->data;

      /* update counters */
      if (step->in_group == FALSE)
        {
          if (to_remove == 0)
            break;

          /* update counter */
          to_remove--;
          undo->n_steps--;
        }

      /* cleanup */
      mousepad_undo_step_free (step);

      /* previous step */
      lprev = li->prev;

      /* remove from list */
      undo->steps = g_list_delete_link (undo->steps, li);
    }
}



static void
mousepad_undo_cache_reset (MousepadUndo *undo)
{
  mousepad_return_if_fail (undo->cache == NULL);

  /* reset variables */
  undo->cache_start = undo->cache_end = -1;
  undo->cache_in_group = FALSE;
  undo->cache_is_space = FALSE;
  undo->cache_points = 0;
}



static void
mousepad_undo_cache_to_step (MousepadUndo *undo)
{
  MousepadUndoStep *step;

  /* only add when the cache contains changes */
  if (G_LIKELY (undo->cache_start != undo->cache_end))
    {
      /* make sure the needle has been reset */
      mousepad_return_if_fail (undo->needle == undo->steps);

      /* allocate slice */
      step = g_slice_new0 (MousepadUndoStep);

      /* set data */
      step->action = undo->cache_action;
      step->start = undo->cache_start;
      step->end = undo->cache_end;
      step->in_group = undo->cache_in_group;

      /* increase real step counter */
      if (step->in_group == FALSE)
        if (++undo->n_steps > MOUSEPAD_UNDO_MAX_STEPS)
          mousepad_undo_clear_oldest_step (undo);

      if (step->action == DELETE)
        {
          /* free cache and set the data */
          step->data = g_string_free (undo->cache, FALSE);

          /* null the cache */
          undo->cache = NULL;
        }
      else
        {
          /* null the data */
          step->data = NULL;
        }

      /* prepend the new step */
      undo->needle = undo->steps = g_list_prepend (undo->steps, step);

      /* reset the cache */
      mousepad_undo_cache_reset (undo);
    }
}



static void
mousepad_undo_needle_reset (MousepadUndo *undo)
{
  MousepadUndoStep *step;

  /* remove steps from the list until we reach the needle */
  while (undo->steps != undo->needle)
    {
      step = undo->steps->data;

      /* decrease real step counter */
      if (step->in_group == FALSE)
        undo->n_steps--;

      /* free the step data */
      mousepad_undo_step_free (step);

      /* delete the element from the list */
      undo->steps = g_list_delete_link (undo->steps, undo->steps);
    }

  /* debug check */
  mousepad_return_if_fail (undo->needle == undo->steps);
}



static void
mousepad_undo_buffer_changed (MousepadUndo       *undo,
                              MousepadUndoAction  action,
                              const gchar        *text,
                              gint                length,
                              gint                start_offset,
                              gint                end_offset)
{
  gunichar c;
  gboolean is_space, is_newline;

  /* when grouping is 0 we going to detect if it's needed to create a
   * new step (existing data in buffer, points, etc). when grouping is
   * > 0 it means we're already inside a group and thus merge as much
   * as possible. */
  if (undo->grouping == 0)
    {
      if (length > 1 || undo->cache_in_group)
        {
          /* the buffer contains still data from a grouped step or more then one
           * character has been changed. in this case we always create a new step */
          goto create_new_step;
        }
      else /* single char changed */
        {
          /* get the changed character */
          c = g_utf8_get_char (text);

          /* if the character is a space */
          is_space = g_unichar_isspace (c);

          /* when the maximum number of points has been passed and the
           * last charater in the buffer differs from the new one, we
           * force a new step */
          if (undo->cache_points > MOUSEPAD_UNDO_POINTS
              && undo->cache_is_space != is_space)
            {
              goto create_new_step;
            }

          /* set the new last character type */
          undo->cache_is_space = is_space;

          /* if the changed character is a new line */
          is_newline = (c == '\n' || c == '\r');

          /* update the point statistics */
          if (is_newline)
            undo->cache_points += MOUSEPAD_UNDO_POINTS_NEW_LINE;
          else if (is_space)
            undo->cache_points += MOUSEPAD_UNDO_POINTS_WORD_BREAK;
          else
            undo->cache_points += MOUSEPAD_UNDO_POINTS_CHAR;
        }
    }

  /* try to merge the new change with the buffer. if this is not possible
   * new put the cache in a new step and insert the last change in the buffer */
  if (undo->cache_action == action
      && action == INSERT
      && undo->cache_end == start_offset)
    {
      /* we can merge with the previous insert */
      undo->cache_end = end_offset;
    }
  else if (undo->cache_action == action
           && action == DELETE
           && undo->cache_start == end_offset)
    {
      /* we can merge with the previous delete */
      undo->cache_start = start_offset;

      /* label */
      prepend_deleted_text:

      /* create a new cache if needed */
      if (undo->cache == NULL)
        undo->cache = g_string_sized_new (MOUSEPAD_UNDO_BUFFER_SIZE);

      /* prepend removed characters */
      undo->cache = g_string_prepend_len (undo->cache, text, length);
    }
  else
    {
      /* label */
      create_new_step:

      /* reset the needle of the steps list */
      mousepad_undo_needle_reset (undo);

      /* put the cache in a new step */
      mousepad_undo_cache_to_step (undo);

      /* set the new cache variables */
      undo->cache_start = start_offset;
      undo->cache_end = end_offset;
      undo->cache_action = action;
      undo->cache_is_space = FALSE;
      undo->cache_in_group = (undo->grouping > 0);

      /* prepend deleted text */
      if (action == DELETE)
        goto prepend_deleted_text;
    }

  /* increase the grouping counter */
  undo->grouping++;

  /* emit signals */
  mousepad_undo_emit_signals (undo);
}



static void
mousepad_undo_buffer_insert (GtkTextBuffer *buffer,
                             GtkTextIter   *pos,
                             const gchar   *text,
                             gint           length,
                             MousepadUndo  *undo)
{
  gint start_pos, end_pos;

  mousepad_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  mousepad_return_if_fail (buffer == undo->buffer);

  /* leave when locked */
  if (G_LIKELY (undo->locked == 0))
    {
      /* buffer positions */
      start_pos = gtk_text_iter_get_offset (pos);
      end_pos = start_pos + length;

      /* handle the change */
      mousepad_undo_buffer_changed (undo, INSERT, text,
                                    length, start_pos, end_pos);
    }
}



static void
mousepad_undo_buffer_delete (GtkTextBuffer *buffer,
                             GtkTextIter   *start_iter,
                             GtkTextIter   *end_iter,
                             MousepadUndo  *undo)
{
  gchar *text;
  gint   start_pos, end_pos;

  mousepad_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  mousepad_return_if_fail (buffer == undo->buffer);

  /* no nothing when locked */
  if (G_LIKELY (undo->locked == 0))
    {
      /* get the removed string */
      text = gtk_text_buffer_get_slice (buffer, start_iter, end_iter, FALSE);

      /* buffer positions */
      start_pos = gtk_text_iter_get_offset (start_iter);
      end_pos = gtk_text_iter_get_offset (end_iter);

      /* handle the change */
      mousepad_undo_buffer_changed (undo, DELETE, text,
                                    ABS (start_pos - end_pos),
                                    start_pos, end_pos);

      /* cleanup */
      g_free (text);
    }
}



static void
mousepad_undo_buffer_begin_user_action (GtkTextBuffer *buffer,
                                        MousepadUndo  *undo)
{
  mousepad_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  mousepad_return_if_fail (buffer == undo->buffer);

  /* only reset the group counter when not locked */
  if (G_LIKELY (undo->locked == 0))
    {
      /* reset the grouping counter */
      undo->grouping = 0;
    }
}



MousepadUndo *
mousepad_undo_new (GtkTextBuffer *buffer)
{
  MousepadUndo *undo;

  mousepad_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);

  /* create the undo object */
  undo = g_object_new (MOUSEPAD_TYPE_UNDO, NULL);

  /* set the buffer with a reference */
  undo->buffer = g_object_ref (G_OBJECT (buffer));

  /* connect signals to the buffer so we can monitor it */
  g_signal_connect (G_OBJECT (buffer), "insert-text", G_CALLBACK (mousepad_undo_buffer_insert), undo);
  g_signal_connect (G_OBJECT (buffer), "delete-range", G_CALLBACK (mousepad_undo_buffer_delete), undo);
  g_signal_connect (G_OBJECT (buffer), "begin-user-action", G_CALLBACK (mousepad_undo_buffer_begin_user_action), undo);

  return undo;
}



void
mousepad_undo_clear (MousepadUndo *undo)
{
  GList *li;

  mousepad_return_if_fail (MOUSEPAD_IS_UNDO (undo));

  /* lock to avoid updates */
  mousepad_undo_lock (undo);

  /* clear cache string */
  if (G_LIKELY (undo->cache))
    g_string_free (undo->cache, TRUE);

  /* cleanup the undo steps */
  for (li = undo->steps; li != NULL; li = li->next)
    mousepad_undo_step_free (li->data);

  /* free the list */
  g_list_free (undo->steps);

  /* null */
  undo->steps = undo->needle = undo->saved = NULL;
  undo->cache = NULL;
  undo->n_steps = 0;
  mousepad_undo_cache_reset (undo);

  /* release lock */
  mousepad_undo_unlock (undo);
}



void
mousepad_undo_lock (MousepadUndo *undo)
{
  mousepad_return_if_fail (MOUSEPAD_IS_UNDO (undo));

  /* increase the lock count */
  undo->locked++;
}



void
mousepad_undo_unlock (MousepadUndo *undo)
{
  mousepad_return_if_fail (MOUSEPAD_IS_UNDO (undo));
  mousepad_return_if_fail (undo->locked > 0);

  /* decrease the lock count */
  undo->locked--;
}



void
mousepad_undo_save_point (MousepadUndo *undo)
{
  mousepad_return_if_fail (MOUSEPAD_IS_UNDO (undo));

  /* reset the needle */
  mousepad_undo_needle_reset (undo);

  /* make sure the buffer is flushed */
  mousepad_undo_cache_to_step (undo);

  /* store the current needle position */
  undo->saved = undo->needle;
}



gboolean
mousepad_undo_can_undo (MousepadUndo *undo)
{
  mousepad_return_val_if_fail (MOUSEPAD_IS_UNDO (undo), FALSE);

  return undo->can_undo;
}



gboolean
mousepad_undo_can_redo (MousepadUndo *undo)
{
  mousepad_return_val_if_fail (MOUSEPAD_IS_UNDO (undo), FALSE);

  return undo->can_redo;
}



void
mousepad_undo_do_undo (MousepadUndo *undo)
{
  mousepad_return_if_fail (MOUSEPAD_IS_UNDO (undo));
  mousepad_return_if_fail (mousepad_undo_can_undo (undo));

  /* undo the last step */
  mousepad_undo_step (undo, FALSE);
}



void
mousepad_undo_do_redo (MousepadUndo *undo)
{
  mousepad_return_if_fail (MOUSEPAD_IS_UNDO (undo));
  mousepad_return_if_fail (mousepad_undo_can_redo (undo));

  /* redo the last undo-ed step */
  mousepad_undo_step (undo, TRUE);
}
