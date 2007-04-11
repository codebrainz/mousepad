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

#ifndef __MOUSEPAD_UNDO_H__
#define __MOUSEPAD_UNDO_H__

G_BEGIN_DECLS

typedef struct _MousepadUndoClass MousepadUndoClass;
typedef struct _MousepadUndo      MousepadUndo;

#define MOUSEPAD_TYPE_UNDO            (mousepad_undo_get_type ())
#define MOUSEPAD_UNDO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_UNDO, MousepadUndo))
#define MOUSEPAD_UNDO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_UNDO, MousepadUndoClass))
#define MOUSEPAD_IS_UNDO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_UNDO))
#define MOUSEPAD_IS_UNDO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MOUSEPAD_TYPE_UNDO))
#define MOUSEPAD_UNDO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_UNDO, MousepadUndoClass))

GType          mousepad_undo_get_type        (void) G_GNUC_CONST;

MousepadUndo  *mousepad_undo_new             (GtkTextBuffer *buffer);

void           mousepad_undo_lock            (MousepadUndo  *undo);

void           mousepad_undo_unlock          (MousepadUndo  *undo);

void           mousepad_undo_do_undo         (MousepadUndo  *undo);

void           mousepad_undo_do_redo         (MousepadUndo  *undo);

gboolean       mousepad_undo_can_undo        (MousepadUndo  *undo);

gboolean       mousepad_undo_can_redo        (MousepadUndo  *undo);

void           mousepad_undo_populate_popup  (GtkTextView   *textview,
                                              GtkMenu       *menu,
                                              MousepadUndo  *undo);

G_END_DECLS

#endif /* !__MOUSEPAD_UNDO_H__ */
