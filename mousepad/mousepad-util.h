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

#ifndef __MOUSEPAD_UTIL_H__
#define __MOUSEPAD_UTIL_H__

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_SEARCH_FLAGS (mousepad_util_search_flags_get_type ())

typedef enum
{
  /* search area */
  MOUSEPAD_SEARCH_FLAGS_ENTIRE_AREA          = 1 << 0,  /* search the whole area */
  MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION       = 1 << 1,  /* search inside selection */
  MOUSEPAD_SEARCH_FLAGS_AREA_ALL_DOCUMENTS   = 1 << 2,  /* search all documents */

  /* iter start point */
  MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START       = 1 << 3,  /* start at from the beginning of the selection */
  MOUSEPAD_SEARCH_FLAGS_ITER_SEL_END         = 1 << 4,  /* start at from the end of the selection */

  /* direction */
  MOUSEPAD_SEARCH_FLAGS_DIR_FORWARD          = 1 << 5,  /* search forward to end of area */
  MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD         = 1 << 6,  /* search backwards to start of area */

  /* search settings */
  MOUSEPAD_SEARCH_FLAGS_MATCH_CASE           = 1 << 7,  /* match case */
  MOUSEPAD_SEARCH_FLAGS_ENABLE_REGEX         = 1 << 8,  /* enable regex search */
  MOUSEPAD_SEARCH_FLAGS_WHOLE_WORD           = 1 << 9,  /* only match whole words */
  MOUSEPAD_SEARCH_FLAGS_WRAP_AROUND          = 1 << 10, /* wrap around */

  /* actions */
  MOUSEPAD_SEARCH_FLAGS_ACTION_HIGHLIGHT_ON  = 1 << 11, /* enable occurrence highlighting */
  MOUSEPAD_SEARCH_FLAGS_ACTION_HIGHLIGHT_OFF = 1 << 12, /* disable occurrence highlighting */
  MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT        = 1 << 13, /* select the match */
  MOUSEPAD_SEARCH_FLAGS_ACTION_REPLACE       = 1 << 14, /* replace the match */
}
MousepadSearchFlags;

gboolean   mousepad_util_iter_starts_word                 (const GtkTextIter   *iter);

gboolean   mousepad_util_iter_ends_word                   (const GtkTextIter   *iter);

gboolean   mousepad_util_iter_inside_word                 (const GtkTextIter   *iter);

gboolean   mousepad_util_iter_forward_word_end            (GtkTextIter         *iter);

gboolean   mousepad_util_iter_backward_word_start         (GtkTextIter         *iter);

gboolean   mousepad_util_iter_forward_text_start          (GtkTextIter         *iter);

gboolean   mousepad_util_iter_backward_text_start         (GtkTextIter         *iter);

gchar     *mousepad_util_config_name                      (const gchar         *name);

gchar     *mousepad_util_key_name                         (const gchar         *name);

gchar     *mousepad_util_utf8_strcapital                  (const gchar         *str);

gchar     *mousepad_util_utf8_stropposite                 (const gchar         *str);

gchar     *mousepad_util_escape_underscores               (const gchar         *str);

GtkWidget *mousepad_util_image_button                     (const gchar         *icon_name,
                                                           const gchar         *label);

void       mousepad_util_entry_error                      (GtkWidget           *widget,
                                                           gboolean             error);

void       mousepad_util_dialog_header                    (GtkDialog           *dialog,
                                                           const gchar         *title,
                                                           const gchar         *subtitle,
                                                           const gchar         *icon);

gint       mousepad_util_get_real_line_offset             (const GtkTextIter   *iter,
                                                           gint                 tab_size);

gboolean   mousepad_util_forward_iter_to_text             (GtkTextIter         *iter,
                                                           const GtkTextIter   *limit);

gchar     *mousepad_util_get_save_location                (const gchar         *relpath,
                                                           gboolean             create_parents);

void       mousepad_util_save_key_file                    (GKeyFile            *keyfile,
                                                           const gchar         *filename);

GType      mousepad_util_search_flags_get_type            (void) G_GNUC_CONST;

gint       mousepad_util_search                           (GtkSourceSearchContext *search_context,
                                                           const gchar            *string,
                                                           const gchar            *replace,
                                                           MousepadSearchFlags     flags);

GtkAction *mousepad_util_find_related_action              (GtkWidget           *widget);

GIcon     *mousepad_util_icon_for_mime_type               (const gchar         *mime_type);

gboolean   mousepad_util_container_has_children           (GtkContainer        *container);

void       mousepad_util_container_clear                  (GtkContainer        *container);

void       mousepad_util_container_move_children          (GtkContainer        *source,
                                                           GtkContainer        *destination);

G_END_DECLS

#endif /* !__MOUSEPAD_UTIL_H__ */
