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

#ifndef __MOUSEPAD_DIALOGS_H__
#define __MOUSEPAD_DIALOGS_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* dialog responses */
enum {
  MOUSEPAD_RESPONSE_CANCEL,
  MOUSEPAD_RESPONSE_CLEAR,
  MOUSEPAD_RESPONSE_CLOSE,
  MOUSEPAD_RESPONSE_DONT_SAVE,
  MOUSEPAD_RESPONSE_FIND,
  MOUSEPAD_RESPONSE_JUMP_TO,
  MOUSEPAD_RESPONSE_OK,
  MOUSEPAD_RESPONSE_OVERWRITE,
  MOUSEPAD_RESPONSE_REPLACE,
  MOUSEPAD_RESPONSE_REVERT,
  MOUSEPAD_RESPONSE_SAVE,
  MOUSEPAD_RESPONSE_SAVE_AS,
};


void       mousepad_dialogs_show_about          (GtkWindow     *parent);

void       mousepad_dialogs_show_error          (GtkWindow     *parent,
                                                 const GError  *error,
                                                 const gchar   *message);

void       mousepad_dialogs_show_help           (GtkWindow     *parent,
                                                 const gchar   *page,
                                                 const gchar   *offset);

gint       mousepad_dialogs_other_tab_size      (GtkWindow     *parent,
                                                 gint           active_size);

gboolean   mousepad_dialogs_go_to               (GtkWindow     *parent,
                                                 GtkTextBuffer *buffer);

gboolean   mousepad_dialogs_clear_recent        (GtkWindow     *parent);

gint       mousepad_dialogs_save_changes        (GtkWindow     *parent,
                                                 gboolean       readonly);

gint       mousepad_dialogs_externally_modified (GtkWindow     *parent);

gint       mousepad_dialogs_revert              (GtkWindow     *parent);

G_END_DECLS

#endif /* !__MOUSEPAD_DIALOGS_H__ */
