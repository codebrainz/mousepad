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

#ifndef __MOUSEPAD_DIALOGS_H__
#define __MOUSEPAD_DIALOGS_H__

G_BEGIN_DECLS

enum {
  MOUSEPAD_RESPONSE_CANCEL,
  MOUSEPAD_RESPONSE_CLEAR,
  MOUSEPAD_RESPONSE_DONT_SAVE,
  MOUSEPAD_RESPONSE_JUMP_TO,
  MOUSEPAD_RESPONSE_OVERWRITE,
  MOUSEPAD_RESPONSE_RELOAD,
  MOUSEPAD_RESPONSE_SAVE,
  MOUSEPAD_RESPONSE_SAVE_AS,
};

void      mousepad_dialogs_show_about    (GtkWindow    *parent);


void      mousepad_dialogs_show_error    (GtkWindow    *parent,
                                          const GError *error,
                                          const gchar  *message);

gint      mousepad_dialogs_jump_to       (GtkWindow    *parent,
                                          gint          current_line,
                                          gint          last_line);

gboolean  mousepad_dialogs_clear_recent  (GtkWindow    *parent);

gint      mousepad_dialogs_save_changes  (GtkWindow    *parent);

gchar    *mousepad_dialogs_save_as       (GtkWindow    *parent,
                                          const gchar  *filename);

gint      mousepad_dialogs_ask_overwrite (GtkWindow    *parent,
                                          const gchar  *filename);

gint      mousepad_dialogs_ask_reload    (GtkWindow    *parent);

G_END_DECLS

#endif /* !__MOUSEPAD_DIALOGS_H__ */
