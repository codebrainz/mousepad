/* $Id$ */
/*
 * Copyright (c) 2007 Nick Schermer <nick@xfce.org>
 * Copyright (c) 2007 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-types.h>
#include <mousepad/mousepad-application.h>
#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-dialogs.h>
#include <mousepad/mousepad-preferences.h>
#include <mousepad/mousepad-search-bar.h>
#include <mousepad/mousepad-statusbar.h>
#include <mousepad/mousepad-window.h>
#include <mousepad/mousepad-window-ui.h>



#define WINDOW_SPACING 3



/* class functions */
static void              mousepad_window_class_init                   (MousepadWindowClass    *klass);
static void              mousepad_window_init                         (MousepadWindow         *window);
static void              mousepad_window_dispose                      (GObject                *object);
static void              mousepad_window_finalize                     (GObject                *object);
static gboolean          mousepad_window_configure_event              (GtkWidget              *widget,
                                                                       GdkEventConfigure      *event);

/* statusbar tooltips */
static void              mousepad_window_connect_proxy                (GtkUIManager           *manager,
                                                                       GtkAction              *action,
                                                                       GtkWidget              *proxy,
                                                                       MousepadWindow         *window);
static void              mousepad_window_disconnect_proxy             (GtkUIManager           *manager,
                                                                       GtkAction              *action,
                                                                       GtkWidget              *proxy,
                                                                       MousepadWindow         *window);
static void              mousepad_window_menu_item_selected           (GtkWidget              *menu_item,
                                                                       MousepadWindow         *window);
static void              mousepad_window_menu_item_deselected         (GtkWidget              *menu_item,
                                                                       MousepadWindow         *window);

/* save windows geometry */
static gboolean          mousepad_window_save_geometry_timer          (gpointer                user_data);
static void              mousepad_window_save_geometry_timer_destroy  (gpointer                user_data);

/* window functions */
static gboolean          mousepad_window_save                         (MousepadWindow         *window,
                                                                       MousepadDocument       *document,
                                                                       gboolean                force_save_as);
static void              mousepad_window_add                          (MousepadWindow         *window,
                                                                       MousepadDocument       *document);
static gboolean          mousepad_window_close_document               (MousepadWindow         *window,
                                                                       MousepadDocument       *document);
static void              mousepad_window_set_title                    (MousepadWindow         *window,
                                                                       MousepadDocument       *document);
static void              mousepad_window_toggle_overwrite             (MousepadWindow         *window,
                                                                       gboolean                overwrite);

/* notebook signals */
static void              mousepad_window_page_notified                (GtkNotebook            *notebook,
                                                                       GParamSpec             *pspec,
                                                                       MousepadWindow         *window);
static void              mousepad_window_tab_removed                  (GtkNotebook            *notebook,
                                                                       MousepadDocument       *document,
                                                                       MousepadWindow         *window);
static void              mousepad_window_page_reordered               (GtkNotebook            *notebook,
                                                                       GtkNotebookPage        *page,
                                                                       guint                   page_num,
                                                                       MousepadWindow         *window);
static void              mousepad_window_tab_popup_position           (GtkMenu                *menu,
                                                                       gint                   *x,
                                                                       gint                   *y,
                                                                       gboolean               *push_in,
                                                                       gpointer                user_data);
static gboolean          mousepad_window_tab_popup                    (GtkNotebook            *notebook,
                                                                       GdkEventButton         *event,
                                                                       MousepadWindow         *window);

/* document signals */
static void              mousepad_window_modified_changed             (MousepadDocument       *document,
                                                                       MousepadWindow         *window);
static void              mousepad_window_cursor_changed               (MousepadDocument       *document,
                                                                       guint                   line,
                                                                       guint                   column,
                                                                       MousepadWindow         *window);
static void              mousepad_window_overwrite_changed            (MousepadDocument       *document,
                                                                       gboolean                overwrite,
                                                                       MousepadWindow         *window);
static void              mousepad_window_selection_changed            (MousepadDocument       *document,
                                                                       gboolean                selected,
                                                                       MousepadWindow         *window);
static void              mousepad_window_can_undo                     (MousepadWindow         *window,
                                                                       gboolean                can_undo);
static void              mousepad_window_can_redo                     (MousepadWindow         *window,
                                                                       gboolean                can_redo);

/* menu updaters */
static void              mousepad_window_update_actions               (MousepadWindow         *window);
static gboolean          mousepad_window_update_gomenu_idle           (gpointer                user_data);
static void              mousepad_window_update_gomenu_idle_destroy   (gpointer                user_data);
static void              mousepad_window_update_gomenu                (MousepadWindow         *window);

/* recent functions */
static void              mousepad_window_recent_add                   (MousepadWindow         *window,
                                                                       const gchar            *filename);
static gchar            *mousepad_window_recent_escape_underscores    (const gchar            *str);
static gint              mousepad_window_recent_sort                  (GtkRecentInfo          *a,
                                                                       GtkRecentInfo          *b);
static gboolean          mousepad_window_recent_menu_idle             (gpointer                user_data);
static void              mousepad_window_recent_menu_idle_destroy     (gpointer                user_data);
static void              mousepad_window_recent_menu                  (MousepadWindow         *window);
static void              mousepad_window_recent_clear                 (MousepadWindow         *window);

/* actions */
static void              mousepad_window_action_open_new_window       (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_open_file             (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_open_recent           (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_clear_recent          (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_save_file             (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_save_file_as          (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_reload                (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_print                 (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_close_tab             (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_close                 (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_close_all_windows     (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_open_new_tab          (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_undo                  (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_redo                  (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_cut                   (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_copy                  (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_paste                 (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_delete                (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_select_all            (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_find                  (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_find_next             (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_find_previous         (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_replace               (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_jump_to               (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_select_font           (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_statusbar             (GtkToggleAction        *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_word_wrap             (GtkToggleAction        *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_line_numbers          (GtkToggleAction        *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_auto_indent           (GtkToggleAction        *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_prev_tab              (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_next_tab              (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_goto                  (GtkRadioAction         *action,
                                                                       GtkNotebook            *notebook);
static void              mousepad_window_action_about                 (GtkAction              *action,
                                                                       MousepadWindow         *window);

/* miscellaneous actions */
static void              mousepad_window_button_close_tab             (MousepadDocument       *document,
                                                                       MousepadWindow         *window);
static gboolean          mousepad_window_delete_event                 (MousepadWindow         *window,
                                                                       GdkEvent               *event);




struct _MousepadWindowClass
{
  GtkWindowClass __parent__;
};

struct _MousepadWindow
{
  GtkWindow            __parent__;

  /* mousepad preferences */
  MousepadPreferences *preferences;

  /* the current active document */
  MousepadDocument    *active;

  /* closures for the menu callbacks */
  GClosure            *menu_item_selected_closure;
  GClosure            *menu_item_deselected_closure;

  /* action groups */
  GtkActionGroup      *window_actions;
  GtkActionGroup      *gomenu_actions;
  GtkActionGroup      *recent_actions;

  /* UI manager */
  GtkUIManager        *ui_manager;

  /* recent items manager */
  GtkRecentManager    *recent_manager;

  /* main window widgets */
  GtkWidget           *table;
  GtkWidget           *notebook;
  GtkWidget           *container;
  GtkWidget           *search_bar;
  GtkWidget           *statusbar;

  /* support to remember window geometry */
  guint                save_geometry_timer_id;

  /* idle update functions for the recent and go menu */
  guint                update_recent_menu_id;
  guint                update_go_menu_id;
};



static const GtkActionEntry action_entries[] =
{
  { "file-menu", NULL, N_("_File"), NULL, NULL, NULL, },
    { "new-tab", "tab-new", N_("New _Tab"), "<control>T", N_("Create a new document"), G_CALLBACK (mousepad_window_action_open_new_tab), },
    { "new-window", "window-new", N_("_New Window"), "<control>N", N_("Create a new document in a new window"), G_CALLBACK (mousepad_window_action_open_new_window), },
    { "open-file", GTK_STOCK_OPEN, N_("_Open File"), NULL, N_("Open a file"), G_CALLBACK (mousepad_window_action_open_file), },
    { "recent-menu", NULL, N_("Open _Recent"), NULL, NULL, NULL, },
      { "no-recent-items", NULL, N_("No items found"), NULL, NULL, NULL, },
      { "clear-recent", GTK_STOCK_CLEAR, N_("Clear _History"), NULL, N_("Clear the recently used files history"), G_CALLBACK (mousepad_window_action_clear_recent), },
    { "save-file", GTK_STOCK_SAVE, N_("_Save"), NULL, N_("Save the current file"), G_CALLBACK (mousepad_window_action_save_file), },
    { "save-file-as", GTK_STOCK_SAVE_AS, N_("Save _As"), NULL, N_("Save current document as another file"), G_CALLBACK (mousepad_window_action_save_file_as), },
    { "reload", GTK_STOCK_REFRESH, N_("Re_load"), NULL, N_("Reload this document."), G_CALLBACK (mousepad_window_action_reload), },
    { "print-document", GTK_STOCK_PRINT, N_("_Print"), "<control>P", N_("Prin the current page"), G_CALLBACK (mousepad_window_action_print), },
    { "close-tab", GTK_STOCK_CLOSE, N_("C_lose Tab"), "<control>W", N_("Close the current file"), G_CALLBACK (mousepad_window_action_close_tab), },
    { "close-window", GTK_STOCK_QUIT, N_("_Close Window"), "<control>Q", N_("Quit the program"), G_CALLBACK (mousepad_window_action_close), },
    { "close-all-windows", NULL, N_("Close _All Windows"), "<control><shift>W", N_("Close all Mousepad windows"), G_CALLBACK (mousepad_window_action_close_all_windows), },

  { "edit-menu", NULL, N_("_Edit"), NULL, NULL, NULL, },
    { "undo", GTK_STOCK_UNDO, N_("_Undo"), "<control>Z", N_("Undo the last action"), G_CALLBACK (mousepad_window_action_undo), },
    { "redo", GTK_STOCK_REDO, N_("_Redo"), "<control>Y", N_("Redo the last undone action"), G_CALLBACK (mousepad_window_action_redo), },
    { "cut", GTK_STOCK_CUT, N_("Cu_t"), NULL, N_("Cut the selection"), G_CALLBACK (mousepad_window_action_cut), },
    { "copy", GTK_STOCK_COPY, N_("_Copy"), NULL, N_("Copy the selection"), G_CALLBACK (mousepad_window_action_copy), },
    { "paste", GTK_STOCK_PASTE, N_("_Paste"), NULL, N_("Paste the clipboard"), G_CALLBACK (mousepad_window_action_paste), },
    { "delete", GTK_STOCK_DELETE, N_("_Delete"), NULL, N_("Delete the selected text"), G_CALLBACK (mousepad_window_action_delete), },
    { "select-all", GTK_STOCK_SELECT_ALL, N_("Select _All"), NULL, N_("Select the entire document"), G_CALLBACK (mousepad_window_action_select_all), },

  { "search-menu", NULL, N_("_Search"), NULL, NULL, NULL, },
    { "find", GTK_STOCK_FIND, N_("_Find"), NULL, N_("Search for text"), G_CALLBACK (mousepad_window_action_find), },
    { "find-next", NULL, N_("Find _Next"), NULL, N_("Search forwards for the same text"), G_CALLBACK (mousepad_window_action_find_next), },
    { "find-previous", NULL, N_("Find _Previous"), NULL, N_("Search backwards for the same text"), G_CALLBACK (mousepad_window_action_find_previous), },
    { "replace", GTK_STOCK_FIND_AND_REPLACE, N_("_Replace"), NULL, N_("Search for and replace text"), G_CALLBACK (mousepad_window_action_replace), },
    { "jump-to", GTK_STOCK_JUMP_TO, N_("_Jump To"), NULL, N_("Go to a specific line"), G_CALLBACK (mousepad_window_action_jump_to), },

  { "view-menu", NULL, N_("_View"), NULL, NULL, NULL, },
    { "font", GTK_STOCK_SELECT_FONT, N_("_Font"), NULL, N_("Change the editor font"), G_CALLBACK (mousepad_window_action_select_font), },

  { "document-menu", NULL, N_("_Document"), NULL, NULL, NULL, },

  { "go-menu", NULL, N_("_Go"), NULL, },
    { "back", GTK_STOCK_GO_BACK, N_("Back"), NULL, N_("Select the previous tab"), G_CALLBACK (mousepad_window_action_prev_tab), },
    { "forward", GTK_STOCK_GO_FORWARD, N_("Forward"), NULL, N_("Select the next tab"), G_CALLBACK (mousepad_window_action_next_tab), },

  { "help-menu", NULL, N_("_Help"), NULL, },
    { "about", GTK_STOCK_ABOUT, N_("_About"), NULL, N_("About this application"), G_CALLBACK (mousepad_window_action_about), },
};

static const GtkToggleActionEntry toggle_action_entries[] =
{
  { "statusbar", NULL, N_("_Statusbar"), NULL, N_("Change the visibility of the statusbar"), G_CALLBACK (mousepad_window_action_statusbar), FALSE, },
  { "word-wrap", NULL, N_("_Word Wrap"), NULL, N_("Toggle breaking lines in between words"), G_CALLBACK (mousepad_window_action_word_wrap), FALSE, },
  { "line-numbers", NULL, N_("_Line Numbers"), NULL, NULL, G_CALLBACK (mousepad_window_action_line_numbers), FALSE, },
  { "auto-indent", NULL, N_("_Auto Indent"), NULL, NULL, G_CALLBACK (mousepad_window_action_auto_indent), FALSE, },
};



static GObjectClass *mousepad_window_parent_class;
static gboolean      lock_menu_updates = FALSE;


GtkWidget *
mousepad_window_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_WINDOW, NULL);
}



GType
mousepad_window_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_type_register_static_simple (GTK_TYPE_WINDOW,
                                            I_("MousepadWindow"),
                                            sizeof (MousepadWindowClass),
                                            (GClassInitFunc) mousepad_window_class_init,
                                            sizeof (MousepadWindow),
                                            (GInstanceInitFunc) mousepad_window_init,
                                            0);
    }

  return type;
}



static void
mousepad_window_class_init (MousepadWindowClass *klass)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *gtkwidget_class;

  mousepad_window_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = mousepad_window_dispose;
  gobject_class->finalize = mousepad_window_finalize;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->configure_event = mousepad_window_configure_event;
}



static void
mousepad_window_init (MousepadWindow *window)
{
  GtkAccelGroup *accel_group;
  GtkWidget     *menubar;
  GtkWidget     *label;
  GtkWidget     *separator;
  GtkWidget     *ebox;
  GtkAction     *action;
  gint           width, height;
  gboolean       visible;

  /* initialize stuff */
  window->save_geometry_timer_id = 0;
  window->update_recent_menu_id = 0;
  window->update_go_menu_id = 0;
  window->search_bar = NULL;
  window->statusbar = NULL;
  window->active = NULL;

  /* add the preferences to the window */
  window->preferences = mousepad_preferences_get ();

  /* allocate a closure for the menu_item_selected() callback */
  window->menu_item_selected_closure = g_cclosure_new_object (G_CALLBACK (mousepad_window_menu_item_selected), G_OBJECT (window));
  g_closure_ref (window->menu_item_selected_closure);
  g_closure_sink (window->menu_item_selected_closure);

  /* allocate a closure for the menu_item_deselected() callback */
  window->menu_item_deselected_closure = g_cclosure_new_object (G_CALLBACK (mousepad_window_menu_item_deselected), G_OBJECT (window));
  g_closure_ref (window->menu_item_deselected_closure);
  g_closure_sink (window->menu_item_deselected_closure);

  /* signal for handling the window delete event */
  g_signal_connect (G_OBJECT (window), "delete-event",
                    G_CALLBACK (mousepad_window_delete_event), NULL);

  /* read settings from the preferences */
  g_object_get (G_OBJECT (window->preferences),
                "last-window-width", &width,
                "last-window-height", &height,
                NULL);

  /* set the default window size */
  gtk_window_set_default_size (GTK_WINDOW (window), width, height);

  /* the action group for this window */
  window->window_actions = gtk_action_group_new ("WindowActions");
  gtk_action_group_set_translation_domain (window->window_actions, GETTEXT_PACKAGE);
  gtk_action_group_add_actions (window->window_actions, action_entries, G_N_ELEMENTS (action_entries), GTK_WIDGET (window));
  gtk_action_group_add_toggle_actions (window->window_actions, toggle_action_entries, G_N_ELEMENTS (toggle_action_entries), GTK_WIDGET (window));

  /* create the ui manager and connect proxy signals for the statusbar */
  window->ui_manager = gtk_ui_manager_new ();
  g_signal_connect (G_OBJECT (window->ui_manager), "connect-proxy", G_CALLBACK (mousepad_window_connect_proxy), window);
  g_signal_connect (G_OBJECT (window->ui_manager), "disconnect-proxy", G_CALLBACK (mousepad_window_disconnect_proxy), window);
  gtk_ui_manager_insert_action_group (window->ui_manager, window->window_actions, 0);
  gtk_ui_manager_add_ui_from_string (window->ui_manager, mousepad_window_ui, mousepad_window_ui_length, NULL);

  /* create the recent manager */
  window->recent_manager = gtk_recent_manager_get_default ();
  window->recent_actions = gtk_action_group_new ("RecentActions");
  gtk_ui_manager_insert_action_group (window->ui_manager, window->recent_actions, -1);
  g_signal_connect_swapped (G_OBJECT (window->recent_manager), "changed",
                            G_CALLBACK (mousepad_window_recent_menu), window);

  /* create the recent menu */
  mousepad_window_recent_menu (window);

  /* create action group for the go menu */
  window->gomenu_actions = gtk_action_group_new ("GoMenuActions");
  gtk_ui_manager_insert_action_group (window->ui_manager, window->gomenu_actions, -1);

  /* set accel group for the window */
  accel_group = gtk_ui_manager_get_accel_group (window->ui_manager);
  gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

  /* create the main table */
  window->table = gtk_table_new (6, 1, FALSE);
  gtk_container_add (GTK_CONTAINER (window), window->table);
  gtk_widget_show (window->table);

  menubar = gtk_ui_manager_get_widget (window->ui_manager, "/main-menu");
  gtk_table_attach (GTK_TABLE (window->table), menubar, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (menubar);

  /* check if we need to add the root warning */
  if (G_UNLIKELY (geteuid () == 0))
    {
      /* install default settings for the root warning text box */
      gtk_rc_parse_string ("style\"mousepad-window-root-style\"{bg[NORMAL]=\"#b4254b\"\nfg[NORMAL]=\"#fefefe\"}\n"
                           "widget\"MousepadWindow.*.root-warning\"style\"mousepad-window-root-style\"\n"
                           "widget\"MousepadWindow.*.root-warning.GtkLabel\"style\"mousepad-window-root-style\"\n");

      /* add the box for the root warning */
      ebox = gtk_event_box_new ();
      gtk_widget_set_name (ebox, "root-warning");
      gtk_table_attach (GTK_TABLE (window->table), ebox, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
      gtk_widget_show (ebox);

      /* add the label with the root warning */
      label = gtk_label_new (_("Warning, you are using the root account, you may harm your system."));
      gtk_misc_set_padding (GTK_MISC (label), 6, 3);
      gtk_container_add (GTK_CONTAINER (ebox), label);
      gtk_widget_show (label);

      separator = gtk_hseparator_new ();
      gtk_table_attach (GTK_TABLE (window->table), separator, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
      gtk_widget_show (separator);
    }

  /* create the notebook */
  window->notebook = g_object_new (GTK_TYPE_NOTEBOOK,
                                   "homogeneous", FALSE,
                                   "scrollable", TRUE,
                                   "show-border", FALSE,
                                   "show-tabs", FALSE,
                                   "tab-hborder", 0,
                                   "tab-vborder", 0,
                                   NULL);

  /* connect signals to the notebooks */
  g_signal_connect (G_OBJECT (window->notebook), "notify::page",
                    G_CALLBACK (mousepad_window_page_notified), window);
  g_signal_connect (G_OBJECT (window->notebook), "remove",
                    G_CALLBACK (mousepad_window_tab_removed), window);
  g_signal_connect (G_OBJECT (window->notebook), "page-reordered",
                    G_CALLBACK (mousepad_window_page_reordered), window);
  g_signal_connect (G_OBJECT (window->notebook), "button-press-event",
                    G_CALLBACK (mousepad_window_tab_popup), window);

  /* append and show the notebook */
  gtk_table_attach (GTK_TABLE (window->table), window->notebook, 0, 1, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_table_set_row_spacing (GTK_TABLE (window->table), 2, WINDOW_SPACING);
  gtk_widget_show (window->notebook);

  /* check if we should display the statusbar by default */
  g_object_get (G_OBJECT (window->preferences), "last-statusbar-visible", &visible, NULL);
  action = gtk_action_group_get_action (window->window_actions, "statusbar");
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);
}



static void
mousepad_window_dispose (GObject *object)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (object);

  /* destroy the save geometry timer source */
  if (G_UNLIKELY (window->save_geometry_timer_id != 0))
    g_source_remove (window->save_geometry_timer_id);

  (*G_OBJECT_CLASS (mousepad_window_parent_class)->dispose) (object);
}



static void
mousepad_window_finalize (GObject *object)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (object);

  /* cancel a scheduled recent menu update */
  if (G_UNLIKELY (window->update_recent_menu_id != 0))
    g_source_remove (window->update_recent_menu_id);

  /* cancel a scheduled go menu update */
  if (G_UNLIKELY (window->update_go_menu_id != 0))
    g_source_remove (window->update_go_menu_id);

  /* drop our references on the menu_item_selected()/menu_item_deselected() closures */
  g_closure_unref (window->menu_item_deselected_closure);
  g_closure_unref (window->menu_item_selected_closure);

  /* release the ui manager */
  g_signal_handlers_disconnect_matched (G_OBJECT (window->ui_manager), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, window);
  g_object_unref (G_OBJECT (window->ui_manager));

  /* release the action groups */
  g_object_unref (G_OBJECT (window->window_actions));
  g_object_unref (G_OBJECT (window->gomenu_actions));
  g_object_unref (G_OBJECT (window->recent_actions));

  /* release the preferences reference */
  g_object_unref (G_OBJECT (window->preferences));

  (*G_OBJECT_CLASS (mousepad_window_parent_class)->finalize) (object);
}



static gboolean
mousepad_window_configure_event (GtkWidget         *widget,
                                 GdkEventConfigure *event)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (widget);

  /* check if we have a new dimension here */
  if (widget->allocation.width != event->width || widget->allocation.height != event->height)
    {
      /* drop any previous timer source */
      if (window->save_geometry_timer_id > 0)
        g_source_remove (window->save_geometry_timer_id);

      /* check if we should schedule another save timer */
      if (GTK_WIDGET_VISIBLE (widget))
        {
          /* save the geometry one second after the last configure event */
          window->save_geometry_timer_id = g_timeout_add_full (G_PRIORITY_LOW, 1000, mousepad_window_save_geometry_timer,
                                                               window, mousepad_window_save_geometry_timer_destroy);
        }
    }

  /* let Gtk+ handle the configure event */
  return (*GTK_WIDGET_CLASS (mousepad_window_parent_class)->configure_event) (widget, event);
}



/**
 * Statusbar Tooltip Functions
 **/
static void
mousepad_window_connect_proxy (GtkUIManager   *manager,
                               GtkAction      *action,
                               GtkWidget      *proxy,
                               MousepadWindow *window)
{
  /* we want to get informed when the user hovers a menu item */
  if (GTK_IS_MENU_ITEM (proxy))
    {
      g_signal_connect_closure (G_OBJECT (proxy), "select", window->menu_item_selected_closure, FALSE);
      g_signal_connect_closure (G_OBJECT (proxy), "deselect", window->menu_item_deselected_closure, FALSE);
    }
}



static void
mousepad_window_disconnect_proxy (GtkUIManager   *manager,
                                  GtkAction      *action,
                                  GtkWidget      *proxy,
                                  MousepadWindow *window)
{
  /* undo what we did in connect_proxy() */
  if (GTK_IS_MENU_ITEM (proxy))
    {
      g_signal_handlers_disconnect_matched (G_OBJECT (proxy), G_SIGNAL_MATCH_CLOSURE, 0, 0, window->menu_item_selected_closure, NULL, NULL);
      g_signal_handlers_disconnect_matched (G_OBJECT (proxy), G_SIGNAL_MATCH_CLOSURE, 0, 0, window->menu_item_deselected_closure, NULL, NULL);
    }
}



static void
mousepad_window_menu_item_selected (GtkWidget      *menu_item,
                                    MousepadWindow *window)
{
  GtkAction *action;
  gchar     *tooltip;

  /* we can only display tooltips if we have a statusbar */
  if (G_LIKELY (window->statusbar != NULL))
    {
      /* get the action from the menu item */
      action = g_object_get_data (G_OBJECT (menu_item), I_("gtk-action"));
      if (G_LIKELY (action))
        {
          /* read the tooltip from the action, if there is one */
          g_object_get (G_OBJECT (action), "tooltip", &tooltip, NULL);
          if (G_LIKELY (tooltip != NULL))
            {
              /* show the tooltip */
              mousepad_statusbar_set_text (MOUSEPAD_STATUSBAR (window->statusbar), tooltip);

              /* cleanup */
              g_free (tooltip);
            }
        }
    }
}



static void
mousepad_window_menu_item_deselected (GtkWidget      *menu_item,
                                      MousepadWindow *window)
{
  /* we can only undisplay tooltips if we have a statusbar */
  if (G_LIKELY (window->statusbar != NULL))
    {
      /* hide the tip from the statusbar */
      mousepad_statusbar_set_text (MOUSEPAD_STATUSBAR (window->statusbar), NULL);
    }
}



/**
 * Save Geometry Functions
 **/
static gboolean
mousepad_window_save_geometry_timer (gpointer user_data)
{
  GdkWindowState   state;
  MousepadWindow  *window = MOUSEPAD_WINDOW (user_data);
  gboolean         remember_geometry;
  gint             width;
  gint             height;

  GDK_THREADS_ENTER ();

  /* check if we should remember the window geometry */
  g_object_get (G_OBJECT (window->preferences), "misc-remember-geometry", &remember_geometry, NULL);
  if (G_LIKELY (remember_geometry))
    {
      /* check if the window is still visible */
      if (GTK_WIDGET_VISIBLE (window))
        {
          /* determine the current state of the window */
          state = gdk_window_get_state (GTK_WIDGET (window)->window);

          /* don't save geometry for maximized or fullscreen windows */
          if ((state & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)) == 0)
            {
              /* determine the current width/height of the window... */
              gtk_window_get_size (GTK_WINDOW (window), &width, &height);

              /* ...and remember them as default for new windows */
              g_object_set (G_OBJECT (window->preferences),
                            "last-window-width", width,
                            "last-window-height", height, NULL);
            }
        }
    }

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
mousepad_window_save_geometry_timer_destroy (gpointer user_data)
{
  MOUSEPAD_WINDOW (user_data)->save_geometry_timer_id = 0;
}



/**
 * Mousepad Window Functions
 **/
gboolean
mousepad_window_open_tab (MousepadWindow *window,
                          const gchar    *filename)
{
  GtkWidget   *document;
  GError      *error = NULL;
  gboolean     succeed = TRUE;
  gint         npages = 0, i;
  gchar       *font_name;
  const gchar *opened_filename;

  /* get the number of page in the notebook */
  if (filename)
    npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

  /* walk though the tabs */
  for (i = 0; i < npages; i++)
    {
      document = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i);

      if (G_LIKELY (document != NULL))
        {
          /* get the filename */
          opened_filename = mousepad_document_get_filename (MOUSEPAD_DOCUMENT (document));

          /* see if the file is already opened */
          if (opened_filename && strcmp (filename, opened_filename) == 0)
            {
              /* switch to the tab */
              gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), i);

              /* and we're done */
              return TRUE;
            }
        }
    }

  /* new document */
  document = mousepad_document_new ();

  if (filename)
    /* load the file content in the document */
    succeed = mousepad_document_open_file (MOUSEPAD_DOCUMENT (document), filename, &error);

  if (G_LIKELY (succeed))
    {
      /* add the document to the notebook and connect some signals */
      mousepad_window_add (window, MOUSEPAD_DOCUMENT (document));

      /* set the textview font */
      g_object_get (G_OBJECT (window->preferences), "font-name", &font_name, NULL);
      mousepad_document_set_font (MOUSEPAD_DOCUMENT (document), font_name);
      g_free (font_name);
    }
  else
    {
      /* something went wrong, remove the document */
      gtk_widget_destroy (GTK_WIDGET (document));

      /* show the warning */
      mousepad_dialogs_show_error (GTK_WINDOW (window), error, _("Failed to open file"));
      g_error_free (error);
    }

  return succeed;
}



gboolean
mousepad_window_open_files (MousepadWindow  *window,
                            const gchar     *working_directory,
                            gchar          **filenames)
{
  gint   n, npages;
  gchar *filename = NULL;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);
  _mousepad_return_val_if_fail (working_directory != NULL, FALSE);
  _mousepad_return_val_if_fail (filenames != NULL, FALSE);
  _mousepad_return_val_if_fail (*filenames != NULL, FALSE);

  /* block menu updates */
  lock_menu_updates = TRUE;

  /* walk through all the filenames */
  for (n = 0; filenames[n] != NULL; ++n)
    {
      /* check if the filename looks like an uri */
      if (strncmp (filenames[n], "file:", 5) == 0)
        {
          /* convert the uri to an absolute filename */
          filename = g_filename_from_uri (filenames[n], NULL, NULL);
        }
      else if (g_path_is_absolute (filenames[n]) == FALSE)
        {
          /* create an absolute file */
          filename = g_build_filename (working_directory, filenames[n], NULL);
        }

      /* open a new tab with the file */
      mousepad_window_open_tab (window, filename ? filename : filenames[n]);

      /* cleanup */
      g_free (filename);
      filename = NULL;
    }

  /* allow menu updates again */
  lock_menu_updates = FALSE;

  /* check if there were tabs opened, if not we return false and in
   * window-application we open an empty tab */
  npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));
  if (G_UNLIKELY (npages == 0))
    return FALSE;

  /* update the menus */
  mousepad_window_recent_menu (window);
  mousepad_window_update_gomenu (window);

  return TRUE;
}



static gboolean
mousepad_window_save (MousepadWindow   *window,
                      MousepadDocument *document,
                      gboolean          force_save_as)
{
  gchar       *new_filename = NULL;
  const gchar *filename;
  gboolean     succeed = FALSE;
  GError      *error = NULL;
  const gchar *message;
  gint         action = MOUSEPAD_RESPONSE_OVERWRITE;

  /* get the current filename */
  filename = mousepad_document_get_filename (document);

  if (force_save_as || filename == NULL)
    {
      /* get the new filename */
      new_filename = mousepad_dialogs_save_as (GTK_WINDOW (window), filename);

      if (new_filename)
        {
          /* try the save the file */
          succeed = mousepad_document_save_file (document, new_filename, &error);

          /* the warnings message for save as */
          message = _("Failed to save the document as another file");

          if (succeed)
            {
              /* set the filename */
              mousepad_document_set_filename (document, new_filename);

              /* update the window title */
              mousepad_window_set_title (window, document);

              /* add the new document to the recent menu */
              mousepad_window_recent_add (window, new_filename);

              /* update the go menu */
              mousepad_window_update_gomenu (window);
            }

          /* cleanup */
          g_free (new_filename);
        }
    }
  else
    {
      /* check if the file has been modified externally, if so ask the user if
       * he or she wants to overwrite/reload or cancel the action */
      if (G_UNLIKELY (mousepad_document_get_externally_modified (document)))
        action = mousepad_dialogs_ask_overwrite (GTK_WINDOW (window), filename);

      switch (action)
        {
          case MOUSEPAD_RESPONSE_OVERWRITE:
            /* save the file */
            succeed = mousepad_document_save_file (document, filename, &error);

            /* the warning message for save */
            message = _("Failed to save the document");
            break;

          case MOUSEPAD_RESPONSE_RELOAD:
            /* reload the document */
            succeed = mousepad_document_reload (document, &error);

            /* the warning message for save */
            message = _("Failed to reload the document");
            break;

          case MOUSEPAD_RESPONSE_CANCEL:
            /* do nothing */
            break;
        }
    }

  /* display the error */
  if (G_UNLIKELY (error))
    {
      mousepad_dialogs_show_error (GTK_WINDOW (window), error, message);
      g_error_free (error);
    }

  return succeed;
}



static void
mousepad_window_add (MousepadWindow   *window,
                     MousepadDocument *document)
{
  GtkWidget        *label;
  gboolean          always_show_tabs;
  gboolean          word_wrap;
  gint              page, npages;
  MousepadDocument *active;

  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* create the tab label */
  label = mousepad_document_get_tab_label (document);

  /* signals for the document */
  g_signal_connect (G_OBJECT (document), "close-tab", G_CALLBACK (mousepad_window_button_close_tab), window);
  g_signal_connect (G_OBJECT (document), "selection-changed", G_CALLBACK (mousepad_window_selection_changed), window);
  g_signal_connect (G_OBJECT (document), "modified-changed", G_CALLBACK (mousepad_window_modified_changed), window);
  g_signal_connect (G_OBJECT (document), "cursor-changed", G_CALLBACK (mousepad_window_cursor_changed), window);
  g_signal_connect (G_OBJECT (document), "overwrite-changed", G_CALLBACK (mousepad_window_overwrite_changed), window);
  g_signal_connect_swapped (G_OBJECT (document), "can-undo", G_CALLBACK (mousepad_window_can_undo), window);
  g_signal_connect_swapped (G_OBJECT (document), "can-redo", G_CALLBACK (mousepad_window_can_redo), window);

  /* insert the page right from the active tab */
  page = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));
  page = gtk_notebook_insert_page (GTK_NOTEBOOK (window->notebook), GTK_WIDGET (document), label, page + 1);

  /* allow tab reordering */
  gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (window->notebook), GTK_WIDGET (document), TRUE);

  /* check if we should always display tabs */
  g_object_get (G_OBJECT (window->preferences), "misc-always-show-tabs", &always_show_tabs, NULL);

  /* change the visibility of the tabs accordingly */
  npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (window->notebook), always_show_tabs || (npages > 1));

  /* don't focus the notebook */
  GTK_WIDGET_UNSET_FLAGS (window->notebook, GTK_CAN_FOCUS);

  /* set the wrapping mode */
  g_object_get (G_OBJECT (window->preferences), "word-wrap", &word_wrap, NULL);
  mousepad_document_set_word_wrap (document, word_wrap);

  /* rebuild the go menu */
  mousepad_window_update_gomenu (window);

  /* show the document */
  gtk_widget_show (GTK_WIDGET (document));

  /* get the active tab */
  active = window->active;

  /* switch to the new tab */
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), page);

  /* destroy the previous tab if it was not modified, untitled and the new tab is not untitled */
  if (active != NULL &&
      mousepad_document_get_modified (active) == FALSE &&
      mousepad_document_get_filename (active) == NULL &&
      mousepad_document_get_filename (document) != NULL)
    gtk_widget_destroy (GTK_WIDGET (active));
}



static gboolean
mousepad_window_close_document (MousepadWindow   *window,
                                MousepadDocument *document)
{
  gint       response;
  gboolean   succeed = FALSE;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);
  _mousepad_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), FALSE);

  /* check if the document needs to be saved */
  if (mousepad_document_get_modified (document))
    {
      /* run the dialog */
      response = mousepad_dialogs_save_changes (GTK_WINDOW (window));

      switch (response)
        {
          case MOUSEPAD_RESPONSE_DONT_SAVE:
            /* don't save, only destroy the document */
            succeed = TRUE;
            break;
          case MOUSEPAD_RESPONSE_CANCEL:
            /* we do nothing */
            break;
          case MOUSEPAD_RESPONSE_SAVE:
            succeed = mousepad_window_save (window, document, FALSE);
            break;
        }
    }
  else
    {
      /* no changes in the document, safe to destroy it */
      succeed = TRUE;
    }

  /* destroy the document */
  if (succeed)
    gtk_widget_destroy (GTK_WIDGET (document));

  return succeed;
}



static void
mousepad_window_set_title (MousepadWindow   *window,
                           MousepadDocument *document)
{
  gchar       *string;
  const gchar *title;
  gboolean     show_full_path;

  /* whether to show the full path in the window title */
  g_object_get (G_OBJECT (window->preferences), "misc-show-full-path-in-title", &show_full_path, NULL);

  /* the window title */
  title = mousepad_document_get_title (document, show_full_path);

  if (G_UNLIKELY (mousepad_document_get_readonly (document)))
    {
      string = g_strdup_printf ("%s [%s] - %s",
                                title, _("Read Only"), PACKAGE_NAME);
    }
  else
    {
      string = g_strdup_printf ("%s%s - %s",
                                mousepad_document_get_modified (document) ? "*" : "",
                                title, PACKAGE_NAME);
    }

  /* set the window title */
  gtk_window_set_title (GTK_WINDOW (window), string);

  /* cleanup */
  g_free (string);
}



static void
mousepad_window_toggle_overwrite (MousepadWindow *window,
                                  gboolean        overwrite)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  if (G_LIKELY (window->active))
    mousepad_document_set_overwrite (window->active, overwrite);
}



/**
 * Notebook Signal Functions
 **/
static void
mousepad_window_page_notified (GtkNotebook    *notebook,
                               GParamSpec     *pspec,
                               MousepadWindow *window)
{
  gint              page_num;

  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* get the notebook */
  notebook = GTK_NOTEBOOK (window->notebook);

  /* get the current page */
  page_num = gtk_notebook_get_current_page (notebook);

  if (G_LIKELY (page_num >= 0))
    window->active = MOUSEPAD_DOCUMENT (gtk_notebook_get_nth_page (notebook, page_num));
  else
    window->active = NULL;

  if (G_LIKELY (window->active != NULL))
    {
      /* set the window title */
      mousepad_window_set_title (window, window->active);

      /* update the menu actions */
      mousepad_window_update_actions (window);

      /* update the statusbar */
      mousepad_document_send_statusbar_signals (window->active);
    }
}



static void
mousepad_window_tab_removed (GtkNotebook      *notebook,
                             MousepadDocument *document,
                             MousepadWindow   *window)
{
  gboolean always_show_tabs;
  gint     npages;

  npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

  if (G_UNLIKELY (npages == 0))
    {
      /* destroy the window when there are no tabs */
      gtk_widget_destroy (GTK_WIDGET (window));
    }
  else
    {
      /* check tabs should always be visible */
      g_object_get (G_OBJECT (window->preferences), "misc-always-show-tabs", &always_show_tabs, NULL);

      /* change the visibility of the tabs accordingly */
      gtk_notebook_set_show_tabs (GTK_NOTEBOOK (window->notebook), always_show_tabs || (npages > 1));

      /* don't focus the notebook */
      GTK_WIDGET_UNSET_FLAGS (window->notebook, GTK_CAN_FOCUS);

      /* rebuild the go menu */
      mousepad_window_update_gomenu (window);

      /* update all document sensitive actions */
      mousepad_window_update_actions (window);
    }
}



static void
mousepad_window_page_reordered (GtkNotebook     *notebook,
                                GtkNotebookPage *page,
                                guint            page_num,
                                MousepadWindow  *window)
{
  /* update the go menu */
  mousepad_window_update_gomenu (window);
}



static void
mousepad_window_tab_popup_position (GtkMenu  *menu,
                                    gint     *x,
                                    gint     *y,
                                    gboolean *push_in,
                                    gpointer  user_data)
{
  GtkWidget *widget = GTK_WIDGET (user_data);

  gdk_window_get_origin (widget->window, x, y);

  *x += widget->allocation.x;
  *y += widget->allocation.y + widget->allocation.height;

  *push_in = TRUE;
}

static gboolean
mousepad_window_tab_popup (GtkNotebook    *notebook,
                           GdkEventButton *event,
                           MousepadWindow *window)
{
  GtkWidget *page, *label;
  GtkWidget *menu;
  guint      page_num = 0;
  gint       x_root;

  if (event->type == GDK_BUTTON_PRESS && event->button == 3)
  {
    /* walk through the tabs and look for the tab under the cursor */
    while ((page = gtk_notebook_get_nth_page (notebook, page_num)) != NULL)
      {
        label = gtk_notebook_get_tab_label (notebook, page);

        /* get the origin of the label */
        gdk_window_get_origin (label->window, &x_root, NULL);
        x_root = x_root + label->allocation.x;

        /* check if the cursor is inside this label */
        if (event->x_root >= x_root && event->x_root <= (x_root + label->allocation.width))
          {
            /* switch to this tab */
            gtk_notebook_set_current_page (notebook, page_num);

            /* get the menu */
            menu = gtk_ui_manager_get_widget (window->ui_manager, "/tab-menu");

            /* show it */
            gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                            mousepad_window_tab_popup_position, label,
                            event->button, event->time);

            /* we succeed */
            return TRUE;
          }

        /* try the next tab */
        ++page_num;
      }
  }

  return FALSE;
}



/**
 * Document Signals Functions
 **/
static void
mousepad_window_modified_changed (MousepadDocument *document,
                                  MousepadWindow   *window)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  mousepad_window_set_title (window, document);
}



static void
mousepad_window_cursor_changed (MousepadDocument *document,
                                guint             line,
                                guint             column,
                                MousepadWindow   *window)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* set the new statusbar position */
  if (window->statusbar)
    mousepad_statusbar_set_cursor_position (MOUSEPAD_STATUSBAR (window->statusbar), line, column);
}



static void
mousepad_window_overwrite_changed (MousepadDocument *document,
                                   gboolean          overwrite,
                                   MousepadWindow   *window)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* set the new overwrite mode in the statusbar */
  if (window->statusbar)
    mousepad_statusbar_set_overwrite (MOUSEPAD_STATUSBAR (window->statusbar), overwrite);
}



static void
mousepad_window_selection_changed (MousepadDocument *document,
                                   gboolean          selected,
                                   MousepadWindow   *window)
{
  GtkAction *action;

  action = gtk_action_group_get_action (window->window_actions, "cut");
  gtk_action_set_sensitive (action, selected);

  action = gtk_action_group_get_action (window->window_actions, "copy");
  gtk_action_set_sensitive (action, selected);

  action = gtk_action_group_get_action (window->window_actions, "delete");
  gtk_action_set_sensitive (action, selected);
}



static void
mousepad_window_can_undo (MousepadWindow *window,
                          gboolean        can_undo)
{
  GtkAction *action;

  action = gtk_action_group_get_action (window->window_actions, "undo");
  gtk_action_set_sensitive (action, can_undo);
}



static void
mousepad_window_can_redo (MousepadWindow *window,
                          gboolean        can_redo)
{
  GtkAction *action;

  action = gtk_action_group_get_action (window->window_actions, "redo");
  gtk_action_set_sensitive (action, can_redo);
}



/**
 * Menu Update Functions
 **/
static void
mousepad_window_update_actions (MousepadWindow *window)
{
  GtkAction        *action;
  GtkNotebook      *notebook = GTK_NOTEBOOK (window->notebook);
  MousepadDocument *document;
  gboolean          cycle_tabs;
  gint              n_pages;
  gint              page_num;
  gboolean          has_selection;
  gboolean          active;

  /* determine the number of pages */
  n_pages = gtk_notebook_get_n_pages (notebook);

  /* set sensitivity for the "Close Tab" action */
  action = gtk_action_group_get_action (window->window_actions, "close-tab");
  gtk_action_set_sensitive (action, (n_pages > 1));

  /* update the actions for the current document */
  document = window->active;
  if (G_LIKELY (document != NULL))
    {
      page_num = gtk_notebook_page_num (notebook, GTK_WIDGET (document));

      /* whether we cycle tabs */
      g_object_get (G_OBJECT (window->preferences), "misc-cycle-tabs", &cycle_tabs, NULL);

      action = gtk_action_group_get_action (window->window_actions, "back");
      gtk_action_set_sensitive (action, (cycle_tabs && n_pages > 1) || (page_num > 0));

      action = gtk_action_group_get_action (window->window_actions, "forward");
      gtk_action_set_sensitive (action, (cycle_tabs && n_pages > 1 ) || (page_num < n_pages - 1));

      action = gtk_action_group_get_action (window->window_actions, "save-file");
      gtk_action_set_sensitive (action, !mousepad_document_get_readonly (document));

      action = gtk_action_group_get_action (window->window_actions, "reload");
      gtk_action_set_sensitive (action, mousepad_document_get_filename (document) != NULL);

      active = mousepad_document_get_word_wrap (document);
      action = gtk_action_group_get_action (window->window_actions, "word-wrap");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);

      active = mousepad_document_get_line_numbers (document);
      action = gtk_action_group_get_action (window->window_actions, "line-numbers");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);

      active = mousepad_document_get_auto_indent (document);
      action = gtk_action_group_get_action (window->window_actions, "auto-indent");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);

      /* set the sensitivity of the undo and redo actions */
      mousepad_window_can_undo (window, mousepad_document_get_can_undo (document));
      mousepad_window_can_redo (window, mousepad_document_get_can_redo (document));

      /* set the sensitivity of the selection actions */
      has_selection = mousepad_document_get_has_selection (document);
      mousepad_window_selection_changed (document, has_selection, window);

      /* active this tab in the go menu */
      action = g_object_get_data (G_OBJECT (document), I_("go-menu-action"));
      if (G_LIKELY (action != NULL))
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
    }
}




static gboolean
mousepad_window_update_gomenu_idle (gpointer user_data)
{
  GtkWidget      *document;
  MousepadWindow *window = MOUSEPAD_WINDOW (user_data);
  gint            npages;
  gint            n;
  gchar           name[15];
  const gchar    *title;
  const gchar    *tooltip;
  gchar           accelerator[7];
  GtkRadioAction *action;
  GSList         *group = NULL;
  GList          *actions, *li;
  static guint    ui_merge_id = 0;

  GDK_THREADS_ENTER ();

  /* remove the old merge */
  if (ui_merge_id != 0)
    gtk_ui_manager_remove_ui (window->ui_manager, ui_merge_id);

  /* drop all the previous actions from the action group */
  actions = gtk_action_group_list_actions (window->gomenu_actions);
  for (li = actions; li != NULL; li = li->next)
    gtk_action_group_remove_action (window->gomenu_actions, GTK_ACTION (li->data));
  g_list_free (actions);

  /* create a new merge id */
  ui_merge_id = gtk_ui_manager_new_merge_id (window->ui_manager);

  /* walk through the notebook pages */
  npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

  for (n = 0; n < npages; ++n)
    {
      document = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), n);

      /* create a new action name */
      g_snprintf (name, sizeof (name), "document-%d", n);

      /* get the name and file name */
      title = mousepad_document_get_title (MOUSEPAD_DOCUMENT (document), FALSE);
      tooltip = mousepad_document_get_title (MOUSEPAD_DOCUMENT (document), TRUE);

      /* create the radio action */
      action = gtk_radio_action_new (name, title, tooltip, NULL, n);
      gtk_radio_action_set_group (action, group);
      group = gtk_radio_action_get_group (action);
      g_signal_connect (G_OBJECT (action), "activate",
                        G_CALLBACK (mousepad_window_action_goto), window->notebook);

      /* connect the action to the document to we can easily active it when the user switched from tab */
      g_object_set_data (G_OBJECT (document), I_("go-menu-action"), action);

      if (G_LIKELY (n < 9))
        {
          /* create an accelerator and add it to the menu */
          g_snprintf (accelerator, sizeof (accelerator), "<Alt>%d", n+1);
          gtk_action_group_add_action_with_accel (window->gomenu_actions, GTK_ACTION (action), accelerator);
        }
      else
        /* add a menu item without accelerator */
        gtk_action_group_add_action (window->gomenu_actions, GTK_ACTION (action));

      /* select this action */
      if (gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook)) == n)
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

      /* release the action */
      g_object_unref (G_OBJECT (action));

      /* add the action to the go menu */
      gtk_ui_manager_add_ui (window->ui_manager, ui_merge_id,
                             "/main-menu/go-menu/placeholder-go-items",
                             name, name, GTK_UI_MANAGER_MENUITEM, FALSE);
    }

  /* make sure the ui is up2date to avoid flickering */
  gtk_ui_manager_ensure_update (window->ui_manager);

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
mousepad_window_update_gomenu_idle_destroy (gpointer user_data)
{
  MOUSEPAD_WINDOW (user_data)->update_go_menu_id = 0;
}



static void
mousepad_window_update_gomenu (MousepadWindow *window)
{
  /* leave when we're updating multiple files or there is this an idle function pending */
  if (lock_menu_updates && window->update_go_menu_id != 0)
    return;

  /* schedule a go menu update */
  window->update_go_menu_id = g_idle_add_full (G_PRIORITY_LOW, mousepad_window_update_gomenu_idle,
                                               window, mousepad_window_update_gomenu_idle_destroy);
}



/**
 * Funtions for managing the recent files
 **/
static void
mousepad_window_recent_add (MousepadWindow *window,
                            const gchar    *filename)
{
  GtkRecentData  info;
  gchar         *uri;
  static gchar  *groups[] = { PACKAGE_NAME, NULL, };

  _mousepad_return_if_fail (filename != NULL);

  /* create the recent data */
  info.display_name = NULL;
  info.description  = NULL;
  info.mime_type    = "text/plain";
  info.app_name     = PACKAGE_NAME;
  info.app_exec     = PACKAGE " %u";
  info.groups       = groups;
  info.is_private   = FALSE;

  /* create an uri from the filename */
  uri = g_filename_to_uri (filename, NULL, NULL);

  /* add it */
  if (G_LIKELY (uri != NULL))
    {
      gtk_recent_manager_add_full (window->recent_manager, uri, &info);
      g_free (uri);
    }
}



static gchar *
mousepad_window_recent_escape_underscores (const gchar *str)
{
  GString     *result;
  gssize       length;
  const gchar *end, *next;

  length = strlen (str);
  result = g_string_sized_new (length);

  end = str + length;

  while (str != end)
    {
      next = g_utf8_next_char (str);

      switch (*str)
        {
          case '_':
            g_string_append (result, "__");
            break;
          default:
            g_string_append_len (result, str, next - str);
            break;
        }

      str = next;
    }

  return g_string_free (result, FALSE);
}



static gint
mousepad_window_recent_sort (GtkRecentInfo *a,
                             GtkRecentInfo *b)
{
  return (gtk_recent_info_get_modified (a) < gtk_recent_info_get_modified (b));
}



static gboolean
mousepad_window_recent_menu_idle (gpointer user_data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (user_data);
  static guint    ui_merge_id = 0;
  GList          *items, *li, *actions;
  GList          *filtered = NULL;
  GtkRecentInfo  *info;
  const gchar    *display_name;
  gchar          *uri_display;
  gchar          *tooltip, *name, *label;
  GtkAction      *action;
  gint            n;

  GDK_THREADS_ENTER ();

  /* unmerge the ui controls from the previous update */
  if (ui_merge_id != 0)
    gtk_ui_manager_remove_ui (window->ui_manager, ui_merge_id);

  /* drop all the previous actions from the action group */
  actions = gtk_action_group_list_actions (window->recent_actions);
  for (li = actions; li != NULL; li = li->next)
    gtk_action_group_remove_action (window->recent_actions, li->data);
  g_list_free (actions);

  /* create a new merge id */
  ui_merge_id = gtk_ui_manager_new_merge_id (window->ui_manager);

  /* get all the items in the manager */
  items = gtk_recent_manager_get_items (window->recent_manager);

  /* walk through the items in the manager and pick the ones that or in the mousepad group */
  for (li = items; li != NULL; li = li->next)
    {
      /* check if the item is in the Mousepad group */
      if (!gtk_recent_info_has_group (li->data, PACKAGE_NAME))
        continue;

      /* append the item */
      filtered = g_list_prepend (filtered, li->data);
    }

  /* sort the filtered items by modification date */
  filtered = g_list_sort (filtered, (GCompareFunc) mousepad_window_recent_sort);

  /* get the recent menu limit number */
  g_object_get (G_OBJECT (window->preferences), "misc-recent-menu-limit", &n, NULL);

  /* append the items to the menu */
  for (li = filtered; n > 0 && li != NULL; li = li->next, --n)
    {
      info = li->data;

      /* create the action name */
      name = g_strdup_printf ("recent-info-%d", n);

      /* get the name of the item and escape the underscores */
      display_name = gtk_recent_info_get_display_name (info);
      label = mousepad_window_recent_escape_underscores (display_name);

      /* create the tooltip with an utf-8 valid version of the filename */
      uri_display = gtk_recent_info_get_uri_display (info);
      tooltip = g_strdup_printf (_("Open '%s'"), uri_display);
      g_free (uri_display);

      /* create the action */
      action = gtk_action_new (name, label, tooltip, NULL);

      /* cleanup */
      g_free (tooltip);
      g_free (label);

      /* add the info data and connect a menu signal */
      g_object_set_data_full (G_OBJECT (action), I_("gtk-recent-info"), gtk_recent_info_ref (info), (GDestroyNotify) gtk_recent_info_unref);
      g_signal_connect (G_OBJECT (action), "activate",
                        G_CALLBACK (mousepad_window_action_open_recent), window);

      /* add the action to the recent actions group */
      gtk_action_group_add_action (window->recent_actions, action);

      /* add the action to the menu */
      gtk_ui_manager_add_ui (window->ui_manager, ui_merge_id,
                             "/main-menu/file-menu/recent-menu/placeholder-recent-items",
                             name, name, GTK_UI_MANAGER_MENUITEM, FALSE);

      /* cleanup */
      g_free (name);
    }

  /* set the visibility of the 'No items found' action */
  action = gtk_action_group_get_action (window->window_actions, "no-recent-items");
  gtk_action_set_visible (action, (filtered == NULL));
  gtk_action_set_sensitive (action, FALSE);

  /* set the sensitivity of the clear button */
  action = gtk_action_group_get_action (window->window_actions, "clear-recent");
  gtk_action_set_sensitive (action, (filtered != NULL));

  /* cleanup */
  g_list_free (filtered);
  g_list_foreach (items, (GFunc) gtk_recent_info_unref, NULL);
  g_list_free (items);

  /* make sure the ui is up2date to avoid flickering */
  gtk_ui_manager_ensure_update (window->ui_manager);

  GDK_THREADS_LEAVE ();

  /* stop the idle function */
  return FALSE;
}



static void
mousepad_window_recent_menu_idle_destroy (gpointer user_data)
{
  MOUSEPAD_WINDOW (user_data)->update_recent_menu_id = 0;
}



static void
mousepad_window_recent_menu (MousepadWindow *window)
{
  /* leave when we're updating multiple files or there is this an idle function pending */
  if (lock_menu_updates && window->update_recent_menu_id != 0)
    return;

  /* schedule a recent menu update */
  window->update_recent_menu_id = g_idle_add_full (G_PRIORITY_LOW, mousepad_window_recent_menu_idle,
                                                   window, mousepad_window_recent_menu_idle_destroy);
}



static void
mousepad_window_recent_clear (MousepadWindow *window)
{
  GList         *items, *li;
  const gchar   *uri;
  GError        *error = NULL;
  GtkRecentInfo *info;

  /* get all the items in the manager */
  items = gtk_recent_manager_get_items (window->recent_manager);

  /* walk through the items */
  for (li = items; li != NULL; li = li->next)
    {
      info = li->data;

      /* check if the item is in the Mousepad group */
      if (!gtk_recent_info_has_group (info, PACKAGE_NAME))
        continue;

      /* get the uri of the recent item */
      uri = gtk_recent_info_get_uri (info);

      /* try to remove it, if it fails, break the loop to avoid multiple errors */
      if (G_UNLIKELY (gtk_recent_manager_remove_item (window->recent_manager, uri, &error) == FALSE))
        break;
     }

  /* cleanup */
  g_list_foreach (items, (GFunc) gtk_recent_info_unref, NULL);
  g_list_free (items);

  /* print a warning is there is one */
  if (G_UNLIKELY (error != NULL))
    {
      mousepad_dialogs_show_error (GTK_WINDOW (window), error,
                                   _("Failed to remove an item from the document history"));
      g_error_free (error);
    }
}



/**
 * Menu Actions
 *
 * All those function should be sorted by the menu structure so it's
 * easy to find a function. They should also use the other window
 * functions as much as possible to avoid code duplication.
 **/
static void
mousepad_window_action_open_new_window (GtkAction      *action,
                                        MousepadWindow *window)
{
  MousepadApplication *application;
  GdkScreen           *screen;

  screen = gtk_widget_get_screen (GTK_WIDGET (window));

  application = mousepad_application_get ();
  mousepad_application_open_window (application, screen, NULL, NULL);
  g_object_unref (G_OBJECT (application));
}



static void
mousepad_window_action_open_new_tab (GtkAction      *action,
                                     MousepadWindow *window)
{
  mousepad_window_open_tab (window, NULL);
}



static void
mousepad_window_action_open_file (GtkAction      *action,
                                  MousepadWindow *window)
{
  GtkWidget *chooser;
  gchar     *filename;
  GSList    *filenames, *li;

  /* create new chooser dialog */
  chooser = gtk_file_chooser_dialog_new (_("Open File"),
                                         GTK_WINDOW (window),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                         NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);
  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (chooser), TRUE);

  /* run the dialog */
  if (G_LIKELY (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT))
    {
      /* open the new file */
      filenames = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (chooser));

      /* block menu updates */
      lock_menu_updates = TRUE;

      /* open the selected files */
      for (li = filenames; li != NULL; li = li->next)
        {
          filename = li->data;

          /* open the file in a new tab */
          mousepad_window_open_tab (window, filename);

          /* add to the recent files */
          mousepad_window_recent_add (window, filename);

          /* cleanup */
          g_free (filename);
        }

      /* free the list */
      g_slist_free (filenames);

      /* allow menu updates again */
      lock_menu_updates = FALSE;

      /* update the menus */
      mousepad_window_recent_menu (window);
      mousepad_window_update_gomenu (window);
    }

  /* destroy dialog */
  gtk_widget_destroy (chooser);
}



static void
mousepad_window_action_open_recent (GtkAction      *action,
                                    MousepadWindow *window)
{
  const gchar   *uri;
  GError        *error = NULL;
  gchar         *filename;
  gboolean       succeed = FALSE;
  GtkRecentInfo *info;

  /* get the info */
  info = g_object_get_data (G_OBJECT (action), I_("gtk-recent-info"));

  if (G_LIKELY (info != NULL))
    {
      /* get the file uri */
      uri = gtk_recent_info_get_uri (info);

      /* get the filename from the uri */
      filename = g_filename_from_uri (uri, NULL, NULL);

      if (G_LIKELY (filename != NULL))
        {
          /* open the file in a new tab if it exists */
          if (g_file_test (filename, G_FILE_TEST_EXISTS))
            succeed = mousepad_window_open_tab (window, filename);
          else
            {
              /* create a warning */
              g_set_error (&error,  G_FILE_ERROR, G_FILE_ERROR_IO,
                           _("Failed to open \"%s\" for reading. It will be removed from the document history"), filename);

              /* show the warning and cleanup */
              mousepad_dialogs_show_error (GTK_WINDOW (window), error, _("Failed to open file"));
              g_error_free (error);
            }

          /* update the document history */
          if (G_LIKELY (succeed))
            /* update the recent manager count and time */
            gtk_recent_manager_add_item (window->recent_manager, uri);
          else
            /* remove the item from the history */
            gtk_recent_manager_remove_item (window->recent_manager, uri, NULL);

          /* cleanup */
          g_free (filename);
        }
    }
}



static void
mousepad_window_action_clear_recent (GtkAction      *action,
                                     MousepadWindow *window)
{
  /* ask the user if he or she really want to clear the history */
  if (mousepad_dialogs_clear_recent (GTK_WINDOW (window)))
    {
      /* avoid updating the menu */
      lock_menu_updates = TRUE;

      /* clear the document history */
      mousepad_window_recent_clear (window);

      /* allow menu updates again */
      lock_menu_updates = FALSE;

      /* update the recent menu */
      mousepad_window_recent_menu (window);
    }
}



static void
mousepad_window_action_save_file (GtkAction      *action,
                                  MousepadWindow *window)
{
  MousepadDocument *document;

  document = window->active;
  if (G_LIKELY (document != NULL))
    {
      /* save the file */
      mousepad_window_save (window, document, FALSE);
    }
}



static void
mousepad_window_action_save_file_as (GtkAction      *action,
                                     MousepadWindow *window)
{
  MousepadDocument *document;

  document = window->active;
  if (G_LIKELY (document != NULL))
    {
      /* save the file */
      if (mousepad_window_save (window, document, TRUE))
        {
          /* make sure save-file is sensitive */
          action = gtk_action_group_get_action (window->window_actions, "save-file");
          gtk_action_set_sensitive (action, TRUE);
        }
    }
}



static void
mousepad_window_action_reload (GtkAction      *action,
                               MousepadWindow *window)
{
  MousepadDocument *document;
  GError           *error = NULL;
  const gchar      *message;
  gint              response = MOUSEPAD_RESPONSE_RELOAD;

  document = window->active;
  if (G_LIKELY (document != NULL))
    {
      /* ask what to do when the document still has modifications */
      if (mousepad_document_get_modified (document))
        response = mousepad_dialogs_ask_reload (GTK_WINDOW (window));

      switch (response)
        {
          case MOUSEPAD_RESPONSE_CANCEL:
            /* do nothing */
            break;

          case MOUSEPAD_RESPONSE_SAVE_AS:
            /* try to save the document, break when this went wrong, else
             * fall-though and try to reload the document */
            if (!mousepad_window_save (window, document, TRUE))
              break;

          case MOUSEPAD_RESPONSE_RELOAD:
            if (!mousepad_document_reload (document, &error))
              message = _("Failed to reload the document");
            break;
        }
    }

  if (G_UNLIKELY (error != NULL))
    {
      mousepad_dialogs_show_error (GTK_WINDOW (window), error, message);
      g_error_free (error);
    }
}



static void
mousepad_window_action_print (GtkAction      *action,
                              MousepadWindow *window)
{

}



static void
mousepad_window_action_close_tab (GtkAction      *action,
                                  MousepadWindow *window)
{
  MousepadDocument *document;

  document = window->active;
  if (G_LIKELY (document != NULL))
    mousepad_window_close_document (window, document);
}



static void
mousepad_window_action_close (GtkAction      *action,
                              MousepadWindow *window)
{
  gint       npages, i;
  GtkWidget *document;

  /* get the number of page in the notebook */
  npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook)) - 1;

  /* prevent menu updates */
  lock_menu_updates = TRUE;

  /* close all the tabs, one by one. the window will close
   * when all the tabs are closed */
  for (i = npages; i >= 0; --i)
    {
      document = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i);

      if (G_LIKELY (document != NULL))
        {
          /* switch to the tab we're going to close */
          gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), i);

          /* ask user what to do, break when he/she hits the cancel button */
          if (!mousepad_window_close_document (window, MOUSEPAD_DOCUMENT (document)))
            {
              /* allow updates again */
              lock_menu_updates = FALSE;

              /* update the go menu */
              mousepad_window_update_gomenu (window);

              break;
            }
        }
    }
}



static void
mousepad_window_action_close_all_windows (GtkAction      *action,
                                          MousepadWindow *window)
{
  MousepadApplication *application;
  GList               *windows, *li;

  /* query the list of currently open windows */
  application = mousepad_application_get ();
  windows = mousepad_application_get_windows (application);
  g_object_unref (G_OBJECT (application));

  /* close all open windows */
  for (li = windows; li != NULL; li = li->next)
    mousepad_window_action_close (NULL, MOUSEPAD_WINDOW (li->data));

  g_list_free (windows);
}



static void
mousepad_window_action_undo (GtkAction      *action,
                             MousepadWindow *window)
{
  if (G_LIKELY (window->active != NULL))
    mousepad_document_undo (window->active);
}



static void
mousepad_window_action_redo (GtkAction      *action,
                             MousepadWindow *window)
{
  if (G_LIKELY (window->active != NULL))
    mousepad_document_redo (window->active);
}



static void
mousepad_window_action_cut (GtkAction      *action,
                            MousepadWindow *window)
{
  if (G_LIKELY (window->active != NULL))
    mousepad_document_cut_selection (window->active);
}



static void
mousepad_window_action_copy (GtkAction      *action,
                             MousepadWindow *window)
{
  if (G_LIKELY (window->active != NULL))
    mousepad_document_copy_selection (window->active);
}



static void
mousepad_window_action_paste (GtkAction      *action,
                              MousepadWindow *window)
{
  if (G_LIKELY (window->active != NULL))
    mousepad_document_paste_clipboard (window->active);
}



static void
mousepad_window_action_delete (GtkAction      *action,
                               MousepadWindow *window)
{
  if (G_LIKELY (window->active != NULL))
    mousepad_document_delete_selection (window->active);
}



static void
mousepad_window_action_select_all (GtkAction      *action,
                                   MousepadWindow *window)
{
  if (G_LIKELY (window->active != NULL))
    mousepad_document_select_all (window->active);
}



static void
mousepad_window_hide_search_bar (MousepadWindow *window)
{
 _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* hide the search bar */
  gtk_widget_hide (window->search_bar);
  gtk_table_set_row_spacing (GTK_TABLE (window->table), 3, 0);

  /* focus the active document's text view */
  if (G_LIKELY (window->active))
    mousepad_document_focus_textview (window->active);
}



static gboolean
mousepad_window_find_string (MousepadWindow      *window,
                             const gchar         *string,
                             MousepadSearchFlags  flags)
{
  gboolean found;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);

  found = mousepad_document_find (window->active, string, flags);

  return found;
}

static gboolean
mousepad_window_highlight_all (MousepadWindow      *window,
                               const gchar         *string,
                               MousepadSearchFlags  flags)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);

  /* hightlight all the occurences in the active document */
  mousepad_document_highlight_all (window->active, string, flags);

  return FALSE;
}

static void
mousepad_window_action_find (GtkAction      *action,
                             MousepadWindow *window)
{
  if (G_UNLIKELY (window->search_bar == NULL))
    {
      /* create a new toolbar */
      window->search_bar = mousepad_search_bar_new ();
      gtk_table_attach (GTK_TABLE (window->table), window->search_bar, 0, 1, 4, 5, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

      /* connect signals to the search bar */
      g_signal_connect_swapped (G_OBJECT (window->search_bar), "hide-bar", G_CALLBACK (mousepad_window_hide_search_bar), window);
      g_signal_connect_swapped (G_OBJECT (window->search_bar), "find-string", G_CALLBACK (mousepad_window_find_string), window);
      g_signal_connect_swapped (G_OBJECT (window->search_bar), "highlight-all", G_CALLBACK (mousepad_window_highlight_all), window);
    }

  /* show the search bar and give some space to the table */
  gtk_widget_show (window->search_bar);
  gtk_table_set_row_spacing (GTK_TABLE (window->table), 3, WINDOW_SPACING);

  /* focus the search entry */
  mousepad_search_bar_focus (MOUSEPAD_SEARCH_BAR (window->search_bar));
}



static void
mousepad_window_action_find_next (GtkAction      *action,
                                  MousepadWindow *window)
{
  /* only find the next occurence when the search bar is initialized */
  if (G_LIKELY (window->search_bar != NULL))
    mousepad_search_bar_find_next (MOUSEPAD_SEARCH_BAR (window->search_bar));
}



static void
mousepad_window_action_find_previous (GtkAction      *action,
                                      MousepadWindow *window)
{
  /* only find the previous occurence when the search bar is initialized */
  if (G_LIKELY (window->search_bar != NULL))
    mousepad_search_bar_find_previous (MOUSEPAD_SEARCH_BAR (window->search_bar));
}



static void
mousepad_window_action_replace (GtkAction      *action,
                                MousepadWindow *window)
{

}



static void
mousepad_window_action_jump_to (GtkAction      *action,
                                MousepadWindow *window)
{
  MousepadDocument *document;
  gint              current_line, last_line, line;

  /* get the active document */
  document = window->active;
  if (G_LIKELY (document != NULL))
    {
      /* get the current and last line number */
      mousepad_document_line_numbers (document, &current_line, &last_line);

      /* run the jump to dialog and wait for the response */
      line = mousepad_dialogs_jump_to (GTK_WINDOW (window), current_line, last_line);

      if (G_LIKELY (line > 0))
        mousepad_document_jump_to_line (document, line);
    }
}



static void
mousepad_window_action_select_font (GtkAction      *action,
                                    MousepadWindow *window)
{
  GtkWidget *dialog;
  GtkWidget *document;
  gchar     *font_name;
  guint      npages, i;

  dialog = gtk_font_selection_dialog_new (_("Choose Mousepad Font"));
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));

  /* get the current font */
  g_object_get (G_OBJECT (window->preferences), "font-name", &font_name, NULL);

  if (G_LIKELY (font_name))
    {
      gtk_font_selection_dialog_set_font_name (GTK_FONT_SELECTION_DIALOG (dialog), font_name);
      g_free (font_name);
    }

  /* run the dialog */
  if (G_LIKELY (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK))
    {
      /* send the new font to the preferences */
      font_name = gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG (dialog));
      g_object_set (G_OBJECT (window->preferences), "font-name", font_name, NULL);

      /* send the new font to all tabs in this window */
      npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));
      for (i = 0; i < npages; i++)
        {
          document = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i);
          if (G_LIKELY (document != NULL))
            mousepad_document_set_font (MOUSEPAD_DOCUMENT (document), font_name);
        }

      /* cleanup */
      g_free (font_name);
    }

  /* destroy dialog */
  gtk_widget_destroy (dialog);
}



static void
mousepad_window_action_statusbar (GtkToggleAction *action,
                                  MousepadWindow  *window)
{
  gboolean          active;
  MousepadDocument *document;

  /* determine the new state of the action */
  active = gtk_toggle_action_get_active (action);

  /* check if we should drop the statusbar */
  if (!active && window->statusbar != NULL)
    {
      /* just get rid of the statusbar */
      gtk_widget_destroy (window->statusbar);
      window->statusbar = NULL;
    }
  else if (active && window->statusbar == NULL)
    {
      /* setup a new statusbar */
      window->statusbar = mousepad_statusbar_new ();
      gtk_table_attach (GTK_TABLE (window->table), window->statusbar, 0, 1, 5, 6, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
      gtk_widget_show (window->statusbar);

      /* overwrite toggle signal */
      g_signal_connect_swapped (G_OBJECT (window->statusbar), "enable-overwrite",
                                G_CALLBACK (mousepad_window_toggle_overwrite), window);

      /* set the statsbar text */
      document = window->active;
      if (document != NULL)
        mousepad_document_send_statusbar_signals (document);
    }

  /* set the spacing above the statusbar */
  gtk_table_set_row_spacing (GTK_TABLE (window->table), 4, active ? WINDOW_SPACING : 0);

  /* remember the setting */
  g_object_set (G_OBJECT (window->preferences), "last-statusbar-visible", active, NULL);
}



static void
mousepad_window_action_word_wrap (GtkToggleAction *action,
                                  MousepadWindow  *window)
{
  gboolean word_wrap;

  word_wrap = gtk_toggle_action_get_active (action);

  /* store this as the last used wrap mode */
  g_object_set (G_OBJECT (window->preferences), "word-wrap", word_wrap, NULL);

  /* set the wrapping mode of the current document */
  if (G_LIKELY (window->active != NULL))
    mousepad_document_set_word_wrap (window->active, word_wrap);
}



static void
mousepad_window_action_line_numbers (GtkToggleAction *action,
                                     MousepadWindow  *window)
{
  gboolean line_numbers;

  line_numbers = gtk_toggle_action_get_active (action);

  if (G_LIKELY (window->active != NULL))
    mousepad_document_set_line_numbers (window->active, line_numbers);
}



static void
mousepad_window_action_auto_indent (GtkToggleAction *action,
                                    MousepadWindow  *window)
{
  gboolean auto_indent;

  auto_indent = gtk_toggle_action_get_active (action);

  if (G_LIKELY (window->active != NULL))
    mousepad_document_set_auto_indent (window->active, auto_indent);
}



static void
mousepad_window_action_prev_tab (GtkAction      *action,
                                 MousepadWindow *window)
{
  gint page_num;
  gint n_pages;

  /* get notebook info */
  page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));
  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

  /* switch to the previous tab or cycle to the last tab */
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook),
                                 (page_num - 1) % n_pages);
}



static void
mousepad_window_action_next_tab (GtkAction      *action,
                                 MousepadWindow *window)
{
  gint page_num;
  gint n_pages;

  /* get notebook info */
  page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));
  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

  /* switch to the next tab or cycle to the first tab */
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook),
                                 (page_num + 1) % n_pages);
}



static void
mousepad_window_action_goto (GtkRadioAction *action,
                             GtkNotebook    *notebook)
{
  /* get the page number from the action value */
  gint page = gtk_radio_action_get_current_value (action);

  /* set the page */
  gtk_notebook_set_current_page (notebook, page);
}



static void
mousepad_window_action_about (GtkAction      *action,
                              MousepadWindow *window)
{
  mousepad_dialogs_show_about (GTK_WINDOW (window));
}



/**
 * Miscellaneous Actions
 **/
static void
mousepad_window_button_close_tab (MousepadDocument *document,
                                  MousepadWindow   *window)
{
  gint page_num;

  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* switch to the tab we're going to close */
  page_num = gtk_notebook_page_num (GTK_NOTEBOOK (window->notebook), GTK_WIDGET (document));
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), page_num);

  /* close the document */
  mousepad_window_close_document (window, document);
}



static gboolean
mousepad_window_delete_event (MousepadWindow *window,
                              GdkEvent       *event)
{
  /* try to close the windows */
  mousepad_window_action_close (NULL, window);

  /* we will close the window when all the tabs are closed */
  return TRUE;
}



void
mousepad_gtk_set_tooltip (GtkWidget   *widget,
                          const gchar *tooltip)
{
  static GtkTooltips *tooltips = NULL;

  _mousepad_return_if_fail (GTK_IS_WIDGET (widget));

  /* allocate the shared tooltips on-demand */
  if (G_UNLIKELY (tooltips == NULL))
    tooltips = gtk_tooltips_new ();

  /* setup the tooltip for the widget */
  gtk_tooltips_set_tip (tooltips, widget, tooltip, NULL);
}

