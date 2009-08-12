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

#ifndef __MOUSEPAD_REPLACE_DIALOG_H__
#define __MOUSEPAD_REPLACE_DIALOG_H__

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_REPLACE_DIALOG            (mousepad_replace_dialog_get_type ())
#define MOUSEPAD_REPLACE_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_REPLACE_DIALOG, MousepadReplaceDialog))
#define MOUSEPAD_REPLACE_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_REPLACE_DIALOG, MousepadReplaceDialogClass))
#define MOUSEPAD_IS_REPLACE_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_REPLACE_DIALOG))
#define MOUSEPAD_IS_REPLACE_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPADL_TYPE_REPLACE_DIALOG))
#define MOUSEPAD_REPLACE_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_REPLACE_DIALOG, MousepadReplaceDialogClass))

typedef struct _MousepadReplaceDialogClass MousepadReplaceDialogClass;
typedef struct _MousepadReplaceDialog      MousepadReplaceDialog;

GType           mousepad_replace_dialog_get_type       (void) G_GNUC_CONST;

GtkWidget      *mousepad_replace_dialog_new            (void);

void            mousepad_replace_dialog_history_clean  (void);

void            mousepad_replace_dialog_page_switched  (MousepadReplaceDialog *dialog);

G_END_DECLS

#endif /* !__MOUSEPAD_REPLACE_DIALOG_H__ */
