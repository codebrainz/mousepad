/*
 *  selector.h
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

#include "mousepad.h"

#define DEFAULT_ITEM_NUM 2

static gint mode;
static const gchar *other_codeset_title;
/*
static gchar *lineend_str[] = {
	"UNIX (LF)",
	"DOS (CR+LF)",
	"Mac (CR)"
};
*/
static gchar *lineend_str[] = {
	"LF",
	"CR+LF",
	"CR"
};

static void cb_select_lineend(GtkOptionMenu *option_menu, FileInfo *selected_fi)
{
	switch (gtk_option_menu_get_history(option_menu)) {
	case 1:
		selected_fi->lineend = CR+LF;
		break;
	case 2:
		selected_fi->lineend = CR;
		break;
	default:
		selected_fi->lineend = LF;
	}
}

static GtkWidget *create_lineend_menu(FileInfo *selected_fi)
{
	GtkWidget *option_menu;
	GtkWidget *menu;
	GtkWidget *menu_item;
	gint i;
	
	option_menu = gtk_option_menu_new();
	menu = gtk_menu_new();
	for (i = 0; i <= 2; i++) {
		menu_item = gtk_menu_item_new_with_label(lineend_str[i]);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
		gtk_widget_show(menu_item); /* <- required for width adjustment */
	}
	gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
	
	g_signal_connect(G_OBJECT(option_menu), "changed",
		G_CALLBACK(cb_select_lineend), selected_fi);
	
	i = 0;
	switch (selected_fi->lineend) {
	case CR+LF:
		i = 1;
		break;
	case CR:
		i = 2;
	}
	gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), i);
	
	return option_menu;
}

typedef struct {
	const gchar *charset[ENCODING_MAX_ITEM_NUM + DEFAULT_ITEM_NUM];
	const gchar *str[ENCODING_MAX_ITEM_NUM + DEFAULT_ITEM_NUM];
	gint num;
} CharsetTable;

static CharsetTable *get_charset_table(void)
{
	static CharsetTable *ctable = NULL;
	EncArray *encarray;
	gint i;
	
	if (!ctable) {
		ctable = g_malloc(sizeof(CharsetTable));
		ctable->num = 0;
		ctable->charset[ctable->num] = get_default_charset();
		ctable->str[ctable->num] = g_strdup_printf(_("Current Locale (%s)"), get_default_charset());
		ctable->num++;
		ctable->charset[ctable->num] = "UTF-8";
	/*	ctable->str[ctable->num] = g_strdup("Unicode (UTF-8)"); */
		ctable->str[ctable->num] = ctable->charset[ctable->num];
		ctable->num++;
		encarray = get_encoding_items(get_encoding_code());
		for (i = 0; i < ENCODING_MAX_ITEM_NUM; i++)
			if (encarray->item[i]) {
				ctable->charset[ctable->num] = encarray->item[i];
			/*	switch (i) {
				case 0:
					ctable->str[ctable->num] =
						g_strdup_printf("IANA Preferred (%s)", encarray->item[i]);
					break;
				case 1:
					ctable->str[ctable->num] =
						g_strdup_printf("OpenI18N Codeset (%s)", encarray->item[i]);
					break;
				case 2:
					ctable->str[ctable->num] =
						g_strdup_printf("Microsoft Codepage (%s)", encarray->item[i]);
					break;
				} */
				ctable->str[ctable->num] = encarray->item[i];
				ctable->num++;
			}
	}
	
	return ctable;
}

static void toggle_sensitivity(GtkWidget *entry)
{
	gtk_dialog_set_response_sensitive(GTK_DIALOG(gtk_widget_get_toplevel(entry)),
		GTK_RESPONSE_OK, strlen(gtk_entry_get_text(GTK_ENTRY(entry))) ? TRUE : FALSE);
}

static GtkWidget *menu_item_manual_charset;
static GtkWidget *init_menu_item_manual_charset(gchar *manual_charset)
{
	static GtkLabel *label;
	gchar *str;
	
	other_codeset_title = _("Other Codeset");
	
	str = manual_charset
		? g_strdup_printf("%s (%s)", other_codeset_title, manual_charset)
		: g_strdup_printf("%s...", other_codeset_title);
	
	if (!menu_item_manual_charset) {
		menu_item_manual_charset = gtk_menu_item_new_with_label(str);
		label = GTK_LABEL(GTK_BIN(menu_item_manual_charset)->child);
	} else
/*		gtk_label_set_text(GTK_LABEL(GTK_BIN(menu_item_manual_charset)->child), str); */
		gtk_label_set_text(label, str);
	g_free(str);
	
	return menu_item_manual_charset;
}

static gboolean get_manual_charset(GtkOptionMenu *option_menu, FileInfo *selected_fi)
{
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *entry;
	GError *err = NULL;
	gchar *str;
	
	dialog = gtk_dialog_new_with_buttons(other_codeset_title,
			GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(option_menu))),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
	
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), vbox, FALSE, FALSE, 0);
	
	label = gtk_label_new_with_mnemonic(_("Code_set:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
	
	entry = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 5);
	 
	gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_OK, FALSE);
	g_signal_connect_after(G_OBJECT(entry), "changed",
		G_CALLBACK(toggle_sensitivity), NULL);
	if (selected_fi->manual_charset)
		gtk_entry_set_text(GTK_ENTRY(entry), selected_fi->manual_charset);
	
	gtk_widget_show_all(vbox);
	
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
		g_convert("TEST", -1, "UTF-8", gtk_entry_get_text(GTK_ENTRY(entry)), NULL, NULL, &err);
		if (err) {
			g_error_free(err);
			gtk_widget_hide(dialog);
			str = g_strdup_printf(_("'%s' is not supported"), gtk_entry_get_text(GTK_ENTRY(entry)));
/*			run_dialog_message(filesel, GTK_MESSAGE_ERROR, str); */
			run_dialog_message(gtk_widget_get_toplevel(GTK_WIDGET(option_menu)),
				GTK_MESSAGE_ERROR, str);
			g_free(str);
		} else {
			if (selected_fi->manual_charset)
				g_free(selected_fi->manual_charset);
			selected_fi->manual_charset = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
			if (selected_fi->charset)
				g_free(selected_fi->charset);
			selected_fi->charset = g_strdup(selected_fi->manual_charset);
			gtk_widget_destroy(dialog);
			
			init_menu_item_manual_charset(selected_fi->manual_charset);
			
			return TRUE;
		}
	}
	gtk_widget_destroy(dialog);
	
	return FALSE;
}

gboolean charset_menu_init_flag;

static void cb_select_charset(GtkOptionMenu *option_menu, FileInfo *selected_fi)
{
	CharsetTable *ctable;
	static guint index_history = 0, prev_history;
	
	prev_history = index_history;
	index_history = gtk_option_menu_get_history(option_menu);
	if (!charset_menu_init_flag) {
		ctable = get_charset_table();
		if (index_history < ctable->num + mode) {
			if (selected_fi->charset)
				g_free(selected_fi->charset);
			if (index_history == 0 && mode == OPEN)
				selected_fi->charset = NULL;
			else {
				selected_fi->charset =
					g_strdup(ctable->charset[index_history - mode]);
			}
		} else
			if (!get_manual_charset(option_menu, selected_fi)) {
				index_history = prev_history;
				gtk_option_menu_set_history(option_menu, index_history);
			}
	}
}

static GtkWidget *create_charset_menu(FileInfo *selected_fi)
{
	GtkWidget *option_menu;
	GtkWidget *menu;
	GtkWidget *menu_item;
	CharsetTable *ctable;
	gint i;
	
	option_menu = gtk_option_menu_new();
	menu = gtk_menu_new();
	
	if (mode == OPEN) {
		menu_item = gtk_menu_item_new_with_label(_("Auto Detect"));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
		gtk_widget_show(menu_item); /* <- required for width adjustment */
	}
	ctable = get_charset_table();
	for (i = 0; i < ctable->num; i++) {
		menu_item = gtk_menu_item_new_with_label(ctable->str[i]);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
		gtk_widget_show(menu_item); /* <- required for width adjustment */
	}
	menu_item_manual_charset = NULL;
	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
		init_menu_item_manual_charset(selected_fi->manual_charset));
	gtk_widget_show(menu_item_manual_charset); /* <- required for width adjustment */
	
	charset_menu_init_flag = TRUE;
	g_signal_connect(G_OBJECT(option_menu), "changed",
		G_CALLBACK(cb_select_charset), selected_fi);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
	i = 0;
	if (selected_fi->charset) {
		do {
			if (g_utf8_collate(g_utf8_casefold(selected_fi->charset, sizeof(selected_fi->charset)), 
			                   g_utf8_casefold(ctable->charset[i], sizeof(ctable->charset[i]))
			   ) == 0)
				break;
			i++;
		} while (i < ctable->num);
		if (mode == OPEN) {
			g_free(selected_fi->charset);
			selected_fi->charset = NULL;
		} else if (i == ctable->num && selected_fi->manual_charset == NULL) {
			selected_fi->manual_charset = g_strdup(selected_fi->charset);
			init_menu_item_manual_charset(selected_fi->manual_charset);
		}
		i += mode;
	}
	if (mode == SAVE || selected_fi->manual_charset)
		gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), i);
	charset_menu_init_flag = FALSE;
	
	return option_menu;
}

static GtkWidget *create_file_chooser(FileInfo *selected_fi, GtkWidget *window)
{
	GtkWidget *filesel;
	GtkWidget *align;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *option_menu_charset;
	GtkWidget *option_menu_lineend;
	const gchar *title;
	
	title = mode ? _("Open") : _("Save As");
	
	if(mode == OPEN)
		filesel = gtk_file_chooser_dialog_new(title, GTK_WINDOW(window),
				GTK_FILE_CHOOSER_ACTION_OPEN,
				GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
				GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	else
		filesel = gtk_file_chooser_dialog_new(title, GTK_WINDOW(window),
				GTK_FILE_CHOOSER_ACTION_SAVE,
				GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
				GTK_STOCK_SAVE,GTK_RESPONSE_ACCEPT, NULL);
	
	gtk_dialog_set_default_response(GTK_DIALOG (filesel),GTK_RESPONSE_ACCEPT);

	align = gtk_alignment_new(1, 0, 0, 0);
	gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(filesel),align);
	table = gtk_table_new(2, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_container_add(GTK_CONTAINER(align), table);
	
	option_menu_charset = create_charset_menu(selected_fi);
	label = gtk_label_new_with_mnemonic(_("Ch_aracter Coding: ")); /* dummy for pot */
/*	gtk_label_set_mnemonic_widget(GTK_LABEL(label), option_menu_charset); */
	gtk_table_attach_defaults(GTK_TABLE(table), option_menu_charset, 0, 1, 0, 1);
	if (mode == SAVE) {
		option_menu_lineend = create_lineend_menu(selected_fi);
		gtk_table_attach_defaults(GTK_TABLE(table), option_menu_lineend, 1, 2, 0, 1);
	}
	gtk_widget_show_all(GTK_WIDGET(filesel));
	
	if (selected_fi->filename)
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(filesel), selected_fi->filename);
	
	return filesel;
}

FileInfo *get_fileinfo_from_chooser(GtkWidget *window, FileInfo *fi, gint requested_mode)
{
	FileInfo *selected_fi;
	GtkWidget *filesel;
	gchar *basename, *str;
	gint res, len;
	
	/* init values */
	mode = requested_mode;
	selected_fi = g_malloc(sizeof(FileInfo));
	selected_fi->filename =
		fi->filename ? g_strdup(fi->filename) : NULL;
	selected_fi->charset =
		fi->charset ? g_strdup(fi->charset) : NULL;
	selected_fi->lineend = fi->lineend;
	selected_fi->manual_charset =
		fi->manual_charset ? g_strdup(fi->manual_charset) : NULL;
	
	filesel = create_file_chooser(selected_fi,window);
	
	gtk_window_set_transient_for(GTK_WINDOW(filesel), GTK_WINDOW(window));
	
	do {
		res = gtk_dialog_run(GTK_DIALOG(filesel));
		if (res == GTK_RESPONSE_ACCEPT) {
			if (selected_fi->filename)
				g_free(selected_fi->filename);
			selected_fi->filename = g_strdup(
				gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(filesel)));
			if (g_file_test(selected_fi->filename, G_FILE_TEST_IS_DIR)) {
				len = strlen(selected_fi->filename);
				if (len < 1 || selected_fi->filename[len - 1] != G_DIR_SEPARATOR)
					str = g_strconcat(selected_fi->filename, G_DIR_SEPARATOR_S, NULL);
				else
					str = g_strdup(selected_fi->filename);
				gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(filesel), str);
				g_free(str);
				continue;
			}
			if (mode == SAVE && g_file_test(selected_fi->filename, G_FILE_TEST_EXISTS)) {
				basename = g_path_get_basename(selected_fi->filename);
				str = g_strdup_printf(_("'%s' already exists. Overwrite?"), basename);
				res = run_dialog_message_question(filesel, str);
				g_free(str);
				g_free(basename);
				switch (res) {
				case GTK_RESPONSE_NO:
					continue;
				case GTK_RESPONSE_YES:
					res = GTK_RESPONSE_ACCEPT;
				}
			}
		}
		gtk_widget_hide(filesel);
	} while (GTK_WIDGET_VISIBLE(filesel));
	
	if (res != GTK_RESPONSE_ACCEPT) {
		if (selected_fi->charset)
			g_free(selected_fi->charset);
		if (selected_fi->manual_charset)
			g_free(selected_fi->manual_charset);
		g_free(selected_fi);
		selected_fi = NULL;
	}
	
	gtk_widget_destroy(filesel);
	
	return selected_fi;
}
