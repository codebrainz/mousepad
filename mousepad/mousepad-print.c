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

#include <pango/pango.h>
#include <cairo.h>

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-preferences.h>
#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-util.h>
#include <mousepad/mousepad-print.h>

#define DOCUMENT_SPACING (10)



static void           mousepad_print_finalize              (GObject                 *object);
static void           mousepad_print_settings_load         (GtkPrintOperation       *operation);
static void           mousepad_print_settings_save_foreach (const gchar             *key,
                                                            const gchar             *value,
                                                            gpointer                 user_data);
static void           mousepad_print_settings_save         (GtkPrintOperation       *operation);
static void           mousepad_print_begin_print           (GtkPrintOperation       *operation,
                                                            GtkPrintContext         *context);
static void           mousepad_print_draw_page             (GtkPrintOperation       *operation,
                                                            GtkPrintContext         *context,
                                                            gint                     page_nr);
static void           mousepad_print_end_print             (GtkPrintOperation       *operation,
                                                            GtkPrintContext         *context);
static void           mousepad_print_page_setup_dialog     (GtkWidget               *button,
                                                            GtkPrintOperation       *operation);
static void           mousepad_print_button_toggled        (GtkWidget               *button,
                                                            MousepadPrint           *print);
static void           mousepad_print_button_font_set       (GtkFontButton           *button,
                                                            MousepadPrint           *print);
static PangoAttrList *mousepad_print_attr_list_bold        (void);
static GtkWidget     *mousepad_print_create_custom_widget  (GtkPrintOperation       *operation);
static void           mousepad_print_status_changed        (GtkPrintOperation       *operation);
static void           mousepad_print_done                  (GtkPrintOperation       *operation,
                                                            GtkPrintOperationResult  result);



struct _MousepadPrintClass
{
  GtkPrintOperationClass __parent__;
};

struct _MousepadPrint
{
  GtkPrintOperation __parent__;

  /* the document we're going to print */
  MousepadDocument *document;

  /* pango layout containing all text */
  PangoLayout      *layout;

  /* array with the lines drawn on each page */
  GArray           *lines;

  /* drawing offsets */
  gint              x_offset;
  gint              y_offset;

  /* page line number counter */
  gint              line_number;

  /* print dialog widgets */
  GtkWidget        *widget_page_headers;
  GtkWidget        *widget_line_numbers;
  GtkWidget        *widget_text_wrapping;

  /* settings */
  guint             print_page_headers : 1;
  guint             print_line_numbers : 1;
  guint             text_wrapping : 1;
  gchar            *font_name;
};



G_DEFINE_TYPE (MousepadPrint, mousepad_print, GTK_TYPE_PRINT_OPERATION);



static void
mousepad_print_class_init (MousepadPrintClass *klass)
{
  GObjectClass           *gobject_class;
  GtkPrintOperationClass *gtkprintoperation_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_print_finalize;

  gtkprintoperation_class = GTK_PRINT_OPERATION_CLASS (klass);
  gtkprintoperation_class->begin_print = mousepad_print_begin_print;
  gtkprintoperation_class->draw_page = mousepad_print_draw_page;
  gtkprintoperation_class->end_print = mousepad_print_end_print;
  gtkprintoperation_class->create_custom_widget = mousepad_print_create_custom_widget;
  gtkprintoperation_class->status_changed = mousepad_print_status_changed;
  gtkprintoperation_class->done = mousepad_print_done;
}



static void
mousepad_print_init (MousepadPrint *print)
{
  /* init */
  print->print_page_headers = FALSE;
  print->print_line_numbers = FALSE;
  print->text_wrapping = FALSE;
  print->x_offset = 0;
  print->y_offset = 0;
  print->line_number = 0;
  print->font_name = NULL;

  /* set a custom tab label */
  gtk_print_operation_set_custom_tab_label (GTK_PRINT_OPERATION (print), _("Document Settings"));
}



static void
mousepad_print_finalize (GObject *object)
{
  MousepadPrint *print = MOUSEPAD_PRINT (object);

  /* cleanup */
  g_free (print->font_name);

  (*G_OBJECT_CLASS (mousepad_print_parent_class)->finalize) (object);
}



static void
mousepad_print_settings_load (GtkPrintOperation *operation)
{
  MousepadPrint         *print = MOUSEPAD_PRINT (operation);
  GKeyFile              *keyfile;
  gchar                 *filename;
  GtkPrintSettings      *settings = NULL;
  gchar                **keys;
  gint                   i;
  gchar                 *key;
  gchar                 *value;
  GtkPageSetup          *page_setup;
  GtkPaperSize          *paper_size;
  PangoContext          *context;
  PangoFontDescription  *font_desc;

  mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (print->document));
  mousepad_return_if_fail (GTK_IS_WIDGET (print->document->textview));

  /* get the config file filename */
  filename = mousepad_util_get_save_location (MOUSEPAD_RC_RELPATH, FALSE);
  if (G_UNLIKELY (filename == NULL))
    return;

  /* create a new keyfile */
  keyfile = g_key_file_new ();

  if (G_LIKELY (g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, NULL)))
    {
      /* get all the keys from the config file */
      keys = g_key_file_get_keys (keyfile, "Print Settings", NULL, NULL);

      if (G_LIKELY (keys))
        {
          /* create new settings object */
          settings = gtk_print_settings_new ();

          /* set all the keys */
          for (i = 0; keys[i] != NULL; i++)
            {
              /* read the value from the config file */
              value = g_key_file_get_value (keyfile, "Print Settings", keys[i], NULL);

              /* set the value */
              if (G_LIKELY (value))
                {
                  key = mousepad_util_key_name (keys[i]);
                  gtk_print_settings_set (settings, key, value);
                  g_free (key);
                  g_free (value);
                }
            }

          /* cleanup */
          g_strfreev (keys);
        }
    }

  /* free the key file */
  g_key_file_free (keyfile);

  /* cleanup */
  g_free (filename);

  if (G_LIKELY (settings))
    {
      /* apply the settings */
      gtk_print_operation_set_print_settings (operation, settings);

      if (gtk_print_settings_get_bool (settings, "page-setup-saved") == TRUE)
        {
          /* create new page setup */
          page_setup = gtk_page_setup_new ();

          /* set orientation */
          gtk_page_setup_set_orientation (page_setup, gtk_print_settings_get_orientation (settings));

          /* restore margins */
          gtk_page_setup_set_top_margin (page_setup, gtk_print_settings_get_double (settings, "top-margin"), GTK_UNIT_MM);
          gtk_page_setup_set_bottom_margin (page_setup, gtk_print_settings_get_double (settings, "bottom-margin"), GTK_UNIT_MM);
          gtk_page_setup_set_right_margin (page_setup, gtk_print_settings_get_double (settings, "right-margin"), GTK_UNIT_MM);
          gtk_page_setup_set_left_margin (page_setup, gtk_print_settings_get_double (settings, "left-margin"), GTK_UNIT_MM);

          /* set paper size */
          paper_size = gtk_print_settings_get_paper_size (settings);
          if (G_LIKELY (paper_size))
            gtk_page_setup_set_paper_size (page_setup, paper_size);

          /* set the default page setup */
          gtk_print_operation_set_default_page_setup (operation, page_setup);

          /* release reference */
          g_object_unref (G_OBJECT (page_setup));
        }

      /* restore print settings */
      print->print_page_headers = gtk_print_settings_get_bool (settings, "print-page-headers");
      print->print_line_numbers = gtk_print_settings_get_bool (settings, "print-line-numbers");
      print->text_wrapping = gtk_print_settings_get_bool (settings, "text-wrapping");
      print->font_name = g_strdup (gtk_print_settings_get (settings, "font-name"));

      /* release reference */
      g_object_unref (G_OBJECT (settings));
    }

  /* if no font name is set, get the one used in the widget */
  if (G_UNLIKELY (print->font_name == NULL))
    {
      /* get the font description from the context and convert it into a string */
      context = gtk_widget_get_pango_context (GTK_WIDGET (print->document->textview));
      font_desc = pango_context_get_font_description (context);
      print->font_name = pango_font_description_to_string (font_desc);
    }
}



static void
mousepad_print_settings_save_foreach (const gchar *key,
                                      const gchar *value,
                                      gpointer     user_data)
{
  GKeyFile *keyfile = user_data;
  gchar    *config;

  /* save the setting */
  if (G_LIKELY (key && value))
    {
      config = mousepad_util_config_name (key);
      g_key_file_set_value (keyfile, "Print Settings", config, value);
      g_free (config);
    }
}



static void
mousepad_print_settings_save (GtkPrintOperation *operation)
{
  MousepadPrint    *print = MOUSEPAD_PRINT (operation);
  GKeyFile         *keyfile;
  gchar            *filename;
  GtkPrintSettings *settings;
  GtkPageSetup     *page_setup;
  GtkPaperSize     *paper_size;

  /* get the save location */
  filename = mousepad_util_get_save_location (MOUSEPAD_RC_RELPATH, TRUE);

  /* create a new keyfile */
  keyfile = g_key_file_new ();

  /* load the existing settings */
  if (G_LIKELY (g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, NULL)))
    {
      /* get the print settings */
      settings = gtk_print_operation_get_print_settings (operation);

      if (G_LIKELY (settings != NULL))
        {
          /* get the page setup */
          page_setup = gtk_print_operation_get_default_page_setup (operation);

          /* restore the page setup */
          if (G_LIKELY (page_setup != NULL))
            {
              /* the the settings page orienation */
              gtk_print_settings_set_orientation (settings, gtk_page_setup_get_orientation (page_setup));

              /* save margins */
              gtk_print_settings_set_double (settings, "top-margin", gtk_page_setup_get_top_margin (page_setup, GTK_UNIT_MM));
              gtk_print_settings_set_double (settings, "bottom-margin", gtk_page_setup_get_bottom_margin (page_setup, GTK_UNIT_MM));
              gtk_print_settings_set_double (settings, "right-margin", gtk_page_setup_get_right_margin (page_setup, GTK_UNIT_MM));
              gtk_print_settings_set_double (settings, "left-margin", gtk_page_setup_get_left_margin (page_setup, GTK_UNIT_MM));

              /* get the paper size */
              paper_size = gtk_page_setup_get_paper_size (page_setup);

              /* set settings page size */
              if (G_LIKELY (paper_size))
                gtk_print_settings_set_paper_size (settings, paper_size);
            }

          /* a bool we use for loading */
          gtk_print_settings_set_bool (settings, "page-setup-saved", page_setup != NULL);

          /* set print settings */
          gtk_print_settings_set_bool (settings, "print-page-headers", print->print_page_headers);
          gtk_print_settings_set_bool (settings, "print-line-numbers", print->print_line_numbers);
          gtk_print_settings_set_bool (settings, "text-wrapping", print->text_wrapping);
          gtk_print_settings_set (settings, "font-name", print->font_name);

          /* store all the print settings */
          gtk_print_settings_foreach (settings, mousepad_print_settings_save_foreach, keyfile);

          /* save the contents */
          mousepad_util_save_key_file (keyfile, filename);
        }
    }

  /* cleanup */
  g_key_file_free (keyfile);
  g_free (filename);
}



static void
mousepad_print_begin_print (GtkPrintOperation *operation,
                            GtkPrintContext   *context)
{
  MousepadPrint        *print = MOUSEPAD_PRINT (operation);
  MousepadDocument     *document = print->document;
  GtkTextIter           start_iter, end_iter;
  gint                  page_height;
  gint                  page_width;
  gint                  layout_height;
  PangoRectangle        rect;
  PangoLayoutLine      *line;
  PangoFontDescription *font_desc;
  gchar                *text;
  gint                  i;
  gint                  size;
  gint                  n_pages = 1;

  /* create the pango layout */
  print->layout = gtk_print_context_create_pango_layout (context);

  /* set layout font */
  font_desc = pango_font_description_from_string (print->font_name);
  pango_layout_set_font_description (print->layout, font_desc);
  pango_font_description_free (font_desc);

  /* calculate page header height */
  if (print->print_page_headers)
    {
      /* set some pango layout text */
      pango_layout_set_text (print->layout, "Page 1234", -1);

      /* get the height */
      pango_layout_get_pixel_size (print->layout, NULL, &size);

      /* set the header offset */
      print->y_offset = size + DOCUMENT_SPACING;
    }

  /* calculate the line number offset */
  if (print->print_line_numbers)
    {
      /* insert the highest line number in the layout */
      text = g_strdup_printf ("%d", MAX (99, gtk_text_buffer_get_line_count (document->buffer)));
      pango_layout_set_text (print->layout, text, -1);
      g_free (text);

      /* get the width of the layout */
      pango_layout_get_pixel_size (print->layout, &size, NULL);

      /* set the text offset */
      print->x_offset = size + DOCUMENT_SPACING;
    }

  /* set the layout width */
  page_width = gtk_print_context_get_width (context);
  pango_layout_set_width (print->layout, PANGO_SCALE * (page_width - print->x_offset));

  /* wrapping mode for the layout */
  pango_layout_set_wrap (print->layout, print->text_wrapping ? PANGO_WRAP_WORD_CHAR : PANGO_WRAP_CHAR);

  /* get the text in the entire buffer */
  gtk_text_buffer_get_bounds (document->buffer, &start_iter, &end_iter);
  text = gtk_text_buffer_get_slice (document->buffer, &start_iter, &end_iter, TRUE);

  /* set the layout text */
  pango_layout_set_text (print->layout, text, -1);

  /* cleanup */
  g_free (text);

  /* get the page height in pango units */
  page_height = gtk_print_context_get_height (context) - print->y_offset;

  /* create and empty array to store the line numbers */
  print->lines = g_array_new (FALSE, FALSE, sizeof (gint));

  /* reset layout height */
  layout_height = 0;

  /* start with a zerro */
  g_array_append_val (print->lines, layout_height);

  for (i = 0; i < pango_layout_get_line_count (print->layout); i++)
    {
      /* get the line */
#if PANGO_VERSION_CHECK (1, 16, 0)
      line = pango_layout_get_line_readonly (print->layout, i);
#else
      line = pango_layout_get_line (print->layout, i);
#endif

      /* if we don't wrap lines, skip the lines that don't start a paragraph */
      if (print->text_wrapping == FALSE && line->is_paragraph_start == FALSE)
        continue;

      /* get the line height */
      pango_layout_line_get_pixel_extents (line, NULL, &rect);

      /* append the height to the total page height */
      layout_height += rect.height;

      /* check if the layout still fits in the page */
      if (layout_height > page_height)
        {
          /* reset the layout height */
          layout_height = 0;

          /* increase page counter */
          n_pages++;

          /* append the line number to the array */
          g_array_append_val (print->lines, i);
        }
    }

  /* append the last line to the array */
  g_array_append_val (print->lines, i);

  /* set the number of pages we're going to draw */
  gtk_print_operation_set_n_pages (operation, n_pages);
}



static void
mousepad_print_draw_page (GtkPrintOperation *operation,
                          GtkPrintContext   *context,
                          gint               page_nr)
{
  MousepadPrint    *print = MOUSEPAD_PRINT (operation);
  MousepadDocument *document = print->document;
  PangoLayoutLine  *line;
  cairo_t          *cr;
  PangoRectangle    rect;
  gint              i;
  gint              x, y;
  gint              width, height;
  gint              start, end;
  gchar            *text;
  PangoLayout      *layout;

  /* get the cairo context */
  cr = gtk_print_context_get_cairo_context (context);

  /* get the start and end line from the array */
  start = g_array_index (print->lines, gint, page_nr);
  end = g_array_index (print->lines, gint, page_nr + 1);

  /* set line width */
  cairo_set_line_width (cr, 1);

  /* create an empty pango layout */
  layout = gtk_print_context_create_pango_layout (context);
  pango_layout_set_font_description (layout, pango_layout_get_font_description (print->layout));

  /* print header */
  if (print->print_page_headers)
    {
      /* create page number */
      text = g_strdup_printf (_("Page %d of %d"), page_nr + 1, print->lines->len - 1);
      pango_layout_set_text (layout, text, -1);
      g_free (text);

      /* get layout size */
      pango_layout_get_pixel_size (layout, &width, &height);

      /* position right of the document */
      x = gtk_print_context_get_width (context) - width;

      /* show the layout */
      cairo_move_to (cr, x, 0);
      pango_cairo_show_layout (cr, layout);

      /* set the layout width of the filename */
      pango_layout_set_width (layout, PANGO_SCALE * (x - DOCUMENT_SPACING * 2));

      /* ellipsize the start of the filename */
      pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_START);

      /* set the document filename as text */
      if (mousepad_document_get_filename (document))
        pango_layout_set_text (layout, mousepad_document_get_filename (document), -1);
      else
        pango_layout_set_text (layout, mousepad_document_get_basename (document), -1);

      /* show the layout */
      cairo_move_to (cr, 0, 0);
      pango_cairo_show_layout (cr, layout);

      /* stroke a line under the header */
      cairo_move_to (cr, 0, height + 1);
      cairo_rel_line_to (cr, gtk_print_context_get_width (context), 0);
      cairo_stroke (cr);

      /* restore the layout for the line numbers */
      pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_NONE);
      pango_layout_set_width (layout, -1);
    }

  /* position and render all lines */
  for (i = start, y = print->y_offset; i < end; i++)
    {
      /* get the line */
#if PANGO_VERSION_CHECK (1, 16, 0)
      line = pango_layout_get_line_readonly (print->layout, i);
#else
      line = pango_layout_get_line (print->layout, i);
#endif

      /* if we don't wrap lines, skip the lines that don't start a paragraph */
      if (print->text_wrapping == FALSE && line->is_paragraph_start == FALSE)
        continue;

      /* get the line rectangle */
      pango_layout_line_get_pixel_extents (line, NULL, &rect);

      /* fix the start position */
      if (G_UNLIKELY (i == start))
        y -= rect.height / 3;

      /* add the line hight to the top offset */
      y += rect.height;

      /* move the cursor to the start of the text */
      cairo_move_to (cr, print->x_offset, y);

      /* show the line at the new position */
      pango_cairo_show_layout_line (cr, line);

      /* print line number */
      if (print->print_line_numbers && line->is_paragraph_start)
        {
          /* increase page number */
          print->line_number++;

          /* render a page number in the layout */
          text = g_strdup_printf ("%d", print->line_number);
          pango_layout_set_text (layout, text, -1);
          g_free (text);

          /* move the cursor to the start of the line */
          cairo_move_to (cr, 0, y);

          /* pick the first line and draw it on the cairo context */
#if PANGO_VERSION_CHECK (1, 16, 0)
          line = pango_layout_get_line_readonly (print->layout, 0);
#else
          line = pango_layout_get_line (print->layout, 0);
#endif
          pango_cairo_show_layout_line (cr, line);
        }
    }

  /* release the layout */
  g_object_unref (G_OBJECT (layout));
}



static void
mousepad_print_end_print (GtkPrintOperation *operation,
                          GtkPrintContext   *context)
{
  MousepadPrint *print = MOUSEPAD_PRINT (operation);

  /* release the layout */
  g_object_unref (G_OBJECT (print->layout));

  /* free array */
  g_array_free (print->lines, TRUE);
}



static void
mousepad_print_page_setup_dialog (GtkWidget         *button,
                                  GtkPrintOperation *operation)
{
  GtkWidget        *toplevel;
  GtkPrintSettings *settings;
  GtkPageSetup     *page_setup;

  /* get the toplevel of the button */
  toplevel = gtk_widget_get_toplevel (button);
  if (G_UNLIKELY (!GTK_WIDGET_TOPLEVEL (toplevel)))
    toplevel = NULL;

  /* get the print settings */
  settings = gtk_print_operation_get_print_settings (operation);
  if (G_UNLIKELY (settings == NULL))
    settings = gtk_print_settings_new ();

  /* get the page setup */
  page_setup = gtk_print_operation_get_default_page_setup (operation);

  /* run the dialog */
  page_setup = gtk_print_run_page_setup_dialog (GTK_WINDOW (toplevel), page_setup, settings);

  /* set the new page setup */
  gtk_print_operation_set_default_page_setup (operation, page_setup);
}



static void
mousepad_print_button_toggled (GtkWidget     *button,
                               MousepadPrint *print)
{
  gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  /* save the correct setting */
  if (button == print->widget_page_headers)
    print->print_page_headers = active;
  else if (button == print->widget_line_numbers)
    print->print_line_numbers = active;
  else if (button == print->widget_text_wrapping)
    print->text_wrapping = active;
}



static void
mousepad_print_button_font_set (GtkFontButton *button,
                                MousepadPrint *print)
{
  /* remove old font name */
  g_free (print->font_name);

  /* set new font */
  print->font_name = g_strdup (gtk_font_button_get_font_name (button));
}



static PangoAttrList *
mousepad_print_attr_list_bold (void)
{
  static PangoAttrList *attr_list = NULL;
  PangoAttribute       *attr;

  if (G_UNLIKELY (attr_list == NULL))
    {
      /* create new attributes list */
      attr_list = pango_attr_list_new ();

      /* create attribute */
      attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
      attr->start_index = 0;
      attr->end_index = -1;

      /* insert bold element */
      pango_attr_list_insert (attr_list, attr);
    }

  return attr_list;
}



static GtkWidget *
mousepad_print_create_custom_widget (GtkPrintOperation *operation)
{
  MousepadPrint *print = MOUSEPAD_PRINT (operation);
  GtkWidget     *button;
  GtkWidget     *vbox, *vbox2;
  GtkWidget     *frame;
  GtkWidget     *alignment;
  GtkWidget     *label;

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Page Setup"));
  gtk_label_set_attributes (GTK_LABEL (label), mousepad_print_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  alignment = gtk_alignment_new (0.0, 0.5, 0.0, 1.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 6, 12, 6);
  gtk_container_add (GTK_CONTAINER (frame), alignment);
  gtk_widget_show (alignment);

  button = mousepad_util_image_button (GTK_STOCK_PROPERTIES, _("_Adjust page size and orientation"));
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (mousepad_print_page_setup_dialog), operation);
  gtk_container_add (GTK_CONTAINER (alignment), button);
  gtk_widget_show (button);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Appearance"));
  gtk_label_set_attributes (GTK_LABEL (label), mousepad_print_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 6, 12, 6);
  gtk_container_add (GTK_CONTAINER (frame), alignment);
  gtk_widget_show (alignment);

  vbox2 = gtk_vbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (alignment), vbox2);
  gtk_widget_show (vbox2);

  button = print->widget_page_headers = gtk_check_button_new_with_mnemonic (_("Print page _headers"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), print->print_page_headers);
  g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (mousepad_print_button_toggled), print);
  gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  button = print->widget_line_numbers = gtk_check_button_new_with_mnemonic (_("Print _line numbers"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), print->print_line_numbers);
  g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (mousepad_print_button_toggled), print);
  gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  button = print->widget_text_wrapping = gtk_check_button_new_with_mnemonic (_("Enable text _wrapping"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), print->text_wrapping);
  g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (mousepad_print_button_toggled), print);
  gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Font"));
  gtk_label_set_attributes (GTK_LABEL (label), mousepad_print_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  alignment = gtk_alignment_new (0.0, 0.5, 0.0, 1.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 6, 12, 6);
  gtk_container_add (GTK_CONTAINER (frame), alignment);
  gtk_widget_show (alignment);

  button = gtk_font_button_new_with_font (print->font_name);
  gtk_container_add (GTK_CONTAINER (alignment), button);
  g_signal_connect (G_OBJECT (button), "font-set", G_CALLBACK (mousepad_print_button_font_set), print);
  gtk_widget_show (button);

  return vbox;
}



static void
mousepad_print_status_changed (GtkPrintOperation *operation)
{
  /* nothing usefull for now, we could set a statusbar text or something */
}



static void
mousepad_print_done (GtkPrintOperation       *operation,
                     GtkPrintOperationResult  result)
{
  /* check if the print succeeded */
  if (result == GTK_PRINT_OPERATION_RESULT_APPLY)
    {
      /* save the settings */
      mousepad_print_settings_save (operation);
    }
}



MousepadPrint *
mousepad_print_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_PRINT, NULL);
}



gboolean
mousepad_print_document_interactive (MousepadPrint     *print,
                                     MousepadDocument  *document,
                                     GtkWindow         *parent,
                                     GError           **error)
{
  GtkPrintOperationResult result;

  mousepad_return_val_if_fail (MOUSEPAD_IS_PRINT (print), FALSE);
  mousepad_return_val_if_fail (GTK_IS_PRINT_OPERATION (print), FALSE);
  mousepad_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), FALSE);
  mousepad_return_val_if_fail (GTK_IS_WINDOW (parent), FALSE);
  mousepad_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* set the document */
  print->document = document;

  /* load settings */
  mousepad_print_settings_load (GTK_PRINT_OPERATION (print));

  /* allow async printing is support by the platform */
  gtk_print_operation_set_allow_async (GTK_PRINT_OPERATION (print), TRUE);

  /* run the operation */
  result = gtk_print_operation_run (GTK_PRINT_OPERATION (print),
                                    GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
                                    parent, error);

  return (result != GTK_PRINT_OPERATION_RESULT_ERROR);
}
