/*
 *  main.c
 *  This file is part of Mousepad
 *
 *  Copyright (C) 2006 Benedikt Meurer <benny@xfce.org>
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

#include "mousepad.h"

static gint load_config_file(StructData *sd)
{

	FILE *fp = NULL;
	gchar *path;
	gchar buf[BUFSIZ];
	gchar **num;
	
	sd->conf.width = 600;
	sd->conf.height = 400;
	sd->conf.fontname = g_strdup("Monospace 12");
	sd->conf.wordwrap = FALSE;
	sd->conf.linenumbers = FALSE;
	sd->conf.autoindent = FALSE;
	sd->conf.charset = NULL;
	
	path = xfce_resource_lookup(XFCE_RESOURCE_CONFIG, g_build_filename(PACKAGE, 
	                                                                   g_strconcat(PACKAGE, "rc", NULL),
	                                                                   NULL));
	if (path == NULL) {
		/*Test for old rc file location. Open and unlink*/
		path = xfce_resource_save_location(XFCE_RESOURCE_CONFIG, PACKAGE, FALSE);
		if (g_file_test(path, (G_FILE_TEST_IS_REGULAR))) {
			fp = fopen(path, "r");
			unlink(path);
		}
		else { /*New location*/
			path = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, 
	                	                    g_build_filename(PACKAGE, 
		                                                     g_strconcat(PACKAGE, "rc", NULL),
		                                                     NULL),
		                                    TRUE);
			fp = fopen(path, "r");
		}
	}
	else {
		fp = fopen(path, "r");
	}

	if (!fp)
		return -1;
	
	/* version num */
	if (!fgets(buf, sizeof(buf), fp)) return -1;
	num = g_strsplit(buf, "." , 3);
	if ((atoi(num[1]) >= 1) && (atoi(num[2]) >= 0)) {
		if (!fgets(buf, sizeof(buf), fp)) return -1;
		if (buf[0] >= '0' && buf[0] <= '9')
			sd->conf.width = atoi(buf);
		else
			return -1;

		if (!fgets(buf, sizeof(buf), fp)) return -1;
		if (buf[0] >= '0' && buf[0] <= '9')
			sd->conf.height = atoi(buf);
		else
			return -1;

		if (!fgets(buf, sizeof(buf), fp)) return -1;
		sd->conf.fontname = g_strdup(buf);

		if (!fgets(buf, sizeof(buf), fp)) return -1;
		if (buf[0] >= '0' && buf[0] <= '1')
			sd->conf.wordwrap = atoi(buf);
		else
			return -1;

		if (!fgets(buf, sizeof(buf), fp)) return -1;
		if (buf[0] >= '0' && buf[0] <= '1')
			sd->conf.linenumbers = atoi(buf);
		else
			return -1;

		if (!fgets(buf, sizeof(buf), fp)) return -1;
		if (buf[0] >= '0' && buf[0] <= '1')
			sd->conf.autoindent = atoi(buf);
		else
			return -1;

		if (!fgets(buf, sizeof(buf), fp)) return -1;
		if (strcmp(buf, "0") != 0)
			sd->conf.charset = g_strdup(buf);
	}
	g_strfreev(num);
	fclose(fp);
	g_free(path);
	
	return 0;
}

static gint save_config_file(StructData *sd)
{
	FILE *fp;
	gchar *path;
	GtkItemFactory *ifactory;
	gint width, height;
	gchar *fontname;
	gboolean wordwrap, linenumbers, autoindent;
	
	gtk_window_get_size(GTK_WINDOW(sd->mainwin->window), &width, &height);
	fontname = get_font_name_from_widget(sd->mainwin->textview);
	ifactory = gtk_item_factory_from_widget(sd->mainwin->menubar);
	wordwrap = gtk_check_menu_item_get_active(
		GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(ifactory, "/Options/Word Wrap")));
	linenumbers = gtk_check_menu_item_get_active(
		GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(ifactory, "/Options/Line Numbers")));
	autoindent = gtk_check_menu_item_get_active(
		GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(ifactory, "/Options/Auto Indent")));
	
	path = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, 
	                                    g_build_filename(PACKAGE, 
	                                                     g_strconcat(PACKAGE, "rc", NULL),
	                                                     NULL),
	                                    TRUE);
	fp = fopen(path, "w");
	
	if (!fp) {
		g_print("warning: can't save config file - %s\n", path);
		return -1;
	}
	
	fprintf(fp, "%s\n", PACKAGE_VERSION);
	fprintf(fp, "%d\n", width);
	fprintf(fp, "%d\n", height);
	fprintf(fp, "%s\n", fontname);
	fprintf(fp, "%d\n", wordwrap);
	fprintf(fp, "%d\n", linenumbers);
	fprintf(fp, "%d\n", autoindent);
	if (sd->fi->manual_charset)
		fprintf(fp, "%s", sd->fi->manual_charset);
	else
		fprintf(fp, "0");
	fclose(fp);
	g_free(path);
	
	g_free(fontname);
	return 0;
}

static void create_new_process(gchar *filename)
{
	StructData *sd = g_malloc(sizeof(StructData));
	FileInfo *fi;
	GtkItemFactory *ifactory;
	
	load_config_file(sd);
	sd->mainwin = create_main_window(sd);
	gtk_widget_show_all(sd->mainwin->window);
	
	set_text_font_by_name(sd->mainwin->textview, sd->conf.fontname);
	ifactory = gtk_item_factory_from_widget(sd->mainwin->menubar);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
		gtk_item_factory_get_widget(ifactory, "<main>/Options/Word Wrap")),
		sd->conf.wordwrap);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
		gtk_item_factory_get_widget(ifactory, "<main>/Options/Line Numbers")),
		sd->conf.linenumbers);
	indent_refresh_tab_width(sd->mainwin->textview);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
		gtk_item_factory_get_widget(ifactory, "<main>/Options/Auto Indent")),
		sd->conf.autoindent);
	
	fi = g_malloc(sizeof(FileInfo));
	fi->filename = filename;
	fi->charset = NULL;
	fi->lineend = 0;
	if (sd->conf.charset)
		fi->manual_charset = sd->conf.charset;
	else
		fi->manual_charset = NULL;
	sd->fi = fi;
 	if (sd->fi->filename) {
		if (file_open_real(sd->mainwin->textview, sd->fi))
			cb_file_new(sd);
		else {
			set_main_window_title(sd);
			undo_init(sd->mainwin->textview, sd->mainwin->textbuffer, sd->mainwin->menubar);
		}
	} else
		cb_file_new(sd);
	
/*	dnd_init(sd->mainwin->window); */
	dnd_init(sd->mainwin->textview);
	keyevent_init(sd->mainwin->textview);

	sd->search.string_find = NULL;
	sd->search.string_replace = NULL;
	
	gtk_main();
	
	save_config_file(sd);
/*	gtk_widget_destroy(sd->mainwin->window);
	g_free(sd->fi);
	g_free(sd->mainwin);
	g_free(sd->conf.fontname);
	g_free(sd); */
}

gint main(gint argc, gchar *argv[])
{
	gchar *filename = NULL;
	gchar **strs;
	
	xfce_textdomain(PACKAGE, LOCALEDIR, "UTF-8");
	
/*	if (getenv("G_BROKEN_FILENAMES") == NULL)
		if (strcmp(get_default_charset(), "UTF-8") != 0)
			setenv("G_BROKEN_FILENAMES", "1", 0);
*/	
	gtk_init(&argc, &argv);
	
	if (argv[1]) {
		if (g_strstr_len(argv[1], 5, "file:")) {
			filename = g_filename_from_uri(argv[1], NULL, NULL);
			if (g_strrstr(filename, " ")) {
				strs = g_strsplit(filename, " ", -1);
				g_free(filename);
				filename = g_strjoinv("\\ ", strs);
				g_strfreev(strs);
			}
		} else {
		/*	if  (!g_path_is_absolute(argv[1]))
				filename = g_build_filename(g_get_current_dir(), argv[1], NULL);
			else */
				filename = g_strdup(argv[1]);
		}
	}
	
	create_new_process(filename);
	
	return 0;
}
