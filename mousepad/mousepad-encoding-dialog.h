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

#ifndef __MOUSEPAD_ENCODING_DIALOG_H__
#define __MOUSEPAD_ENCODING_DIALOG_H__

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_ENCODING_DIALOG            (mousepad_encoding_dialog_get_type ())
#define MOUSEPAD_ENCODING_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_ENCODING_DIALOG, MousepadEncodingDialog))
#define MOUSEPAD_ENCODING_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_ENCODING_DIALOG, MousepadEncodingDialogClass))
#define MOUSEPAD_IS_ENCODING_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_ENCODING_DIALOG))
#define MOUSEPAD_IS_ENCODING_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_ENCODING_DIALOG))
#define MOUSEPAD_ENCODING_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_ENCODING_DIALOG, MousepadEncodingDialogClass))

typedef struct _MousepadEncodingDialogClass MousepadEncodingDialogClass;
typedef struct _MousepadEncodingDialog      MousepadEncodingDialog;

GType        mousepad_encoding_dialog_get_type     (void) G_GNUC_CONST;

GtkWidget   *mousepad_encoding_dialog_new          (GtkWindow              *parent,
                                                    MousepadFile           *file);

const gchar *mousepad_encoding_dialog_get_encoding (MousepadEncodingDialog *dialog);

G_END_DECLS

#endif /* !__MOUSEPAD_ENCODING_DIALOG_H__ */
