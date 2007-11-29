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

#ifndef __MOUSEPAD_UTIL_H__
#define __MOUSEPAD_UTIL_H__

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_SEARCH_FLAGS (mousepad_util_search_flags_get_type ())

typedef enum _MousepadSearchFlags MousepadSearchFlags;

enum _MousepadSearchFlags
{
  /* search area */
  MOUSEPAD_SEARCH_FLAGS_AREA_DOCUMENT     = 1 << 0,  /* search the entire document */
  MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION    = 1 << 1,  /* search inside selection */

  /* iter start point */
  MOUSEPAD_SEARCH_FLAGS_ITER_AREA_START   = 1 << 2,  /* search from the beginning of the area */
  MOUSEPAD_SEARCH_FLAGS_ITER_AREA_END     = 1 << 3,  /* search from the end of the area */
  MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START    = 1 << 4,  /* start at from the beginning of the selection */
  MOUSEPAD_SEARCH_FLAGS_ITER_SEL_END      = 1 << 5,  /* start at from the end of the selection */

  /* direction */
  MOUSEPAD_SEARCH_FLAGS_DIR_FORWARD       = 1 << 6,  /* search forward to end of area */
  MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD      = 1 << 7,  /* search backwards to start of area */

  /* search settings */
  MOUSEPAD_SEARCH_FLAGS_MATCH_CASE        = 1 << 8,  /* match case */
  MOUSEPAD_SEARCH_FLAGS_WHOLE_WORD        = 1 << 9,  /* only match whole words */
  MOUSEPAD_SEARCH_FLAGS_WRAP_AROUND       = 1 << 10, /* wrap around */
  MOUSEPAD_SEARCH_FLAGS_ENTIRE_AREA       = 1 << 11, /* keep searching until the end of the area is reached */
  MOUSEPAD_SEARCH_FLAGS_ALL_DOCUMENTS     = 1 << 12, /* search all documents */


  /* actions */
  MOUSEPAD_SEARCH_FLAGS_ACTION_NONE       = 1 << 13, /* no visible actions */
  MOUSEPAD_SEARCH_FLAGS_ACTION_HIGHTLIGHT = 1 << 14, /* highlight all the occurences */
  MOUSEPAD_SEARCH_FLAGS_ACTION_CLEANUP    = 1 << 15, /* cleanup the highlighted occurences */
  MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT     = 1 << 16, /* select the match */
  MOUSEPAD_SEARCH_FLAGS_ACTION_REPLACE    = 1 << 17, /* replace the match */
};

gboolean   mousepad_util_iter_starts_word         (const GtkTextIter   *iter);

gboolean   mousepad_util_iter_ends_word           (const GtkTextIter   *iter);

gboolean   mousepad_util_iter_inside_word         (const GtkTextIter   *iter);

gboolean   mousepad_util_iter_forward_word_end    (GtkTextIter         *iter);

gboolean   mousepad_util_iter_backward_word_start (GtkTextIter         *iter);

gboolean   mousepad_util_iter_forward_text_start  (GtkTextIter         *iter);

gboolean   mousepad_util_iter_backward_text_start (GtkTextIter         *iter);

gchar     *mousepad_util_config_name              (const gchar         *name);

gchar     *mousepad_util_key_name                 (const gchar         *name);

GtkWidget *mousepad_util_image_button             (const gchar         *stock_id,
                                                   const gchar         *label);

void       mousepad_util_entry_error              (GtkWidget           *widget,
                                                   gboolean             error);

void       mousepad_util_dialog_header            (GtkDialog           *dialog,
                                                   const gchar         *title,
                                                   const gchar         *subtitle,
                                                   const gchar         *icon);

void       mousepad_util_set_tooltip              (GtkWidget           *widget,
                                                   const gchar         *string);

gint       mousepad_util_get_real_line_offset     (const GtkTextIter   *iter,
                                                   gint                 tab_size);

gboolean   mousepad_util_forward_iter_to_text     (GtkTextIter         *iter,
                                                   const GtkTextIter   *limit);

GType      mousepad_util_search_flags_get_type    (void) G_GNUC_CONST;

gint       mousepad_util_highlight                (GtkTextBuffer       *buffer,
                                                   GtkTextTag          *tag,
                                                   const gchar         *string,
                                                   MousepadSearchFlags  flags);

gint       mousepad_util_search                   (GtkTextBuffer       *buffer,
                                                   const gchar         *string,
                                                   const gchar         *replace,
                                                   MousepadSearchFlags  flags);

G_END_DECLS

#endif /* !__MOUSEPAD_UTIL_H__ */
