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



/* we can assume there is a buffer for this view */
#define mousepad_view_get_buffer(view) (GTK_TEXT_VIEW (view)->buffer)



static void      mousepad_view_class_init                         (MousepadViewClass  *klass);
static void      mousepad_view_init                               (MousepadView       *view);
static void      mousepad_view_finalize                           (GObject            *object);
static void      mousepad_view_style_set                          (GtkWidget          *widget,
                                                                   GtkStyle           *previous_style);
static gint      mousepad_view_expose                             (GtkWidget          *widget,
                                                                   GdkEventExpose     *event);
static gboolean  mousepad_view_key_press_event                    (GtkWidget          *widget,
                                                                   GdkEventKey        *event);
static gboolean  mousepad_view_button_press_event                 (GtkWidget          *widget,
                                                                   GdkEventButton     *event);
static gboolean  mousepad_view_button_release_event               (GtkWidget          *widget,
                                                                   GdkEventButton     *event);
static void      mousepad_view_vertical_selection_handle          (MousepadView       *view,
                                                                   gboolean            prepend_marks);
static gboolean  mousepad_view_vertical_selection_finish          (MousepadView       *view);
static void      mousepad_view_vertical_selection_free_marks      (MousepadView       *view);
static void      mousepad_view_vertical_selection_clear           (MousepadView       *view);
static gboolean  mousepad_view_vertical_selection_timeout         (gpointer            user_data);
static void      mousepad_view_vertical_selection_timeout_destroy (gpointer            user_data);
static void      mousepad_view_increase_indent_iter               (MousepadView       *view,
                                                                   GtkTextIter        *iter,
                                                                   gboolean            tab);
static void      mousepad_view_decrease_indent_iter               (MousepadView       *view,
                                                                   GtkTextIter        *iter,
                                                                   gboolean            tab);
static void      mousepad_view_indent_selection                   (MousepadView       *view,
                                                                   gboolean            increase,
                                                                   gboolean            tab);

static gchar    *mousepad_view_indentation_string                 (GtkTextBuffer      *buffer,
                                                                   const GtkTextIter  *iter);
static gint      mousepad_view_calculate_layout_width             (GtkWidget          *widget,
                                                                   gsize               length,
                                                                   gchar               fill_char);
static void      mousepad_view_clipboard                          (MousepadView       *view,
                                                                   GtkClipboard       *clipboard,
                                                                   gboolean            remove);



struct _MousepadViewClass
{
  GtkTextViewClass __parent__;
};

struct _MousepadView
{
  GtkTextView  __parent__;

  /* coordinates for the vertical selection */
  gint         vertical_start_x, vertical_start_y;
  gint         vertical_end_x, vertical_end_y;

  /* pointer array with all the vertical selection marks */
  GSList      *marks;

  /* vertical selection timeout id */
  guint        vertical_timeout_id;

  /* the selection tag */
  GtkTextTag  *tag;

  /* settings */
  guint        auto_indent : 1;
  guint        line_numbers : 1;
  guint        insert_spaces : 1;
  guint        tab_width;
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
  view->tab_width = 8;

  /* initialize vertical selection */
  view->marks = NULL;
  view->tag = NULL;
  view->vertical_start_x = -1;
  view->vertical_start_y = -1;
  view->vertical_end_x = -1;
  view->vertical_end_y = -1;
  view->vertical_timeout_id = 0;
}



static void
mousepad_view_finalize (GObject *object)
{
  MousepadView *view = MOUSEPAD_VIEW (object);

  /* stop a pending vertical selection timeout */
  if (G_UNLIKELY (view->vertical_timeout_id != 0))
    g_source_remove (view->vertical_timeout_id);

  /* free the marks list (we're not owning a ref on the marks) */
  if (G_UNLIKELY (view->marks != NULL))
    g_slist_free (view->marks);

  (*G_OBJECT_CLASS (mousepad_view_parent_class)->finalize) (object);
}



static gboolean
mousepad_view_expose (GtkWidget      *widget,
                      GdkEventExpose *event)
{
  GdkWindow   *window;
  GtkTextView *text_view = GTK_TEXT_VIEW (widget);
  gint         y_top, y_offset, y_bottom;
  gint         y, height;
  gint         line_number = 0, line_count;
  GtkTextIter  iter;
  gint         width;
  PangoLayout *layout;
  gchar        str[8]; /* maximum of 10e6 lines */

  /* get the left window */
  window = gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_LEFT);

  /* check if the event if for the left window */
  if (event->window == window)
    {
      /* get the real start position */
      gtk_text_view_window_to_buffer_coords (text_view, GTK_TEXT_WINDOW_LEFT,
                                             0, event->area.y, NULL, &y_top);

      /* get the bottom position */
      y_bottom = y_top + event->area.height;

      /* get the buffer offset (negative value or 0) */
      y_offset = event->area.y - y_top;

      /* get the start iter */
      gtk_text_view_get_line_at_y (text_view, &iter, y_top, NULL);

      /* get the number of lines in the buffer */
      line_count = gtk_text_buffer_get_line_count (text_view->buffer);

      /* string with the 'last' line number */
      g_snprintf (str, sizeof (str), "%d", MAX (99, line_count));

      /* create the pango layout */
      layout = gtk_widget_create_pango_layout (widget, str);
      pango_layout_get_pixel_size (layout, &width, NULL);
      pango_layout_set_width (layout, width);
      pango_layout_set_alignment (layout, PANGO_ALIGN_RIGHT);

      /* set the width of the left window border */
      gtk_text_view_set_border_window_size (text_view, GTK_TEXT_WINDOW_LEFT, width + 8);

      /* draw a vertical line to separate the numbers and text */
      gtk_paint_vline (widget->style, window,
                       GTK_WIDGET_STATE (widget),
                       NULL, widget, NULL,
                       event->area.y,
                       event->area.y + event->area.height,
                       width + 6);

      /* walk through the lines until we hit the last line */
      while (line_number < line_count)
        {
          /* get the y position and the height of the iter */
          gtk_text_view_get_line_yrange (text_view, &iter, &y, &height);

          /* include the buffer offset */
          y += y_offset;

          /* get the number of the line */
          line_number = gtk_text_iter_get_line (&iter) + 1;

          /* create the number */
          g_snprintf (str, sizeof (str), "%d", line_number);

          /* create the pange layout */
          pango_layout_set_text (layout, str, strlen (str));

          /* draw the layout on the left window */
          gtk_paint_layout (widget->style, window,
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
      if (G_UNLIKELY (view->tag))
        {
          gtk_text_buffer_get_bounds (buffer, &start, &end);
          gtk_text_buffer_remove_tag (buffer, view->tag, &start, &end);
        }

      /* create the new selection tag */
      view->tag = gtk_text_buffer_create_tag (buffer, NULL,
                                              "background-gdk", &style->base[GTK_STATE_SELECTED],
                                              "foreground-gdk", &style->text[GTK_STATE_SELECTED],
                                              NULL);

      /* update the tab stops */
      mousepad_view_set_tab_width (view, view->tab_width);
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

  /* get the modifiers state */
  modifiers = event->state & gtk_accelerator_get_default_mod_mask ();

  /* get the textview buffer */
  buffer = mousepad_view_get_buffer (view);

  /* handle the key event */
  switch (event->keyval)
    {
      case GDK_Return:
      case GDK_KP_Enter:
        if (!(event->state & GDK_SHIFT_MASK) && view->auto_indent)
          {
            /* get the iter position of the cursor */
            cursor = gtk_text_buffer_get_insert (buffer);
            gtk_text_buffer_get_iter_at_mark (buffer, &iter, cursor);

            /* get the string of tabs and spaces we're going to indent */
            string = mousepad_view_indentation_string (buffer, &iter);

            if (string != NULL)
              {
                /* begin a user action */
                gtk_text_buffer_begin_user_action (buffer);

                /* insert the indent characters */
                gtk_text_buffer_insert (buffer, &iter, "\n", 1);
                gtk_text_buffer_insert (buffer, &iter, string, -1);

                /* end user action */
                gtk_text_buffer_end_user_action (buffer);

                /* cleanup */
                g_free (string);

                /* make sure the new string is visible for the user */
                mousepad_view_put_cursor_on_screen (view);

                /* we've inserted the new line, nothing to do for gtk */
                return TRUE;
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

             /* move the cursor */
             if (modifiers & GDK_SHIFT_MASK)
               gtk_text_buffer_move_mark (buffer, cursor, &iter);
             else
               gtk_text_buffer_place_cursor (buffer, &iter);

             /* make sure the cursor is visible for the user */
             mousepad_view_put_cursor_on_screen (view);

             return TRUE;
          }
        break;

      case GDK_Tab:
      case GDK_KP_Tab:
      case GDK_ISO_Left_Tab:
        if (mousepad_view_get_has_selection (view))
          {
            /* indent the selection */
            mousepad_view_indent_selection (view, (modifiers != GDK_SHIFT_MASK), TRUE);

            return TRUE;
          }
        else if (view->insert_spaces)
          {
            /* get the iter position of the cursor */
            cursor = gtk_text_buffer_get_insert (buffer);
            gtk_text_buffer_get_iter_at_mark (buffer, &iter, cursor);

            /* insert spaces */
            mousepad_view_increase_indent_iter (view, &iter, TRUE);

            return TRUE;
          }

      case GDK_BackSpace:
      case GDK_space:
        /* indent on Space unindent on Backspace or Tab */
        if (modifiers == GDK_SHIFT_MASK && mousepad_view_get_has_selection (view))
          {
            /* indent the selection */
            mousepad_view_indent_selection (view, (event->keyval == GDK_space), FALSE);

            return TRUE;
          }
        break;
    }

  /* reset the vertical selection */
  if (G_UNLIKELY (view->marks != NULL && !event->is_modifier))
    mousepad_view_vertical_selection_clear (view);

  return (*GTK_WIDGET_CLASS (mousepad_view_parent_class)->key_press_event) (widget, event);
}



static gboolean
mousepad_view_button_press_event (GtkWidget      *widget,
                                  GdkEventButton *event)
{
  MousepadView *view = MOUSEPAD_VIEW (widget);
  GtkTextView  *textview;

  /* cleanup vertical selection */
  if (G_UNLIKELY (view->marks != NULL))
    mousepad_view_vertical_selection_clear (view);

  /* Shift + Ctrl + Button 1 are pressed */
  if (G_UNLIKELY (event->type == GDK_BUTTON_PRESS
                  && event->button == 1
                  && (event->state & GDK_CONTROL_MASK)
                  && (event->state & GDK_SHIFT_MASK)
                  && event->x >= 0
                  && event->y >= 0))
    {
      /* get the textview */
      textview = GTK_TEXT_VIEW (widget);

      /* set the vertical selection start position, inclusing textview offset */
      view->vertical_start_x = event->x + textview->xoffset;
      view->vertical_start_y = event->y + textview->yoffset;

      /* start a vertical selection timeout */
      view->vertical_timeout_id = g_timeout_add_full (G_PRIORITY_LOW, 50, mousepad_view_vertical_selection_timeout,
                                                      view, mousepad_view_vertical_selection_timeout_destroy);

      return TRUE;
    }

  return (*GTK_WIDGET_CLASS (mousepad_view_parent_class)->button_press_event) (widget, event);
}



static gboolean
mousepad_view_button_release_event (GtkWidget      *widget,
                                    GdkEventButton *event)
{
  MousepadView  *view = MOUSEPAD_VIEW (widget);

  _mousepad_return_val_if_fail (view->marks == NULL, TRUE);

  /* end of a vertical selection */
  if (G_UNLIKELY (view->vertical_start_x != -1 && event->button == 1))
    {
      /* stop the timeout */
      g_source_remove (view->vertical_timeout_id);

      /* create the marks list and emit has-selection signal */
      if (G_LIKELY (mousepad_view_vertical_selection_finish (view)))
        return TRUE;
    }

  return (*GTK_WIDGET_CLASS (mousepad_view_parent_class)->button_release_event) (widget, event);
}



/**
 * Vertical Selection Functions
 **/
static void
mousepad_view_vertical_selection_handle (MousepadView *view,
                                         gboolean      prepend_marks)
{
  GtkTextBuffer *buffer;
  GtkTextView   *textview = GTK_TEXT_VIEW (view);
  GtkTextIter    start_iter, end_iter;
  gint           y, y_begin, y_height, y_end;
  GtkTextMark   *start_mark, *end_mark;

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* remove the old _selection_ tags */
  gtk_text_buffer_get_bounds (buffer, &start_iter, &end_iter);
  gtk_text_buffer_remove_tag (buffer, view->tag, &start_iter, &end_iter);

  /* get the start and end iter */
  gtk_text_view_get_iter_at_location (textview, &start_iter, 0, view->vertical_start_y);
  gtk_text_view_get_iter_at_location (textview, &end_iter, 0, view->vertical_end_y);

  /* make sure the start iter is before the end iter */
  gtk_text_iter_order (&start_iter, &end_iter);

  /* get the y coordinates we're going to walk */
  gtk_text_view_get_line_yrange (textview, &start_iter, &y_begin, &y_height);
  gtk_text_view_get_line_yrange (textview, &end_iter, &y_end, NULL);

  /* make sure atleast one line is selected */
  y_end += y_height;

  /* walk */
  for (y = y_begin; y < y_end; y += y_height)
    {
      /* get the start and end iter in the line */
      gtk_text_view_get_iter_at_location (textview, &start_iter, view->vertical_start_x, y);
      gtk_text_view_get_iter_at_location (textview, &end_iter, view->vertical_end_x, y);

      /* continue when the iters are the same */
      if (G_UNLIKELY (gtk_text_iter_equal (&start_iter, &end_iter)))
        continue;

      /* apply tag */
      gtk_text_buffer_apply_tag (buffer, view->tag, &start_iter, &end_iter);

      /* prepend to list */
      if (G_UNLIKELY (prepend_marks))
        {
          /* make sure the iters are correctly sorted */
          gtk_text_iter_order (&start_iter, &end_iter);

          /* append start mark to the list */
          start_mark = gtk_text_buffer_create_mark (buffer, NULL, &start_iter, FALSE);
          view->marks = g_slist_prepend (view->marks, start_mark);

          /* append end mark to the list */
          end_mark = gtk_text_buffer_create_mark (buffer, NULL, &end_iter, FALSE);
          view->marks = g_slist_prepend (view->marks, end_mark);
        }
    }
}



static gboolean
mousepad_view_vertical_selection_finish (MousepadView *view)
{
  GtkTextBuffer *buffer;

  /* cleanup the marks list */
  if (view->marks)
    mousepad_view_vertical_selection_free_marks (view);

  /* update the highlight an prepend the marks */
  mousepad_view_vertical_selection_handle (view, TRUE);

  if (G_LIKELY (view->marks != NULL))
    {
      /* reverse the list after rebuilding */
      view->marks = g_slist_reverse (view->marks);

      /* get the buffer */
      buffer = mousepad_view_get_buffer (view);

      /* notify the has-selection signal */
      buffer->has_selection = TRUE;
      g_object_notify (G_OBJECT (buffer), "has-selection");

      return TRUE;
    }

  return FALSE;
}



static void
mousepad_view_vertical_selection_free_marks (MousepadView *view)
{
  GSList        *li;
  GtkTextBuffer *buffer;

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* remove all the marks */
  for (li = view->marks; li != NULL; li = li->next)
    gtk_text_buffer_delete_mark (buffer, li->data);

  /* free the list */
  g_slist_free (view->marks);

  /* null */
  view->marks = NULL;
}


static void
mousepad_view_vertical_selection_clear (MousepadView *view)
{
  GtkTextBuffer *buffer;
  GtkTextIter    start, end;

  _mousepad_return_if_fail (view->marks != NULL);

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* no selection */
  buffer->has_selection = FALSE;
  g_object_notify (G_OBJECT (buffer), "has-selection");

  /* remove the selections */
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  gtk_text_buffer_remove_tag (buffer, view->tag, &start, &end);

  /* reset the coordinates */
  view->vertical_start_x = view->vertical_start_y =
  view->vertical_end_x = view->vertical_end_y = -1;

  /* cleanup marks */
  mousepad_view_vertical_selection_free_marks (view);
}



static gboolean
mousepad_view_vertical_selection_timeout (gpointer user_data)
{
  MousepadView  *view = MOUSEPAD_VIEW (user_data);
  GtkTextView   *textview = GTK_TEXT_VIEW (view);
  gint           vertical_start_x, vertical_start_y;
  gint           vertical_end_x, vertical_end_y;
  GtkTextBuffer *buffer;
  GtkTextIter    iter;

  GDK_THREADS_ENTER ();

  /* get the cursor coordinate */
  gdk_window_get_pointer (gtk_text_view_get_window (textview, GTK_TEXT_WINDOW_TEXT),
                          &vertical_end_x, &vertical_end_y, NULL);

  /* convert to positive values and buffer coordinates */
  vertical_end_x = MAX (vertical_end_x, 0) + textview->xoffset;
  vertical_end_y = MAX (vertical_end_y, 0) + textview->yoffset;

  /* only update the selection when the cursor moved */
  if (view->vertical_end_x != vertical_end_x || view->vertical_end_y != vertical_end_y)
    {
      /* update the end coordinates */
      view->vertical_end_x = vertical_end_x;
      view->vertical_end_y = vertical_end_y;

      /* backup start coordinates */
      vertical_start_x = view->vertical_start_x;
      vertical_start_y = view->vertical_start_y;

      /* during the timeout, we're only interested in the visible selection */
      view->vertical_start_x = MAX (view->vertical_start_x, textview->xoffset);
      view->vertical_start_y = MAX (view->vertical_start_y, textview->yoffset);

      /* update the selection */
      mousepad_view_vertical_selection_handle (view, FALSE);

      /* restore the start coordinates */
      view->vertical_start_x = vertical_start_x;
      view->vertical_start_y = vertical_start_y;

      /* get the text buffer */
      buffer = mousepad_view_get_buffer (view);

      /* put the cursor under the mouse pointer */
      gtk_text_view_get_iter_at_location (textview, &iter, vertical_end_x, vertical_end_y);
      gtk_text_buffer_place_cursor (buffer, &iter);

      /* put cursor on screen */
      mousepad_view_put_cursor_on_screen (view);
    }

  GDK_THREADS_LEAVE ();

  /* keep the timeout running */
  return TRUE;
}



static void
mousepad_view_vertical_selection_timeout_destroy (gpointer user_data)
{
  MOUSEPAD_VIEW (user_data)->vertical_timeout_id = 0;
}


/**
 * Indentation Functions
 **/
static void
mousepad_view_increase_indent_iter (MousepadView *view,
                                    GtkTextIter  *iter,
                                    gboolean      tab)
{
  gchar         *string;
  gint           offset, length, inline_len;
  GtkTextBuffer *buffer;

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  if (view->insert_spaces && tab)
    {
      /* get the offset */
      offset = mousepad_util_get_real_line_offset (iter, view->tab_width);

      /* calculate the length to inline with a tab */
      inline_len = offset % view->tab_width;

      if (inline_len == 0)
        length = view->tab_width;
      else
        length = view->tab_width - inline_len;

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
      gtk_text_buffer_insert (buffer, iter, tab ? "\t" : " ", -1);
    }
}



static void
mousepad_view_decrease_indent_iter (MousepadView *view,
                                    GtkTextIter  *iter,
                                    gboolean      tab)
{
  GtkTextBuffer *buffer;
  GtkTextIter    start, end;
  gint           columns = 1;
  gunichar       c;

  /* set iters */
  start = end = *iter;

  if (gtk_text_iter_starts_line (iter))
    {
       /* set number of columns */
       columns = tab ? view->tab_width : 1;

       /* walk until we've removed enough columns */
       while (columns > 0)
         {
           /* get the character */
           c = gtk_text_iter_get_char (&end);

           if (c == '\t')
             columns -= view->tab_width;
           else if (c == ' ')
             columns--;
           else
             break;

           /* go to the next character */
           gtk_text_iter_forward_char (&end);
         }
    }
  else
    {
      /* walk backwards until we've removed enough columns */
      while (columns > 0
             && !gtk_text_iter_starts_line (iter)
             && gtk_text_iter_backward_char (iter))
        {
          /* get character */
          c = gtk_text_iter_get_char (iter);

          if (c == '\t')
            columns -= view->tab_width;
          else if (c == ' ')
             columns--;
           else
             break;

          /* update the start iter */
          start = *iter;
        }
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
                                gboolean      increase,
                                gboolean      tab)
{
  GtkTextBuffer *buffer;
  GtkTextIter    start_iter, end_iter;
  GSList        *li;
  gint           start_line, end_line;
  gint           i;

  /* get the textview buffer */
  buffer = mousepad_view_get_buffer (view);

  /* begin a user action */
  gtk_text_buffer_begin_user_action (buffer);

  if (view->marks != NULL)
    {
      for (li = view->marks; li != NULL; li = li->next)
        {
          /* get the iter */
          gtk_text_buffer_get_iter_at_mark (buffer, &start_iter, li->data);

          /* only indent when the iter does not start a line */
          if (!gtk_text_iter_starts_line (&start_iter))
            {
              /* increase or decrease the indentation */
              if (increase)
                mousepad_view_increase_indent_iter (view, &start_iter, tab);
              else
                mousepad_view_decrease_indent_iter (view, &start_iter, tab);
            }

          /* skip end mark in the list (always pairs of two) */
          li = li->next;
        }
    }
  else if (gtk_text_buffer_get_selection_bounds (buffer, &start_iter, &end_iter))
    {
      /* get start and end line */
      start_line = gtk_text_iter_get_line (&start_iter);
      end_line = gtk_text_iter_get_line (&end_iter);

      /* only change indentation when an entire line is selected or multiple lines */
      if (start_line != end_line
          || (gtk_text_iter_starts_line (&start_iter)
              && gtk_text_iter_ends_line (&end_iter)))
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
                mousepad_view_increase_indent_iter (view, &start_iter, tab);
              else
                mousepad_view_decrease_indent_iter (view, &start_iter, tab);
            }
        }
    }

  /* end user action */
  gtk_text_buffer_end_user_action (buffer);

  /* put cursor on screen */
  mousepad_view_put_cursor_on_screen (view);
}



static gchar *
mousepad_view_indentation_string (GtkTextBuffer     *buffer,
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



static void
mousepad_view_clipboard (MousepadView *view,
                         GtkClipboard *clipboard,
                         gboolean      remove)
{
  GString       *string = NULL;
  GSList        *li;
  gint           ln, previous_ln = -1;
  gchar         *slice;
  GtkTextBuffer *buffer;
  GtkTextMark   *mark_start, *mark_end;
  GtkTextIter    iter_start, iter_end;

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* create a new string */
  if (clipboard)
    string = g_string_new (NULL);

  /* begin user action */
  gtk_text_buffer_begin_user_action (buffer);

  /* walk through the marks */
  for (li = view->marks; li != NULL; li = li->next)
    {
      /* get the start mark */
      mark_start = li->data;

      /* jump to next item in the list */
      li = li->next;

      /* get end mark */
      mark_end = li->data;

      /* and their iter positions */
      gtk_text_buffer_get_iter_at_mark (buffer, &iter_start, mark_start);
      gtk_text_buffer_get_iter_at_mark (buffer, &iter_end, mark_end);

      /* only handle the selection when there is clipbaord */
      if (clipboard)
        {
          /* the line number of the iter */
          ln = gtk_text_iter_get_line (&iter_start);

          /* fix the number of new lines between the parts */
          if (previous_ln == -1)
            previous_ln = ln;
          else
            for (; previous_ln < ln; previous_ln++)
              string = g_string_append_c (string, '\n');

          /* get the text slice */
          slice = gtk_text_buffer_get_slice (buffer, &iter_start, &iter_end, TRUE);

          /* append the slice to the string */
          string = g_string_append (string, slice);

          /* cleanup */
          g_free (slice);
        }

      /* delete the text between the iters if requested */
      if (remove)
        gtk_text_buffer_delete (buffer, &iter_start, &iter_end);
    }

  /* end user action */
  gtk_text_buffer_end_user_action (buffer);

  /* set the clipboard text */
  if (G_LIKELY (clipboard))
    {
      /* set the clipboard text */
      gtk_clipboard_set_text (clipboard, string->str, string->len);

      /* free the string */
      g_string_free (string, TRUE);
    }
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



void
mousepad_view_clipboard_cut (MousepadView *view)
{
  GtkClipboard  *clipboard;
  GtkTextBuffer *buffer;

  _mousepad_return_if_fail (MOUSEPAD_IS_VIEW (view));
  _mousepad_return_if_fail (mousepad_view_get_has_selection (view));

  /* get the clipboard */
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view), GDK_SELECTION_CLIPBOARD);

  if (view->marks)
    {
      /* call the clipboard function with remove bool */
      mousepad_view_clipboard (view, clipboard, TRUE);

      /* cleanup the vertical selection */
      mousepad_view_vertical_selection_clear (view);
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
  _mousepad_return_if_fail (mousepad_view_get_has_selection (view));

  /* get the clipboard */
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view), GDK_SELECTION_CLIPBOARD);

  if (view->marks)
    {
      /* call the clipboard function */
      mousepad_view_clipboard (view, clipboard, FALSE);
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
  _mousepad_return_if_fail (mousepad_view_get_has_selection (view));

  if (view->marks)
    {
      /* remove the text in the vertical selection */
      mousepad_view_clipboard (view, NULL, TRUE);

      /* cleanup the vertical selection */
      mousepad_view_vertical_selection_clear (view);
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

  /* cleanup vertical selection */
  if (view->marks)
    mousepad_view_vertical_selection_clear (view);

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
mousepad_view_set_tab_width (MousepadView *view,
                             gint          tab_width)
{
  PangoTabArray *tab_array;
  gint           layout_width;

  _mousepad_return_if_fail (MOUSEPAD_IS_VIEW (view));
  _mousepad_return_if_fail (GTK_IS_TEXT_VIEW (view));

  /* set the value */
  view->tab_width = tab_width;

  /* get the pixel width of the tab size */
  layout_width = mousepad_view_calculate_layout_width (GTK_WIDGET (view), view->tab_width, ' ');

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
mousepad_view_get_has_selection (MousepadView *view)
{
  GtkTextBuffer *buffer;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_VIEW (view), FALSE);

  /* we have a vertical selection */
  if (view->marks != NULL)
    return TRUE;

  /* get the text buffer */
  buffer = mousepad_view_get_buffer (view);

  /* normal selection */
  return gtk_text_buffer_get_selection_bounds (buffer, NULL, NULL);
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
mousepad_view_get_tab_width (MousepadView *view)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_VIEW (view), -1);

  return view->tab_width;
}



gboolean
mousepad_view_get_insert_spaces (MousepadView *view)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_VIEW (view), FALSE);

  return view->insert_spaces;
}
