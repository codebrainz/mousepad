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
#include <mousepad/mousepad-undo.h>


#define DEFAULT_CACHE_SIZE (50)



typedef struct _MousepadUndoCache  MousepadUndoCache;
typedef struct _MousepadUndoStep   MousepadUndoStep;
typedef enum   _MousepadUndoAction MousepadUndoAction;



static void mousepad_undo_class_init         (MousepadUndoClass  *klass);
static void mousepad_undo_init               (MousepadUndo       *undo);
static void mousepad_undo_finalize           (GObject            *object);
static void mousepad_undo_emit_signals       (MousepadUndo       *undo);
static void mousepad_undo_step_free          (MousepadUndoStep   *step,
                                              MousepadUndo       *undo);
static void mousepad_undo_step_perform       (MousepadUndo       *undo,
                                              gboolean            redo);
static void mousepad_undo_cache_to_step      (MousepadUndo       *undo);
static void mousepad_undo_cache_reset_needle (MousepadUndo       *undo);
static void mousepad_undo_cache_update       (MousepadUndo       *undo,
                                              MousepadUndoAction  action,
                                              gint                start,
                                              gint                end,
                                              const gchar        *text);
static void mousepad_undo_buffer_insert      (GtkTextBuffer      *buffer,
                                              GtkTextIter        *pos,
                                              const gchar        *text,
                                              gint                length,
                                              MousepadUndo       *undo);
static void mousepad_undo_buffer_delete      (GtkTextBuffer      *buffer,
                                              GtkTextIter        *start_iter,
                                              GtkTextIter        *end_iter,
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
  INSERT,  /* insert action */
  DELETE,  /* delete action */
};

struct _MousepadUndoCache
{
  /* string to cache inserted or deleted character */
  GString            *string;

  /* current cached action */
  MousepadUndoAction  action;

  /* cache start and end positions */
  gint                start;
  gint                end;

  /* whether the last character was a word breaking character */
  guint               is_space : 1;

  /* whether the changes in the cache are part of a group */
  guint               in_group : 1;
};

struct _MousepadUndo
{
  GObject __parent__;

  /* the text buffer we're monitoring */
  GtkTextBuffer     *buffer;

  /* whether the undo manager is locked */
  gint               locked;

  /* whether we should put multiple changes into one action */
  gint               grouping;

  /* whether we can undo or redo */
  guint              can_undo : 1;
  guint              can_redo : 1;

  /* list containing the steps */
  GList             *steps;

  /* steps list pointer when undoing */
  GList             *needle;

  /* internal cache */
  MousepadUndoCache  cache;
};

struct _MousepadUndoStep
{
  /* step action */
  MousepadUndoAction  action;

  /* pointer to the string or another entry in the list */
  gpointer            data;

  /* start and end positions */
  gint                start;
  gint                end;

  /* whether this step is part of a group */
  guint               in_group : 1;
};



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
                  G_SIGNAL_NO_HOOKS,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  undo_signals[CAN_REDO] =
    g_signal_new (I_("can-redo"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_NO_HOOKS,
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
  undo->can_undo = FALSE;
  undo->can_redo = FALSE;
  undo->steps = NULL;
  undo->needle = NULL;

  /* initialize the cache */
  undo->cache.string = NULL;
  undo->cache.start = -1;
  undo->cache.end = -1;
  undo->cache.is_space = FALSE;
  undo->cache.in_group = FALSE;
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
  can_undo = (undo->needle != NULL);
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
mousepad_undo_step_free (MousepadUndoStep *step,
                         MousepadUndo     *undo)
{
  /* remove from the list */
  if (G_LIKELY (undo))
    undo->steps = g_list_remove (undo->steps, step);

  /* free the string */
  g_free (step->data);

  /* free the slice */
  g_slice_free (MousepadUndoStep, step);
}



static void
mousepad_undo_step_perform (MousepadUndo *undo,
                            gboolean      redo)
{
  MousepadUndoStep   *step;
  MousepadUndoAction  action;
  GList              *li;
  GtkTextIter         start_iter, end_iter;

  /* lock for updates */
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

  for (li = undo->needle; li != NULL; li = redo ? li->prev : li->next)
    {
      /* get the step data */
      step = li->data;

      if (G_LIKELY (step))
        {
          /* get the action */
          action = step->action;

          /* invert the action if we redo  */
          if (redo)
            action = (action == INSERT ? DELETE : INSERT);

          /* get the start iter position */
          gtk_text_buffer_get_iter_at_offset (undo->buffer, &start_iter, step->start);

          switch (action)
            {
              case INSERT:
                /* get the end iter */
                gtk_text_buffer_get_iter_at_offset (undo->buffer, &end_iter, step->end);

                /* set the string we're going to remove for redo */
                if (step->data == NULL)
                  step->data = gtk_text_buffer_get_slice (undo->buffer, &start_iter, &end_iter, TRUE);

                /* delete the inserted text */
                gtk_text_buffer_delete (undo->buffer, &start_iter, &end_iter);
                break;

              case DELETE:
                _mousepad_return_if_fail (step->data != NULL);

                /* insert the deleted text */
                gtk_text_buffer_insert (undo->buffer, &start_iter, (gchar *)step->data, -1);
                break;

              default:
                _mousepad_assert_not_reached ();
                break;
            }

          /* set the cursor, we scroll to the cursor in mousepad-document */
          gtk_text_buffer_place_cursor (undo->buffer, &start_iter);
        }

      if (redo)
        {
          /* get the previous step to see if it's part of a group */
          if (g_list_previous (li) != NULL)
            step = g_list_previous (li)->data;
          else
            step = NULL;
        }

      /* as long as the step is part of a group, we continue */
      if (step == NULL || step->in_group == FALSE)
        break;
    }

  /* thawn buffer notifications */
  g_object_thaw_notify (G_OBJECT (undo->buffer));

  /* set the needle element */
  if (redo)
    undo->needle = li;
  else
    undo->needle = g_list_next (li);

  /* emit undo and redo signals */
  mousepad_undo_emit_signals (undo);

  /* release the lock */
  mousepad_undo_unlock (undo);
}



static void
mousepad_undo_cache_to_step (MousepadUndo *undo)
{
  MousepadUndoStep *step;

  /* leave when the cache is empty */
  if (undo->cache.start == undo->cache.end)
    return;

  /* allocate slice */
  step = g_slice_new0 (MousepadUndoStep);

  /* set step data */
  step->action = undo->cache.action;
  step->start = undo->cache.start;
  step->end = undo->cache.end;
  step->in_group = undo->cache.in_group;

  if (step->action == DELETE)
    {
      /* set the step string and allocate a new one */
      step->data = g_string_free (undo->cache.string, FALSE);
      undo->cache.string = NULL;
    }
  else
    {
      /* set the data to null on insert actions */
      step->data = NULL;
    }

  /* prepend the new step */
  undo->steps = g_list_prepend (undo->steps, step);

  /* reset the needle */
  undo->needle = undo->steps;

  /* reset the cache */
  undo->cache.start = undo->cache.end = -1;
  undo->cache.is_space = FALSE;
  undo->cache.in_group = FALSE;
}



static void
mousepad_undo_cache_reset_needle (MousepadUndo *undo)
{
  gint   i;

  /* make sure the needle is the start of the list */
  if (undo->needle != undo->steps)
    {
      /* walk from the start to the needle and remove them */
      for (i = g_list_position (undo->steps, undo->needle); i > 0; i--)
        mousepad_undo_step_free (g_list_first (undo->steps)->data, undo);

      /* check needle */
      _mousepad_return_if_fail (undo->needle == undo->needle);
    }
}



static void
mousepad_undo_cache_update (MousepadUndo       *undo,
                            MousepadUndoAction  action,
                            gint                start,
                            gint                end,
                            const gchar        *text)
{
  gint   length;
  guchar c;

  /* length of the text */
  length = ABS (end - start);

  /* initialize the cache, if not already done */
  if (undo->cache.string == NULL && action == DELETE)
    undo->cache.string = g_string_sized_new (DEFAULT_CACHE_SIZE);

  /* check if we should start a new step before handling this one */
  if (undo->grouping == 0)
    {
      if (undo->cache.in_group == TRUE)
        {
          /* force a new step if the new step is not part of a group and the
           * content in the cache is */
          goto force_new_step;
        }
      else if (length == 1)
        {
          /* get the character */
          c = g_utf8_get_char (text);

          /* detect if the character is a word breaking char */
          if (g_unichar_isspace (c))
            {
              /* the char is a word breaker. we don't care if the previous
               * one was one too, because we merge multiple spaces/tabs/newlines
               * into one step */
              undo->cache.is_space = TRUE;
            }
          else if (undo->cache.is_space)
            {
              /* the new character is not a work breaking char, but the
               * previous one was, force a new step */
              goto force_new_step;
            }
        }
      else
        {
          /* grouping is not enabled and multiple chars are inseted or
           * deleted, force a new step */
          goto force_new_step;
        }
    }

  /* handle the buffer change and try to append it to the cache */
  if (undo->cache.action == action
      && action == INSERT
      && undo->cache.end == start)
    {
      /* we can append with the previous insert change, update end postion */
      undo->cache.end = end;
    }
  else if (undo->cache.action == action
           && action == DELETE
           && undo->cache.start == end)
    {
      /* we can append with the previous delete change, update */
      undo->cache.start = start;

      /* prepend removed characters */
      undo->cache.string = g_string_prepend_len (undo->cache.string, text, length);
    }
  else
    {
      /* label */
      force_new_step:

      /* reset the needle */
      mousepad_undo_cache_reset_needle (undo);

      /* we cannot cache with the previous change, put the cache into a step */
      mousepad_undo_cache_to_step (undo);

      /* start a new cache */
      undo->cache.action = action;
      undo->cache.start = start;
      undo->cache.end = end;
      undo->cache.in_group = (undo->grouping > 0);

      if (action == DELETE)
        {
          undo->cache.string = g_string_sized_new (DEFAULT_CACHE_SIZE);
          undo->cache.string = g_string_prepend_len (undo->cache.string, text, length);
        }
    }

  /* increase the grouping counter */
  undo->grouping++;

  /* stuff has been added to the cache, so we can undo now */
  if (undo->can_undo != TRUE)
    {
      undo->can_undo = TRUE;

      /* emit the can-undo signal */
      g_signal_emit (G_OBJECT (undo), undo_signals[CAN_UNDO], 0, undo->can_undo);
    }
}



static void
mousepad_undo_buffer_insert (GtkTextBuffer *buffer,
                             GtkTextIter   *pos,
                             const gchar   *text,
                             gint           length,
                             MousepadUndo  *undo)
{
  gint start, end;

  _mousepad_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  _mousepad_return_if_fail (buffer == undo->buffer);

  /* leave when locked */
  if (G_UNLIKELY (undo->locked > 0))
    return;

  /* buffer positions */
  start = gtk_text_iter_get_offset (pos);
  end = start + length;

  /* update the cache */
  mousepad_undo_cache_update (undo, INSERT, start, end, text);
}



static void
mousepad_undo_buffer_delete (GtkTextBuffer *buffer,
                             GtkTextIter   *start_iter,
                             GtkTextIter   *end_iter,
                             MousepadUndo  *undo)
{
  gchar *text;
  gint   start, end;

  _mousepad_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  _mousepad_return_if_fail (buffer == undo->buffer);

  /* leave when locked */
  if (G_UNLIKELY (undo->locked > 0))
    return;

  /* get the removed string */
  text = gtk_text_buffer_get_slice (buffer, start_iter, end_iter, FALSE);

  /* buffer positions */
  start = gtk_text_iter_get_offset (start_iter);
  end = gtk_text_iter_get_offset (end_iter);

  /* update the cache */
  mousepad_undo_cache_update (undo, DELETE, start, end, text);

  /* cleanup */
  g_free (text);
}

static void
mousepad_undo_buffer_begin_user_action (GtkTextBuffer *buffer,
                                        MousepadUndo  *undo)
{
  _mousepad_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  _mousepad_return_if_fail (buffer == undo->buffer);

  /* leave when locked */
  if (G_UNLIKELY (undo->locked > 0))
    return;

  /* reset the grouping couter */
  undo->grouping = 0;
}


MousepadUndo *
mousepad_undo_new (GtkTextBuffer *buffer)
{
  MousepadUndo *undo;

  _mousepad_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);

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

  _mousepad_return_if_fail (MOUSEPAD_IS_UNDO (undo));

  /* lock to avoid updates */
  mousepad_undo_lock (undo);

  /* clear cache string */
  if (G_LIKELY (undo->cache.string))
    g_string_free (undo->cache.string, TRUE);

  /* cleanup the undo steps */
  for (li = undo->steps; li != NULL; li = li->next)
    mousepad_undo_step_free (li->data, undo);

  /* free the list */
  g_list_free (undo->steps);

  /* null */
  undo->steps = undo->needle = NULL;
  undo->cache.string = NULL;

  /* release lock */
  mousepad_undo_unlock (undo);
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
mousepad_undo_do_undo (MousepadUndo *undo)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_UNDO (undo));
  _mousepad_return_if_fail (mousepad_undo_can_undo (undo));

  /* undo the last step */
  mousepad_undo_step_perform (undo, FALSE);
}



void
mousepad_undo_do_redo (MousepadUndo *undo)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_UNDO (undo));
  _mousepad_return_if_fail (mousepad_undo_can_redo (undo));

  /* redo the last undo-ed step */
  mousepad_undo_step_perform (undo, TRUE);
}



void
mousepad_undo_populate_popup (GtkTextView  *textview,
                              GtkMenu      *menu,
                              MousepadUndo *undo)
{
  GtkWidget *item;
  gboolean   editable;

  _mousepad_return_if_fail (MOUSEPAD_IS_UNDO (undo));
  _mousepad_return_if_fail (GTK_IS_TEXT_VIEW (textview));
  _mousepad_return_if_fail (GTK_IS_MENU (menu));

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
