/*
 *  undo.c
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
#include <gdk/gdkkeysyms.h>
#include "keyevent.h"
#include "undo.h"

#define DV(x)

/* This struct is used to carry info about a undo/redo steps.
 * ->next holds a pointer to the next element in the stack. 
          See the GTrashStack docs for more (vague) info.
 * ->command holds the type of action, backspace, delete, or inset.
 * ->start is the _character_ offset of the beginning of the step
 * ->end is offset for the end of the step, mostly for deletions
 * ->seq is a flag for cases (like overwriting text with a paste)
 *       where one user level undo involves multiple steps. For 
 *       example, overwriting text via a paste involves an deletion
 *       and an insertion. The insertion will be at the top of the 
 *       stack with the seq flag set, the deletion below it, with
 *       the flag unset. If a step with the flag set is undone or
 *       redone, we perform the next step as well, until we undo
 *       or redo a step without the flag set
 * ->sty, the text of the change
 */
typedef struct {
	GTrashStack *next;
	gint command;
	gint start;
	gint end;
	gboolean seq; 
	gchar *str;
} UndoInfo;

/* For the command field in UndoInfo*/
enum {
	BS = 0,
	DEL,
	INS
};

static GtkWidget *undo_menu_item = NULL;
static GtkWidget *redo_menu_item = NULL;

static GTrashStack *undo_stack = NULL;
static GTrashStack *redo_stack = NULL;
/*undo_gstr acts like a temp for UndoInfo->str, growing or shrinking
  with insertions until a word boundry is set, and it's copied to
  ui->str */
static GString *undo_gstr;
/*we keep one of these around to "build up" before copying it
into the undo list when a word boundry is met*/
static UndoInfo *ui_tmp;
/*doesn't seem to do anything but store 0 so that we can know when we're
out of data in the undo_list EXPME*/
static gint step_modif;
static GtkWidget *view;
/*I dunno why we track whether or not overwrite is on. I mean, it
seems like a good idea, I guess EXPME*/
static gboolean overwrite_mode = FALSE;
/*instead of passing stuff in through arguments, we use locals. Fun*/
static guint keyval;
/*on occasion we need the previous keyval to determine whether or not we've
hit a word boundry*/
static guint prev_keyval;/*, keyevent_setval(0); */

static void cb_toggle_overwrite(void)
{
	overwrite_mode =! overwrite_mode;
DV(g_print("toggle-overwrite: %d\n", overwrite_mode));
}

static void undo_clear_undo_info(void)
{
	UndoInfo *ui;
	
	/*Peeking is O(1) while checking the height is O(N)*/
	while (g_trash_stack_peek(&undo_stack)) {
		ui = (UndoInfo *)g_trash_stack_pop(&undo_stack);
		g_free(ui->str);
		g_free(ui);
	}
}

static void undo_clear_redo_info(void)
{
	UndoInfo *ri;
	
	while (g_trash_stack_peek(&redo_stack)) {
		ri = (UndoInfo *)g_trash_stack_pop(&redo_stack);
		g_free(ri->str);
		g_free(ri);
	}
	
}

/*takes command, offsets and a string, and builds a UndoInfo object, which
we stuff into the undo_list. Note that you can't set ->seq this way.
Also (FIXME) we have no use for the buffer passed to us*/
static void undo_append_undo_info(GtkTextBuffer *buffer, gint command, gint start, gint end, gchar *str)
{
	UndoInfo *ui = g_malloc(sizeof(UndoInfo));
	
	ui->command = command;
	ui->start = start;
	ui->end = end;
	ui->seq = FALSE;
	ui->str = str;
	g_trash_stack_push(&undo_stack, ui);
DV(g_print("undo_append_undo_info: command: %d string: %s range: (%d-%d)\n", command, str, start, end));
}

/* Here it is. The monster that drives everything around here and does
 * most of the work. The various callbacks hooked into the buffer pass
 * the buck here. Since this function will be called for every little
 * change to the buffer it attempts to determine if the current change
 * is part of an ongoing action the user will want to undo/redo in one 
 * go (confusingly, also called a sequence) or the start of a new 
 * sequence. If it's part of an ongoing sequence, it builds up data in 
 * the undo_gstr or ui_tmp variables. If it's the start of a new 
 * sequence, it copies the ui_tmp and undo_gstr data into the 
 * undo_info list (via undo_append_undo_info), and then clears ui_tmp
 * and undo_gstr for the next run. 
 *
 * Note that a sequence is not just a series of insertions and a word
 * boundry, but also a linefeed, a deletion or a backspace
 */ 
static void undo_create_undo_info(GtkTextBuffer *buffer, gint command, gint start, gint end)
{
	GtkTextIter start_iter, end_iter;
	gboolean seq_flag = FALSE;
	gchar *str;
	
	gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, start);
	gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, end);
	
	/*The callbacks have the text - why do we get it from the buffer? 
	  For redo, maybe? EXPME*/
	str = gtk_text_buffer_get_text(buffer, &start_iter, &end_iter, FALSE);
	
	if (undo_gstr->len) {
		if (end - start == 1 && command == ui_tmp->command) {
			switch (keyval) {
			case GDK_BackSpace:
				if (end == ui_tmp->start)
					seq_flag = TRUE;
				break;
			case GDK_Delete:
				if (start == ui_tmp->start)
					seq_flag = TRUE;
				break;
		/*	case GDK_Return:
				if (start == ui_tmp->end) {
					if (prev_keyval == GDK_Return)
						if (!g_strstr_len(undo_gstr->str, 1, "\n"))
							break;
					seq_flag = TRUE;
				}
				break; */
			case GDK_Tab:
			case GDK_space:
				if (start == ui_tmp->end)
					seq_flag = TRUE;
				break;
			default:
				if (start == ui_tmp->end)
					if (keyval && keyval < 0xF000)
						switch (prev_keyval) {
						case GDK_Return:
						case GDK_Tab:
						case GDK_space:
							break;
						default:
							seq_flag = TRUE;
						}
			}
		}
		if (seq_flag) {
			switch (command) {
			case BS:
				undo_gstr = g_string_prepend(undo_gstr, str);
				ui_tmp->start--;
				break;
			default:
				undo_gstr = g_string_append(undo_gstr, str);
				ui_tmp->end++;
			}
			undo_clear_redo_info();
			prev_keyval = keyval;
			keyevent_setval(0);
			gtk_widget_set_sensitive(undo_menu_item, TRUE);
			gtk_widget_set_sensitive(redo_menu_item, FALSE);
			return;
		}
		undo_append_undo_info(buffer, ui_tmp->command, ui_tmp->start, ui_tmp->end, g_strdup(undo_gstr->str));
		undo_gstr = g_string_erase(undo_gstr, 0, -1);
	}
	
	if (end - start == 1 &&
		((keyval && keyval < 0xF000) ||
		  keyval == GDK_BackSpace || keyval == GDK_Delete || keyval == GDK_Tab)) {
		ui_tmp->command = command;
		ui_tmp->start = start;
		ui_tmp->end = end;
		undo_gstr = g_string_erase(undo_gstr, 0, -1);
		g_string_append(undo_gstr, str);
	} else 
		undo_append_undo_info(buffer, command, start, end, g_strdup(str));
DV(g_print("undo_create_undo_info: ui_tmp->command: %d", ui_tmp->command));
	undo_clear_redo_info();
	prev_keyval = keyval;
	keyevent_setval(0);
	gtk_widget_set_sensitive(undo_menu_item, TRUE);
	gtk_widget_set_sensitive(redo_menu_item, FALSE);
}

static void cb_delete_range(GtkTextBuffer *buffer, GtkTextIter *start_iter, GtkTextIter *end_iter)
{
	gint start, end;
	gint command;
	
	keyval = keyevent_getval();
	start = gtk_text_iter_get_offset(start_iter);
	end = gtk_text_iter_get_offset(end_iter);
	
DV(g_print("delete-range: keyval = 0x%x\n", keyval));

	/*Overwriting text from the menubar or context menu. I have
	no idea why ovewriting from the context menu generates a keyevent
	but inserting does not (see comments in the inset-text callback*/	
	if (!keyval && prev_keyval) 
		undo_set_sequency(TRUE);
	
	if (keyval == 0x10000 && prev_keyval > 0x10000) /* for Ctrl+V overwrite */
		undo_set_sequency(TRUE);
	if (keyval == GDK_BackSpace)
		command = BS;
	else
		command = DEL;
	undo_create_undo_info(buffer, command, start, end);
}

static void cb_insert_text(GtkTextBuffer *buffer, GtkTextIter *iter, gchar *str)
{
	gint start, end;
	
	keyval = keyevent_getval();
	end = gtk_text_iter_get_offset(iter);
	start = end - g_utf8_strlen(str, -1);
	
	undo_create_undo_info(buffer, INS, start, end);
}

static void set_main_window_title_with_asterisk(gboolean flag)
{
	const gchar *old_title =
		gtk_window_get_title(GTK_WINDOW(gtk_widget_get_toplevel(view)));
	gchar *new_title;
	
	if (flag)
		new_title = g_strconcat("*", old_title, NULL);
	else {
		GString *gstr = g_string_new(old_title);
		gstr = g_string_erase(gstr, 0, 1);
		new_title = g_string_free(gstr, FALSE);
	}
	gtk_window_set_title(GTK_WINDOW(gtk_widget_get_toplevel(view)), new_title);
	g_free(new_title);
}

static void cb_modified_changed(void)
{
	set_main_window_title_with_asterisk(TRUE);
}

/*I'm this is only called by init, and when we save. We call it from 
the save call back, so that we can properly handle the asterisk in the
titlebar and warn that the document is altered after a save, but 
retain the undo history. The leads to (FIXME) bug 2730*/
void undo_reset_step_modif(void)
{
	step_modif = g_trash_stack_height(&undo_stack);
DV(g_print("undo_reset_step_modif: Reseted step_modif by %d\n", step_modif));
}

static void undo_check_step_modif(GtkTextBuffer *buffer)
{
	if (g_trash_stack_height(&undo_stack) == step_modif) {
		g_signal_handlers_block_by_func(G_OBJECT(buffer), G_CALLBACK(cb_modified_changed), NULL);
		gtk_text_buffer_set_modified(buffer, FALSE);
		g_signal_handlers_unblock_by_func(G_OBJECT(buffer), G_CALLBACK(cb_modified_changed), NULL);
		set_main_window_title_with_asterisk(FALSE);
	}
}

/*used by undo_init to attach the various callbacks we need
  to generate the undo history. Of course, it is a bit of 
  a misnomer, as it only connects tp signals from the buffer, 
  undo_init connects to signals emitted by the textview
 */
static gint undo_connect_signal(GtkTextBuffer *buffer)
{
	g_signal_connect(G_OBJECT(buffer), "delete-range",
		G_CALLBACK(cb_delete_range), buffer);
	g_signal_connect_after(G_OBJECT(buffer), "insert-text",
		G_CALLBACK(cb_insert_text), buffer);
	return 
	g_signal_connect(G_OBJECT(buffer), "modified-changed",
		G_CALLBACK(cb_modified_changed), NULL);
}

/*Sets up this party. It also tracks if it's been called multiple times, 
  and cleans up after itself (sorta) if it has. Which is odd, as only 
  we only call it during startup.*/
void undo_init(GtkWidget *textview, GtkTextBuffer *buffer, GtkWidget *menubar)
{
	GtkItemFactory *ifactory;
	static guint init_flag = 0; /* TODO: divide to undo_clear() */
	
	if (undo_stack != NULL)
		undo_clear_undo_info();
	if (redo_stack != NULL)
		undo_clear_redo_info();
	undo_reset_step_modif();
DV(g_print("undo_init: list reseted\n"));
	
	/*This is retarded*/
	if (!undo_menu_item) {
		ifactory = gtk_item_factory_from_widget(menubar);
		undo_menu_item = gtk_item_factory_get_widget(ifactory, "/Edit/Undo");
		redo_menu_item = gtk_item_factory_get_widget(ifactory, "/Edit/Redo");
	}
	gtk_widget_set_sensitive(undo_menu_item, FALSE);
	gtk_widget_set_sensitive(redo_menu_item, FALSE);
	
	if (!init_flag) {
		g_signal_connect(G_OBJECT(textview), "toggle-overwrite",
			G_CALLBACK(cb_toggle_overwrite), NULL); /*I dunno why we care*/
		view = textview;
		init_flag = undo_connect_signal(buffer);
		keyevent_setval(0);
		ui_tmp = g_malloc(sizeof(UndoInfo));
		/* ui_tmp->str = g_strdup(""); */
		undo_gstr = g_string_new("");
	}
	/*Not sure why we do this here, and not in the insert callback*/
	ui_tmp->command = INS;
	undo_gstr = g_string_erase(undo_gstr, 0, -1);
}

/*Straight forward. When performing the undo/redo, we don't want our
  deletion and insertion callbacks getting called and going all wonky
  so we provide a couple of functions to block and unblock. This 
  constitutes all the code in this file which doesn't suck*/
gint undo_block_signal(GtkTextBuffer *buffer)
{
	return 
	g_signal_handlers_block_by_func(G_OBJECT(buffer), G_CALLBACK(cb_delete_range), buffer) +
/*	g_signal_handlers_block_by_func(G_OBJECT(buffer), G_CALLBACK(cb_modified_changed), buffer) + */
	g_signal_handlers_block_by_func(G_OBJECT(buffer), G_CALLBACK(cb_insert_text), buffer);
}

gint undo_unblock_signal(GtkTextBuffer *buffer)
{
	return 
	g_signal_handlers_unblock_by_func(G_OBJECT(buffer), G_CALLBACK(cb_delete_range), buffer) +
/*	g_signal_handlers_unblock_by_func(G_OBJECT(buffer), G_CALLBACK(cb_modified_changed), buffer) + */
	g_signal_handlers_unblock_by_func(G_OBJECT(buffer), G_CALLBACK(cb_insert_text), buffer);
}


/* More in the world of the bizarre sequency flag. If we ever need to set the
   flag, by definition the UndoInfo struct for the undo step is already on 
   the stack, and a new temp object already being filled up - probably
   getting ready to be saved itself. So we need a way to set the flag
   on the UndoInfo struct at the top of the stack.
   TaaDaa!
  */
void undo_set_sequency(gboolean seq)
{
	UndoInfo *ui;

	ui = g_trash_stack_peek(&undo_stack);
	if (ui != NULL) {
		ui->seq = seq;
	}
DV(g_print("<undo_set_sequency: %d>\n", seq));	
}

/* Pretty straightforward, believe it or not. First thing it does is
 * put any data from undo_gstr in a UndoInfo struct on the stack.
 * Once that's done, it sees if there is anything on the stack,
 * pops it, blocks the undo callbacks on the buffer, and inspects
 * the command on the UndoInfo struct. If were undoing an insertion
 * then we need to delete text. Grab iterators from the offsets
 * in the UndoInfo object, and delete that sucker. If it is a 
 * deletion were undoing, then insert the text back in. REMEMBER:
 * overwritten text winds up being handled via a two part process
 * with the sequency flag. If the sequency flag is set for the
 * NEXT step in the stack, then (Lord Help Us and also FIXME)
 * we return true, so we tell the caller to call us again so
 * we can do it all over again. If, on the other hand, the
 * sequency flag is not set, then we tidy up after ourselves, 
 * putting the curser in the right place, and scrolling the textview,
 * setting the redo menu item sensitive, fixing the title bar if 
 * necessary and finally set the undo item insensitive if we've 
 * emptied the stack. Whew!
 */
void undo_undo(GtkTextBuffer *buffer)
{
	GtkTextIter start_iter, end_iter;
	UndoInfo *ui;
	
	if (undo_gstr->len) {
DV(g_print("undo_undo_real: ui_tmp->command %d", ui_tmp->command));
		undo_append_undo_info(buffer, ui_tmp->command, ui_tmp->start, ui_tmp->end, g_strdup(undo_gstr->str));
		undo_gstr = g_string_erase(undo_gstr, 0, -1);
	}
	if ((g_trash_stack_peek(&undo_stack)) != NULL) {
		undo_block_signal(buffer);
		ui = (UndoInfo *)(g_trash_stack_pop(&undo_stack));
DV(g_print("undo_undo_real: ui->command %d", ui->command));
		gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, ui->start);
		switch (ui->command) {
		case INS:
			gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, ui->end);
			gtk_text_buffer_delete(buffer, &start_iter, &end_iter);
			break;
		default:
			gtk_text_buffer_insert(buffer, &start_iter, ui->str, -1);
		}
		g_trash_stack_push(&redo_stack, ui);
DV(g_print("cb_edit_undo: undo list left %d\n", g_trash_stack_height(&undo_stack)));
		undo_unblock_signal(buffer);
		undo_check_step_modif(buffer);
		if (undo_stack != NULL) {
			if (((UndoInfo *)g_trash_stack_peek(&undo_stack))->seq) {
				undo_undo(buffer);
				return;
			}
		} else
			gtk_widget_set_sensitive(undo_menu_item, FALSE);
		gtk_widget_set_sensitive(redo_menu_item, TRUE);
		if (ui->command == DEL)
			gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, ui->start);
		gtk_text_buffer_place_cursor(buffer, &start_iter);
		gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(view), &start_iter,
			0.1, FALSE, 0.5, 0.5);
	}
	return;
}

/*yeah, so, basically this is exactly like undo_undo, except simpler*/
void undo_redo(GtkTextBuffer *buffer)
{
	GtkTextIter start_iter, end_iter;
	UndoInfo *ri;
	
	if (redo_stack != NULL) {
		undo_block_signal(buffer);
		ri = (UndoInfo *)(g_trash_stack_pop(&redo_stack));
		gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, ri->start);
		switch (ri->command) {
		case INS:
			gtk_text_buffer_insert(buffer, &start_iter, ri->str, -1);
			break;
		default:
			gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, ri->end);
			gtk_text_buffer_delete(buffer, &start_iter, &end_iter);
		}
		g_trash_stack_push(&undo_stack, ri);
DV(g_print("cb_edit_redo: redo list left %d\n", g_trash_stack_height(&redo_stack)));
		undo_unblock_signal(buffer);
		undo_check_step_modif(buffer);
		if (ri->seq) {
			undo_redo(buffer);
			return;
		}
		if (redo_stack == NULL)
			gtk_widget_set_sensitive(redo_menu_item, FALSE);
		gtk_widget_set_sensitive(undo_menu_item, TRUE);
		gtk_text_buffer_place_cursor(buffer, &start_iter);
		gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(view), &start_iter,
			0.1, FALSE, 0.5, 0.5);
	}
	return;
}
