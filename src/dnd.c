/*
 *  dnd.c
 *  This file is part of Leafpad
 *
 *  Copyright (C) 2004 Tarot Osuji
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <gtk/gtk.h>
#include "config.h"

#define DV(x)

static void dnd_drag_data_recieved_handler(GtkWidget *widget,
	GdkDragContext *context, gint x, gint y,
	GtkSelectionData *selection_data, guint info, guint time)
{
	static gboolean flag_called_once = FALSE;
	gchar **files, **strs;
	gchar *filename;
	gchar *comline;
	
	if (flag_called_once) {
		flag_called_once = FALSE;
DV(g_print("Drop finished.\n"));
		return;
	} else
		flag_called_once = TRUE;
DV({	
	g_print("time                      = %d\n", time);
	g_print("selection_data->selection = %s\n", gdk_atom_name(selection_data->selection));
	g_print("selection_data->target    = %s\n", gdk_atom_name(selection_data->target));
	g_print("selection_data->type      = %s\n", gdk_atom_name(selection_data->type));
	g_print("selection_data->format    = %d\n", selection_data->format);
	g_print("selection_data->data      = %s\n", selection_data->data);
	g_print("selection_data->length    = %d\n", selection_data->length);
});	
	
	if (selection_data->data && g_strstr_len((gchar *) selection_data->data, 5, "file:")) {
		files = g_strsplit((gchar *) selection_data->data, "\n" , 2);
		filename = g_filename_from_uri(files[0], NULL, NULL);
		if (g_strrstr(filename, " ")) {
			strs = g_strsplit(filename, " ", -1);
			g_free(filename);
			filename = g_strjoinv("\\ ", strs);
			g_strfreev(strs);
		}
		comline = g_strdup_printf("%s %s", PACKAGE, g_strstrip(filename));
DV(g_print("[%s]\n", comline));
		g_spawn_command_line_async(comline, NULL);
		g_free(comline);
		g_free(filename);
		g_strfreev(files);
	}
}

static GtkTargetEntry drag_types[] =
{
	{ "text/uri-list", 0, 0 }
};

static gint n_drag_types = sizeof(drag_types) / sizeof(drag_types[0]);

void dnd_init(GtkWidget *widget)
{
#if 0
	GtkWidget *w = gtk_widget_get_ancestor(widget, GTK_TYPE_CONTAINER);
	GtkWidget *w = gtk_widget_get_parent(widget);
	GtkWidget *w = gtk_widget_get_parent(gtk_widget_get_parent(gtk_widget_get_parent(widget)));
	
	g_print(gtk_widget_get_name(w));
#endif
	gtk_drag_dest_set(widget, GTK_DEST_DEFAULT_ALL,
		drag_types, n_drag_types, GDK_ACTION_COPY);
	g_signal_connect(G_OBJECT(widget), "drag_data_received",
		G_CALLBACK(dnd_drag_data_recieved_handler), NULL);
}
