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
#include <mousepad/mousepad-preferences.h>
#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-print.h>



static void mousepad_print_class_init (MousepadPrintClass *klass);
static void mousepad_print_init (MousepadPrint *print);
static void mousepad_print_finalize (GObject *object);
static void mousepad_print_begin_print (GtkPrintOperation *operation, GtkPrintContext   *context);
static void mousepad_print_draw_page (GtkPrintOperation *operation, GtkPrintContext *context, gint page_nr);
static void mousepad_print_end_print (GtkPrintOperation *operation, GtkPrintContext *context);
static GtkWidget *mousepad_print_create_custom_widget (GtkPrintOperation *operation);
static void mousepad_print_status_changed (GtkPrintOperation *operation);



struct _MousepadPrintClass
{
  GtkPrintOperationClass __parent__;
};

struct _MousepadPrint
{
  GtkPrintOperation __parent__;

  /* the document we're going to print */
  MousepadDocument    *document;

  /* calculated lines per page */
  gint                 lines_per_page;
};



static GObjectClass *mousepad_print_parent_class;



GType
mousepad_print_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_type_register_static_simple (GTK_TYPE_PRINT_OPERATION,
                                            I_("MousepadPrint"),
                                            sizeof (MousepadPrintClass),
                                            (GClassInitFunc) mousepad_print_class_init,
                                            sizeof (MousepadPrint),
                                            (GInstanceInitFunc) mousepad_print_init,
                                            0);
    }

  return type;
}



static void
mousepad_print_class_init (MousepadPrintClass *klass)
{
  GObjectClass           *gobject_class;
  GtkPrintOperationClass *gtkprintoperation_class;

  mousepad_print_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_print_finalize;

  gtkprintoperation_class = GTK_PRINT_OPERATION_CLASS (klass);
  gtkprintoperation_class->begin_print = mousepad_print_begin_print;
  gtkprintoperation_class->draw_page = mousepad_print_draw_page;
  gtkprintoperation_class->end_print = mousepad_print_end_print;
  gtkprintoperation_class->create_custom_widget = mousepad_print_create_custom_widget;
  gtkprintoperation_class->status_changed = mousepad_print_status_changed;
}



static void
mousepad_print_init (MousepadPrint *print)
{
  /* set a custom tab label */
  //gtk_print_operation_set_custom_tab_label (GTK_PRINT_OPERATION (print), _("Document Settings"));
}



static void
mousepad_print_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (mousepad_print_parent_class)->finalize) (object);
}



static void
mousepad_print_begin_print (GtkPrintOperation *operation,
                            GtkPrintContext   *context)
{
  MousepadPrint *print = MOUSEPAD_PRINT (operation);
  gint           n_lines, font_height;
  gint           page_height, n_pages;

  _mousepad_return_if_fail (MOUSEPAD_IS_PRINT (print));
  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (print->document));
  _mousepad_return_if_fail (GTK_IS_TEXT_BUFFER (print->document->buffer));

  /* get the document font information */
  mousepad_document_get_font_information (print->document, NULL, &font_height);

  /* get the number of lines in the buffer */
  n_lines = gtk_text_buffer_get_line_count (print->document->buffer);

  /* get the page height */
  page_height = gtk_print_context_get_height (context);

  /* lines per page */
  print->lines_per_page = page_height / font_height;

  /* calculate the number of pages */
  n_pages = (n_lines / print->lines_per_page) + 1;

  /* set the number of pages */
  gtk_print_operation_set_n_pages (operation, n_pages);
}



static void
mousepad_print_draw_page (GtkPrintOperation *operation,
                          GtkPrintContext   *context,
                          gint               page_nr)
{
  MousepadPrint        *print = MOUSEPAD_PRINT (operation);
  PangoLayout          *layout;
  PangoLayoutLine      *line;
  PangoFontDescription *font_desc;
  gint                  start, end;
  gint                  i, font_height;
  GtkTextIter           start_iter, end_iter;
  MousepadDocument     *document = print->document;
  gchar                *text;
  cairo_t              *cr;

  _mousepad_return_if_fail (MOUSEPAD_IS_PRINT (print));

  /* start and end line in the buffer */
  start = print->lines_per_page * page_nr;
  end = start + print->lines_per_page - 1;

  /* get the iters */
  gtk_text_buffer_get_iter_at_line (document->buffer, &start_iter, start);
  gtk_text_buffer_get_iter_at_line (document->buffer, &end_iter, end);
  gtk_text_iter_forward_to_line_end (&end_iter);

  /* create the pango layout */
  layout = gtk_print_context_create_pango_layout (context);

  /* get the document font information */
  mousepad_document_get_font_information (document, &font_desc, &font_height);

  /* set the layout font description */
  pango_layout_set_font_description (layout, font_desc);

  /* get the text slice */
  text = gtk_text_buffer_get_slice (document->buffer, &start_iter, &end_iter, TRUE);

  /* set the pango layout text */
  pango_layout_set_text (layout, text, -1);

  /* cleanup */
  g_free (text);

  /* get the cairo context */
  cr = gtk_print_context_get_cairo_context (context);

  /* show all the lines at the correct position */
  for (i = 0; i < pango_layout_get_line_count (layout); i++)
    {
      /* get the line */
      line = pango_layout_get_line_readonly (layout, i);

      /* move the cairo position */
      cairo_move_to (cr, 0, font_height * (i + 1));

      /* show the line at the new position */
      pango_cairo_show_layout_line (cr, line);
    }

  /* release the layout */
  g_object_unref (G_OBJECT (layout));
}



static void
mousepad_print_end_print (GtkPrintOperation *operation,
                          GtkPrintContext   *context)
{

}



static GtkWidget *
mousepad_print_create_custom_widget (GtkPrintOperation *operation)
{
  return NULL;
}



static void
mousepad_print_status_changed (GtkPrintOperation *operation)
{

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

  _mousepad_return_val_if_fail (MOUSEPAD_IS_PRINT (print), FALSE);
  _mousepad_return_val_if_fail (GTK_IS_PRINT_OPERATION (print), FALSE);
  _mousepad_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), FALSE);
  _mousepad_return_val_if_fail (GTK_IS_WINDOW (parent), FALSE);
  _mousepad_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* set the document */
  print->document = document;

  /* run the operation */
  result = gtk_print_operation_run (GTK_PRINT_OPERATION (print),
                                    GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
                                    parent, error);

  return (result != GTK_PRINT_OPERATION_RESULT_ERROR);
}
