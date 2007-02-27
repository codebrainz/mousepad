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

#ifndef __MOUSEPAD_TAB_LABEL_H__
#define __MOUSEPAD_TAB_LABEL_H__

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_TAB_LABEL            (mousepad_tab_label_get_type ())
#define MOUSEPAD_TAB_LABEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_TAB_LABEL, MousepadTabLabel))
#define MOUSEPAD_TAB_LABEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_TAB_LABEL, MousepadTabLabelClass))
#define MOUSEPAD_IS_TAB_LABEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_TAB_LABEL))
#define MOUSEPAD_IS_TAB_LABEL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPADL_TYPE_TAB_LABEL))
#define MOUSEPAD_TAB_LABEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_TAB_LABEL, MousepadTabLabelClass))

typedef struct _MousepadTabLabelClass MousepadTabLabelClass;
typedef struct _MousepadTabLabel      MousepadTabLabel;

GType      mousepad_tab_label_get_type (void) G_GNUC_CONST;

GtkWidget *mousepad_tab_label_new      (void);

G_END_DECLS

#endif /* !__MOUSEPAD_TAB_LABEL_H__ */
