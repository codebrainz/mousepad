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

#ifndef __MOUSEPAD_DOCUMENT_H__
#define __MOUSEPAD_DOCUMENT_H__

G_BEGIN_DECLS

#include <mousepad/mousepad-types.h>

typedef struct _MousepadDocumentClass MousepadDocumentClass;
typedef struct _MousepadDocument      MousepadDocument;

#define MOUSEPAD_SCROLL_MARGIN 0.02

#define MOUSEPAD_TYPE_DOCUMENT            (mousepad_document_get_type ())
#define MOUSEPAD_DOCUMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_DOCUMENT, MousepadDocument))
#define MOUSEPAD_DOCUMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_DOCUMENT, MousepadDocumentClass))
#define MOUSEPAD_IS_DOCUMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_DOCUMENT))
#define MOUSEPAD_IS_DOCUMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MOUSEPAD_TYPE_DOCUMENT))
#define MOUSEPAD_DOCUMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_DOCUMENT, MousepadDocumentClass))

GType           mousepad_document_get_type                 (void) G_GNUC_CONST;

GtkWidget      *mousepad_document_new                      (void);

gboolean        mousepad_document_reload                   (MousepadDocument    *document,
                                                            GError             **error);

gboolean        mousepad_document_save_file                (MousepadDocument    *document,
                                                            const gchar         *filename,
                                                            GError             **error);

void            mousepad_document_set_filename             (MousepadDocument    *document,
                                                            const gchar         *filename);

void            mousepad_document_set_font                 (MousepadDocument    *document,
                                                            const gchar         *font_name);

void            mousepad_document_set_auto_indent          (MousepadDocument    *document,
                                                            gboolean             auto_indent);

void            mousepad_document_set_line_numbers         (MousepadDocument    *document,
                                                            gboolean             line_numbers);

void            mousepad_document_set_overwrite            (MousepadDocument    *document,
                                                            gboolean             overwrite);

void            mousepad_document_set_word_wrap            (MousepadDocument    *document,
                                                            gboolean             word_wrap);

gboolean        mousepad_document_open_file                (MousepadDocument    *document,
                                                            const gchar         *filename,
                                                            GError             **error);

gboolean        mousepad_document_find                     (MousepadDocument    *document,
                                                            const gchar         *string,
                                                            MousepadSearchFlags  flags);

void            mousepad_document_replace                  (MousepadDocument    *document);

void            mousepad_document_highlight_all            (MousepadDocument    *document,
                                                            const gchar         *string,
                                                            MousepadSearchFlags  flags);

void            mousepad_document_cut_selection            (MousepadDocument    *document);;

void            mousepad_document_copy_selection           (MousepadDocument    *document);

void            mousepad_document_paste_clipboard          (MousepadDocument    *document);

void            mousepad_document_paste_column_clipboard   (MousepadDocument    *document);

void            mousepad_document_delete_selection         (MousepadDocument    *document);

void            mousepad_document_select_all               (MousepadDocument    *document);

void            mousepad_document_focus_textview           (MousepadDocument    *document);

void            mousepad_document_jump_to_line             (MousepadDocument    *document,
                                                            gint                 line_number);

void            mousepad_document_send_statusbar_signals   (MousepadDocument    *document);

void            mousepad_document_line_numbers             (MousepadDocument    *document,
                                                            gint                *current_line,
                                                            gint                *last_line);

gboolean        mousepad_document_get_externally_modified  (MousepadDocument    *document);

gboolean        mousepad_document_get_can_undo             (MousepadDocument    *document);

gboolean        mousepad_document_get_can_redo             (MousepadDocument    *document);

const gchar    *mousepad_document_get_filename             (MousepadDocument    *document);

gboolean        mousepad_document_get_has_selection        (MousepadDocument    *document);

gboolean        mousepad_document_get_modified             (MousepadDocument    *document);

gboolean        mousepad_document_get_readonly             (MousepadDocument    *document);

GtkWidget      *mousepad_document_get_tab_label            (MousepadDocument    *document);

const gchar    *mousepad_document_get_title                (MousepadDocument    *document,
                                                            gboolean             show_full_path);

gboolean        mousepad_document_get_word_wrap            (MousepadDocument    *document);

gboolean        mousepad_document_get_line_numbers         (MousepadDocument    *document);

gboolean        mousepad_document_get_auto_indent          (MousepadDocument    *document);

void            mousepad_document_undo                     (MousepadDocument    *document);

void            mousepad_document_redo                     (MousepadDocument    *document);

G_END_DECLS

#endif /* !__MOUSEPAD_DOCUMENT_H__ */
