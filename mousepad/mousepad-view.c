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
#include <mousepad/mousepad-gtkcompat.h>
#include <mousepad/mousepad-settings.h>
#include <mousepad/mousepad-util.h>
#include <mousepad/mousepad-view.h>

#include <gtksourceview/gtksourcebuffer.h>
#include <gtksourceview/gtksourceview.h>
#include <gtksourceview/gtksourcestylescheme.h>
#include <gtksourceview/gtksourcestyleschememanager.h>

/* GTK+ 3 removed textview->im_context which is used for multi-selection and
 * nobody has re-written multi-selection feature to not use this field
 * so if building with GTK3 or with GSEAL_ENABLE disable the multi-select
 * feature altogether :( */
#if ! GTK_CHECK_VERSION(3, 0, 0) && ! defined(GSEAL_ENABLE)
#define HAVE_MULTISELECT 1
#endif



#define MOUSEPAD_VIEW_DEFAULT_FONT "Monospace"
#define mousepad_view_get_buffer(view) (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)))



static void      mousepad_view_finalize                      (GObject            *object);
static void      mousepad_view_set_property                  (GObject            *object,
                                                              guint               prop_id,
                                                              const GValue       *value,
                                                              GParamSpec         *pspec);
static void      mousepad_view_get_property                  (GObject            *object,
                                                              guint               prop_id,
                                                              GValue             *value,
                                                              GParamSpec         *pspec);
#ifdef HAVE_MULTISELECT
static void      mousepad_view_style_set                     (GtkWidget          *widget,
                                                              GtkStyle           *previous_style);
static gint      mousepad_view_expose                        (GtkWidget          *widget,
                                                              GdkEventExpose     *event);
#endif
static gboolean  mousepad_view_key_press_event               (GtkWidget          *widget,
                                                              GdkEventKey        *event);
#ifdef HAVE_MULTISELECT
static gboolean  mousepad_view_button_press_event            (GtkWidget          *widget,
                                                              GdkEventButton     *event);
static gboolean  mousepad_view_button_release_event          (GtkWidget          *widget,
                                                              GdkEventButton     *event);
static gboolean  mousepad_view_selection_word_range          (const GtkTextIter  *iter,
                                                              GtkTextIter        *range_start,
                                                              GtkTextIter        *range_end);
static void      mousepad_view_selection_key_press_event     (MousepadView       *view,
                                                              const gchar        *text,
                                                              guint               keyval,
                                                              guint               modifiers);
static void      mousepad_view_commit_handler                (GtkIMContext       *context,
                                                              const gchar        *str,
                                                              MousepadView       *view);
static void      mousepad_view_selection_delete_content      (MousepadView       *view);
static void      mousepad_view_selection_destroy             (MousepadView       *view);
static void      mousepad_view_selection_draw                (MousepadView       *view,
                                                              gboolean            append);
static gboolean  mousepad_view_selection_timeout             (gpointer            user_data);
static void      mousepad_view_selection_timeout_destroy     (gpointer            user_data);
static gchar    *mousepad_view_selection_string              (MousepadView       *view);
#endif
static void      mousepad_view_indent_increase               (MousepadView       *view,
                                                              GtkTextIter        *iter);
static void      mousepad_view_indent_decrease               (MousepadView       *view,
                                                              GtkTextIter        *iter);
static void      mousepad_view_indent_selection              (MousepadView       *view,
                                                              gboolean            increase,
                                                              gboolean            force);
#ifdef HAVE_MULTISELECT
static void      mousepad_view_transpose_multi_selection     (GtkTextBuffer       *buffer,
                                                              MousepadView        *view);
#endif
static void      mousepad_view_transpose_range               (GtkTextBuffer       *buffer,
                                                              GtkTextIter         *start_iter,
                                                              GtkTextIter         *end_iter);
static void      mousepad_view_transpose_lines               (GtkTextBuffer       *buffer,
                                                              GtkTextIter         *start_iter,
                                                              GtkTextIter         *end_iter);
static void      mousepad_view_transpose_words               (GtkTextBuffer       *buffer,
                                                              GtkTextIter         *iter);
static void      mousepad_view_update_font                   (MousepadView        *view);



struct _MousepadViewClass
{
  GtkSourceViewClass __parent__;
};

struct _MousepadView
{
  GtkSourceView         __parent__;

  /* the selection style tag */
  GtkTextTag           *selection_tag;

  /* list with all the selection marks */
  GSList               *selection_marks;

  /* selection timeout id */
  guint                 selection_timeout_id;

  /* coordinates for the selection */
  gint                  selection_start_x;
  gint                  selection_start_y;
  gint                  selection_end_x;
  gint                  selection_end_y;

  /* length of the selection (-1 = zerro width selection, 0 = no selection) */
  gint                  selection_length;

  /* if the selection is in editing mode */
  guint                 selection_editing : 1;

  /* the font used in the view */
  gchar                *font_name;
  PangoFontDescription *font_desc;

  /* whitespace visualization */
  gboolean              show_whitespace;
  gboolean              show_line_endings;

  gchar                *color_scheme;

  gboolean              match_braces;
};



enum
{
  PROP_0,
  PROP_FONT_NAME,
  PROP_SHOW_WHITESPACE,
  PROP_SHOW_LINE_ENDINGS,
  PROP_COLOR_SCHEME,
  PROP_WORD_WRAP,
  PROP_MATCH_BRACES,
  NUM_PROPERTIES
};



G_DEFINE_TYPE (MousepadView, mousepad_view, GTK_SOURCE_TYPE_VIEW)



static void
mousepad_view_class_init (MousepadViewClass *klass)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_view_finalize;
  gobject_class->set_property = mousepad_view_set_property;
  gobject_class->get_property = mousepad_view_get_property;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->key_press_event      = mousepad_view_key_press_event;
#ifdef HAVE_MULTISELECT
  widget_class->expose_event         = mousepad_view_expose;
  widget_class->style_set            = mousepad_view_style_set;
  widget_class->button_press_event   = mousepad_view_button_press_event;
  widget_class->button_release_event = mousepad_view_button_release_event;
#endif

  g_object_class_install_property (
    gobject_class,
    PROP_FONT_NAME,
    g_param_spec_string ("font-name",
                         "FontName",
                         "The name of the font to use in the view",
                         MOUSEPAD_VIEW_DEFAULT_FONT,
                         G_PARAM_READWRITE));

  g_object_class_install_property (
    gobject_class,
    PROP_SHOW_WHITESPACE,
    g_param_spec_boolean ("show-whitespace",
                          "ShowWhitespace",
                          "Whether whitespace is visualized in the view",
                          FALSE,
                          G_PARAM_READWRITE));

  g_object_class_install_property (
    gobject_class,
    PROP_SHOW_LINE_ENDINGS,
    g_param_spec_boolean ("show-line-endings",
                          "ShowLineEndings",
                          "Whether line-endings are visualized in the view",
                          FALSE,
                          G_PARAM_READWRITE));

  g_object_class_install_property (
    gobject_class,
    PROP_COLOR_SCHEME,
    g_param_spec_string ("color-scheme",
                         "ColorScheme",
                         "The id of the syntax highlighting color scheme to use",
                         NULL,
                         G_PARAM_READWRITE));

  g_object_class_install_property (
    gobject_class,
    PROP_WORD_WRAP,
    g_param_spec_boolean ("word-wrap",
                          "WordWrap",
                          "Whether to virtually wrap long lines in the view",
                          FALSE,
                          G_PARAM_READWRITE));

  g_object_class_install_property (
    gobject_class,
    PROP_MATCH_BRACES,
    g_param_spec_boolean ("match-braces",
                          "MatchBraces",
                          "Whether to highlight matching braces, parens, brackets, etc.",
                          FALSE,
                          G_PARAM_READWRITE));
}



static void
mousepad_view_buffer_changed (MousepadView *view,
                              GParamSpec   *pspec,
                              gpointer      user_data)
{
  GtkSourceBuffer *buffer;

  buffer = (GtkSourceBuffer*) gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  if (GTK_SOURCE_IS_BUFFER (buffer))
    {
      GtkSourceStyleSchemeManager *manager;
      GtkSourceStyleScheme        *scheme;

      manager = gtk_source_style_scheme_manager_get_default ();
      scheme = gtk_source_style_scheme_manager_get_scheme (manager,
        view->color_scheme ? view->color_scheme : "");
      gtk_source_buffer_set_style_scheme (buffer, scheme);

      gtk_source_buffer_set_highlight_matching_brackets (buffer, view->match_braces);
    }
}



static void
mousepad_view_use_default_font_setting_changed (MousepadView *view,
                                                gchar        *key,
                                                GSettings    *settings)
{
  mousepad_view_update_font (view);
}



static void
mousepad_view_init (MousepadView *view)
{
  /* initialize selection variables */
  view->selection_timeout_id = 0;
  view->selection_tag = NULL;
  view->selection_marks = NULL;
  view->selection_length = 0;
  view->selection_editing = FALSE;
  view->color_scheme = g_strdup ("none");
  view->font_name = NULL;
  view->font_desc = NULL;
  view->match_braces = FALSE;

  /* make sure any buffers set on the view get the color scheme applied to them */
  g_signal_connect (view,
                    "notify::buffer",
                    G_CALLBACK (mousepad_view_buffer_changed),
                    NULL);

  /* reset drag coordinates */
  view->selection_start_x = view->selection_end_x = -1;
  view->selection_start_y = view->selection_end_y = -1;

#ifdef HAVE_MULTISELECT
  g_signal_connect (GTK_TEXT_VIEW (view)->im_context, "commit",
                    G_CALLBACK (mousepad_view_commit_handler), view);
#endif

  /* bind Gsettings */
#define BIND_(setting, prop) \
  MOUSEPAD_SETTING_BIND (setting, view, prop, G_SETTINGS_BIND_DEFAULT)

  BIND_ (AUTO_INDENT,            "auto-indent");
  BIND_ (FONT_NAME,              "font-name");
  BIND_ (SHOW_WHITESPACE,        "show-whitespace");
  BIND_ (SHOW_LINE_ENDINGS,      "show-line-endings");
  BIND_ (HIGHLIGHT_CURRENT_LINE, "highlight-current-line");
  BIND_ (INDENT_ON_TAB,          "indent-on-tab");
  BIND_ (INDENT_WIDTH,           "indent-width");
  BIND_ (INSERT_SPACES,          "insert-spaces-instead-of-tabs");
  BIND_ (RIGHT_MARGIN_POSITION,  "right-margin-position");
  BIND_ (SHOW_LINE_MARKS,        "show-line-marks");
  BIND_ (SHOW_LINE_NUMBERS,      "show-line-numbers");
  BIND_ (SHOW_RIGHT_MARGIN,      "show-right-margin");
  BIND_ (SMART_HOME_END,         "smart-home-end");
  BIND_ (TAB_WIDTH,              "tab-width");
  BIND_ (COLOR_SCHEME,           "color-scheme");
  BIND_ (WORD_WRAP,              "word-wrap");
  BIND_ (MATCH_BRACES,           "match-braces");

  /* override with default font when the setting is enabled */
  MOUSEPAD_SETTING_CONNECT_OBJECT (USE_DEFAULT_FONT,
                                   G_CALLBACK (mousepad_view_use_default_font_setting_changed),
                                   view,
                                   G_CONNECT_SWAPPED);

#undef BIND_
}



static void
mousepad_view_finalize (GObject *object)
{
  MousepadView *view = MOUSEPAD_VIEW (object);

  /* stop a running selection timeout */
  if (G_UNLIKELY (view->selection_timeout_id != 0))
    g_source_remove (view->selection_timeout_id);

  /* free the selection marks list (marks are owned by the buffer) */
  if (G_UNLIKELY (view->selection_marks != NULL))
    g_slist_free (view->selection_marks);

  /* cleanup color scheme name */
  g_free (view->color_scheme);

  (*G_OBJECT_CLASS (mousepad_view_parent_class)->finalize) (object);
}



static void
mousepad_view_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  MousepadView *view = MOUSEPAD_VIEW (object);

  switch (prop_id)
    {
    case PROP_FONT_NAME:
      mousepad_view_set_font_name (view, g_value_get_string (value));
      break;
    case PROP_SHOW_WHITESPACE:
      mousepad_view_set_show_whitespace (view, g_value_get_boolean (value));
      break;
    case PROP_SHOW_LINE_ENDINGS:
      mousepad_view_set_show_line_endings (view, g_value_get_boolean (value));
      break;
    case PROP_COLOR_SCHEME:
      mousepad_view_set_color_scheme (view, g_value_get_string (value));
      break;
    case PROP_WORD_WRAP:
      mousepad_view_set_word_wrap (view, g_value_get_boolean (value));
      break;
    case PROP_MATCH_BRACES:
      mousepad_view_set_match_braces (view, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
mousepad_view_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  MousepadView *view = MOUSEPAD_VIEW (object);

  switch (prop_id)
    {
    case PROP_FONT_NAME:
      g_value_set_string (value, mousepad_view_get_font_name (view));
      break;
    case PROP_SHOW_WHITESPACE:
      g_value_set_boolean (value, mousepad_view_get_show_whitespace (view));
      break;
    case PROP_SHOW_LINE_ENDINGS:
      g_value_set_boolean (value, mousepad_view_get_show_line_endings (view));
      break;
    case PROP_COLOR_SCHEME:
      g_value_set_string (value, mousepad_view_get_color_scheme (view));
      break;
    case PROP_WORD_WRAP:
      g_value_set_boolean (value, mousepad_view_get_word_wrap (view));
      break;
    case PROP_MATCH_BRACES:
      g_value_set_boolean (value, mousepad_view_get_match_braces (view));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



#ifdef HAVE_MULTISELECT
static gboolean
mousepad_view_expose (GtkWidget      *widget,
                      GdkEventExpose *event)
{
  GtkTextView  *textview = GTK_TEXT_VIEW (widget);
  MousepadView *view = MOUSEPAD_VIEW (widget);

  if (G_UNLIKELY (view->selection_length == -1
      && (view->selection_marks != NULL || view->selection_end_x != -1)
      && event->window == gtk_text_view_get_window (textview, GTK_TEXT_WINDOW_TEXT)))
    {
      /* redraw the cursor lines for the vertical selection */
      mousepad_view_selection_draw (view, FALSE);
    }

  /* gtk can draw the text now */
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

      /* redraw selection */
      if (view->selection_marks != NULL)
        mousepad_view_selection_draw (view, FALSE);
    }
}
#endif



static gboolean
mousepad_view_key_press_event (GtkWidget   *widget,
                               GdkEventKey *event)
{
  MousepadView  *view = MOUSEPAD_VIEW (widget);
  GtkTextBuffer *buffer;
  GtkTextIter    iter;
  GtkTextMark   *cursor;
  guint          modifiers;
  gboolean       is_editable;

  /* get the modifiers state */
  modifiers = event->state & gtk_accelerator_get_default_mod_mask ();

  /* get the textview buffer */
  buffer = mousepad_view_get_buffer (view);

  /* whether the textview is editable */
  is_editable = gtk_text_view_get_editable(GTK_TEXT_VIEW (view));

  /* handle the key event */
  switch (event->keyval)
    {
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
             mousepad_view_scroll_to_cursor (view);

             return TRUE;
          }
        break;

      case GDK_Delete:
      case GDK_KP_Delete:
        if (view->selection_marks != NULL && is_editable)
          {
            /* delete or destroy the selection */
            if (view->selection_length > 0)
              mousepad_view_delete_selection (view);
#ifdef HAVE_MULTISELECT
            else
              mousepad_view_selection_destroy (view);
#endif
            return TRUE;
          }
        break;

#ifdef HAVE_MULTISELECT
      case GDK_BackSpace:
        if (view->selection_marks != NULL && is_editable)
          {
            /* backspace in the selection */
            mousepad_view_selection_key_press_event (view, NULL, GDK_BackSpace, modifiers);

            return TRUE;
          }
        break;
#endif

      default:
#ifndef HAVE_MULTISELECT
        break;
#else
        if (G_UNLIKELY (view->selection_marks != NULL && is_editable))
          {
            gboolean im_handled;

            /* block textview updates (from gtk) */
            gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);

            /* handle the im context */
            im_handled = gtk_im_context_filter_keypress (GTK_TEXT_VIEW (view)->im_context, event);

            /* allow textview updates again */
            gtk_text_view_set_editable (GTK_TEXT_VIEW (view), TRUE);

            /* only leave when the im handle the event */
            if (im_handled)
              return TRUE;
          }
#endif
    }

#ifdef HAVE_MULTISELECT
  /* remove the selection when no valid key combination has been pressed */
  if (G_UNLIKELY (view->selection_marks != NULL && event->is_modifier == FALSE))
    mousepad_view_selection_destroy (view);
#endif

  return (*GTK_WIDGET_CLASS (mousepad_view_parent_class)->key_press_event) (widget, event);
}



#ifdef HAVE_MULTISELECT
static void
mousepad_view_commit_handler (GtkIMContext *context,
                              const gchar  *str,
                              MousepadView *view)
{
  g_return_if_fail (MOUSEPAD_IS_VIEW (view));
  g_return_if_fail (GTK_IS_IM_CONTEXT (context));

  /* if there is a selection, insert this string there too */
  if (G_UNLIKELY (view->selection_marks != NULL))
    {
      /* debug check */
      g_return_if_fail (gtk_text_view_get_editable (GTK_TEXT_VIEW (view)) == FALSE);

      /* handle the text input for the multi selection */
      mousepad_view_selection_key_press_event (view, str, 0, 0);
    }
}



static gboolean
mousepad_view_button_press_event (GtkWidget      *widget,
                                  GdkEventButton *event)
{
  MousepadView  *view = MOUSEPAD_VIEW (widget);
  GtkTextView   *textview = GTK_TEXT_VIEW (widget);
  GtkTextIter    iter, start_iter, end_iter;
  GtkTextBuffer *buffer;

  /* destroy old selection */
  if (view->selection_marks != NULL && event->button != 3)
    mousepad_view_selection_destroy (view);

  /* work with vertical selection while ctrl is pressed */
  if (event->state & GDK_CONTROL_MASK
      && event->x >= 0
      && event->y >= 0
      && event->button == 1
      && event->type == GDK_BUTTON_PRESS)
    {
      /* set the vertical selection start position, including textview offset */
      gtk_text_view_window_to_buffer_coords(textview,GTK_TEXT_WINDOW_TEXT,event->x,event->y,&view->selection_start_x,&view->selection_start_y);
      /* hide cursor */
      gtk_text_view_set_cursor_visible (textview, FALSE);

      /* start a vertical selection timeout */
      view->selection_timeout_id = g_timeout_add_full (G_PRIORITY_LOW, 50, mousepad_view_selection_timeout,
                                                       view, mousepad_view_selection_timeout_destroy);

      return TRUE;
    }
  else if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
    {
      /* get the iter under the cursor */
      gint x,y;
      gtk_text_view_window_to_buffer_coords(textview,GTK_TEXT_WINDOW_TEXT,event->x,event->y,&x,&y);
      gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &iter,
                                          x,y);

      /* try to select a whole word or space */
      if (mousepad_view_selection_word_range (&iter, &start_iter, &end_iter))
        {
          /* get the buffer */
          buffer = mousepad_view_get_buffer (view);

          /* select range */
          gtk_text_buffer_select_range (buffer, &end_iter, &start_iter);

          return TRUE;
        }
    }

  return (*GTK_WIDGET_CLASS (mousepad_view_parent_class)->button_press_event) (widget, event);
}



static gboolean
mousepad_view_button_release_event (GtkWidget      *widget,
                                    GdkEventButton *event)
{
  MousepadView  *view = MOUSEPAD_VIEW (widget);

  /* end of a vertical selection */
  if (G_UNLIKELY (view->selection_timeout_id != 0))
    {
      /* stop the timeout */
      g_source_remove (view->selection_timeout_id);

      /* append the selected marks */
      mousepad_view_selection_draw (view, TRUE);

      /* reset the drag coordinates */
      view->selection_start_x = view->selection_end_x = -1;
      view->selection_start_y = view->selection_end_y = -1;
    }

  return (*GTK_WIDGET_CLASS (mousepad_view_parent_class)->button_release_event) (widget, event);
}
#endif



/**
 * Selection Functions
 **/
#ifdef HAVE_MULTISELECT
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
                                         const gchar  *text,
                                         guint         keyval,
                                         guint         modifiers)
{
  GtkTextIter    start_iter, end_iter;
  GSList        *li;
  GtkTextBuffer *buffer;

  g_return_if_fail (view->selection_marks != NULL);
  g_return_if_fail (view->selection_start_x == -1);
  g_return_if_fail (view->selection_end_x == -1);
  g_return_if_fail ((keyval == 0 && text != NULL) || keyval != 0);
  g_return_if_fail (g_slist_length (view->selection_marks) % 2 == 0);

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* begin user action */
  gtk_text_buffer_begin_user_action (buffer);

  /* delete the content of the selection when we're not in editing mode yet */
  if (view->selection_editing == FALSE && view->selection_length > 0)
    mousepad_view_selection_delete_content (view);

  /* enter editing mode */
  view->selection_editing = TRUE;

  for (li = view->selection_marks; li != NULL; li = li->next)
    {
      /* get end iter */
      gtk_text_buffer_get_iter_at_mark (buffer, &end_iter, li->next->data);

      /* handle the event */
      if (keyval == GDK_Tab)
        {
          /* insert a (soft) tab */
          mousepad_view_indent_increase (view, &end_iter);
        }
      else if (keyval == GDK_BackSpace)
        {
          /* get start iter */
          gtk_text_buffer_get_iter_at_mark (buffer, &start_iter, li->data);

          /* only backspace when there is text inside the selection or ctrl is pressed */
          if (!gtk_text_iter_equal (&start_iter, &end_iter) || modifiers == GDK_CONTROL_MASK)
            gtk_text_buffer_backspace (buffer, &end_iter, FALSE, TRUE);
        }
      else if (text != NULL)
        {
          /* insert the text */
          gtk_text_buffer_insert (buffer, &end_iter, text, -1);
        }

      /* go to next element in the list */
      li = li->next;
    }

  /* redraw the content */
  mousepad_view_selection_draw (view, FALSE);

  /* end user action */
  gtk_text_buffer_end_user_action (buffer);
}



static void
mousepad_view_selection_delete_content (MousepadView *view)
{
  GtkTextBuffer *buffer;
  GtkTextIter    start_iter, end_iter;
  GSList        *li;

  g_return_if_fail (view->selection_marks != NULL);
  g_return_if_fail (view->selection_length > 0);
  g_return_if_fail (g_slist_length (view->selection_marks) % 2 == 0);

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* begin user action */
  gtk_text_buffer_begin_user_action (buffer);

  /* remove everything between the marks */
  for (li = view->selection_marks; li != NULL; li = li->next)
    {
      /* get start iter */
      gtk_text_buffer_get_iter_at_mark (buffer, &start_iter, li->data);

      /* next element */
      li = li->next;

      /* get end iter */
      gtk_text_buffer_get_iter_at_mark (buffer, &end_iter, li->data);

      /* delete content between the iters */
      gtk_text_buffer_delete (buffer, &start_iter, &end_iter);
    }

  /* set the selection length to -1 */
  view->selection_length = -1;

  /* end user action */
  gtk_text_buffer_end_user_action (buffer);
}



static void
mousepad_view_selection_destroy (MousepadView *view)
{
  GtkTextBuffer *buffer;
  GSList        *li;
  GtkTextIter    start_iter, end_iter;

  g_return_if_fail (view->selection_marks != NULL);

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* freeze notifications */
  g_object_freeze_notify (G_OBJECT (buffer));

  /* remove the selection tags */
  gtk_text_buffer_get_bounds (buffer, &start_iter, &end_iter);
  gtk_text_buffer_remove_tag (buffer, view->selection_tag, &start_iter, &end_iter);

  /* remove all the selection marks */
  if (G_LIKELY (view->selection_marks))
    {
      /* delete marks from the buffer */
      for (li = view->selection_marks; li != NULL; li = li->next)
        gtk_text_buffer_delete_mark (buffer, li->data);

      /* free the list */
      g_slist_free (view->selection_marks);

      /* null */
      view->selection_marks = NULL;
    }

  /* set selection length to zerro */
  view->selection_length = 0;

  /* unset editing mode */
  view->selection_editing = FALSE;

  /* show cursor again */
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), TRUE);

  /* update buffer status */
  g_object_notify (G_OBJECT (buffer), "has-selection");

  /* allow notifications again */
  g_object_thaw_notify (G_OBJECT (buffer));
}



static void
mousepad_view_selection_draw (MousepadView *view,
                              gboolean      append)
{
  GtkTextBuffer *buffer;
  GtkTextView   *textview = GTK_TEXT_VIEW (view);
  GtkTextIter    start_iter, end_iter;
  GdkWindow     *window;
  gint           y, y_begin, y_height, y_end;
  gint           line_x, line_y;
  GdkRectangle   rect;
  gint           length = 0;
  gint           visible_start_y = 0;
  GtkTextMark   *start_mark, *end_mark;
  GSList        *li;

  g_return_if_fail (MOUSEPAD_IS_VIEW (view));
  g_return_if_fail (view->selection_marks == NULL || g_slist_length (view->selection_marks) % 2 == 0);

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* freeze buffer notifications */
  g_object_freeze_notify (G_OBJECT (buffer));

  /* get the drawable window */
  window = gtk_text_view_get_window (textview, GTK_TEXT_WINDOW_TEXT);

  /* cleanup the old selection */
  if (view->selection_length > 0)
    {
      /* remove the old tags */
      gtk_text_buffer_get_bounds (buffer, &start_iter, &end_iter);
      gtk_text_buffer_remove_tag (buffer, view->selection_tag, &start_iter, &end_iter);
    }
  else if (view->selection_length == -1)
    {
      /* invalidate the window so no retangles are left */
      gtk_widget_queue_draw (GTK_WIDGET (view));
    }

  /* draw the dragging selection or the marks in the list */
  if (view->selection_start_y != -1 && view->selection_end_y != -1)
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

      /* get the visible area */
      if (G_LIKELY (append == FALSE))
        {
          gint xoffset,yoffset;
          gtk_text_view_window_to_buffer_coords(textview,GTK_TEXT_WINDOW_TEXT,
		                                        0,0,&xoffset,&yoffset);
          visible_start_y = MIN (y_begin, y_end);
          visible_start_y = MAX (visible_start_y, yoffset);
        }

      /* walk the lines inside the selection area */
      for (y = y_begin; y < y_end; y += y_height)
        {
          /* get the start and end iter in the line */
          gtk_text_view_get_iter_at_location (textview, &start_iter, view->selection_start_x, y);
          gtk_text_view_get_iter_at_location (textview, &end_iter, view->selection_end_x, y);

          /* make sure the iters are correctly sorted */
          gtk_text_iter_order (&start_iter, &end_iter);

          /* calculate length of selection */
          length += gtk_text_iter_get_line_offset (&end_iter) - gtk_text_iter_get_line_offset (&start_iter);

          /* continue if this iter is outside the visible area */
          if (y < visible_start_y)
            continue;

          if (length == 0)
            {
              /* get the iter location and size */
              gtk_text_view_get_iter_location (textview, &end_iter, &rect);

              /* calculate line cooridinates */
              gtk_text_view_buffer_to_window_coords(textview,GTK_TEXT_WINDOW_TEXT,
                                                    rect.x,rect.y, &line_x,&line_y);

              /* draw a line in front of the iter */
              gdk_draw_line (GDK_DRAWABLE (window),
                             gtk_widget_get_style(GTK_WIDGET (view))->base_gc[GTK_STATE_SELECTED],
                             line_x, line_y, line_x, line_y + rect.height - 1);
            }
          else if (!gtk_text_iter_equal (&start_iter, &end_iter))
            {
              /* apply tag */
              gtk_text_buffer_apply_tag (buffer, view->selection_tag, &start_iter, &end_iter);
            }
          else
            {
              /* when both of the conditions above were not met, we'll
               * never added them to the marks list and there is nothing to draw
               * eighter */
              continue;
            }

          if (append)
            {
              /* create marks */
              start_mark = gtk_text_buffer_create_mark (buffer, NULL, &start_iter, TRUE);
              end_mark = gtk_text_buffer_create_mark (buffer, NULL, &end_iter, FALSE);

              /* append to the list */
              view->selection_marks = g_slist_append (view->selection_marks, start_mark);
              view->selection_marks = g_slist_append (view->selection_marks, end_mark);
            }
        }
    }
  else
    {
      for (li = view->selection_marks; li != NULL; li = li->next)
        {
          /* get the start iter */
          gtk_text_buffer_get_iter_at_mark (buffer, &start_iter, li->data);

          /* next element in the list */
          li = li->next;

          /* get the end iter */
          gtk_text_buffer_get_iter_at_mark (buffer, &end_iter, li->data);

          /* calculate length of selection */
          length += gtk_text_iter_get_line_offset (&end_iter) - gtk_text_iter_get_line_offset (&start_iter);

          if (length == 0)
            {
              /* get the iter location and size */
              gtk_text_view_get_iter_location (textview, &start_iter, &rect);

              /* calculate line coordinates */
              gtk_text_view_buffer_to_window_coords(textview,GTK_TEXT_WINDOW_TEXT,
                                                    rect.x,rect.y, &line_x,&line_y);
                                                    
              /* draw a line in front of the iter */
              gdk_draw_line (GDK_DRAWABLE (window),
                             gtk_widget_get_style(GTK_WIDGET (view))->base_gc[GTK_STATE_SELECTED],
                             line_x, line_y, line_x, line_y + rect.height - 1);
            }
          else
            {
              /* apply tag */
              gtk_text_buffer_apply_tag (buffer, view->selection_tag, &start_iter, &end_iter);
            }
        }
    }

  /* emit signal for selection (type) change */
  if ((length == 0 && view->selection_length != -1)
      || (length > 0 && view->selection_length <= 0))
    g_object_notify (G_OBJECT (buffer), "has-selection");

  /* update the selection length */
  view->selection_length = length > 0 ? length : -1;

  /* allow sending notifications again */
  g_object_thaw_notify (G_OBJECT (buffer));
}



static gboolean
mousepad_view_selection_timeout (gpointer user_data)
{
  MousepadView  *view = MOUSEPAD_VIEW (user_data);
  GtkTextView   *textview = GTK_TEXT_VIEW (view);
  gint           pointer_x, pointer_y;
  GtkTextBuffer *buffer;
  GtkTextIter    iter, cursor;

  GDK_THREADS_ENTER ();

  /* get the cursor coordinate */
  gdk_window_get_pointer (gtk_text_view_get_window (textview, GTK_TEXT_WINDOW_TEXT),
                          &pointer_x, &pointer_y, NULL);

  /* convert to positive values and add buffer offset */
  gtk_text_view_window_to_buffer_coords(textview,GTK_TEXT_WINDOW_TEXT,MAX (pointer_x, 0),MAX (pointer_y, 0),
                                        &pointer_x,&pointer_y);

  /* only update the selection when the cursor moved */
  if (view->selection_end_x != pointer_x || view->selection_end_y != pointer_y)
    {
      /* update the end coordinates */
      view->selection_end_x = pointer_x;
      view->selection_end_y = pointer_y;

      /* get the text buffer */
      buffer = mousepad_view_get_buffer (view);

      /* get the iter position of the cursor based on the coordinates */
      gtk_text_view_get_iter_at_location (textview, &iter, pointer_x, pointer_y);

      /* get the real cursor position */
      gtk_text_buffer_get_iter_at_mark (buffer, &cursor, gtk_text_buffer_get_insert (buffer));

      /* redraw the selection when the cursor position changed */
      if (!gtk_text_iter_equal (&iter, &cursor))
        {
          /* show the selection */
          mousepad_view_selection_draw (view, FALSE);

          /* update the cursor position */
          gtk_text_buffer_place_cursor (buffer, &iter);

          /* put cursor on screen */
          mousepad_view_scroll_to_cursor (view);
        }
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



static gchar *
mousepad_view_selection_string (MousepadView *view)
{
  GString       *string;
  GtkTextBuffer *buffer;
  gint           line, previous_line = -1;
  gchar         *slice;
  GtkTextIter    start_iter, end_iter;
  GSList        *li;

  g_return_val_if_fail (view->selection_marks == NULL || g_slist_length (view->selection_marks) % 2 == 0, NULL);

  /* create new string */
  string = g_string_new (NULL);

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  for (li = view->selection_marks; li != NULL; li = li->next)
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
      else
        for (; previous_line < line; previous_line++)
          string = g_string_append_c (string, '\n');

      /* get the text slice */
      slice = gtk_text_buffer_get_slice (buffer, &start_iter, &end_iter, TRUE);

      /* append the slice to the string */
      string = g_string_append (string, slice);

      /* cleanup */
      g_free (slice);
    }

  /* return the string */
  return g_string_free (string, FALSE);
}
#endif



/**
 * Indentation Functions
 **/
static void
mousepad_view_indent_increase (MousepadView *view,
                               GtkTextIter  *iter)
{
  gchar         *string;
  gint           offset, length, inline_len, tab_size;
  GtkTextBuffer *buffer;

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);
  tab_size = gtk_source_view_get_tab_width (GTK_SOURCE_VIEW (view));

  if (gtk_source_view_get_insert_spaces_instead_of_tabs (GTK_SOURCE_VIEW (view)))
    {
      /* get the offset */
      offset = mousepad_util_get_real_line_offset (iter, tab_size);

      /* calculate the length to inline with a tab */
      inline_len = offset % tab_size;

      if (inline_len == 0)
        length = tab_size;
      else
        length = tab_size - inline_len;

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
  gint           columns, tab_size;
  gunichar       c;

  /* set iters */
  start = end = *iter;

  tab_size = gtk_source_view_get_tab_width (GTK_SOURCE_VIEW (view));
  columns = tab_size;

  /* walk until we've removed enough columns */
  while (columns > 0)
    {
      /* get the character */
      c = gtk_text_iter_get_char (&end);

      if (c == '\t')
        columns -= tab_size;
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
                                gboolean      increase,
                                gboolean      force)
{
  GtkTextBuffer *buffer;
  GtkTextIter    start_iter, end_iter;
  gint           start_line, end_line;
  gint           i;

  /* get the textview buffer */
  buffer = mousepad_view_get_buffer (view);

  if (gtk_text_buffer_get_selection_bounds (buffer, &start_iter, &end_iter) || force)
    {
      /* begin a user action */
      gtk_text_buffer_begin_user_action (buffer);

      /* get start and end line */
      start_line = gtk_text_iter_get_line (&start_iter);
      end_line = gtk_text_iter_get_line (&end_iter);

      /* only change indentation when an entire line is selected or multiple lines */
      if (start_line != end_line
          || ((gtk_text_iter_starts_line (&start_iter) && gtk_text_iter_ends_line (&end_iter)) || force))
        {
          /* change indentation of each line */
          for (i = start_line; i <= end_line && i < G_MAXINT; i++)
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
      mousepad_view_scroll_to_cursor (view);
    }
}


void
mousepad_view_scroll_to_cursor (MousepadView *view)
{
  GtkTextBuffer *buffer;

  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* scroll to visible area */
  gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view),
                                gtk_text_buffer_get_insert (buffer),
                                0.02, FALSE, 0.0, 0.0);
}



#ifdef HAVE_MULTISELECT
static void
mousepad_view_transpose_multi_selection (GtkTextBuffer *buffer,
                                         MousepadView  *view)
{
  GSList      *li, *ls;
  GSList      *strings = NULL;
  GtkTextIter  start_iter, end_iter;

  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  /* get the strings and delete the existing strings */
  for (li = view->selection_marks; li != NULL; li = li->next)
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
  for (li = view->selection_marks, ls = strings; li != NULL && ls != NULL; li = li->next, ls = ls->next)
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
#endif



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
  if (G_LIKELY (string))
    {
      /* reverse the string */
      reversed = g_utf8_strreverse (string, -1);

      /* only change the buffer then the string changed */
      if (G_LIKELY (reversed && strcmp (reversed, string) != 0))
        {
          /* delete the text between the iters */
          gtk_text_buffer_delete (buffer, start_iter, end_iter);

          /* insert the reversed string */
          gtk_text_buffer_insert (buffer, end_iter, reversed, -1);

          /* restore start iter */
          gtk_text_buffer_get_iter_at_offset (buffer, start_iter, offset);
        }

      /* cleanup */
      g_free (string);
      g_free (reversed);
    }
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
  for (i = start_line; i <= end_line && i < G_MAXINT; i++)
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

  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* begin user action */
  gtk_text_buffer_begin_user_action (buffer);

#ifdef HAVE_MULTISELECT
  if (view->selection_marks != NULL)
    {
      /* transpose a multi selection */
      mousepad_view_transpose_multi_selection (buffer, view);
    }
  else
#endif
  if (gtk_text_buffer_get_selection_bounds (buffer, &sel_start, &sel_end))
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
      gtk_text_buffer_select_range (buffer, &sel_end, &sel_start);
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

  g_return_if_fail (MOUSEPAD_IS_VIEW (view));
  g_return_if_fail (mousepad_view_get_selection_length (view, NULL) > 0);

  /* get the clipboard */
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view), GDK_SELECTION_CLIPBOARD);

#ifdef HAVE_MULTISELECT
  if (view->selection_marks != NULL)
    {
      gchar *string;

      /* get the selection string */
      string = mousepad_view_selection_string (view);

      /* set the clipboard text */
      gtk_clipboard_set_text (clipboard, string, -1);

      /* cleanup */
      g_free (string);

      /* cleanup the vertical selection */
      mousepad_view_selection_delete_content (view);

      /* destroy selection */
      mousepad_view_selection_destroy (view);
    }
  else
#endif
    {
      /* get the buffer */
      buffer = mousepad_view_get_buffer (view);

      /* cut from buffer */
      gtk_text_buffer_cut_clipboard (buffer, clipboard, gtk_text_view_get_editable (GTK_TEXT_VIEW (view)));
    }

  /* put cursor on screen */
  mousepad_view_scroll_to_cursor (view);
}



void
mousepad_view_clipboard_copy (MousepadView *view)
{
  GtkClipboard  *clipboard;
  GtkTextBuffer *buffer;

  g_return_if_fail (MOUSEPAD_IS_VIEW (view));
  g_return_if_fail (mousepad_view_get_selection_length (view, NULL) > 0);

  /* get the clipboard */
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view), GDK_SELECTION_CLIPBOARD);

#ifdef HAVE_MULTISELECT
  if (view->selection_marks != NULL)
    {
      gchar *string;

      /* get the selection string */
      string = mousepad_view_selection_string (view);

      /* set the clipboard text */
      gtk_clipboard_set_text (clipboard, string, -1);

      /* cleanup */
      g_free (string);
    }
  else
#endif
    {
      /* get the buffer */
      buffer = mousepad_view_get_buffer (view);

      /* copy from buffer */
      gtk_text_buffer_copy_clipboard (buffer, clipboard);
    }

  /* put cursor on screen */
  mousepad_view_scroll_to_cursor (view);
}



void
mousepad_view_clipboard_paste (MousepadView *view,
                               const gchar  *string,
                               gboolean      paste_as_column)
{
  GtkClipboard   *clipboard;
  GtkTextBuffer  *buffer;
  GtkTextView    *textview = GTK_TEXT_VIEW (view);
  gchar          *text = NULL;
  GtkTextMark    *mark;
  GtkTextIter     iter;
  GtkTextIter     start_iter, end_iter;
  GdkRectangle    rect;
  gchar         **pieces;
  gint            i, y;

  if (string == NULL)
    {
      /* get the clipboard */
      clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view), GDK_SELECTION_CLIPBOARD);

      /* get the clipboard text */
      text = gtk_clipboard_wait_for_text (clipboard);

      /* leave when the text is null */
      if (G_UNLIKELY (text == NULL))
        return;

      /* set the string */
      string = text;
    }

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* begin user action */
  gtk_text_buffer_begin_user_action (buffer);

  if (paste_as_column)
    {
      /* chop the string into pieces */
      pieces = g_strsplit (string, "\n", -1);

      /* get iter at cursor position */
      mark = gtk_text_buffer_get_insert (buffer);
      gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

      /* get the iter location */
      gtk_text_view_get_iter_location (textview, &iter, &rect);

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
    }
  else
    {
      /* get selection bounds */
      gtk_text_buffer_get_selection_bounds (buffer, &start_iter, &end_iter);

      /* remove the existing selection if the iters are not equal */
      if (!gtk_text_iter_equal (&start_iter, &end_iter))
        gtk_text_buffer_delete (buffer, &start_iter, &end_iter);

      /* insert string */
      gtk_text_buffer_insert (buffer, &start_iter, string, -1);
    }

  /* cleanup */
  g_free (text);

  /* end user action */
  gtk_text_buffer_end_user_action (buffer);

  /* put cursor on screen */
  mousepad_view_scroll_to_cursor (view);
}



void
mousepad_view_delete_selection (MousepadView *view)
{
  GtkTextBuffer *buffer;

  g_return_if_fail (MOUSEPAD_IS_VIEW (view));
  g_return_if_fail (mousepad_view_get_selection_length (view, NULL) > 0);

#ifdef HAVE_MULTISELECT
  if (view->selection_marks != NULL)
    {
      /* remove the text in our selection */
      mousepad_view_selection_delete_content (view);

      /* cleanup the vertical selection */
      mousepad_view_selection_destroy (view);
    }
  else
#endif
    {
      /* get the buffer */
      buffer = mousepad_view_get_buffer (view);

      /* delete the selection */
      gtk_text_buffer_delete_selection (buffer, TRUE, gtk_text_view_get_editable (GTK_TEXT_VIEW (view)));
    }

  /* put cursor on screen */
  mousepad_view_scroll_to_cursor (view);
}



void
mousepad_view_select_all (MousepadView *view)
{
  GtkTextIter    start, end;
  GtkTextBuffer *buffer;

  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

#ifdef HAVE_MULTISELECT
  /* cleanup our selection */
  if (view->selection_marks != NULL)
    mousepad_view_selection_destroy (view);
#endif

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* get the start and end iter */
  gtk_text_buffer_get_bounds (buffer, &start, &end);

  /* select everything between those iters */
  gtk_text_buffer_select_range (buffer, &end, &start);
}



void
mousepad_view_change_selection (MousepadView *view)
{
#ifdef HAVE_MULTISELECT
  GtkTextBuffer *buffer;
  GtkTextIter    start_iter, end_iter;
  GdkRectangle   rect;

  g_return_if_fail (MOUSEPAD_IS_VIEW (view));
  g_return_if_fail (mousepad_view_get_selection_length (view, NULL) != 0);

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* freeze notifications */
  g_object_freeze_notify (G_OBJECT (buffer));

  if (view->selection_marks != NULL)
    {
      /* get the first and last iter from the selection */
      gtk_text_buffer_get_iter_at_mark (buffer, &start_iter, view->selection_marks->data);
      gtk_text_buffer_get_iter_at_mark (buffer, &end_iter, g_slist_last (view->selection_marks)->data);

      /* sort the iters */
      gtk_text_iter_order (&start_iter, &end_iter);

      /* destroy the selection */
      mousepad_view_selection_destroy (view);

      /* select the range */
      gtk_text_buffer_select_range (buffer, &end_iter, &start_iter);
    }
  else if (gtk_text_buffer_get_selection_bounds (buffer, &start_iter, &end_iter))
    {
      /* set the start coordinates */
      gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), &start_iter, &rect);
      view->selection_start_x = rect.x;
      view->selection_start_y = rect.y;

      /* set the end coordinates */
      gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), &end_iter, &rect);
      view->selection_end_x = rect.x;
      view->selection_end_y = rect.y;

      /* drop the normal selection and hide cursor */
      gtk_text_buffer_place_cursor (buffer, &end_iter);
      gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);

      /* poke selection function to add the marks */
      mousepad_view_selection_draw (view, TRUE);

      /* reset the coordinates */
      view->selection_start_x = view->selection_start_y = -1;
      view->selection_end_x = view->selection_end_y = -1;
    }

  /* allow notifications again */
  g_object_thaw_notify (G_OBJECT (buffer));
#endif
}



void
mousepad_view_convert_selection_case (MousepadView *view,
                                      gint          type)
{
  gchar         *text;
  gchar         *converted;
  GtkTextBuffer *buffer;
  GtkTextIter    start_iter, end_iter;
  gint           offset = -1;
  GSList        *li = NULL;

  g_return_if_fail (MOUSEPAD_IS_VIEW (view));
  g_return_if_fail (mousepad_view_get_selection_length (view, NULL) > 0);

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* begin a user action */
  gtk_text_buffer_begin_user_action (buffer);

  if (view->selection_marks == NULL)
    {
      /* get selection bounds */
      gtk_text_buffer_get_selection_bounds (buffer, &start_iter, &end_iter);

      /* get string offset */
      offset = gtk_text_iter_get_offset (&start_iter);

      /* enter the loop without hassle */
      goto selection_enter;
    }

  /* replace all selected items */
  for (li = view->selection_marks; li != NULL; li = li->next)
    {
      /* get iters from column selection */
      gtk_text_buffer_get_iter_at_mark (buffer, &start_iter, li->data);
      li = li->next;
      gtk_text_buffer_get_iter_at_mark (buffer, &end_iter, li->data);

      /* label for normal selections */
      selection_enter:

      /* get the selected string */
      text = gtk_text_buffer_get_slice (buffer, &start_iter, &end_iter, FALSE);
      if (G_LIKELY (text != NULL))
        {
          switch (type)
            {
              case LOWERCASE:
                converted = g_utf8_strdown (text, -1);
                break;

              case UPPERCASE:
                converted = g_utf8_strup (text, -1);
                break;

              case TITLECASE:
                converted = mousepad_util_utf8_strcapital (text);
                break;

              case OPPOSITE_CASE:
                converted = mousepad_util_utf8_stropposite (text);
                break;

              default:
                g_assert_not_reached ();
                break;
            }

          /* only update the buffer if the string changed */
          if (G_LIKELY (converted && strcmp (text, converted) != 0))
            {
              /* debug check */
              g_return_if_fail (g_utf8_validate (converted, -1, NULL));

              /* delete old string */
              gtk_text_buffer_delete (buffer, &start_iter, &end_iter);

              /* insert string */
              gtk_text_buffer_insert (buffer, &end_iter, converted, -1);
            }

          /* cleanup */
          g_free (converted);
          g_free (text);
        }

      /* leave for normal selections */
      if (offset != -1)
        break;
    }

  /* restore selection if needed */
  if (offset != -1)
    {
      /* restore start iter */
      gtk_text_buffer_get_iter_at_offset (buffer, &start_iter, offset);

      /* select range */
      gtk_text_buffer_select_range (buffer, &end_iter, &start_iter);
    }
#ifdef HAVE_MULTISELECT
  else
    {
      /* redraw column selection */
      mousepad_view_selection_draw (view, FALSE);
    }
#endif

  /* end user action */
  gtk_text_buffer_end_user_action (buffer);
}



void
mousepad_view_convert_spaces_and_tabs (MousepadView *view,
                                       gint          type)
{
  GtkTextBuffer *buffer;
  GtkTextMark   *mark;
  GtkTextIter    start_iter, end_iter;
  GtkTextIter    iter;
  gint           tab_size;
  gboolean       in_range = FALSE;
  gboolean       no_forward;
  gunichar       c;
  gint           offset;
  gint           n_spaces = 0;
  gint           start_offset = -1;
  gchar         *string;

  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* get the tab size */
  tab_size = gtk_source_view_get_tab_width (GTK_SOURCE_VIEW (view));

  /* get the start and end iter */
  if (gtk_text_buffer_get_selection_bounds (buffer, &start_iter, &end_iter))
    {
      /* move to the start of the line when replacing spaces */
      if (type == SPACES_TO_TABS && !gtk_text_iter_starts_line (&start_iter))
        gtk_text_iter_set_line_offset (&start_iter, 0);

      /* offset for restoring the selection afterwards */
      start_offset = gtk_text_iter_get_offset (&start_iter);
    }
  else
    {
      /* get the document bounds */
      gtk_text_buffer_get_bounds (buffer, &start_iter, &end_iter);
    }

  /* leave when the iters are equal (empty docs) */
  if (gtk_text_iter_equal (&start_iter, &end_iter))
    return;

  /* begin a user action and free notifications */
  g_object_freeze_notify (G_OBJECT (buffer));
  gtk_text_buffer_begin_user_action (buffer);

  /* create a mark to restore the end iter after modifieing the buffer */
  mark = gtk_text_buffer_create_mark (buffer, NULL, &end_iter, FALSE);

  /* walk the text between the iters */
  for (;;)
    {
      /* get the character */
      c = gtk_text_iter_get_char (&start_iter);

      /* reset */
      no_forward = FALSE;

      if (type == SPACES_TO_TABS)
        {
          if (c == ' ' || in_range)
            {
              if (in_range == FALSE)
                {
                  /* set the start iter */
                  iter = start_iter;

                  /* get the real offset of the start iter */
                  offset = mousepad_util_get_real_line_offset (&iter, tab_size);

                  /* the number of spaces to inline with the tabs */
                  n_spaces = tab_size - offset % tab_size;
                }

              /* check if we can already insert a tab */
              if (n_spaces == 0)
                {
                  /* delete the selected spaces */
                  gtk_text_buffer_delete (buffer, &iter, &start_iter);
                  gtk_text_buffer_insert (buffer, &start_iter, "\t", 1);

                  /* restore the end iter */
                  gtk_text_buffer_get_iter_at_mark (buffer, &end_iter, mark);

                  /* stop being inside a range */
                  in_range = FALSE;

                  /* no need to forward (iter moved after the insert */
                  no_forward = TRUE;
                }
              else
                {
                  /* check whether we're still in a range */
                  in_range = (c == ' ');
                }

              /* decrease counter */
              n_spaces--;
            }

          /* go to next line if we reached text */
          if (!g_unichar_isspace (c))
            {
              /* continue to the next line if we hit non spaces */
              gtk_text_iter_forward_line (&start_iter);

              /* make sure there is no valid range anymore */
              in_range = FALSE;

              /* no need to forward */
              no_forward = TRUE;
            }
        }
      else if (type == TABS_TO_SPACES && c == '\t')
        {
          /* get the real offset of the iter */
          offset = mousepad_util_get_real_line_offset (&start_iter, tab_size);

          /* the number of spaces to inline with the tabs */
          n_spaces = tab_size - offset % tab_size;

          /* move one character forwards */
          iter = start_iter;
          gtk_text_iter_forward_char (&start_iter);

          /* delete the tab */
          gtk_text_buffer_delete (buffer, &iter, &start_iter);

          /* create a string with the number of spaces */
          string = g_strnfill (n_spaces, ' ');

          /* insert the spaces */
          gtk_text_buffer_insert (buffer, &start_iter, string, n_spaces);

          /* cleanup */
          g_free (string);

          /* restore the end iter */
          gtk_text_buffer_get_iter_at_mark (buffer, &end_iter, mark);

          /* iter already moved by the insert */
          no_forward = TRUE;
        }

      /* break when the iters are equal (or start is bigger) */
      if (gtk_text_iter_compare (&start_iter, &end_iter) >= 0)
        break;

      /* forward the iter */
      if (G_LIKELY (no_forward == FALSE))
        gtk_text_iter_forward_char (&start_iter);
    }

  /* delete our mark */
  gtk_text_buffer_delete_mark (buffer, mark);

  /* restore the selection if needed */
  if (start_offset > -1)
    {
      /* restore iter and select range */
      gtk_text_buffer_get_iter_at_offset (buffer, &start_iter, start_offset);
      gtk_text_buffer_select_range (buffer, &end_iter, &start_iter);
    }

  /* end the user action */
  gtk_text_buffer_end_user_action (buffer);
  g_object_thaw_notify (G_OBJECT (buffer));
}



void
mousepad_view_strip_trailing_spaces (MousepadView *view)
{
  GtkTextBuffer *buffer;
  GtkTextIter    start_iter, end_iter, needle;
  gint           start, end, i;
  gunichar       c;

  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* get range in line numbers */
  if (gtk_text_buffer_get_selection_bounds (buffer, &start_iter, &end_iter))
    {
      start = gtk_text_iter_get_line (&start_iter);
      end = gtk_text_iter_get_line (&end_iter) + 1;
    }
  else
    {
      start = 0;
      end = gtk_text_buffer_get_line_count (buffer);
    }

  /* begin a user action and free notifications */
  g_object_freeze_notify (G_OBJECT (buffer));
  gtk_text_buffer_begin_user_action (buffer);

  /* walk all the selected lines */
  for (i = start; i < end; i++)
    {
      /* get the iter at the line */
      gtk_text_buffer_get_iter_at_line (buffer, &end_iter, i);

      /* continue if the line is empty */
      if (gtk_text_iter_ends_line (&end_iter))
        continue;

      /* move the iter to the end of the line */
      gtk_text_iter_forward_to_line_end (&end_iter);

      /* initialize the iters */
      needle = start_iter = end_iter;

      /* walk backwards until we hit something else then a space or tab */
      while (gtk_text_iter_backward_char (&needle))
        {
          /* get the character */
          c = gtk_text_iter_get_char (&needle);

          /* set the start iter of the spaces range or stop searching */
          if (c == ' ' || c == '\t')
            start_iter = needle;
          else
            break;
        }

      /* remove the spaces/tabs if the iters are not equal */
      if (!gtk_text_iter_equal (&start_iter, &end_iter))
        gtk_text_buffer_delete (buffer, &start_iter, &end_iter);
    }

  /* end the user action */
  gtk_text_buffer_end_user_action (buffer);
  g_object_thaw_notify (G_OBJECT (buffer));
}



void
mousepad_view_move_selection (MousepadView *view,
                              gint          type)
{
  GtkTextBuffer *buffer;
  GtkTextIter    start_iter, end_iter, iter;
  GtkTextMark   *mark;
  gchar         *text;
  gboolean       insert_eol;

  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* begin a user action */
  gtk_text_buffer_begin_user_action (buffer);

  if (gtk_text_buffer_get_selection_bounds (buffer, &start_iter, &end_iter))
    {
      if (type == MOVE_LINE_UP)
        {
          /* useless action if we're already at the start of the buffer */
          if (gtk_text_iter_get_line (&start_iter) == 0)
            goto leave;

          /* set insert point to end of selected line */
          iter = end_iter;
          if (!gtk_text_iter_ends_line (&iter))
            gtk_text_iter_forward_to_line_end (&iter);

          /* make start iter to previous line (can not fail) */
          gtk_text_iter_backward_line (&start_iter);

          /* move end iter to end of line */
          end_iter = start_iter;
          if (!gtk_text_iter_ends_line (&end_iter))
            gtk_text_iter_forward_to_line_end (&end_iter);

          /* move start iter one line back */
          insert_eol = !gtk_text_iter_backward_char (&start_iter);
        }
      else /* MOVE_LINE_DOWN */
        {
          /* useless action if we're already at the end of the buffer */
          if (gtk_text_iter_get_line (&end_iter) == gtk_text_buffer_get_line_count (buffer) - 1)
            goto leave;

          /* move insert iter to start of the line */
          iter = start_iter;
          if (!gtk_text_iter_starts_line (&iter))
            gtk_text_iter_set_line_offset (&iter, 0);

          /* move start iter to the start of the line below the selection */
          start_iter = end_iter;
          gtk_text_iter_forward_line (&start_iter);

          /* set the end iter */
          end_iter = start_iter;
          insert_eol = !gtk_text_iter_forward_line (&end_iter);
        }

      /* create mark at insert point */
      mark = gtk_text_buffer_create_mark (buffer, NULL, &iter, TRUE);

      /* copy the line above the selection  */
      text = gtk_text_buffer_get_slice (buffer, &start_iter, &end_iter, FALSE);

      /* delete the new line that we're going to insert later on */
      if (insert_eol && type == MOVE_LINE_UP)
        gtk_text_iter_forward_char (&end_iter);
      else if (insert_eol && type == MOVE_LINE_DOWN)
        gtk_text_iter_backward_char (&start_iter);

      /* delete */
      gtk_text_buffer_delete (buffer, &start_iter, &end_iter);

      /* restore mark */
      gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

      /* insert new line if needed */
      if (insert_eol && type == MOVE_LINE_UP)
        gtk_text_buffer_insert (buffer, &iter, "\n", 1);

      /* insert text */
      gtk_text_buffer_insert (buffer, &iter, text, -1);

      /* insert new line if needed */
      if (insert_eol && type == MOVE_LINE_DOWN)
        gtk_text_buffer_insert (buffer, &iter, "\n", 1);

      /* cleanup */
      g_free (text);

      /* sometimes we need to restore the selection (left gravity of the selection marks) */
      if (type == MOVE_LINE_UP)
        {
          /* get selection bounds */
          gtk_text_buffer_get_selection_bounds (buffer, &start_iter, &end_iter);

          /* check if we need to restore the selected range */
          if (gtk_text_iter_equal (&iter, &end_iter))
            {
              /* restore end iter from mark and select the range again */
              gtk_text_buffer_get_iter_at_mark (buffer, &end_iter, mark);
              gtk_text_buffer_select_range (buffer, &start_iter, &end_iter);
            }
        }

      /* delete mark */
      gtk_text_buffer_delete_mark (buffer, mark);
    }

  /* labeltje */
  leave:

  /* end the action */
  gtk_text_buffer_end_user_action (buffer);

  /* show */
  mousepad_view_scroll_to_cursor (view);
}



void
mousepad_view_duplicate (MousepadView *view)
{
  GtkTextBuffer *buffer;
  GtkTextIter    start_iter, end_iter;
  gboolean       has_selection;
  gboolean       insert_eol = FALSE;

  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* begin a user action */
  gtk_text_buffer_begin_user_action (buffer);

  /* get iters */
  has_selection = gtk_text_buffer_get_selection_bounds (buffer, &start_iter, &end_iter);

  /* select entire line */
  if (has_selection == FALSE)
    {
      /* set to the start of the line */
      if (!gtk_text_iter_starts_line (&start_iter))
        gtk_text_iter_set_line_offset (&start_iter, 0);

      /* move the other iter to the start of the next line */
      insert_eol = !gtk_text_iter_forward_line (&end_iter);
    }

  /* insert a dupplicate of the text before the iter */
  gtk_text_buffer_insert_range (buffer, &start_iter, &start_iter, &end_iter);

  /* insert a new line if needed */
  if (insert_eol)
    gtk_text_buffer_insert (buffer, &start_iter, "\n", 1);

  /* end the action */
  gtk_text_buffer_end_user_action (buffer);
}



void
mousepad_view_indent (MousepadView *view,
                      gint          type)
{
  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  /* run a forced indent of the line(s) */
  mousepad_view_indent_selection (view, type == INCREASE_INDENT, TRUE);
}



gboolean
mousepad_view_get_selection_length (MousepadView *view,
                                    gboolean     *is_column_selection)
{
  GtkTextBuffer *buffer;
  GtkTextIter    sel_start, sel_end;
  gint           sel_length = 0;
  gboolean       column_selection;

  g_return_val_if_fail (MOUSEPAD_IS_VIEW (view), FALSE);

  /* get the text buffer */
  buffer = mousepad_view_get_buffer (view);

  /* whether this is a column selection */
  column_selection = view->selection_marks != NULL || view->selection_start_x != -1;

  /* we have a vertical selection */
  if (column_selection)
    {
      /* get the selection count from the selection draw function */
      sel_length = view->selection_length;
    }
  else if (gtk_text_buffer_get_selection_bounds (buffer, &sel_start, &sel_end))
    {
      /* calculate the selection length from the iter offset (absolute) */
      sel_length = ABS (gtk_text_iter_get_offset (&sel_end) - gtk_text_iter_get_offset (&sel_start));
    }

  /* whether this is a column selection */
  if (is_column_selection)
    *is_column_selection = column_selection;

  /* return length */
  return sel_length;
}



void
mousepad_view_set_font_name (MousepadView *view,
                             const gchar  *font_name)
{
  PangoFontDescription *font_desc;

  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  /* Passing NULL resets to the default font */
  if (font_name == NULL)
    font_name = MOUSEPAD_VIEW_DEFAULT_FONT;

  font_desc = pango_font_description_from_string (font_name);
  if (G_LIKELY (font_desc != NULL))
    {
      /* Save the normalized representation of the font as a string */
      g_free (view->font_name);
      view->font_name = pango_font_description_to_string (font_desc);

      /* save the font description for later updating */
      pango_font_description_free (view->font_desc);
      view->font_desc = font_desc;

      /* apply either the default or this font depending on settings */
      mousepad_view_update_font (view);

      /* Notify interested parties that the property has changed */
      g_object_notify (G_OBJECT (view), "font-name");
    }
  else
    g_critical ("Invalid font-name given: %s", font_name);
}



static void
mousepad_view_update_draw_spaces (MousepadView *view)
{
  GtkSourceDrawSpacesFlags flags = 0;

  if (view->show_whitespace)
    {
      flags |= GTK_SOURCE_DRAW_SPACES_SPACE |
               GTK_SOURCE_DRAW_SPACES_TAB |
               GTK_SOURCE_DRAW_SPACES_NBSP |
               GTK_SOURCE_DRAW_SPACES_LEADING |
               GTK_SOURCE_DRAW_SPACES_TEXT |
               GTK_SOURCE_DRAW_SPACES_TRAILING;
    }

  if (view->show_line_endings)
    flags |= GTK_SOURCE_DRAW_SPACES_NEWLINE;

  gtk_source_view_set_draw_spaces (GTK_SOURCE_VIEW (view), flags);
}



static void
mousepad_view_override_font (MousepadView         *view,
                             PangoFontDescription *font_desc)
{
#if GTK_CHECK_VERSION (3, 0, 0)
  gtk_widget_override_font (GTK_WIDGET (view), font_desc);
#else
  gtk_widget_modify_font (GTK_WIDGET (view), font_desc);
#endif
}



static void
mousepad_view_update_font (MousepadView *view)
{
  gboolean use_default;

  use_default = MOUSEPAD_SETTING_GET_BOOLEAN (USE_DEFAULT_FONT);

  if (use_default)
    {
      PangoFontDescription *font_desc;
      font_desc = pango_font_description_from_string (MOUSEPAD_VIEW_DEFAULT_FONT);
      mousepad_view_override_font (view, font_desc);
      pango_font_description_free (font_desc);
    }
  else
    mousepad_view_override_font (view, view->font_desc);
}



const gchar *
mousepad_view_get_font_name (MousepadView *view)
{
  g_return_val_if_fail (MOUSEPAD_IS_VIEW (view), NULL);

  return view->font_name;
}



void
mousepad_view_set_show_whitespace (MousepadView *view,
                                   gboolean      show)
{
  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  view->show_whitespace = show;
  mousepad_view_update_draw_spaces (view);
  g_object_notify (G_OBJECT (view), "show-whitespace");
}



gboolean
mousepad_view_get_show_whitespace (MousepadView *view)
{
  g_return_val_if_fail (MOUSEPAD_IS_VIEW (view), FALSE);

  return view->show_whitespace;
}



void
mousepad_view_set_show_line_endings (MousepadView *view,
                                     gboolean      show)
{
  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  view->show_line_endings = show;
  mousepad_view_update_draw_spaces (view);
  g_object_notify (G_OBJECT (view), "show-line-endings");
}



gboolean
mousepad_view_get_show_line_endings (MousepadView *view)
{
  g_return_val_if_fail (MOUSEPAD_IS_VIEW (view), FALSE);

  return view->show_line_endings;
}



void
mousepad_view_set_color_scheme (MousepadView *view,
                                const gchar  *color_scheme)
{
  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  if (g_strcmp0 (color_scheme, view->color_scheme) != 0)
    {
      g_free (view->color_scheme);
      view->color_scheme = g_strdup (color_scheme);

      /* update the buffer if there is one */
      mousepad_view_buffer_changed (view, NULL, NULL);

      g_object_notify (G_OBJECT (view), "color-scheme");
    }
}



const gchar *
mousepad_view_get_color_scheme (MousepadView *view)
{
  g_return_val_if_fail (MOUSEPAD_IS_VIEW (view), NULL);

  return view->color_scheme;
}



void
mousepad_view_set_word_wrap (MousepadView *view,
                             gboolean      enabled)
{
  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view),
                               enabled ? GTK_WRAP_WORD : GTK_WRAP_NONE);
  g_object_notify (G_OBJECT (view), "word-wrap");
}



gboolean
mousepad_view_get_word_wrap (MousepadView *view)
{
  GtkWrapMode mode;

  g_return_val_if_fail (MOUSEPAD_IS_VIEW (view), FALSE);

  mode = gtk_text_view_get_wrap_mode (GTK_TEXT_VIEW (view));

  return (mode == GTK_WRAP_WORD);
}



void
mousepad_view_set_match_braces (MousepadView *view,
                                gboolean      enabled)
{
  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  view->match_braces = enabled;

  mousepad_view_buffer_changed (view, NULL, NULL);

  g_object_notify (G_OBJECT (view), "match-braces");
}



gboolean
mousepad_view_get_match_braces (MousepadView *view)
{
  g_return_val_if_fail (MOUSEPAD_IS_VIEW (view), FALSE);

  return view->match_braces;
}
