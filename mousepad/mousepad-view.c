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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gdk/gdkkeysyms.h>

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-view.h>



static void      mousepad_view_class_init           (MousepadViewClass  *klass);
static void      mousepad_view_init                 (MousepadView       *view);
static void      mousepad_view_finalize             (GObject            *object);
static gboolean  mousepad_view_key_press_event      (GtkWidget          *widget,
                                                     GdkEventKey        *event);
static void      mousepad_view_indent_lines         (MousepadView       *view,
                                                     GtkTextIter        *start,
                                                     GtkTextIter        *end,
                                                     gboolean            indent);
static gchar    *mousepad_view_compute_indentation  (MousepadView       *view,
                                                     GtkTextIter        *iter);
static gint      mousepad_view_expose               (GtkWidget          *widget,
                                                     GdkEventExpose     *event);
static void      mousepad_view_get_lines            (GtkTextView        *text_view,
                                                     gint                first_y,
                                                     gint                last_y,
                                                     GArray            **pixels,
                                                     GArray            **numbers,
                                                     gint               *countp);



struct _MousepadViewClass
{
  GtkTextViewClass __parent__;
};

struct _MousepadView
{
  GtkTextView  __parent__;

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
  widget_class->key_press_event = mousepad_view_key_press_event;
  widget_class->expose_event = mousepad_view_expose;
}



static void
mousepad_view_init (MousepadView *view)
{
  /* empty */
}



static void
mousepad_view_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (mousepad_view_parent_class)->finalize) (object);
}



/**
 * mousepad_view_key_press_event:
 * @widget : A #GtkWidget.
 * @event  : A #GdkEventKey
 *
 * This function is triggered when the user pressed a key in the
 * #MousepadView. It checks if we need to auto indent the new line
 * or when multiple lines are selected we (un)indent all of them.
 *
 * Return value: %TRUE if we handled the event, else we trigger the
 *               parent class so Gtk can handle the event.
 **/
static gboolean
mousepad_view_key_press_event (GtkWidget   *widget,
                               GdkEventKey *event)
{
  MousepadView  *view = MOUSEPAD_VIEW (widget);
  GtkTextBuffer *buffer;
  GtkTextIter    start, end;
  GtkTextMark   *mark;
  guint          modifiers;
  guint          key = event->keyval;
  gchar         *string;
  gboolean       has_selection;

  /* get the textview buffer */
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));

  /* check if we have to indent when the user pressed enter */
  if ((key == GDK_Return || key == GDK_KP_Enter) &&
      !(event->state & GDK_SHIFT_MASK) && view->auto_indent)
    {
      /* get the iter */
      mark = gtk_text_buffer_get_insert (buffer);
      gtk_text_buffer_get_iter_at_mark (buffer, &start, mark);

      /* get the string of tabs and spaces we're going to indent */
      string = mousepad_view_compute_indentation (view, &start);

      if (string != NULL)
        {
          /* tell others we're going to change the buffer */
          gtk_text_buffer_begin_user_action (buffer);

          /* insert the indent characters */
          gtk_text_buffer_insert (buffer, &start, "\n", 1);
          gtk_text_buffer_insert (buffer, &start, string, strlen (string));
          g_free (string);

          /* we're finished */
          gtk_text_buffer_end_user_action (buffer);

          /* make sure the new string is visible for the user */
          gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (widget), mark);

          return TRUE;
        }
    }

  /* get the modifiers state */
  modifiers = gtk_accelerator_get_default_mod_mask ();

  /* check if the user pressed the tab button for indenting lines */
  if ((key == GDK_Tab || key == GDK_KP_Tab || key == GDK_ISO_Left_Tab) &&
      ((event->state & modifiers) == 0 || (event->state & modifiers) == GDK_SHIFT_MASK))
        {
          /* get the selected text */
          has_selection = gtk_text_buffer_get_selection_bounds (buffer, &start, &end);

          /* Shift + Tab */
          if (event->state & GDK_SHIFT_MASK)
            {
              /* unindent */
              mousepad_view_indent_lines (view, &start, &end, FALSE);

              return TRUE;
            }

          if (has_selection &&
              ((gtk_text_iter_starts_line (&start) && gtk_text_iter_ends_line (&end)) ||
               (gtk_text_iter_get_line (&start) != gtk_text_iter_get_line (&end))))
            {
              /* indent */
              mousepad_view_indent_lines (view, &start, &end, TRUE);

              return TRUE;
            }
        }

  return (*GTK_WIDGET_CLASS (mousepad_view_parent_class)->key_press_event) (widget, event);
}



/**
 * mousepad_view_indent_lines:
 * @view   : A #MousepadView
 * @start  : The start #GtkTextIter of the selection.
 * @end    : The end #GtkTextIter of the selection
 * @indent : Whether we need to indent or unindent the
 *           selected lines.
 *
 * This function (un)indents the selected lines. It simply walks
 * from the start line number to the end line number and prepends
 * or removed a tab (or the tab width in spaces).
 **/
static void
mousepad_view_indent_lines (MousepadView *view,
                            GtkTextIter  *start,
                            GtkTextIter  *end,
                            gboolean      indent)
{
  GtkTextBuffer *buffer;
  GtkTextIter    iter, iter2;
  gint           start_line, end_line;
  gint           i, spaces, tabs;

  /* get the textview buffer */
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

  /* get the line numbers for the start and end iter */
  start_line = gtk_text_iter_get_line (start);
  end_line = gtk_text_iter_get_line (end);

  /* check if the last line does not exist of 'invisible' characters */
  if ((gtk_text_iter_get_visible_line_offset (end) == 0) && (end_line > start_line))
    end_line--;

  /* tell everybody we're going to change the buffer */
  gtk_text_buffer_begin_user_action (buffer);

  /* insert a tab in front of each line */
  for (i = start_line; i <= end_line; i++)
    {
      /* get the iter of the line we're going to indent */
      gtk_text_buffer_get_iter_at_line (buffer, &iter, i);

      /* don't (un)indent empty lines */
      if (gtk_text_iter_ends_line (&iter))
        continue;

      if (indent)
        {
          /* insert the tab */
          gtk_text_buffer_insert (buffer, &iter, "\t", -1);
        }
      else /* unindent */
        {
          if (gtk_text_iter_get_char (&iter) == '\t')
            {
              /* remove the tab */
              iter2 = iter;
              gtk_text_iter_forward_char (&iter2);
              gtk_text_buffer_delete (buffer, &iter, &iter2);
            }
          else if (gtk_text_iter_get_char (&iter) == ' ')
            {
              spaces = 0;

              iter2 = iter;

              /* count the number of spaces before the text */
              while (!gtk_text_iter_ends_line (&iter2))
                {
                  if (gtk_text_iter_get_char (&iter2) == ' ')
                    spaces++;
                  else
                    break;

                  gtk_text_iter_forward_char (&iter2);
                }

              if (spaces > 0)
                {
                  /* the number of tabs in the spaces */
                  tabs = spaces / 8;

                  /* the number of spaces after the 'tabs' */
                  spaces = spaces - (tabs * 8);

                  /* we're going to remove the number of spaces after the 'tabs'
                   * or 1 'tab' */
                  if (spaces == 0)
                    spaces = 8;

                  iter2 = iter;

                  /* delete the spaces */
                  gtk_text_iter_forward_chars (&iter2, spaces);
                  gtk_text_buffer_delete (buffer, &iter, &iter2);
                }
            }
        }
    }

  /* we're done */
  gtk_text_buffer_end_user_action (buffer);

  /* scroll */
  gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (view),
                                      gtk_text_buffer_get_insert (buffer));
}



/**
 * mousepad_view_compute_indentation:
 * @view : A #MousepadView.
 * @iter : The #GtkTextIter of the previous line.
 *
 * This function is used to get the tabs and spaces we need to
 * append when auto indentation is enabled. If scans the line of
 * @iter and returns a slice of the tabs and spaces before the
 * first character or line end.
 *
 * Return value: A string of tabs and spaces or %NULL when the
 *               previous line has no tabs or spaces before the text.
 **/
static gchar *
mousepad_view_compute_indentation (MousepadView *view,
                                   GtkTextIter  *iter)
{
  GtkTextBuffer *buffer;
  GtkTextIter    start, end;
  gint           line;
  gunichar       ch;

  /* get the buffer */
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

  /* get the line of the iter */
  line = gtk_text_iter_get_line (iter);

  /* get the iter of the beginning of this line */
  gtk_text_buffer_get_iter_at_line (buffer, &start, line);

  /* clone baby */
  end = start;

  /* get the first character */
  ch = gtk_text_iter_get_char (&end);

  /* keep walking till we hit text or a new line */
  while (g_unichar_isspace (ch) &&
         (ch != '\n') && (ch != '\r') &&
         (gtk_text_iter_compare (&end, iter) < 0))
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



/**
 * mousepad_view_get_lines:
 * @text_view : A #GtkTextView
 * @first_y   : The first y location of the visible #GtkTextView window.
 * @last_y    : The last y location of the visible #GtkTextView window.
 * @pixels    : Return location for the #GArray with the y location
 *              in pixels we need to draw line numbers.
 * @numbers   : Return location for the #GArray with all the visible
 *              line numbers.
 * @countp    : Return location for the number of lines visible between
 *              @first_y and @last_y.
 *
 * This function returns two arrays with the lines number and there draw
 * location of the visible text view window.
 **/
static void
mousepad_view_get_lines (GtkTextView  *text_view,
                         gint          first_y,
                         gint          last_y,
                         GArray      **pixels,
                         GArray      **numbers,
                         gint         *countp)
{
  GtkTextIter iter;
  gint        count = 0;
  gint        y, height;
  gint        line_number = 0, last_line_number;

  g_array_set_size (*pixels, 0);
  g_array_set_size (*numbers, 0);

  /* get iter at first y */
  gtk_text_view_get_line_at_y (text_view, &iter, first_y, NULL);

  /* for each iter, get its location and add it to the arrays.
   * Stop when we pass last_y */
  while (!gtk_text_iter_is_end (&iter))
    {
      /* get the y position of the iter */
      gtk_text_view_get_line_yrange (text_view, &iter, &y, &height);

      /* get the number of the line */
      line_number = gtk_text_iter_get_line (&iter);

      /* append both values to the array */
      g_array_append_val (*pixels, y);
      g_array_append_val (*numbers, line_number);

      ++count;

      /* stop when we reached last_y */
      if ((y + height) >= last_y)
        break;

      /* jump to the next line */
      gtk_text_iter_forward_line (&iter);
    }

  if (gtk_text_iter_is_end (&iter))
    {
      gtk_text_view_get_line_yrange (text_view, &iter, &y, &height);

      last_line_number = gtk_text_iter_get_line (&iter);

      if (last_line_number != line_number)
        {
          g_array_append_val (*pixels, y);
          g_array_append_val (*numbers, last_line_number);
          ++count;
        }
    }

  /* not lines in the buffer? weird, there is atleast one... */
  if (G_UNLIKELY (count == 0))
    {
      y = 0;
      g_array_append_val (*pixels, y);
      g_array_append_val (*numbers, y);

      ++count;
    }

  *countp = count;
}



/**
 * mousepad_view_expose:
 * @widget : A textview widget.
 * @event  : A #GdkEventExpose event.
 *
 * This function draws the line numbers in the GTK_TEXT_WINDOW_LEFT window
 * when it receives an expose event. At the end of the function we always
 * return the expose_event of the parent class so Gtk can draw the text.
 *
 * Return value: The return value of the parent_class.
 **/
static gboolean
mousepad_view_expose (GtkWidget      *widget,
                      GdkEventExpose *event)
{
  GdkWindow   *window;
  GtkTextView *view = GTK_TEXT_VIEW (widget);
  gint         y1, y2;
  GArray      *numbers, *pixels;
  gint         count, i;
  PangoLayout *layout;
  gchar        str[8]; /* maximum of 10e6 lines */
  gint         width;
  gint         position;
  gint         line_number;

  /* get the left window */
  window = gtk_text_view_get_window (view, GTK_TEXT_WINDOW_LEFT);

  /* check if the event if for the left window */
  if (event->window == window)
    {
      /* get the expose event area */
      y1 = event->area.y;
      y2 = y1 + event->area.height;

      /* correct then to the size of the textview window */
      gtk_text_view_window_to_buffer_coords (view, GTK_TEXT_WINDOW_LEFT,
                                             0, y1, NULL, &y1);
      gtk_text_view_window_to_buffer_coords (view, GTK_TEXT_WINDOW_LEFT,
                                             0, y2, NULL, &y2);

      /* create new arrays */
      pixels  = g_array_new (FALSE, FALSE, sizeof (gint));
      numbers = g_array_new (FALSE, FALSE, sizeof (gint));

      /* get the line numbers and y coordinates. */
      mousepad_view_get_lines (view,
                               y1, y2,
                               &pixels,
                               &numbers,
                               &count);

      /* create the pango layout */
      g_snprintf (str, sizeof (str), "%d",
                  MAX (99, gtk_text_buffer_get_line_count (view->buffer)));
      layout = gtk_widget_create_pango_layout (widget, str);
      pango_layout_get_pixel_size (layout, &width, NULL);
      pango_layout_set_width (layout, width);
      pango_layout_set_alignment (layout, PANGO_ALIGN_RIGHT);

      /* set the width of the left window border */
      gtk_text_view_set_border_window_size (view, GTK_TEXT_WINDOW_LEFT,
                                            width + 6);

      for (i = 0; i < count; i++)
        {
          gtk_text_view_buffer_to_window_coords (view,
                                                 GTK_TEXT_WINDOW_LEFT, 0,
                                                 g_array_index (pixels, gint, i),
                                                 NULL,
                                                 &position);

          /* get the line number */
          line_number = g_array_index (numbers, gint, i) + 1;

          /* create the pango layout */
          g_snprintf (str, sizeof (str), "%d", line_number);
          pango_layout_set_markup (layout, str, strlen (str));

          /* time to draw the layout in the window */
          gtk_paint_layout (widget->style, window,
                            GTK_WIDGET_STATE (widget),
                            FALSE, NULL,
                            widget, NULL,
                            width + 2,
                            position,
                            layout);
        }

      /* cleanup */
      g_array_free (pixels, TRUE);
      g_array_free (numbers, TRUE);

      g_object_unref (G_OBJECT (layout));
    }

  /* Gtk can draw the text now */
  return (*GTK_WIDGET_CLASS (mousepad_view_parent_class)->expose_event) (widget, event);
}



/**
 * mousepad_view_set_show_line_numbers:
 * @view    : A #MousepadView
 * @visible : Boolean whether we show or hide the line
 *            numbers.
 *
 * Whether line numbers are visible in the @view.
 **/
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



/**
 * mousepad_view_set_auto_indent:
 * @view    : A #MousepadView
 * @visible : Boolean whether we enable or disable
 *            auto indentation.
 *
 * Whether we enable auto indentation.
 **/
void
mousepad_view_set_auto_indent (MousepadView *view,
                               gboolean      enable)
{
   _mousepad_return_if_fail (MOUSEPAD_IS_VIEW (view));

   /* set the boolean */
   view->auto_indent = enable;
}
