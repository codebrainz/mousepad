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
  MousepadDocument        *document;

  /* print dialog widgets */
  GtkWidget                *widget_page_headers;
  GtkWidget                *widget_page_footers;
  GtkWidget                *widget_line_numbers;
  GtkWidget                *widget_text_wrapping;
  GtkWidget                *widget_syntax_highlighting;

  /* source view print compositor */
  GtkSourcePrintCompositor *compositor;
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
  gtkprintoperation_class->create_custom_widget = mousepad_print_create_custom_widget;
  gtkprintoperation_class->status_changed = mousepad_print_status_changed;
  gtkprintoperation_class->done = mousepad_print_done;
}



static void
mousepad_print_init (MousepadPrint *print)
{
  /* init */
  print->compositor = NULL;

  /* set a custom tab label */
  gtk_print_operation_set_custom_tab_label (GTK_PRINT_OPERATION (print), _("Document Settings"));
}



static void
mousepad_print_finalize (GObject *object)
{
  MousepadPrint *print = MOUSEPAD_PRINT (object);

  /* cleanup */
  g_object_unref (print->compositor);

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
  gchar                 *font_name = NULL;
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
      g_object_set (print->compositor,
                    "print-header",
                    gtk_print_settings_get_bool (settings, "print-page-headers"),
#if 0
                    "print-footer",
                    gtk_print_settings_get_bool (settings, "print-page-footers"),
#endif
                    "print-line-numbers",
                    gtk_print_settings_get_bool (settings, "print-line-numbers"),
                    "wrap-mode",
                    gtk_print_settings_get_bool (settings, "text-wrapping") ? GTK_WRAP_WORD : GTK_WRAP_NONE,
                    "highlight-syntax",
                    gtk_print_settings_get_bool (settings, "syntax-highlighting"),
                    NULL);

      /* font-name setting sets the header, footer, line numbers and body fonts */
      font_name = g_strdup (gtk_print_settings_get (settings, "font-name"));

      /* release reference */
      g_object_unref (G_OBJECT (settings));
    }

    /* if no font name is set, get the one used in the widget */
    if (G_UNLIKELY (font_name == NULL))
      {
        /* get the font description from the context and convert it into a string */
        context = gtk_widget_get_pango_context (GTK_WIDGET (print->document->textview));
        font_desc = pango_context_get_font_description (context);
        font_name = pango_font_description_to_string (font_desc);
      }

    /* set the same font for all the various parts of the pages the same */
    g_object_set (print->compositor,
                  "body-font-name", font_name,
                  "line-numbers-font-name", font_name,
                  "header-font-name", font_name,
#if 0
                  "footer-font-name", font_name,
#endif
                  NULL);

    /* cleanup */
    g_free (font_name);
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
          gtk_print_settings_set_bool (settings,
                                       "print-page-headers",
                                       gtk_source_print_compositor_get_print_header (print->compositor));
#if 0
          gtk_print_settings_set_bool (settings,
                                       "print-page-footers",
                                       gtk_source_print_compositor_get_print_footer (print->compositor));
#endif
          gtk_print_settings_set_bool (settings,
                                       "print-line-numbers",
                                       gtk_source_print_compositor_get_print_line_numbers (print->compositor));

          gtk_print_settings_set_bool (settings,
                                       "text-wrapping",
                                       gtk_source_print_compositor_get_wrap_mode (print->compositor) == GTK_WRAP_NONE ? FALSE : TRUE);

          gtk_print_settings_set_bool (settings,
                                       "syntax-highlighting",
                                       gtk_source_print_compositor_get_highlight_syntax (print->compositor));

          gtk_print_settings_set (settings,
                                  "font-name",
                                  gtk_source_print_compositor_get_body_font_name (print->compositor));

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
  MousepadPrint    *print = MOUSEPAD_PRINT (operation);
  MousepadDocument *document = print->document;
  gint              n_pages = 1;
  const gchar      *file_name;

  /* print header */
  if (gtk_source_print_compositor_get_print_header (print->compositor))
    {
      if (mousepad_document_get_filename (document))
        file_name = mousepad_document_get_filename (document);
      else
        file_name = mousepad_document_get_basename (document);

      gtk_source_print_compositor_set_header_format (print->compositor,
                                                     TRUE,
                                                     file_name,
                                                     NULL,
                                                     "Page %N of %Q");
    }

  /* paginate all of the pages at once */
  while (!gtk_source_print_compositor_paginate (print->compositor, context))
    ;

  n_pages = gtk_source_print_compositor_get_n_pages (print->compositor);

  /* set the number of pages we're going to draw */
  gtk_print_operation_set_n_pages (operation, n_pages);
}



static void
mousepad_print_draw_page (GtkPrintOperation *operation,
                          GtkPrintContext   *context,
                          gint               page_nr)
{
  MousepadPrint *print = MOUSEPAD_PRINT (operation);

  gtk_source_print_compositor_draw_page (print->compositor, context, page_nr);
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
    gtk_source_print_compositor_set_print_header (print->compositor, active);
#if 0
  else if (button == print->widget_page_footers)
    gtk_source_print_compositor_set_print_footer (print->compositor, active);
#endif
  else if (button == print->widget_line_numbers)
    gtk_source_print_compositor_set_print_line_numbers (print->compositor, active);
  else if (button == print->widget_text_wrapping)
    gtk_source_print_compositor_set_wrap_mode (print->compositor, active ? GTK_WRAP_WORD : GTK_WRAP_NONE);
  else if (button == print->widget_syntax_highlighting)
    gtk_source_print_compositor_set_highlight_syntax (print->compositor, active);
}



static void
mousepad_print_button_font_set (GtkFontButton *button,
                                MousepadPrint *print)
{
  const gchar *font_name;

  font_name = gtk_font_button_get_font_name (button);

  g_object_set (print->compositor,
                "body-font-name", font_name,
                "line-numbers-font-name", font_name,
                "header-font-name", font_name,
                "footer-font-name", font_name,
                NULL);
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
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
                                gtk_source_print_compositor_get_print_header (print->compositor));
  g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (mousepad_print_button_toggled), print);
  gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

#if 0
  button = print->widget_page_footers = gtk_check_button_new_with_mnemonic (_("Print page _footers"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
                                gtk_source_print_compositor_get_print_footer (print->compositor));
  g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (mousepad_print_button_toggled), print);
  gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);
  gtk_widget_show (button);
#endif

  button = print->widget_line_numbers = gtk_check_button_new_with_mnemonic (_("Print _line numbers"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
                                gtk_source_print_compositor_get_print_line_numbers (print->compositor));
  g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (mousepad_print_button_toggled), print);
  gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  button = print->widget_text_wrapping = gtk_check_button_new_with_mnemonic (_("Enable text _wrapping"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
                                gtk_source_print_compositor_get_wrap_mode (print->compositor) == GTK_WRAP_NONE ? FALSE : TRUE);
  g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (mousepad_print_button_toggled), print);
  gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  button = print->widget_syntax_highlighting = gtk_check_button_new_with_mnemonic (_("Enable _syntax highlighting"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
                                gtk_source_print_compositor_get_highlight_syntax (print->compositor));
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

  button = gtk_font_button_new_with_font (gtk_source_print_compositor_get_body_font_name (print->compositor));
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
  mousepad_return_val_if_fail (GTK_IS_SOURCE_BUFFER (document->buffer), FALSE);
  mousepad_return_val_if_fail (GTK_IS_WINDOW (parent), FALSE);
  mousepad_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* set the document */
  print->document = document;
  print->compositor = gtk_source_print_compositor_new (GTK_SOURCE_BUFFER (document->buffer));

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
