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

#ifndef __MOUSEPAD_FILE_H__
#define __MOUSEPAD_FILE_H__

G_BEGIN_DECLS

gboolean  mousepad_file_get_externally_modified  (const gchar    *filename,
                                                  gint            mtime);

gboolean  mousepad_file_save_data                (const gchar    *filename,
                                                  const gchar    *data,
                                                  gsize           bytes,
                                                  gint           *new_mtime,
                                                  GError        **error);

gboolean  mousepad_file_read_to_buffer           (const gchar    *filename,
                                                  GtkTextBuffer  *buffer,
                                                  gint           *new_mtime,
                                                  gboolean       *readonly,
                                                  GError        **error);

gboolean  mousepad_file_is_writable              (const gchar    *filename,
                                                  GError        **error);

G_END_DECLS

#endif /* !__MOUSEPAD_FILE_H__ */
