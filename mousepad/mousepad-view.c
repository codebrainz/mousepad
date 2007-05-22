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
#include <mousepad/mousepad-view.h>



/* we can assume there is a buffer for this view */
#define mousepad_view_get_buffer(view) (GTK_TEXT_VIEW(view)->buffer)



/* class functions */
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

/* vertical selection functions */
static GArray   *mousepad_view_vertical_selection_array      (MousepadView       *view,
                                                              gboolean            forced);
static gboolean  mousepad_view_vertical_selection_timeout    (gpointer            user_data);
static void      mousepad_view_vertical_selection_reset      (MousepadView       *view);

/* indentation functions */
static void      mousepad_view_indent_line                   (GtkTextBuffer      *buffer,
                                                              GtkTextIter        *iter,
                                                              gboolean            increase,
                                                              gboolean            tab);
static gboolean  mousepad_view_indent_lines                  (MousepadView       *view,
                                                              gboolean            indent,
                                                              gboolean            tab);
static gchar    *mousepad_view_compute_indentation           (GtkTextBuffer      *buffer,
                                                              const GtkTextIter  *iter);

/* vertical selection edit handler */
static void      mousepad_view_handle_clipboard                 (MousepadView       *view,
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
  GPtrArray   *marks;

  /* vertical selection timeout id */
  guint        vertical_selection_id;

  /* the selection tag */
  GtkTextTag  *tag;

  /* settings */
  guint        auto_indent : 1;
  guint        line_numbers : 1;
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
  /* initialize */
  view->auto_indent = view->line_numbers = FALSE;

  /* initialize */
  view->marks = NULL;
  view->tag = NULL;

  /* initialize */
  view->vertical_start_x = view->vertical_start_y =
  view->vertical_end_x = view->vertical_end_y = -1;
  view->vertical_selection_id = 0;
}



static void
mousepad_view_finalize (GObject *object)
{
  MousepadView *view = MOUSEPAD_VIEW (object);

  /* stop a pending vertical selection timeout */
  if (G_UNLIKELY (view->vertical_selection_id != 0))
    g_source_remove (view->vertical_selection_id);

  /* free the marks array (we're not owning a ref on the marks) */
  if (G_UNLIKELY (view->marks != NULL))
    g_ptr_array_free (view->marks, TRUE);

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
  GtkStyle     *style;
  MousepadView *view = MOUSEPAD_VIEW (widget);

  /* only set the style once, because we don't care to restart
   * after a theme change */
  if (view->tag == NULL && GTK_WIDGET_VISIBLE (widget))
    {
      /* get the textview style */
      style = gtk_widget_get_style (widget);

      /* create the selection tag */
      view->tag = gtk_text_buffer_create_tag (mousepad_view_get_buffer (view), NULL,
                                              "background-gdk", &style->base[GTK_STATE_SELECTED],
                                              "foreground-gdk", &style->text[GTK_STATE_SELECTED],
                                              NULL);
    }

  (*GTK_WIDGET_CLASS (mousepad_view_parent_class)->style_set) (widget, previous_style);
}



static gboolean
mousepad_view_key_press_event (GtkWidget   *widget,
                               GdkEventKey *event)
{
  MousepadView  *view = MOUSEPAD_VIEW (widget);
  GtkTextBuffer *buffer;
  GtkTextIter    start;
  GtkTextMark   *mark;
  guint          modifiers;
  gchar         *string;
  gboolean       key_is_tab = FALSE;

  /* get the modifiers state */
  modifiers = event->state & gtk_accelerator_get_default_mod_mask ();

  /* handle the key event */
  switch (event->keyval)
    {
      case GDK_Return:
      case GDK_KP_Enter:
        if (!(event->state & GDK_SHIFT_MASK) && view->auto_indent)
          {
            /* get the textview buffer */
            buffer = mousepad_view_get_buffer (view);

            /* get the iter position of the cursor */
            mark = gtk_text_buffer_get_insert (buffer);
            gtk_text_buffer_get_iter_at_mark (buffer, &start, mark);

            /* get the string of tabs and spaces we're going to indent */
            string = mousepad_view_compute_indentation (buffer, &start);

            if (string != NULL)
              {
                /* insert the indent characters */
                gtk_text_buffer_insert (buffer, &start, "\n", 1);
                gtk_text_buffer_insert (buffer, &start, string, strlen (string));

                /* cleanup */
                g_free (string);

                /* make sure the new string is visible for the user */
                gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (widget), mark);

                /* we inserted the new line, nothing to do for gtk */
                return TRUE;
              }
          }
        break;

      case GDK_Tab:
      case GDK_KP_Tab:
      case GDK_ISO_Left_Tab:
        /* indent a tab when no modifiers are pressed, else fall-through */
        if (modifiers == 0 &&
            mousepad_view_indent_lines (view, TRUE, TRUE))
              return TRUE;

        /* a tab was pressed */
        key_is_tab = TRUE;

      case GDK_BackSpace:
      case GDK_space:
        /* indent on Space unindent on Backspace or Tab */
        if (modifiers == GDK_SHIFT_MASK &&
            mousepad_view_indent_lines (view, (event->keyval == GDK_space), key_is_tab))
              return TRUE;
        break;
    }

  /* reset the vertical selection */
  if (G_UNLIKELY (view->marks != NULL && !event->is_modifier))
    mousepad_view_vertical_selection_reset (view);

  return (*GTK_WIDGET_CLASS (mousepad_view_parent_class)->key_press_event) (widget, event);
}



static gboolean
mousepad_view_button_press_event (GtkWidget      *widget,
                                  GdkEventButton *event)
{
  MousepadView *view = MOUSEPAD_VIEW (widget);
  GtkTextView  *text_view;

  /* cleanup the marks if needed */
  if (G_UNLIKELY (view->marks != NULL))
    {
      /* reset the vertical selection */
      mousepad_view_vertical_selection_reset (view);
    }

  /* Shift + Ctrl + Button 1 are pressed */
  if (G_UNLIKELY (event->type == GDK_BUTTON_PRESS
                  && event->button == 1
                  && (event->state & GDK_CONTROL_MASK)
                  && (event->state & GDK_SHIFT_MASK)
                  && event->x >= 0
                  && event->y >= 0))
    {
      /* get the textview */
      text_view = GTK_TEXT_VIEW (widget);

      /* set the vertical selection start position, inclusing textview offset */
      view->vertical_start_x = event->x + text_view->xoffset;
      view->vertical_start_y = event->y + text_view->yoffset;

      /* start a vertical selection timeout */
      view->vertical_selection_id = g_timeout_add (75, mousepad_view_vertical_selection_timeout, view);

      return TRUE;
    }

  return (*GTK_WIDGET_CLASS (mousepad_view_parent_class)->button_press_event) (widget, event);
}



static gboolean
mousepad_view_button_release_event (GtkWidget      *widget,
                                    GdkEventButton *event)
{
  MousepadView  *view = MOUSEPAD_VIEW (widget);
  GArray        *array;
  gint           i;
  GtkTextBuffer *buffer;
  GtkTextIter    iter;
  GtkTextMark   *mark;

  _mousepad_return_val_if_fail (view->marks == NULL, TRUE);

  /* end of a vertical selection */
  if (G_UNLIKELY (view->vertical_start_x != -1
                  && event->button == 1))
    {
      /* stop the timeout */
      g_source_remove (view->vertical_selection_id);
      view->vertical_selection_id = 0;

      /* get the array */
      array = mousepad_view_vertical_selection_array (view, TRUE);

      if (G_LIKELY (array != NULL))
        {
          /* get the buffer */
          buffer = mousepad_view_get_buffer (view);

          /* allocate the array */
          view->marks = g_ptr_array_sized_new (array->len);

          /* walk though the iters in the array */
          for (i = 0; i < array->len; i++)
            {
              /* get the iter from the array */
              iter = g_array_index (array, GtkTextIter, i);

              /* create the mark */
              mark = gtk_text_buffer_create_mark (buffer, NULL, &iter, FALSE);

              /* append the mark to the array */
              g_ptr_array_add (view->marks, mark);
            }

          /* free the array */
          g_array_free (array, TRUE);

          /* notify the has-selection signal */
          buffer->has_selection = TRUE;
          g_object_notify (G_OBJECT (buffer), "has-selection");
        }

      return TRUE;
    }

  return (*GTK_WIDGET_CLASS (mousepad_view_parent_class)->button_release_event) (widget, event);
}



/**
 * Vertical Selection Functions
 **/
static GArray *
mousepad_view_vertical_selection_array (MousepadView *view,
                                        gboolean      forced)
{
  gint           x, y, i;
  gint           start_y, end_y, height;
  GArray        *array = NULL;
  GtkTextIter    start, end;
  GtkTextView   *text_view = GTK_TEXT_VIEW (view);

  /* leave when there is no start coordinate */
  if (G_UNLIKELY (view->vertical_start_x == -1))
    return NULL;

  /* get the cursor position */
  gdk_window_get_pointer (gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT),
                          &x, &y, NULL);

  /* no negative values, that sucks */
  if (G_LIKELY (x >= 0 && y >= 0))
    {
      /* get the buffer coordinates */
      x += text_view->xoffset;
      y += text_view->yoffset;

      /* quick check if we need to update the selection */
      if (G_LIKELY (forced || view->vertical_end_x != x || view->vertical_end_y != y))
        {
          /* update the end coordinates */
          view->vertical_end_x = x;
          view->vertical_end_y = y;

          /* get the start and end iter */
          gtk_text_view_get_iter_at_location (text_view, &start, 0, view->vertical_start_y);
          gtk_text_view_get_iter_at_location (text_view, &end, 0, view->vertical_end_y);

          /* make sure the start iter is before the end iter */
          gtk_text_iter_order (&start, &end);

          /* get the start and end location in pixels */
          gtk_text_view_get_line_yrange (text_view, &start, &start_y, &height);
          gtk_text_view_get_line_yrange (text_view, &end, &end_y, NULL);

          /* allocate a sized array to avoid reallocation (array might be too big if there are equal iters) */
          array = g_array_sized_new (FALSE, FALSE, sizeof (GtkTextIter), (end_y - start_y) / height * 2);

          /* make sure atleast one line is selected */
          end_y += height;

          /* walk through the selected lines */
          for (i = start_y; i < end_y; i += height)
            {
              /* get the begin and end iter */
              gtk_text_view_get_iter_at_location (text_view, &start, view->vertical_start_x, i);
              gtk_text_view_get_iter_at_location (text_view, &end, view->vertical_end_x, i);

              /* continue when the iters are the same */
              if (gtk_text_iter_equal (&start, &end))
                continue;

              /* make sure the iters are in the correct order */
              gtk_text_iter_order (&start, &end);

              /* append the iters to the array */
              g_array_append_val (array, start);
              g_array_append_val (array, end);
            }
        }
    }

  return array;
}


static gboolean
mousepad_view_vertical_selection_timeout (gpointer user_data)
{
  gint           i;
  GArray        *array;
  GtkTextIter    start, end;
  GtkTextBuffer *buffer;
  MousepadView  *view = MOUSEPAD_VIEW (user_data);

  GDK_THREADS_ENTER ();

  /* get the iters array */
  array = mousepad_view_vertical_selection_array (view, FALSE);

  /* update the selection highlight */
  if (array != NULL)
    {
      /* get the buffer */
      buffer = mousepad_view_get_buffer (user_data);

      /* remove the old highlight */
      gtk_text_buffer_get_bounds (buffer, &start, &end);
      gtk_text_buffer_remove_tag (buffer, view->tag, &start, &end);

      /* walk though the iters in the array */
      for (i = 0; i < array->len; i += 2)
        {
          /* get the iters from the array */
          start = g_array_index (array, GtkTextIter, i);
          end   = g_array_index (array, GtkTextIter, i + 1);

          /* highlight the text between the iters */
          gtk_text_buffer_apply_tag (buffer, view->tag, &start, &end);
        }

      /* free the array */
      g_array_free (array, TRUE);

      /* set the cursor below the pointer */
      gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &end, view->vertical_end_x, view->vertical_end_y);
      gtk_text_buffer_place_cursor (buffer, &end);
    }

  GDK_THREADS_LEAVE ();

  /* keep the timeout running */
  return TRUE;
}



static void
mousepad_view_vertical_selection_reset (MousepadView *view)
{
  GtkTextIter    start, end;
  GtkTextBuffer *buffer;
  gint           i;

  _mousepad_return_if_fail (view->marks != NULL);

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  buffer->has_selection = FALSE;
  g_object_notify (G_OBJECT (buffer), "has-selection");

  /* remove the selections */
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  gtk_text_buffer_remove_tag (buffer, view->tag, &start, &end);

  /* reset the coordinates */
  view->vertical_start_x = view->vertical_start_y =
  view->vertical_end_x = view->vertical_end_y = -1;

  /* delete all the marks in the buffer */
  for (i = 0; i < view->marks->len; i++)
    gtk_text_buffer_delete_mark (buffer, g_ptr_array_index (view->marks, i));

  /* cleanup */
  g_ptr_array_free (view->marks, TRUE);
  view->marks = NULL;
}


/**
 * Indentation Functions
 **/
static void
mousepad_view_indent_line (GtkTextBuffer *buffer,
                           GtkTextIter   *iter,
                           gboolean       increase,
                           gboolean       tab)
{
  GtkTextIter end;
  gint        spaces;

  /* increase the indentation */
  if (increase)
    {
      /* insert the tab or a space */
      gtk_text_buffer_insert (buffer, iter, tab ? "\t" : " ", -1);
    }
  else /* decrease the indentation */
    {
      /* set the end iter */
      end = *iter;

      /* remove a tab */
      if (tab && gtk_text_iter_get_char (iter) == '\t')
        {
          /* get the iter after the tab */
          gtk_text_iter_forward_char (&end);

          /* remove the tab */
          gtk_text_buffer_delete (buffer, iter, &end);
        }
      /* remove a space */
      else if (gtk_text_iter_get_char (iter) == ' ')
        {
          /* (re)set the number of spaces */
          spaces = tab ? 0 : 1;

          /* count the number of spaces before the text (when unindenting tabs) */
          while (tab && !gtk_text_iter_ends_line (&end))
            {
              if (gtk_text_iter_get_char (&end) == ' ')
                spaces++;
              else
                break;

              gtk_text_iter_forward_char (&end);
            }

          /* delete the spaces */
          if (spaces > 0)
            {
              /* reset the end iter */
              end = *iter;

              /* correct the number of spaces for matching tabs */
              if (tab)
                {
                  /* the number of spaces we need to remove to be inlign with the tabs */
                  spaces = spaces - (spaces / 8) * 8;

                  /* we're inlign with the tabs, remove 8 spaces */
                  if (spaces == 0)
                    spaces = 8;
                }

              /* delete the spaces */
              gtk_text_iter_forward_chars (&end, spaces);
              gtk_text_buffer_delete (buffer, iter, &end);
            }
        }
    }
}



static gboolean
mousepad_view_indent_lines (MousepadView *view,
                            gboolean      increase,
                            gboolean      tab)
{
  GtkTextBuffer *buffer;
  gint           start_line, end_line;
  gint           i;
  GtkTextIter    start, end, needle;
  GtkTextMark   *mark;
  gboolean       succeed = FALSE;

  /* get the textview buffer */
  buffer = mousepad_view_get_buffer (view);

  /* if we have a vertical selection, we're not handling that here */
  if (view->vertical_start_x != -1)
    {
      /* walk through the start marks (hence the +2) */
      for (i = 0; i < view->marks->len; i += 2)
        {
          /* get the mark */
          mark = g_ptr_array_index (view->marks, i);

          /* get the iter at this mark */
          gtk_text_buffer_get_iter_at_mark (buffer, &start, mark);

          /* first move the iter when we're going to indent */
          if (!increase)
            {
              /* sync the iters */
              needle = end = start;

              /* walk backwards till we hit text */
              while (gtk_text_iter_backward_char (&needle)
                     && !gtk_text_iter_starts_line (&needle))
                {
                  if ((gtk_text_iter_get_char (&needle) == ' ' && !tab)
                      || (gtk_text_iter_get_char (&needle) == '\t' && tab))
                    end = needle;
                  else
                    break;
                }

              /* continue if the old and new iter are the same, this makes sure
               * we don't indent spaces in the block */
              if (gtk_text_iter_equal (&start, &end))
                continue;

              /* set the new start iter */
              start = end;
            }

          /* (de/in)crease the line the line with a tab or a space */
          mousepad_view_indent_line (buffer, &start, increase, tab);
        }

      succeed = TRUE;
    }

  if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    {
      /* only indent when the entire line or multiple lines are selected */
      if (increase
          && !(gtk_text_iter_starts_line (&start) && gtk_text_iter_ends_line (&end))
          && (gtk_text_iter_get_line (&start) == gtk_text_iter_get_line (&end)))
        goto failed;

      /* get the line numbers for the start and end iter */
      start_line = gtk_text_iter_get_line (&start);
      end_line = gtk_text_iter_get_line (&end);

      /* insert a tab in front of each line */
      for (i = start_line; i <= end_line; i++)
        {
          /* get the iter of the line we're going to indent */
          gtk_text_buffer_get_iter_at_line (buffer, &start, i);

          /* don't (un)indent empty lines */
          if (gtk_text_iter_ends_line (&start))
            continue;

          /* change the indentation of this line */
          mousepad_view_indent_line (buffer, &start, increase, tab);
        }

      succeed = TRUE;
    }

failed:
  /* scroll */
  gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (view),
                                      gtk_text_buffer_get_insert (buffer));

  return succeed;
}



static gchar *
mousepad_view_compute_indentation (GtkTextBuffer     *buffer,
                                   const GtkTextIter *iter)
{
  GtkTextIter start, end;
  gint        line;
  gunichar    ch;

  /* get the line of the iter */
  line = gtk_text_iter_get_line (iter);

  /* get the iter of the beginning of this line */
  gtk_text_buffer_get_iter_at_line (buffer, &start, line);

  /* set the end iter */
  end = start;

  /* get the first character */
  ch = gtk_text_iter_get_char (&end);

  /* keep walking till we hit text or a new line */
  while (g_unichar_isspace (ch)
         && ch != '\n'
         && ch != '\r'
         && gtk_text_iter_compare (&end, iter) < 0)
    {
      /* break if we can't step forward */
      if (!gtk_text_iter_forward_char (&end))
        break;

      /* get the next character */
      ch = gtk_text_iter_get_char (&end);
    }

  /* return NULL if the iters are the same */
  if (gtk_text_iter_equal (&start, &end))
    return NULL;

  /* return the text between the iters */
  return gtk_text_iter_get_slice (&start, &end);
}



static void
mousepad_view_handle_clipboard (MousepadView *view,
                                GtkClipboard *clipboard,
                                gboolean      remove)
{
  GString       *string = NULL;
  gint           i;
  gint           ln, previous_ln = 0;
  gchar         *slice;
  GtkTextBuffer *buffer;
  GtkTextMark   *mark_start, *mark_end;
  GtkTextIter    iter_start, iter_end;

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* create a new string */
  if (G_LIKELY (clipboard))
    string = g_string_new (NULL);

  /* walk through the marks */
  for (i = 0; i < view->marks->len; i += 2)
    {
      /* get the marks */
      mark_start = g_ptr_array_index (view->marks, i);
      mark_end   = g_ptr_array_index (view->marks, i + 1);

      /* and their iter positions */
      gtk_text_buffer_get_iter_at_mark (buffer, &iter_start, mark_start);
      gtk_text_buffer_get_iter_at_mark (buffer, &iter_end, mark_end);

      /* only handle the selection when there is clipbaord */
      if (G_LIKELY (clipboard))
        {
          /* the line number of the iter */
          ln = gtk_text_iter_get_line (&iter_start);

          /* fix the number of new lines between the parts */
          if (i == 0)
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

  /* set the clipboard text */
  if (G_LIKELY (clipboard))
    {
      /* set the clipboard text */
      gtk_clipboard_set_text (clipboard, string->str, string->len);

      /* free the string */
      g_string_free (string, TRUE);
    }
}



gboolean
mousepad_view_get_vertical_selection (MousepadView *view)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_VIEW (view), FALSE);

  return (view->marks != NULL);
}



void
mousepad_view_cut_clipboard (MousepadView *view,
                             GtkClipboard *clipboard)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_VIEW (view));
  _mousepad_return_if_fail (view->marks != NULL);
  _mousepad_return_if_fail (GTK_IS_CLIPBOARD (clipboard));

  /* call the clipboard function with remove bool */
  mousepad_view_handle_clipboard (view, clipboard, TRUE);

  /* cleanup the vertical selection */
  mousepad_view_vertical_selection_reset (view);
}



void
mousepad_view_copy_clipboard (MousepadView *view,
                              GtkClipboard *clipboard)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_VIEW (view));
  _mousepad_return_if_fail (view->marks != NULL);
  _mousepad_return_if_fail (GTK_IS_CLIPBOARD (clipboard));

  /* call the clipboard function without remove bool */
  mousepad_view_handle_clipboard (view, clipboard, FALSE);
}



void
mousepad_view_paste_column_clipboard (MousepadView *view,
                                      GtkClipboard *clipboard)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_VIEW (view));
  _mousepad_return_if_fail (GTK_IS_CLIPBOARD (clipboard));

  GtkTextMark   *mark;
  GtkTextIter    iter;
  GtkTextBuffer *buffer;
  GtkTextView   *text_view;
  gchar         *string, **pieces;
  gint           i, y;
  GdkRectangle   rect;

  /* get the text buffer */
  buffer = mousepad_view_get_buffer (view);
  text_view = GTK_TEXT_VIEW (view);

  /* get the clipboard text */
  string = gtk_clipboard_wait_for_text (clipboard);

  if (G_LIKELY (string != NULL))
    {
      /* chop the string into pieces */
      pieces = g_strsplit (string, "\n", -1);

      /* cleanup */
      g_free (string);

      /* get the iter are the cursor position */
      mark = gtk_text_buffer_get_insert (buffer);
      gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

      /* get the buffer location */
      gtk_text_view_get_iter_location (text_view, &iter, &rect);

      /* insert the pieces in the buffer */
      for (i = 0; pieces[i] != NULL; i++)
        {
          /* insert the text in the buffer */
          gtk_text_buffer_insert (buffer, &iter, pieces[i], -1);

          /* don't try to add a new line if we reached the end of the array */
          if (G_UNLIKELY (pieces[i+1] == NULL))
            break;

          /* move the iter to the next line and add a new line if needed */
          if (!gtk_text_iter_forward_line (&iter))
             gtk_text_buffer_insert (buffer, &iter, "\n", 1);

          /* get the y coordinate for this line */
          gtk_text_view_get_line_yrange (text_view, &iter, &y, NULL);

          /* get the iter at the y location of this line and the same x as the cursor */
          gtk_text_view_get_iter_at_location (text_view, &iter, rect.x, y);
        }

      /* cleanup */
      g_strfreev (pieces);

      /* set the cursor at the last iter position */
      gtk_text_buffer_place_cursor (buffer, &iter);
    }
}



void
mousepad_view_delete_selection (MousepadView *view)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_VIEW (view));
  _mousepad_return_if_fail (view->marks != NULL);

  /* set the clipbaord to NULL to remove the selection */
  mousepad_view_handle_clipboard (view, NULL, TRUE);

  /* cleanup the vertical selection */
  mousepad_view_vertical_selection_reset (view);
}



void
mousepad_view_set_show_line_numbers (MousepadView *view,
                                     gboolean      visible)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_VIEW (view));

  if (view->line_numbers != visible)
    {
      /* set the boolean */
      view->line_numbers = visible;

      /* set the left border size */
      gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (view),
                                            GTK_TEXT_WINDOW_LEFT,
                                            visible ? 20 : 0);

      /* make sure there is an expose event */
      gtk_widget_queue_draw (GTK_WIDGET (view));
    }
}



void
mousepad_view_set_auto_indent (MousepadView *view,
                               gboolean      enable)
{
   _mousepad_return_if_fail (MOUSEPAD_IS_VIEW (view));

   /* set the boolean */
   view->auto_indent = enable;
}
