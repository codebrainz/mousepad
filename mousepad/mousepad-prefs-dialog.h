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

#ifndef __MOUSEPAD_PREFS_DIALOG_H__
#define __MOUSEPAD_PREFS_DIALOG_H__ 1

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_PREFS_DIALOG             (mousepad_prefs_dialog_get_type ())
#define MOUSEPAD_PREFS_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_PREFS_DIALOG, MousepadPrefsDialog))
#define MOUSEPAD_PREFS_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_PREFS_DIALOG, MousepadPrefsDialogClass))
#define MOUSEPAD_IS_PREFS_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_PREFS_DIALOG))
#define MOUSEPAD_IS_PREFS_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOUSEPAD_TYPE_PREFS_DIALOG))
#define MOUSEPAD_PREFS_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_PREFS_DIALOG, MousepadPrefsDialogClass))

typedef struct MousepadPrefsDialog_         MousepadPrefsDialog;
typedef struct MousepadPrefsDialogClass_    MousepadPrefsDialogClass;

GType      mousepad_prefs_dialog_get_type (void);

GtkWidget *mousepad_prefs_dialog_new      (void);

G_END_DECLS

#endif /* __MOUSEPAD_PREFS_DIALOG_H__ */
