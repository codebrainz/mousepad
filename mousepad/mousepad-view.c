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

#include <gdk/gdkkeysyms.h>

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-util.h>
#include <mousepad/mousepad-view.h>



#define mousepad_view_get_buffer(view) (GTK_TEXT_VIEW (view)->buffer)



static void      mousepad_view_class_init                    (MousepadViewClass  *klass);
static void      mousepad_view_init                          (MousepadView       *view);
static void      mousepad_view_finalize                      (GObject            *object);
static void      mousepad_view_style_set                     (GtkWidget          *widget,
                                                              GtkStyle           *previous_style);
static gint      mousepad_view_expose                        (GtkWidget          *widget,
                                                              GdkEventExpose     *event);
static gboolean  mousepad_view_key_press_event               (GtkWidget          *widget,
                                                              GdkEventKey        *event);
static gboolean  mousepad_view_button_press_event            (GtkWidget          *widget,
                                                              GdkEventButton     *event);
static gboolean  mousepad_view_button_release_event          (GtkWidget          *widget,
                                                              GdkEventButton     *event);
static gboolean  mousepad_view_selection_word_range          (const GtkTextIter  *iter,
                                                              GtkTextIter        *range_start,
                                                              GtkTextIter        *range_end);
static void      mousepad_view_selection_key_press_event     (MousepadView       *view,
                                                              GdkEventKey        *event,
                                                              guint               modifiers);
static void      mousepad_view_selection_delete_content      (MousepadView       *view);
static void      mousepad_view_selection_destroy             (MousepadView       *view);
static void      mousepad_view_selection_cleanup_equal_marks (MousepadView       *view);
static void      mousepad_view_selection_add_marks           (MousepadView       *view,
                                                              GtkTextIter        *start_iter,
                                                              GtkTextIter        *end_iter);
static void      mousepad_view_selection_draw                (MousepadView       *view,
                                                              gboolean            draw_cursors,
                                                              gboolean            add_marks);
static gboolean  mousepad_view_selection_timeout             (gpointer            user_data);
static void      mousepad_view_selection_timeout_destroy     (gpointer            user_data);
static void      mousepad_view_indent_increase               (MousepadView       *view,
                                                              GtkTextIter        *iter);
static void      mousepad_view_indent_decrease               (MousepadView       *view,
                                                              GtkTextIter        *iter);
static void      mousepad_view_indent_selection              (MousepadView       *view,
                                                              gboolean            increase);
static gchar    *mousepad_view_indent_string                 (GtkTextBuffer      *buffer,
                                                              const GtkTextIter  *iter);
static gint      mousepad_view_calculate_layout_width        (GtkWidget          *widget,
                                                              gsize               length,
                                                              gchar               fill_char);
static void      mousepad_view_transpose_multi_selection     (GtkTextBuffer       *buffer,
                                                              MousepadView        *view);
static void      mousepad_view_transpose_range               (GtkTextBuffer       *buffer,
                                                              GtkTextIter         *start_iter,
                                                              GtkTextIter         *end_iter);
static void      mousepad_view_transpose_lines               (GtkTextBuffer       *buffer,
                                                              GtkTextIter         *start_iter,
                                                              GtkTextIter         *end_iter);
static void      mousepad_view_transpose_words               (GtkTextBuffer       *buffer,
                                                              GtkTextIter         *iter);



enum _MousepadViewFlags
{
  HAS_SELECTION = 1 << 0, /* if there are marks in the selection list */
  EDITING_MODE  = 1 << 1, /* users can type in the selection */
  HAS_CONTENT   = 1 << 2, /* selection contains content */
  DRAGGING      = 1 << 3, /* still dragging */
  MULTIPLE      = 1 << 4  /* whether this is an multiple selection */
};

struct _MousepadViewClass
{
  GtkTextViewClass __parent__;
};

struct _MousepadView
{
  GtkTextView  __parent__;

  /* current status */
  MousepadViewFlags   flags;

  /* the selection style tag */
  GtkTextTag         *selection_tag;

  /* list with all the selection marks */
  GSList             *marks;

  /* selection timeout id */
  guint               selection_timeout_id;

  /* coordinates for the selection */
  gint                selection_start_x;
  gint                selection_start_y;
  gint                selection_end_x;
  gint                selection_end_y;

  /* length of the selection */
  gint                selection_length;

  /* settings */
  guint               auto_indent : 1;
  guint               line_numbers : 1;
  guint               insert_spaces : 1;
  guint               tab_size;
};



static GObjectClass *mousepad_view_parent_class;



GType
mousepad_view_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_type_register_static_simple (GTK_TYPE_TEXT_VIEW,
                                            I_("MousepadView"),
                                            sizeof (MousepadViewClass),
                                            (GClassInitFunc) mousepad_view_class_init,
                                            sizeof (MousepadView),
                                            (GInstanceInitFunc) mousepad_view_init,
                                            0);
    }

  return type;
}



static void
mousepad_view_class_init (MousepadViewClass *klass)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;

  mousepad_view_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_view_finalize;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->expose_event         = mousepad_view_expose;
  widget_class->style_set            = mousepad_view_style_set;
  widget_class->key_press_event      = mousepad_view_key_press_event;
  widget_class->button_press_event   = mousepad_view_button_press_event;
  widget_class->button_release_event = mousepad_view_button_release_event;
}



static void
mousepad_view_init (MousepadView *view)
{
  /* initialize settings */
  view->auto_indent = FALSE;
  view->line_numbers = FALSE;
  view->insert_spaces = FALSE;
  view->tab_size = 8;

  /* initialize selection variables */
  view->selection_timeout_id = 0;
  view->flags = 0;
  view->selection_tag = NULL;
  view->marks = NULL;
  view->selection_length = 0;
}



static void
mousepad_view_finalize (GObject *object)
{
  MousepadView *view = MOUSEPAD_VIEW (object);

  /* stop a running selection timeout */
  if (G_UNLIKELY (view->selection_timeout_id != 0))
    g_source_remove (view->selection_timeout_id);

  /* free the selection marks list */
  if (G_UNLIKELY (view->marks != NULL))
    g_slist_free (view->marks);

  (*G_OBJECT_CLASS (mousepad_view_parent_class)->finalize) (object);
}



static gboolean
mousepad_view_expose (GtkWidget      *widget,
                      GdkEventExpose *event)
{
  GtkTextView  *textview = GTK_TEXT_VIEW (widget);
  MousepadView *view = MOUSEPAD_VIEW (widget);
  gint          y_top, y_offset, y_bottom;
  gint          y, height;
  gint          line_number = 0, line_count;
  GtkTextIter   iter;
  gint          width;
  PangoLayout  *layout;
  gchar         str[8]; /* maximum of 10e6 lines */

  if (view->flags != 0
      && MOUSEPAD_HAS_FLAG (view->flags, HAS_CONTENT) == FALSE
      && event->window == gtk_text_view_get_window (textview, GTK_TEXT_WINDOW_TEXT))
    {
      /* redraw the retangles for the vertical selection */
      mousepad_view_selection_draw (view, TRUE, FALSE);
    }
  else if (event->window == gtk_text_view_get_window (textview, GTK_TEXT_WINDOW_LEFT))
    {
      /* get the real start position */
      gtk_text_view_window_to_buffer_coords (textview, GTK_TEXT_WINDOW_LEFT,
                                             0, event->area.y, NULL, &y_top);

      /* get the bottom position */
      y_bottom = y_top + event->area.height;

      /* get the buffer offset (negative value or 0) */
      y_offset = event->area.y - y_top;

      /* get the start iter */
      gtk_text_view_get_line_at_y (textview, &iter, y_top, NULL);

      /* get the number of lines in the buffer */
      line_count = gtk_text_buffer_get_line_count (textview->buffer);

      /* string with the 'last' line number */
      g_snprintf (str, sizeof (str), "%d", MAX (99, line_count));

      /* create the pango layout */
      layout = gtk_widget_create_pango_layout (widget, str);
      pango_layout_get_pixel_size (layout, &width, NULL);
      pango_layout_set_width (layout, width);
      pango_layout_set_alignment (layout, PANGO_ALIGN_RIGHT);

      /* set the width of the left window border */
      gtk_text_view_set_border_window_size (textview, GTK_TEXT_WINDOW_LEFT, width + 8);

      /* draw a vertical line to separate the numbers and text */
      gtk_paint_vline (widget->style, event->window,
                       GTK_WIDGET_STATE (widget),
                       NULL, widget, NULL,
                       event->area.y,
                       event->area.y + event->area.height,
                       width + 6);

      /* walk through the lines until we hit the last line */
      while (line_number < line_count)
        {
          /* get the y position and the height of the iter */
          gtk_text_view_get_line_yrange (textview, &iter, &y, &height);

          /* include the buffer offset */
          y += y_offset;

          /* get the number of the line */
          line_number = gtk_text_iter_get_line (&iter) + 1;

          /* create the number */
          g_snprintf (str, sizeof (str), "%d", line_number);

          /* create the pange layout */
          pango_layout_set_text (layout, str, strlen (str));

          /* draw the layout on the left window */
          gtk_paint_layout (widget->style, event->window,
                            GTK_WIDGET_STATE (widget),
                            FALSE, NULL, widget, NULL,
                            width + 2, y, layout);

          /* stop when we reached the end of the expose area */
          if ((y + height) >= y_bottom)
            break;

          /* jump to the next line */
          gtk_text_iter_forward_line (&iter);
        }

      /* release the pango layout */
      g_object_unref (G_OBJECT (layout));
    }

  /* Gtk can draw the text now */
  return (*GTK_WIDGET_CLASS (mousepad_view_parent_class)->expose_event) (widget, event);
}



static void
mousepad_view_style_set (GtkWidget *widget,
                         GtkStyle  *previous_style)
{
  GtkStyle      *style;
  GtkTextIter    start, end;
  MousepadView  *view = MOUSEPAD_VIEW (widget);
  GtkTextBuffer *buffer;

  /* run widget handler */
  (*GTK_WIDGET_CLASS (mousepad_view_parent_class)->style_set) (widget, previous_style);

  /* update the style after a real style change */
  if (previous_style)
    {
      /* get the textview style */
      style = gtk_widget_get_style (widget);

      /* get the text buffer */
      buffer = mousepad_view_get_buffer (view);

      /* remove previous selection tag */
      if (G_UNLIKELY (view->selection_tag))
        {
          gtk_text_buffer_get_bounds (buffer, &start, &end);
          gtk_text_buffer_remove_tag (buffer, view->selection_tag, &start, &end);
        }

      /* create the new selection tag */
      view->selection_tag = gtk_text_buffer_create_tag (buffer, NULL,
                                                        "background-gdk", &style->base[GTK_STATE_SELECTED],
                                                        "foreground-gdk", &style->text[GTK_STATE_SELECTED],
                                                        NULL);

      /* update the tab size */
      mousepad_view_set_tab_size (view, view->tab_size);

      /* redraw selection */
      if (view->flags != 0)
        mousepad_view_selection_draw (view, !MOUSEPAD_HAS_FLAG (view->flags, HAS_CONTENT), FALSE);
    }
}



static gboolean
mousepad_view_key_press_event (GtkWidget   *widget,
                               GdkEventKey *event)
{
  MousepadView  *view = MOUSEPAD_VIEW (widget);
  GtkTextBuffer *buffer;
  GtkTextIter    iter;
  GtkTextMark   *cursor;
  guint          modifiers;
  gchar         *string;
  gboolean       im_handled;
  gboolean       is_editable;

  /* get the modifiers state */
  modifiers = event->state & gtk_accelerator_get_default_mod_mask ();

  /* get the textview buffer */
  buffer = mousepad_view_get_buffer (view);

  /* whether the textview is editable */
  is_editable = GTK_TEXT_VIEW (view)->editable;

  /* handle the key event */
  switch (event->keyval)
    {
      case GDK_Return:
      case GDK_KP_Enter:
        if (!(event->state & GDK_SHIFT_MASK) && view->auto_indent && is_editable)
          {
            /* get the iter position of the cursor */
            cursor = gtk_text_buffer_get_insert (buffer);
            gtk_text_buffer_get_iter_at_mark (buffer, &iter, cursor);

            /* get the string of tabs and spaces we're going to indent */
            string = mousepad_view_indent_string (buffer, &iter);

            if (string != NULL)
              {
                /* check if the input method emitted this event */
                im_handled = gtk_im_context_filter_keypress (GTK_TEXT_VIEW (view)->im_context, event);

                /* check if we're allowed to handle this event */
                if (G_LIKELY (im_handled == FALSE))
                  {
                    /* begin a user action */
                    gtk_text_buffer_begin_user_action (buffer);

                    /* insert the indent characters */
                    gtk_text_buffer_insert (buffer, &iter, "\n", 1);
                    gtk_text_buffer_insert (buffer, &iter, string, -1);

                    /* end user action */
                    gtk_text_buffer_end_user_action (buffer);

                    /* make sure the new string is visible for the user */
                    mousepad_view_put_cursor_on_screen (view);
                  }

                /* cleanup */
                g_free (string);

                /* return */
                return (im_handled == FALSE);
              }
          }
        break;

      case GDK_End:
      case GDK_KP_End:
        if (modifiers & GDK_CONTROL_MASK)
          {
            /* get the end iter */
            gtk_text_buffer_get_end_iter (buffer, &iter);

            /* get the cursor mark */
            cursor = gtk_text_buffer_get_insert (buffer);

            goto move_cursor;
          }
        break;

      case GDK_Home:
      case GDK_KP_Home:
        /* get the cursor mark */
        cursor = gtk_text_buffer_get_insert (buffer);

        /* when control is pressed, we jump to the start of the document */
        if (modifiers & GDK_CONTROL_MASK)
          {
            /* get the start iter */
            gtk_text_buffer_get_start_iter (buffer, &iter);

            goto move_cursor;
          }

        /* get the iter position of the cursor */
        gtk_text_buffer_get_iter_at_mark (buffer, &iter, cursor);

        /* if the cursor starts a line, try to move it in front of the text */
        if (gtk_text_iter_starts_line (&iter)
            && mousepad_util_forward_iter_to_text (&iter, NULL))
          {
             /* label for the ctrl home/end events */
             move_cursor:

             /* move (select) or set (jump) cursor */
             if (modifiers & GDK_SHIFT_MASK)
               gtk_text_buffer_move_mark (buffer, cursor, &iter);
             else
               gtk_text_buffer_place_cursor (buffer, &iter);

             /* make sure the cursor is visible for the user */
             mousepad_view_put_cursor_on_screen (view);

             return TRUE;
          }
        break;

      case GDK_Delete:
      case GDK_KP_Delete:
      case GDK_BackSpace:
        if (view->flags != 0)
          {
            goto selection_key_press;
          }
        break;

      case GDK_Tab:
      case GDK_KP_Tab:
      case GDK_ISO_Left_Tab:
        if (G_LIKELY (is_editable))
          {
            if (view->flags != 0)
              {
                goto selection_key_press;
              }
            else if (gtk_text_buffer_get_selection_bounds (buffer, NULL, NULL))
              {
                /* indent the selection */
                mousepad_view_indent_selection (view, (modifiers & GDK_SHIFT_MASK));

                return TRUE;
              }
            else if (view->insert_spaces)
              {
                /* get the iter position of the cursor */
                cursor = gtk_text_buffer_get_insert (buffer);
                gtk_text_buffer_get_iter_at_mark (buffer, &iter, cursor);

                /* begin user action */
                gtk_text_buffer_begin_user_action (buffer);

                /* insert spaces */
                mousepad_view_indent_increase (view, &iter);

                /* end user action */
                gtk_text_buffer_end_user_action (buffer);

                return TRUE;
              }
          }
        break;

      default:
        if (view->flags != 0 && event->length > 0)
          {
            /* label for tab and backspace */
            selection_key_press:

            /* handle a key press event for a selection */
            mousepad_view_selection_key_press_event (view, event, modifiers);

            return TRUE;
          }
        break;
    }

  /* remove the selection when no valid key combination has been pressed */
  if (G_UNLIKELY (view->flags != 0 && event->is_modifier == FALSE))
    mousepad_view_selection_destroy (view);

  return (*GTK_WIDGET_CLASS (mousepad_view_parent_class)->key_press_event) (widget, event);
}



static gboolean
mousepad_view_button_press_event (GtkWidget      *widget,
                                  GdkEventButton *event)
{
  MousepadView  *view = MOUSEPAD_VIEW (widget);
  GtkTextView   *textview = GTK_TEXT_VIEW (widget);
  GtkTextIter    iter, start_iter, end_iter;
  GtkTextBuffer *buffer;

  /* work with vertical selection while ctrl is pressed */
  if (event->state & GDK_CONTROL_MASK
      && event->x >= 0
      && event->y >= 0
      && event->button == 1
      && event->type == GDK_BUTTON_PRESS)
    {
      /* set the vertical selection start position, inclusing textview offset */
      view->selection_start_x = event->x + textview->xoffset;
      view->selection_start_y = event->y + textview->yoffset;

      /* whether this is going to be a multiple selection */
      if (view->flags != 0)
        MOUSEPAD_SET_FLAG (view->flags, MULTIPLE);

      /* enable dragging */
      MOUSEPAD_SET_FLAG (view->flags, DRAGGING);

      /* start a vertical selection timeout */
      view->selection_timeout_id = g_timeout_add_full (G_PRIORITY_LOW, 50, mousepad_view_selection_timeout,
                                                       view, mousepad_view_selection_timeout_destroy);

      return TRUE;
    }
  else if (event->type == GDK_2BUTTON_PRESS) /* todo, button 1 en pos x coords */
    {
      /* get the iter under the cursor */
      gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &iter,
                                          event->x + textview->xoffset, event->y + textview->yoffset);

      /* try to select a whole word or space */
      if (mousepad_view_selection_word_range (&iter, &start_iter, &end_iter))
        {
          /* get buffer */
          buffer = mousepad_view_get_buffer (view);

          /* add to normal selection or multiple selection */
          if (event->state & GDK_CONTROL_MASK)
            {
              /* make it a multiple selection when there is already something selected */
              if (view->flags != 0)
                MOUSEPAD_SET_FLAG (view->flags, MULTIPLE);

              /* add to the list */
              mousepad_view_selection_add_marks (view, &start_iter, &end_iter);

              /* redraw */
              mousepad_view_selection_draw (view, FALSE, FALSE);

              /* set the cursor at the end of the word */
              gtk_text_buffer_place_cursor (buffer, &end_iter);
            }
          else
            {
              /* select range */
              gtk_text_buffer_select_range (buffer, &start_iter, &end_iter);
            }
        }

      return TRUE;
    }
  else if (view->flags != 0 && event->button != 3)
    {
      /* no drag started or continued */
      mousepad_view_selection_destroy (view);
    }

  return (*GTK_WIDGET_CLASS (mousepad_view_parent_class)->button_press_event) (widget, event);
}



static gboolean
mousepad_view_button_release_event (GtkWidget      *widget,
                                    GdkEventButton *event)
{
  MousepadView  *view = MOUSEPAD_VIEW (widget);
  gboolean       has_content;

  /* end of a vertical selection */
  if (G_UNLIKELY (view->selection_timeout_id != 0))
    {
      /* stop the timeout */
      g_source_remove (view->selection_timeout_id);

      /* check if the selection has any content */
      has_content = MOUSEPAD_HAS_FLAG (view->flags, HAS_CONTENT);

      /* prepend the selected marks */
      mousepad_view_selection_draw (view, !has_content, TRUE);

      /* remove the dragging flag */
      MOUSEPAD_UNSET_FLAG (view->flags, DRAGGING);
    }

  return (*GTK_WIDGET_CLASS (mousepad_view_parent_class)->button_release_event) (widget, event);
}



/**
 * Selection Functions
 **/
static gboolean
mousepad_view_selection_word_range (const GtkTextIter *iter,
                                    GtkTextIter       *range_start,
                                    GtkTextIter       *range_end)
{
  /* init iters */
  *range_start = *range_end = *iter;

  /* get the iter character */
  if (!mousepad_util_iter_inside_word (iter))
    {
      /* walk back and forward to select as much spaces as possible */
      if (!mousepad_util_iter_forward_text_start (range_end))
        return FALSE;

      if (!mousepad_util_iter_backward_text_start (range_start))
        return FALSE;
    }
  else
    {
      /* return false when we could not find a word start */
      if (!mousepad_util_iter_backward_word_start (range_start))
        return FALSE;

      /* return false when we could not find a word end */
      if (!mousepad_util_iter_forward_word_end (range_end))
        return FALSE;
    }

  /* sort iters */
  gtk_text_iter_order (range_start, range_end);

  /* when the iters are not equal and the origional iter is in the range, we succeeded */
  return (!gtk_text_iter_equal (range_start, range_end)
          && gtk_text_iter_compare (iter, range_start) >= 0
          && gtk_text_iter_compare (iter, range_end) <= 0);
}



static void
mousepad_view_selection_key_press_event (MousepadView *view,
                                         GdkEventKey  *event,
                                         guint         modifiers)
{
  GtkTextIter    start, end;
  GSList        *li;
  GtkTextBuffer *buffer;
  gboolean       has_content = FALSE;

  _mousepad_return_if_fail (view->marks != NULL);

  /* get buffer */
  buffer = mousepad_view_get_buffer (view);

  /* begin user action */
  gtk_text_buffer_begin_user_action (buffer);

  /* delete the content when the selection has content, we're not
   * in editing more or the delete key is pressed */
  if (MOUSEPAD_HAS_FLAG (view->flags, HAS_CONTENT))
    {
      if (!MOUSEPAD_HAS_FLAG (view->flags, EDITING_MODE)
         || (event->keyval == GDK_Delete || event->keyval == GDK_KP_Delete))
        {
          mousepad_view_selection_delete_content (view);
        }
    }

  /* enter editing mode */
  MOUSEPAD_SET_FLAG (view->flags, EDITING_MODE);

  for (li = view->marks; li != NULL; li = li->next)
    {
      /* get end iter */
      gtk_text_buffer_get_iter_at_mark (buffer, &end, li->next->data);

      /* handle the events */
      switch (event->keyval)
        {
          case GDK_Tab:
          case GDK_KP_Tab:
          case GDK_ISO_Left_Tab:
            /* insert a (soft) tab */
            mousepad_view_indent_increase (view, &end);
            break;

          case GDK_BackSpace:
            /* get start iter */
            gtk_text_buffer_get_iter_at_mark (buffer, &start, li->data);

            /* only backspace when there is text inside the selection or ctrl is pressed */
            if (!gtk_text_iter_equal (&start, &end) || modifiers == GDK_CONTROL_MASK)
              {
                gtk_text_buffer_backspace (buffer, &end, FALSE, TRUE);
              }
            break;

          default:
            /* insert the typed string */
            gtk_text_buffer_insert (buffer, &end, event->string, event->length);
            break;
        }

      /* get the start iter */
      gtk_text_buffer_get_iter_at_mark (buffer, &start, li->data);

      if (!gtk_text_iter_equal (&start, &end))
        {
          /* select the text */
          gtk_text_buffer_apply_tag (buffer, view->selection_tag, &start, &end);

          /* we have some selected text */
          has_content = TRUE;
        }

      /* go to next element in the list */
      li = li->next;
    }

  /* TODO content */

  /* end user action */
  gtk_text_buffer_end_user_action (buffer);
}



static void
mousepad_view_selection_delete_content (MousepadView *view)
{
  GtkTextBuffer *buffer;
  GtkTextIter    start_iter, end_iter;
  GSList        *li;

  _mousepad_return_if_fail (view->marks != NULL);
  _mousepad_return_if_fail (MOUSEPAD_HAS_FLAG (view->flags, HAS_CONTENT));
  _mousepad_return_if_fail (view->marks == NULL || g_slist_length (view->marks) % 2 == 0);

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* begin user action */
  gtk_text_buffer_begin_user_action (buffer);

  /* remove everything between the marks */
  for (li = view->marks; li != NULL; li = li->next)
    {
      /* get end iter */
      gtk_text_buffer_get_iter_at_mark (buffer, &start_iter, li->data);

      /* next element */
      li = li->next;

      /* get start iter */
      gtk_text_buffer_get_iter_at_mark (buffer, &end_iter, li->data);

      /* delete */
      gtk_text_buffer_delete (buffer, &start_iter, &end_iter);
    }

  /* set the selection length to zerro */
  view->selection_length = 0;

  /* end user action */
  gtk_text_buffer_end_user_action (buffer);
}



static void
mousepad_view_selection_destroy (MousepadView *view)
{
  GtkTextBuffer *buffer;
  GSList        *li;
  GtkTextIter    start_iter, end_iter;

  _mousepad_return_if_fail (view->marks != NULL);
  _mousepad_return_if_fail (view->flags != 0);

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* remove the selection tags */
  gtk_text_buffer_get_bounds (buffer, &start_iter, &end_iter);
  gtk_text_buffer_remove_tag (buffer, view->selection_tag, &start_iter, &end_iter);

  /* remove all the selection marks */
  if (G_LIKELY (view->marks))
    {
      /* delete marks from the buffer */
      for (li = view->marks; li != NULL; li = li->next)
        gtk_text_buffer_delete_mark (buffer, li->data);

      /* free the list */
      g_slist_free (view->marks);

      /* null */
      view->marks = NULL;
    }

  /* reset status */
  view->flags = 0;

  /* set selection length to zerro */
  view->selection_length = 0;
}



static void
mousepad_view_selection_cleanup_equal_marks (MousepadView *view)
{
  GtkTextBuffer *buffer;
  GtkTextIter    start_iter, end_iter;
  GSList        *li, *lnext;

  _mousepad_return_if_fail (view->marks == NULL || g_slist_length (view->marks) % 2 == 0);

  /* get buffer */
  buffer = mousepad_view_get_buffer (view);

  for (li = view->marks; li != NULL; li = lnext)
    {
      /* get iters */
      gtk_text_buffer_get_iter_at_mark (buffer, &start_iter, li->data);
      gtk_text_buffer_get_iter_at_mark (buffer, &end_iter, li->next->data);

      /* get next element */
      lnext = li->next->next;

      if (gtk_text_iter_equal (&start_iter, &end_iter))
        {
          gtk_text_buffer_delete_mark (buffer, li->next->data);
          view->marks = g_slist_delete_link (view->marks, li->next);

          gtk_text_buffer_delete_mark (buffer, li->data);
          view->marks = g_slist_delete_link (view->marks, li);
        }
    }
}



static void
mousepad_view_selection_add_marks (MousepadView *view,
                                   GtkTextIter  *start_iter,
                                   GtkTextIter  *end_iter)
{
  GtkTextBuffer *buffer;
  GtkTextIter    iter;
  GtkTextMark   *start_mark, *end_mark;
  GSList        *li, *lnext;
  gboolean       is_start = FALSE;
  gboolean       handled_start = FALSE;
  gint           compare, length;

  _mousepad_return_if_fail (view->marks == NULL || g_slist_length (view->marks) % 2 == 0);

  /* make sure the iters are correctly sorted */
  gtk_text_iter_order (start_iter, end_iter);

  /* get buffer */
  buffer = mousepad_view_get_buffer (view);

  /* calculate length of selection */
  length = gtk_text_iter_get_line_offset (end_iter) - gtk_text_iter_get_line_offset (start_iter);

  /* add to selection length */
  view->selection_length += length;

  /* create marks */
  start_mark = gtk_text_buffer_create_mark (buffer, NULL, start_iter, TRUE);
  end_mark = gtk_text_buffer_create_mark (buffer, NULL, end_iter, FALSE);

  for (li = view->marks; li != NULL; li = lnext)
    {
      /* type of mark */
      is_start = !is_start;

      /* determine next element in the list */
      lnext = li->next;

      /* get the iter of a mark in the list */
      gtk_text_buffer_get_iter_at_mark (buffer, &iter, li->data);

      /* search the insert position of the start mark */
      if (!handled_start)
        {
          /* compare iters */
          compare = gtk_text_iter_compare (&iter, start_iter);

          if (compare >= 0)
            {
              if (compare == 0 && gtk_text_iter_equal (start_iter, end_iter))
                {
                  /* zerro width insert on top of an existing mark, skip those */
                  gtk_text_buffer_delete_mark (buffer, start_mark);
                  gtk_text_buffer_delete_mark (buffer, end_mark);

                  break;
                }
              else if (is_start)
                {
                  /* inserted before a start mark */
                  view->marks = g_slist_insert_before (view->marks, li, start_mark);
                }
              else
                {
                  /* inserted after a start mark, remove the new one */
                  gtk_text_buffer_delete_mark (buffer, start_mark);
                }

              /* we've handled the start mark */
              handled_start = TRUE;
            }
        }

      /* handle the end mark */
      if (handled_start)
        {
          /* check if the iter can be merged inside our selection */
          if (gtk_text_iter_compare (&iter, end_iter) < 0)
            {
              /* delete the existing mark from the buffer */
              gtk_text_buffer_delete_mark (buffer, li->data);

              /* delete item from the list */
              view->marks = g_slist_delete_link (view->marks, li);
            }
          else
            {
              /* handle the end mark */
              if (is_start)
                view->marks = g_slist_insert_before (view->marks, li, end_mark);
              else
                gtk_text_buffer_delete_mark (buffer, end_mark);

              break;
            }
        }
    }

  /* reached end of list */
  if (li == NULL)
    {
      /* still not start mark added, append it to the list */
      if (!handled_start)
        view->marks = g_slist_append (view->marks, start_mark);

      /* append end mark */
      view->marks = g_slist_append (view->marks, end_mark);
    }
}



static void
mousepad_view_selection_draw (MousepadView *view,
                              gboolean      draw_cursors,
                              gboolean      add_marks)
{
  GtkTextBuffer *buffer;
  GtkTextView   *textview = GTK_TEXT_VIEW (view);
  GtkTextIter    start_iter, end_iter;
  gint           line_x, line_y;
  gint           y, y_begin, y_height, y_end;
  gboolean       has_content = FALSE;
  GdkRectangle   rect;
  GdkWindow     *window;
  GSList        *li;

  _mousepad_return_if_fail (view->marks == NULL || g_slist_length (view->marks) % 2 == 0);

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* get the drawable window */
  window = gtk_text_view_get_window (textview, GTK_TEXT_WINDOW_TEXT);

  if (MOUSEPAD_HAS_FLAG (view->flags, HAS_CONTENT))
    {
      /* remove the old tags */
      gtk_text_buffer_get_bounds (buffer, &start_iter, &end_iter);
      gtk_text_buffer_remove_tag (buffer, view->selection_tag, &start_iter, &end_iter);
    }
  else
    {
      /* invalidate the window so no retangles are left */
      gtk_widget_queue_draw (GTK_WIDGET (view));
    }

  /* if the selection already contains marks, show them first */
  for (li = view->marks; li != NULL; li = li->next)
    {
      /* get the end iter */
      gtk_text_buffer_get_iter_at_mark (buffer, &start_iter, li->data);

      /* next element in the list */
      li = li->next;

      if (draw_cursors)
        {
          /* get the iter location and size */
          gtk_text_view_get_iter_location (textview, &start_iter, &rect);

          /* calculate line coordinates */
          line_x = rect.x - textview->xoffset;
          line_y = rect.y - textview->yoffset;

          /* draw a line in front of the iter */
          gdk_draw_line (GDK_DRAWABLE (window),
                         GTK_WIDGET (view)->style->text_gc[GTK_STATE_NORMAL],
                         line_x, line_y, line_x, line_y + rect.height - 1);
        }
      else
        {
          /* get the start iter */
          gtk_text_buffer_get_iter_at_mark (buffer, &end_iter, li->data);

          /* check if the iters are not equal */
          if (!gtk_text_iter_equal (&start_iter, &end_iter))
            {
              /* apply tag */
              gtk_text_buffer_apply_tag (buffer, view->selection_tag, &start_iter, &end_iter);

              /* this selection has atleast some content */
              has_content = TRUE;
            }
        }
    }

  /* when we're still dragging, show the selection in the area */
  if (MOUSEPAD_HAS_FLAG (view->flags, DRAGGING))
    {
      /* get the start and end iter */
      gtk_text_view_get_iter_at_location (textview, &start_iter, 0, view->selection_start_y);
      gtk_text_view_get_iter_at_location (textview, &end_iter, 0, view->selection_end_y);

      /* make sure the start iter is before the end iter */
      gtk_text_iter_order (&start_iter, &end_iter);

      /* get the y coordinates we're going to walk */
      gtk_text_view_get_line_yrange (textview, &start_iter, &y_begin, &y_height);
      gtk_text_view_get_line_yrange (textview, &end_iter, &y_end, NULL);

      /* make sure atleast one line is selected */
      y_end += y_height;

      /* walk the lines inside the selection area */
      for (y = y_begin; y < y_end; y += y_height)
        {
          /* get the start and end iter in the line */
          gtk_text_view_get_iter_at_location (textview, &start_iter, view->selection_start_x, y);
          gtk_text_view_get_iter_at_location (textview, &end_iter, view->selection_end_x, y);

          if (draw_cursors)
            {
              /* get the iter location and size */
              gtk_text_view_get_iter_location (textview, &end_iter, &rect);

              /* calculate line cooridinates */
              line_x = rect.x - textview->xoffset;
              line_y = rect.y - textview->yoffset;

              /* draw a line in front of the iter */
              gdk_draw_line (GDK_DRAWABLE (window),
                             GTK_WIDGET (view)->style->text_gc[GTK_STATE_NORMAL],
                             line_x, line_y, line_x, line_y + rect.height - 1);
            }
          else if (!gtk_text_iter_equal (&start_iter, &end_iter))
            {
              /* apply tag */
              gtk_text_buffer_apply_tag (buffer, view->selection_tag, &start_iter, &end_iter);

              /* this selection has atleast some content */
              has_content = TRUE;
            }
          else
            {
              /* when both of the conditions above were not met, we'll
               * never added them to the marks list */
              continue;
            }

          /* prepend to list */
          if (add_marks)
            {
              /* add the marks */
              mousepad_view_selection_add_marks (view, &start_iter, &end_iter);
            }
        }

      /* update selection status */
      g_object_notify (G_OBJECT (buffer), "cursor-position");
    }

  /* update status when marks have been added */
  if (add_marks && has_content && MOUSEPAD_HAS_FLAG (view->flags, MULTIPLE))
    mousepad_view_selection_cleanup_equal_marks (view);

  if (has_content)
    MOUSEPAD_SET_FLAG (view->flags, HAS_CONTENT);
  else
    MOUSEPAD_UNSET_FLAG (view->flags, HAS_CONTENT);

  /* there is something in the list, prevent 0 flag */
  if (view->marks != NULL)
    MOUSEPAD_SET_FLAG (view->flags, HAS_SELECTION);


}



static gboolean
mousepad_view_selection_timeout (gpointer user_data)
{
  MousepadView  *view = MOUSEPAD_VIEW (user_data);
  GtkTextView   *textview = GTK_TEXT_VIEW (view);
  gint           pointer_x, pointer_y;
  gint           selection_start_x, selection_start_y;
  GtkTextBuffer *buffer;
  GtkTextIter    iter;

  _mousepad_return_val_if_fail (MOUSEPAD_HAS_FLAG (view->flags, DRAGGING), FALSE);

  GDK_THREADS_ENTER ();

  /* get the cursor coordinate */
  gdk_window_get_pointer (gtk_text_view_get_window (textview, GTK_TEXT_WINDOW_TEXT),
                          &pointer_x, &pointer_y, NULL);

  /* convert to positive values and add buffer offset */
  pointer_x = MAX (pointer_x, 0) + textview->xoffset;
  pointer_y = MAX (pointer_y, 0) + textview->yoffset;

  /* only update the selection when the cursor moved */
  if (view->selection_end_x != pointer_x || view->selection_end_y != pointer_y)
    {
      /* update the end coordinates */
      view->selection_end_x = pointer_x;
      view->selection_end_y = pointer_y;

      /* backup start coordinates */
      selection_start_x = view->selection_start_x;
      selection_start_y = view->selection_start_y;

      /* only draw the visible selection during timeout */
      view->selection_start_x = MAX (view->selection_start_x, textview->xoffset);
      view->selection_start_y = MAX (view->selection_start_y, textview->yoffset);

      /* show the selection */
      mousepad_view_selection_draw (view, FALSE, FALSE);

      /* restore the start coordinates */
      view->selection_start_x = selection_start_x;
      view->selection_start_y = selection_start_y;

      /* get the text buffer */
      buffer = mousepad_view_get_buffer (view);

      /* put the cursor under the mouse pointer */
      gtk_text_view_get_iter_at_location (textview, &iter, pointer_x, pointer_y);
      gtk_text_buffer_place_cursor (buffer, &iter);

      /* put cursor on screen */
      mousepad_view_put_cursor_on_screen (view);
    }

  GDK_THREADS_LEAVE ();

  /* keep the timeout running */
  return TRUE;
}



static void
mousepad_view_selection_timeout_destroy (gpointer user_data)
{
  MOUSEPAD_VIEW (user_data)->selection_timeout_id = 0;
}



static void
mousepad_view_selection_clipboard (MousepadView *view,
                                   GtkClipboard *clipboard)
{
  GString       *string;
  GtkTextBuffer *buffer;
  gint           line, previous_line = -1;
  gchar         *slice;
  GtkTextIter    start_iter, end_iter;
  GSList        *li;

  _mousepad_return_if_fail (view->marks == NULL || g_slist_length (view->marks) % 2 == 0);

  /* create new string */
  string = g_string_new (NULL);

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  for (li = view->marks; li != NULL; li = li->next)
    {
      /* get the iters */
      gtk_text_buffer_get_iter_at_mark (buffer, &start_iter, li->data);
      li = li->next;
      gtk_text_buffer_get_iter_at_mark (buffer, &end_iter, li->data);

      /* get the line number */
      line = gtk_text_iter_get_line (&start_iter);

      /* fix the number of new lines between the parts */
      if (previous_line == -1)
        previous_line = line;
      else if (!MOUSEPAD_HAS_FLAG (view->flags, MULTIPLE))
        for (; previous_line < line; previous_line++)
          string = g_string_append_c (string, '\n');
      else
        string = g_string_append_c (string, '\n');

      /* get the text slice */
      slice = gtk_text_buffer_get_slice (buffer, &start_iter, &end_iter, TRUE);

      /* append the slice to the string */
      string = g_string_append (string, slice);

      /* cleanup */
      g_free (slice);
    }

  /* set the clipboard text */
  gtk_clipboard_set_text (clipboard, string->str, string->len);

  /* free the string */
  g_string_free (string, TRUE);
}



/**
 * Indentation Functions
 **/
static void
mousepad_view_indent_increase (MousepadView *view,
                               GtkTextIter  *iter)
{
  gchar         *string;
  gint           offset, length, inline_len;
  GtkTextBuffer *buffer;

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  if (view->insert_spaces)
    {
      /* get the offset */
      offset = mousepad_util_get_real_line_offset (iter, view->tab_size);

      /* calculate the length to inline with a tab */
      inline_len = offset % view->tab_size;

      if (inline_len == 0)
        length = view->tab_size;
      else
        length = view->tab_size - inline_len;

      /* create spaces string */
      string = g_strnfill (length, ' ');

      /* insert string */
      gtk_text_buffer_insert (buffer, iter, string, length);

      /* cleanup */
      g_free (string);
    }
  else
    {
      /* insert a tab or a space */
      gtk_text_buffer_insert (buffer, iter, "\t", -1);
    }
}



static void
mousepad_view_indent_decrease (MousepadView *view,
                               GtkTextIter  *iter)
{
  GtkTextBuffer *buffer;
  GtkTextIter    start, end;
  gint           columns = view->tab_size;
  gunichar       c;

  /* set iters */
  start = end = *iter;

  /* walk until we've removed enough columns */
  while (columns > 0)
    {
      /* get the character */
      c = gtk_text_iter_get_char (&end);

      if (c == '\t')
        columns -= view->tab_size;
      else if (c == ' ')
        columns--;
      else
        break;

      /* go to the next character */
      gtk_text_iter_forward_char (&end);
    }

  /* unindent the selection when the iters are not equal */
  if (!gtk_text_iter_equal (&start, &end))
    {
      /* get the buffer */
      buffer = mousepad_view_get_buffer (view);

      /* remove the columns */
      gtk_text_buffer_delete (buffer, &start, &end);
    }
}



static void
mousepad_view_indent_selection (MousepadView *view,
                                gboolean      increase)
{
  GtkTextBuffer *buffer;
  GtkTextIter    start_iter, end_iter;
  gint           start_line, end_line;
  gint           i;

  /* get the textview buffer */
  buffer = mousepad_view_get_buffer (view);

  if (gtk_text_buffer_get_selection_bounds (buffer, &start_iter, &end_iter))
    {
      /* begin a user action */
      gtk_text_buffer_begin_user_action (buffer);

      /* get start and end line */
      start_line = gtk_text_iter_get_line (&start_iter);
      end_line = gtk_text_iter_get_line (&end_iter);

      /* only change indentation when an entire line is selected or multiple lines */
      if (start_line != end_line
          || (gtk_text_iter_starts_line (&start_iter) && gtk_text_iter_ends_line (&end_iter)))
        {
          /* change indentation of each line */
          for (i = start_line; i <= end_line; i++)
            {
              /* get the iter of the line we're going to indent */
              gtk_text_buffer_get_iter_at_line (buffer, &start_iter, i);

              /* don't change indentation of empty lines */
              if (gtk_text_iter_ends_line (&start_iter))
                continue;

              /* increase or decrease the indentation */
              if (increase)
                mousepad_view_indent_increase (view, &start_iter);
              else
                mousepad_view_indent_decrease (view, &start_iter);
            }
        }

      /* end user action */
      gtk_text_buffer_end_user_action (buffer);

      /* put cursor on screen */
      mousepad_view_put_cursor_on_screen (view);
    }
}



static gchar *
mousepad_view_indent_string (GtkTextBuffer     *buffer,
                             const GtkTextIter *iter)
{
  GtkTextIter start, end;
  gint        line;

  /* get the line of the iter */
  line = gtk_text_iter_get_line (iter);

  /* get the iter of the beginning of this line */
  gtk_text_buffer_get_iter_at_line (buffer, &start, line);

  /* set the end iter */
  end = start;

  /* forward until we hit text */
  if (mousepad_util_forward_iter_to_text (&end, iter) == FALSE)
    return NULL;

  /* return the text between the iters */
  return gtk_text_iter_get_slice (&start, &end);
}



static gint
mousepad_view_calculate_layout_width (GtkWidget *widget,
                                      gsize      length,
                                      gchar      fill_char)
{
  PangoLayout *layout;
  gchar       *string;
  gint         width = -1;

  _mousepad_return_val_if_fail (GTK_IS_WIDGET (widget), -1);
  _mousepad_return_val_if_fail (length > 0, -1);

  /* create character string */
  string = g_strnfill (length, fill_char);

  /* create pango layout from widget */
  layout = gtk_widget_create_pango_layout (widget, string);

  /* cleanup */
  g_free (string);

  if (G_LIKELY (layout))
    {
      /* get the layout size */
      pango_layout_get_pixel_size (layout, &width, NULL);

      /* release layout */
      g_object_unref (G_OBJECT (layout));
    }

  return width;
}



void
mousepad_view_put_cursor_on_screen (MousepadView *view)
{
  GtkTextBuffer *buffer;

  _mousepad_return_if_fail (MOUSEPAD_IS_VIEW (view));

  /* get buffer */
  buffer = mousepad_view_get_buffer (view);

  /* scroll to visible area */
  gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view),
                                gtk_text_buffer_get_insert (buffer),
                                0.02, FALSE, 0.0, 0.0);
}



static void
mousepad_view_transpose_multi_selection (GtkTextBuffer *buffer,
                                         MousepadView  *view)
{
  GSList      *li, *ls;
  GSList      *strings = NULL;
  GtkTextIter  start_iter, end_iter;

  _mousepad_return_if_fail (MOUSEPAD_IS_VIEW (view));

  /* get the strings and delete the existing strings */
  for (li = view->marks; li != NULL; li = li->next)
    {
      /* get the iters */
      gtk_text_buffer_get_iter_at_mark (buffer, &start_iter, li->data);
      li = li->next;
      gtk_text_buffer_get_iter_at_mark (buffer, &end_iter, li->data);

      /* prepend the text between the iters to an internal list */
      strings = g_slist_prepend (strings, gtk_text_buffer_get_slice (buffer, &start_iter, &end_iter, FALSE));

      /* delete the content */
      gtk_text_buffer_delete (buffer, &start_iter, &end_iter);
    }

  /* insert the strings in reversed order */
  for (li = view->marks, ls = strings; li != NULL && ls != NULL; li = li->next, ls = ls->next)
    {
      /* get the end iter */
      gtk_text_buffer_get_iter_at_mark (buffer, &end_iter, li->next->data);

      /* insert the string */
      gtk_text_buffer_insert (buffer, &end_iter, ls->data, -1);

      /* get the start iter */
      gtk_text_buffer_get_iter_at_mark (buffer, &start_iter, li->data);

      /* apply tag */
      gtk_text_buffer_apply_tag (buffer, view->selection_tag, &start_iter, &end_iter);

      /* free string */
      g_free (ls->data);

      /* next iter */
      li = li->next;
    }

  /* free list */
  g_slist_free (strings);
}



static void
mousepad_view_transpose_range (GtkTextBuffer *buffer,
                               GtkTextIter   *start_iter,
                               GtkTextIter   *end_iter)
{
  gchar *string, *reversed;
  gint   offset;

  /* store start iter line offset */
  offset = gtk_text_iter_get_offset (start_iter);

  /* get selected text */
  string = gtk_text_buffer_get_slice (buffer, start_iter, end_iter, FALSE);

  /* reverse the string */
  reversed = g_utf8_strreverse (string, -1);

  /* cleanup */
  g_free (string);

  /* delete the text between the iters */
  gtk_text_buffer_delete (buffer, start_iter, end_iter);

  /* insert the reversed string */
  gtk_text_buffer_insert (buffer, end_iter, reversed, -1);

  /* cleanup */
  g_free (reversed);

  /* restore start iter */
  gtk_text_buffer_get_iter_at_offset (buffer, start_iter, offset);
}



static void
mousepad_view_transpose_lines (GtkTextBuffer *buffer,
                               GtkTextIter   *start_iter,
                               GtkTextIter   *end_iter)
{
  GString *string;
  gint     start_line, end_line;
  gint     i;
  gchar   *slice;

  /* make sure the order is ok */
  gtk_text_iter_order (start_iter, end_iter);

  /* get the line numbers */
  start_line = gtk_text_iter_get_line (start_iter);
  end_line = gtk_text_iter_get_line (end_iter);

  /* new string */
  string = g_string_new (NULL);

  /* add the lines in reversed order to the string */
  for (i = start_line; i <= end_line; i++)
    {
      /* get start iter */
      gtk_text_buffer_get_iter_at_line (buffer, start_iter, i);

      /* set end iter */
      *end_iter = *start_iter;

      /* only prepend when the iters won't be equal */
      if (!gtk_text_iter_ends_line (end_iter))
        {
          /* move the iter to the end of this line */
          gtk_text_iter_forward_to_line_end (end_iter);

          /* prepend line */
          slice = gtk_text_buffer_get_slice (buffer, start_iter, end_iter, FALSE);
          string = g_string_prepend (string, slice);
          g_free (slice);
        }

      /* prepend new line */
      if (i < end_line)
        string = g_string_prepend_c (string, '\n');
    }

  /* get start iter again */
  gtk_text_buffer_get_iter_at_line (buffer, start_iter, start_line);

  /* delete selection */
  gtk_text_buffer_delete (buffer, start_iter, end_iter);

  /* insert reversed lines */
  gtk_text_buffer_insert (buffer, end_iter, string->str, string->len);

  /* cleanup */
  g_string_free (string, TRUE);

  /* restore start iter */
  gtk_text_buffer_get_iter_at_line (buffer, start_iter, start_line);
}

static void
mousepad_view_transpose_words (GtkTextBuffer *buffer,
                               GtkTextIter   *iter)
{
  GtkTextMark *left_mark, *right_mark;
  GtkTextIter  start_left, end_left, start_right, end_right;
  gchar       *word_left, *word_right;
  gboolean     restore_cursor;

  /* left word end */
  end_left = *iter;
  if (!mousepad_util_iter_backward_text_start (&end_left))
    return;

  /* left word start */
  start_left = end_left;
  if (!mousepad_util_iter_backward_word_start (&start_left))
    return;

  /* right word start */
  start_right = *iter;
  if (!mousepad_util_iter_forward_text_start (&start_right))
    return;

  /* right word end */
  end_right = start_right;
  if (!mousepad_util_iter_forward_word_end (&end_right))
    return;

  /* check if we selected something usefull */
  if (gtk_text_iter_get_line (&start_left) == gtk_text_iter_get_line (&end_right)
      && !gtk_text_iter_equal (&start_left, &end_left)
      && !gtk_text_iter_equal (&start_right, &end_right)
      && !gtk_text_iter_equal (&end_left, &start_right))
    {
      /* get the words */
      word_left = gtk_text_buffer_get_slice (buffer, &start_left, &end_left, FALSE);
      word_right = gtk_text_buffer_get_slice (buffer, &start_right, &end_right, FALSE);

      /* check if we need to restore the cursor afterwards */
      restore_cursor = gtk_text_iter_equal (iter, &start_right);

      /* insert marks at the start and end of the right word */
      left_mark = gtk_text_buffer_create_mark (buffer, NULL, &start_right, TRUE);
      right_mark = gtk_text_buffer_create_mark (buffer, NULL, &end_right, FALSE);

      /* delete the left word and insert the right word there */
      gtk_text_buffer_delete (buffer, &start_left, &end_left);
      gtk_text_buffer_insert (buffer, &start_left, word_right, -1);
      g_free (word_right);

      /* restore the right word iters */
      gtk_text_buffer_get_iter_at_mark (buffer, &start_right, left_mark);
      gtk_text_buffer_get_iter_at_mark (buffer, &end_right, right_mark);

      /* delete the right words and insert the left word there */
      gtk_text_buffer_delete (buffer, &start_right, &end_right);
      gtk_text_buffer_insert (buffer, &end_right, word_left, -1);
      g_free (word_left);

      /* restore the cursor if needed */
      if (restore_cursor)
        {
          /* restore the iter from the left mark (left gravity) */
          gtk_text_buffer_get_iter_at_mark (buffer, &start_right, left_mark);
          gtk_text_buffer_place_cursor (buffer, &start_right);
        }

      /* cleanup the marks */
      gtk_text_buffer_delete_mark (buffer, left_mark);
      gtk_text_buffer_delete_mark (buffer, right_mark);
    }
}



void
mousepad_view_transpose (MousepadView *view)
{
  GtkTextBuffer *buffer;
  GtkTextIter    sel_start, sel_end;

  _mousepad_return_if_fail (MOUSEPAD_IS_VIEW (view));

  /* get buffer */
  buffer = mousepad_view_get_buffer (view);

  /* begin user action */
  gtk_text_buffer_begin_user_action (buffer);

  if (view->flags != 0)
    {
      /* transpose a multi selection */
      mousepad_view_transpose_multi_selection (buffer, view);
    }
  else if (gtk_text_buffer_get_selection_bounds (buffer, &sel_start, &sel_end))
    {
      /* if the selection is not on the same line, include the whole lines */
      if (gtk_text_iter_get_line (&sel_start) == gtk_text_iter_get_line (&sel_end))
        {
          /* reverse selection */
          mousepad_view_transpose_range (buffer, &sel_start, &sel_end);
        }
      else
        {
          /* reverse lines */
          mousepad_view_transpose_lines (buffer, &sel_start, &sel_end);
        }

      /* restore selection */
      gtk_text_buffer_select_range (buffer, &sel_start, &sel_end);
    }
  else
    {
      /* get cursor iter */
      gtk_text_buffer_get_iter_at_mark (buffer, &sel_start, gtk_text_buffer_get_insert (buffer));

      /* set end iter */
      sel_end = sel_start;

      if (gtk_text_iter_starts_line (&sel_start))
        {
          /* swap this line with the line above */
          if (gtk_text_iter_backward_line (&sel_end))
            mousepad_view_transpose_lines (buffer, &sel_start, &sel_end);
        }
      else if (gtk_text_iter_ends_line (&sel_start))
        {
          /* swap this line with the line below */
          if (gtk_text_iter_forward_line (&sel_end))
            mousepad_view_transpose_lines (buffer, &sel_start, &sel_end);
        }
      else if (mousepad_util_iter_inside_word (&sel_start))
        {
          /* reverse the characters before and after the cursor */
          if (gtk_text_iter_backward_char (&sel_start) && gtk_text_iter_forward_char (&sel_end))
            mousepad_view_transpose_range (buffer, &sel_start, &sel_end);
        }
      else
        {
          /* swap the words left and right of the cursor */
          mousepad_view_transpose_words (buffer, &sel_start);
        }
    }

  /* end user action */
  gtk_text_buffer_end_user_action (buffer);
}



void
mousepad_view_clipboard_cut (MousepadView *view)
{
  GtkClipboard  *clipboard;
  GtkTextBuffer *buffer;

  _mousepad_return_if_fail (MOUSEPAD_IS_VIEW (view));
  _mousepad_return_if_fail (mousepad_view_get_selection_length (view) > 0);

  /* get the clipboard */
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view), GDK_SELECTION_CLIPBOARD);

  if (view->flags != 0)
    {
      /* copy to clipboard */
      mousepad_view_selection_clipboard (view, clipboard);

      /* cleanup the vertical selection */
      mousepad_view_selection_delete_content (view);

      /* destroy selection */
      mousepad_view_selection_destroy (view);
    }
  else
    {
      /* get buffer */
      buffer = mousepad_view_get_buffer (view);

      /* cut from buffer */
      gtk_text_buffer_cut_clipboard (buffer, clipboard, gtk_text_view_get_editable (GTK_TEXT_VIEW (view)));
    }

  /* put cursor on screen */
  mousepad_view_put_cursor_on_screen (view);
}



void
mousepad_view_clipboard_copy (MousepadView *view)
{
  GtkClipboard  *clipboard;
  GtkTextBuffer *buffer;

  _mousepad_return_if_fail (MOUSEPAD_IS_VIEW (view));
  _mousepad_return_if_fail (mousepad_view_get_selection_length (view) > 0);

  /* get the clipboard */
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view), GDK_SELECTION_CLIPBOARD);


  if (view->flags != 0)
    {
      /* copy selection to clipboard */
      mousepad_view_selection_clipboard (view, clipboard);
    }
  else
    {
      /* get buffer */
      buffer = mousepad_view_get_buffer (view);

      /* copy from buffer */
      gtk_text_buffer_copy_clipboard (buffer, clipboard);
    }

  /* put cursor on screen */
  mousepad_view_put_cursor_on_screen (view);
}



void
mousepad_view_clipboard_paste (MousepadView *view,
                               gboolean      paste_as_column)
{
  GtkClipboard   *clipboard;
  GtkTextBuffer  *buffer;
  GtkTextView    *textview = GTK_TEXT_VIEW (view);
  gchar          *string;
  GtkTextMark    *mark;
  GtkTextIter     iter;
  GdkRectangle    rect;
  gchar         **pieces;
  gint            i, y;

  /* get the clipboard */
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view), GDK_SELECTION_CLIPBOARD);

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  if (paste_as_column)
    {
      /* get the clipboard text */
      string = gtk_clipboard_wait_for_text (clipboard);

      if (G_LIKELY (string))
        {
          /* chop the string into pieces */
          pieces = g_strsplit (string, "\n", -1);

          /* cleanup */
          g_free (string);

          /* get iter at cursor position */
          mark = gtk_text_buffer_get_insert (buffer);
          gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

          /* get the iter location */
          gtk_text_view_get_iter_location (textview, &iter, &rect);

          /* begin user action */
          gtk_text_buffer_begin_user_action (buffer);

          /* insert the pieces in the buffer */
          for (i = 0; pieces[i] != NULL; i++)
            {
              /* insert the text in the buffer */
              gtk_text_buffer_insert (buffer, &iter, pieces[i], -1);

              /* break if the next piece is null */
              if (G_UNLIKELY (pieces[i+1] == NULL))
                break;

              /* move the iter to the next line */
              if (!gtk_text_iter_forward_line (&iter))
                {
                  /* no new line, insert a new line */
                  gtk_text_buffer_insert (buffer, &iter, "\n", 1);
                }
              else
                {
                  /* get the y coordinate for this line */
                  gtk_text_view_get_line_yrange (textview, &iter, &y, NULL);

                  /* get the iter at the correct coordinate */
                  gtk_text_view_get_iter_at_location (textview, &iter, rect.x, y);
                }
            }

          /* cleanup */
          g_strfreev (pieces);

          /* set the cursor to the last iter position */
          gtk_text_buffer_place_cursor (buffer, &iter);

          /* end user action */
          gtk_text_buffer_end_user_action (buffer);
        }
    }
  else
    {
      /* paste as normal string */
      gtk_text_buffer_paste_clipboard (buffer, clipboard, NULL, gtk_text_view_get_editable (textview));
    }

  /* put cursor on screen */
  mousepad_view_put_cursor_on_screen (view);
}



void
mousepad_view_delete_selection (MousepadView *view)
{
  GtkTextBuffer *buffer;

  _mousepad_return_if_fail (MOUSEPAD_IS_VIEW (view));
  _mousepad_return_if_fail (mousepad_view_get_selection_length (view) > 0);

  if (view->flags != 0)
    {
      /* remove the text in our selection */
      mousepad_view_selection_delete_content (view);

      /* cleanup the vertical selection */
      mousepad_view_selection_destroy (view);
    }
  else
    {
      /* get the buffer */
      buffer = mousepad_view_get_buffer (view);

      /* delete the selection */
      gtk_text_buffer_delete_selection (buffer, TRUE, gtk_text_view_get_editable (GTK_TEXT_VIEW (view)));
    }

  /* put cursor on screen */
  mousepad_view_put_cursor_on_screen (view);
}



void
mousepad_view_select_all (MousepadView *view)
{
  GtkTextIter    start, end;
  GtkTextBuffer *buffer;

  _mousepad_return_if_fail (MOUSEPAD_IS_VIEW (view));

  /* cleanup our selection */
  if (view->flags != 0)
    mousepad_view_selection_destroy (view);

  /* get buffer */
  buffer = mousepad_view_get_buffer (view);

  /* get the start and end iter */
  gtk_text_buffer_get_bounds (buffer, &start, &end);

  /* select everything between those iters */
  gtk_text_buffer_select_range (buffer, &start, &end);
}



void
mousepad_view_set_line_numbers (MousepadView *view,
                                gboolean      line_numbers)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_VIEW (view));

  if (view->line_numbers != line_numbers)
    {
      /* set the boolean */
      view->line_numbers = line_numbers;

      /* set the left border size */
      gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (view),
                                            GTK_TEXT_WINDOW_LEFT,
                                            line_numbers ? 20 : 0);

      /* make sure there is an expose event */
      gtk_widget_queue_draw (GTK_WIDGET (view));
    }
}



void
mousepad_view_set_auto_indent (MousepadView *view,
                               gboolean      auto_indent)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_VIEW (view));

  /* set the boolean */
  view->auto_indent = auto_indent;
}



void
mousepad_view_set_tab_size (MousepadView *view,
                             gint          tab_size)
{
  PangoTabArray *tab_array;
  gint           layout_width;

  _mousepad_return_if_fail (MOUSEPAD_IS_VIEW (view));
  _mousepad_return_if_fail (GTK_IS_TEXT_VIEW (view));

  /* set the value */
  view->tab_size = tab_size;

  /* get the pixel width of the tab size */
  layout_width = mousepad_view_calculate_layout_width (GTK_WIDGET (view), view->tab_size, ' ');

  if (G_LIKELY (layout_width != -1))
    {
      /* create the pango tab array */
      tab_array = pango_tab_array_new (1, TRUE);
      pango_tab_array_set_tab (tab_array, 0, PANGO_TAB_LEFT, layout_width);

      /* set the textview tab array */
      gtk_text_view_set_tabs (GTK_TEXT_VIEW (view), tab_array);

      /* cleanup */
      pango_tab_array_free (tab_array);
    }
}



void
mousepad_view_set_insert_spaces (MousepadView *view,
                                 gboolean      insert_spaces)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_VIEW (view));

  /* set boolean */
  view->insert_spaces = insert_spaces;
}



gboolean
mousepad_view_get_selection_length (MousepadView *view)
{
  GtkTextBuffer *buffer;
  GtkTextIter    sel_start, sel_end;
  gint           sel_length = 0;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_VIEW (view), FALSE);

  /* get the text buffer */
  buffer = mousepad_view_get_buffer (view);

  /* we have a vertical selection */
  if (view->flags != 0)
    {
      /* get the selection count from the selection draw function */
      sel_length = view->selection_length;
    }
  else if (gtk_text_buffer_get_selection_bounds (buffer, &sel_start, &sel_end))
    {
      /* calculate the selection length from the iter offset (absolute) */
      sel_length = ABS (gtk_text_iter_get_offset (&sel_end) - gtk_text_iter_get_offset (&sel_start));
    }

  /* return length */
  return sel_length;
}



gboolean
mousepad_view_get_line_numbers (MousepadView *view)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_VIEW (view), FALSE);

  return view->line_numbers;
}



gboolean
mousepad_view_get_auto_indent (MousepadView *view)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_VIEW (view), FALSE);

  return view->auto_indent;
}



gint
mousepad_view_get_tab_size (MousepadView *view)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_VIEW (view), -1);

  return view->tab_size;
}



gboolean
mousepad_view_get_insert_spaces (MousepadView *view)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_VIEW (view), FALSE);

  return view->insert_spaces;
}
