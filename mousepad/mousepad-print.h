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

#ifndef __MOUSEPAD_PRINT_H__
#define __MOUSEPAD_PRINT_H__

G_BEGIN_DECLS

typedef struct _MousepadPrintClass MousepadPrintClass;
typedef struct _MousepadPrint      MousepadPrint;

#define MOUSEPAD_TYPE_PRINT            (mousepad_print_get_type ())
#define MOUSEPAD_PRINT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_PRINT, MousepadPrint))
#define MOUSEPAD_PRINT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_PRINT, MousepadPrintClass))
#define MOUSEPAD_IS_PRINT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_PRINT))
#define MOUSEPAD_IS_PRINT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MOUSEPAD_TYPE_PRINT))
#define MOUSEPAD_PRINT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_PRINT, MousepadPrintClass))

GType          mousepad_print_get_type             (void) G_GNUC_CONST;

MousepadPrint *mousepad_print_new                  (void);

gboolean       mousepad_print_document_interactive (MousepadPrint     *print,
                                                    MousepadDocument  *document,
                                                    GtkWindow         *parent,
                                                    GError           **error);

G_END_DECLS

#endif /* !__MOUSEPAD_PRINT_H__ */
