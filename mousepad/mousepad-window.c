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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <glib/gstdio.h>

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-application.h>
#include <mousepad/mousepad-marshal.h>
#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-dialogs.h>
#include <mousepad/mousepad-preferences.h>
#include <mousepad/mousepad-replace-dialog.h>
#include <mousepad/mousepad-encoding-dialog.h>
#include <mousepad/mousepad-search-bar.h>
#include <mousepad/mousepad-statusbar.h>
#include <mousepad/mousepad-print.h>
#include <mousepad/mousepad-window.h>
#include <mousepad/mousepad-window-ui.h>



#define WINDOW_SPACING 3

#if GTK_CHECK_VERSION (2,12,0)
static gpointer NOTEBOOK_GROUP = "Mousepad";
#endif



enum
{
  NEW_WINDOW,
  NEW_WINDOW_WITH_DOCUMENT,
  HAS_DOCUMENTS,
  LAST_SIGNAL,
};



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
static gboolean          mousepad_window_open_file                    (MousepadWindow         *window,
                                                                       const gchar            *filename,
                                                                       const gchar            *encoding);
static gboolean          mousepad_window_close_document               (MousepadWindow         *window,
                                                                       MousepadDocument       *document);
static void              mousepad_window_set_title                    (MousepadWindow         *window);
static void              mousepad_window_toggle_overwrite             (MousepadWindow         *window,
                                                                       gboolean                overwrite);

/* notebook signals */
static void              mousepad_window_notebook_notified            (GtkNotebook            *notebook,
                                                                       GParamSpec             *pspec,
                                                                       MousepadWindow         *window);
static void              mousepad_window_notebook_reordered           (GtkNotebook            *notebook,
                                                                       GtkWidget              *page,
                                                                       guint                   page_num,
                                                                       MousepadWindow         *window);
static void              mousepad_window_notebook_added               (GtkNotebook            *notebook,
                                                                       GtkWidget              *page,
                                                                       guint                   page_num,
                                                                       MousepadWindow         *window);
static void              mousepad_window_notebook_removed             (GtkNotebook            *notebook,
                                                                       GtkWidget              *page,
                                                                       guint                   page_num,
                                                                       MousepadWindow         *window);
static void              mousepad_window_notebook_menu_position       (GtkMenu                *menu,
                                                                       gint                   *x,
                                                                       gint                   *y,
                                                                       gboolean               *push_in,
                                                                       gpointer                user_data);
static gboolean          mousepad_window_notebook_button_press_event  (GtkNotebook            *notebook,
                                                                       GdkEventButton         *event,
                                                                       MousepadWindow         *window);
#if GTK_CHECK_VERSION (2,12,0)
static GtkNotebook      *mousepad_window_notebook_create_window       (GtkNotebook            *notebook,
                                                                       GtkWidget              *page,
                                                                       gint                    x,
                                                                       gint                    y,
                                                                       MousepadWindow         *window);
#endif

/* document signals */
static void              mousepad_window_modified_changed             (MousepadWindow         *window);
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

/* menu functions */
static void              mousepad_window_menu_tab_sizes               (MousepadWindow         *window);
static void              mousepad_window_menu_tab_sizes_update        (MousepadWindow         *window);
static void              mousepad_window_update_actions               (MousepadWindow         *window);
static gboolean          mousepad_window_update_gomenu_idle           (gpointer                user_data);
static void              mousepad_window_update_gomenu_idle_destroy   (gpointer                user_data);
static void              mousepad_window_update_gomenu                (MousepadWindow         *window);

/* recent functions */
static void              mousepad_window_recent_add                   (MousepadWindow         *window,
                                                                       MousepadFile           *file);
static gchar            *mousepad_window_recent_escape_underscores    (const gchar            *str);
static gint              mousepad_window_recent_sort                  (GtkRecentInfo          *a,
                                                                       GtkRecentInfo          *b);
static void              mousepad_window_recent_manager_init          (MousepadWindow         *window);
static gboolean          mousepad_window_recent_menu_idle             (gpointer                user_data);
static void              mousepad_window_recent_menu_idle_destroy     (gpointer                user_data);
static void              mousepad_window_recent_menu                  (MousepadWindow         *window);
static void              mousepad_window_recent_clear                 (MousepadWindow         *window);

/* dnd */
static void              mousepad_window_drag_data_received           (GtkWidget              *widget,
                                                                       GdkDragContext         *context,
                                                                       gint                    x,
                                                                       gint                    y,
                                                                       GtkSelectionData       *selection_data,
                                                                       guint                   info,
                                                                       guint                   time,
                                                                       MousepadWindow         *window);

/* search bar */
static void              mousepad_window_hide_search_bar              (MousepadWindow         *window);

/* actions */
static void              mousepad_window_action_open_new_tab          (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_open_new_window       (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_open_file             (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_open_recent           (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_clear_recent          (GtkAction              *action,
                                                                       MousepadWindow         *window);
static gboolean          mousepad_window_action_save_file             (GtkAction              *action,
                                                                       MousepadWindow         *window);
static gboolean          mousepad_window_action_save_file_as          (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_reload                (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_print                 (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_detach                (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_close_tab             (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_close                 (GtkAction              *action,
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
static void              mousepad_window_action_paste_column          (GtkAction              *action,
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
static void              mousepad_window_action_select_font           (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_statusbar             (GtkToggleAction        *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_line_numbers          (GtkToggleAction        *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_auto_indent           (GtkToggleAction        *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_word_wrap             (GtkToggleAction        *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_tab_size              (GtkToggleAction        *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_insert_spaces         (GtkToggleAction        *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_prev_tab              (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_next_tab              (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_goto                  (GtkRadioAction         *action,
                                                                       GtkNotebook            *notebook);
static void              mousepad_window_action_go_to_line            (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_contents              (GtkAction              *action,
                                                                       MousepadWindow         *window);
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

  /* action group */
  GtkActionGroup      *action_group;

  /* recent manager */
  GtkRecentManager    *recent_manager;

  /* UI manager */
  GtkUIManager        *ui_manager;
  guint                gomenu_merge_id;
  guint                recent_merge_id;

  /* main window widgets */
  GtkWidget           *table;
  GtkWidget           *notebook;
  GtkWidget           *container;
  GtkWidget           *search_bar;
  GtkWidget           *statusbar;
  GtkWidget           *replace_dialog;

  /* support to remember window geometry */
  guint                save_geometry_timer_id;

  /* idle update functions for the recent and go menu */
  guint                update_recent_menu_id;
  guint                update_go_menu_id;
};



static const GtkActionEntry action_entries[] =
{
  { "file-menu", NULL, N_("_File"), NULL, NULL, NULL, },
    { "new-tab", "tab-new", N_("New Documen_t"), "<control>T", N_("Create a new document"), G_CALLBACK (mousepad_window_action_open_new_tab), },
    { "new-window", "window-new", N_("_New Window"), "<control>N", N_("Create a new document in a new window"), G_CALLBACK (mousepad_window_action_open_new_window), },
    { "open-file", GTK_STOCK_OPEN, N_("_Open File"), NULL, N_("Open a file"), G_CALLBACK (mousepad_window_action_open_file), },
    { "recent-menu", NULL, N_("Open _Recent"), NULL, NULL, NULL, },
      { "no-recent-items", NULL, N_("No items found"), NULL, NULL, NULL, },
      { "clear-recent", GTK_STOCK_CLEAR, N_("Clear _History"), NULL, N_("Clear the recently used files history"), G_CALLBACK (mousepad_window_action_clear_recent), },
    { "save-file", GTK_STOCK_SAVE, NULL, NULL, N_("Save the current file"), G_CALLBACK (mousepad_window_action_save_file), },
    { "save-file-as", GTK_STOCK_SAVE_AS, NULL, NULL, N_("Save current document as another file"), G_CALLBACK (mousepad_window_action_save_file_as), },
    { "reload", GTK_STOCK_REFRESH, N_("Re_load"), NULL, N_("Reload this document."), G_CALLBACK (mousepad_window_action_reload), },
    { "print-document", GTK_STOCK_PRINT, NULL, "<control>P", N_("Prin the current page"), G_CALLBACK (mousepad_window_action_print), },
    { "detach-tab", NULL, N_("_Detach Tab"), "<control>D", N_("Move the current document to a new window"), G_CALLBACK (mousepad_window_action_detach), },
    { "close-tab", GTK_STOCK_CLOSE, N_("C_lose Tab"), "<control>W", N_("Close the current file"), G_CALLBACK (mousepad_window_action_close_tab), },
    { "close-window", GTK_STOCK_QUIT, N_("_Close Window"), "<control>Q", N_("Quit the program"), G_CALLBACK (mousepad_window_action_close), },

  { "edit-menu", NULL, N_("_Edit"), NULL, NULL, NULL, },
    { "undo", GTK_STOCK_UNDO, NULL, "<control>Z", N_("Undo the last action"), G_CALLBACK (mousepad_window_action_undo), },
    { "redo", GTK_STOCK_REDO, NULL, "<control>Y", N_("Redo the last undone action"), G_CALLBACK (mousepad_window_action_redo), },
    { "cut", GTK_STOCK_CUT, NULL, NULL, N_("Cut the selection"), G_CALLBACK (mousepad_window_action_cut), },
    { "copy", GTK_STOCK_COPY, NULL, NULL, N_("Copy the selection"), G_CALLBACK (mousepad_window_action_copy), },
    { "paste", GTK_STOCK_PASTE, NULL, NULL, N_("Paste the clipboard"), G_CALLBACK (mousepad_window_action_paste), },
    { "paste-column", GTK_STOCK_PASTE, N_("Paste _Column"), "<control><shift>V", N_("Paste the clipboard text in a clumn"), G_CALLBACK (mousepad_window_action_paste_column), },
    { "delete", GTK_STOCK_DELETE, NULL, NULL, N_("Delete the selected text"), G_CALLBACK (mousepad_window_action_delete), },
    { "select-all", GTK_STOCK_SELECT_ALL, NULL, NULL, N_("Select the entire document"), G_CALLBACK (mousepad_window_action_select_all), },

  { "search-menu", NULL, N_("_Search"), NULL, NULL, NULL, },
    { "find", GTK_STOCK_FIND, NULL, NULL, N_("Search for text"), G_CALLBACK (mousepad_window_action_find), },
    { "find-next", NULL, N_("Find _Next"), NULL, N_("Search forwards for the same text"), G_CALLBACK (mousepad_window_action_find_next), },
    { "find-previous", NULL, N_("Find _Previous"), NULL, N_("Search backwards for the same text"), G_CALLBACK (mousepad_window_action_find_previous), },
    { "replace", GTK_STOCK_FIND_AND_REPLACE, NULL, NULL, N_("Search for and replace text"), G_CALLBACK (mousepad_window_action_replace), },

  { "view-menu", NULL, N_("_View"), NULL, NULL, NULL, },
    { "font", GTK_STOCK_SELECT_FONT, NULL, NULL, N_("Change the editor font"), G_CALLBACK (mousepad_window_action_select_font), },

  { "document-menu", NULL, N_("_Document"), NULL, NULL, NULL, },
    { "tab-size-menu", NULL, N_("_Tab Size"), NULL, NULL, NULL, },

  { "navigation-menu", NULL, N_("_Navigation"), NULL, },
    { "back", GTK_STOCK_GO_BACK, NULL, NULL, N_("Select the previous tab"), G_CALLBACK (mousepad_window_action_prev_tab), },
    { "forward", GTK_STOCK_GO_FORWARD, NULL, NULL, N_("Select the next tab"), G_CALLBACK (mousepad_window_action_next_tab), },
    { "go-to-line", GTK_STOCK_JUMP_TO, N_("_Go to line..."), NULL, N_("Go to a specific line"), G_CALLBACK (mousepad_window_action_go_to_line), },

  { "help-menu", NULL, N_("_Help"), NULL, },
    { "contents", GTK_STOCK_HELP, N_ ("_Contents"), "F1", N_("Display the Mousepad user manual"), G_CALLBACK (mousepad_window_action_contents), },
    { "about", GTK_STOCK_ABOUT, NULL, NULL, N_("About this application"), G_CALLBACK (mousepad_window_action_about), },
};

static const GtkToggleActionEntry toggle_action_entries[] =
{
  { "statusbar", NULL, N_("_Statusbar"), NULL, N_("Change the visibility of the statusbar"), G_CALLBACK (mousepad_window_action_statusbar), FALSE, },
  { "line-numbers", NULL, N_("_Line Numbers"), NULL, N_("Show line numbers"), G_CALLBACK (mousepad_window_action_line_numbers), FALSE, },
  { "auto-indent", NULL, N_("_Auto Indent"), NULL, N_("Auto indent a new line"), G_CALLBACK (mousepad_window_action_auto_indent), FALSE, },
  { "word-wrap", NULL, N_("_Word Wrap"), NULL, N_("Toggle breaking lines in between words"), G_CALLBACK (mousepad_window_action_word_wrap), FALSE, },
  { "insert-spaces", NULL, N_("_Insert Spaces"), NULL, N_("Insert spaces when the tab button is pressed"), G_CALLBACK (mousepad_window_action_insert_spaces), FALSE, },
};



static GObjectClass *mousepad_window_parent_class;
static guint         window_signals[LAST_SIGNAL];
static gint          lock_menu_updates = 0;



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

  window_signals[NEW_WINDOW] =
    g_signal_new (I_("new-window"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_NO_HOOKS,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  window_signals[NEW_WINDOW_WITH_DOCUMENT] =
    g_signal_new (I_("new-window-with-document"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_NO_HOOKS,
                  0, NULL, NULL,
                  _mousepad_marshal_VOID__OBJECT_INT_INT,
                  G_TYPE_NONE, 3,
                  G_TYPE_OBJECT,
                  G_TYPE_INT, G_TYPE_INT);

  window_signals[HAS_DOCUMENTS] =
    g_signal_new (I_("has-documents"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_NO_HOOKS,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE, 1,
                  G_TYPE_BOOLEAN);
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
  gboolean       statusbar_visible;

  /* initialize stuff */
  window->save_geometry_timer_id = 0;
  window->update_recent_menu_id = 0;
  window->update_go_menu_id = 0;
  window->gomenu_merge_id = 0;
  window->recent_merge_id = 0;
  window->search_bar = NULL;
  window->statusbar = NULL;
  window->replace_dialog = NULL;
  window->active = NULL;
  window->recent_manager = NULL;

  /* add the preferences to the window */
  window->preferences = mousepad_preferences_get ();

  /* allocate a closure for the menu_item_selected() callback */
  window->menu_item_selected_closure = g_cclosure_new_object (G_CALLBACK (mousepad_window_menu_item_selected), G_OBJECT (window));
  g_closure_ref (window->menu_item_selected_closure);
  g_closure_sink (window->menu_item_selected_closure);

  /* signal for handling the window delete event */
  g_signal_connect (G_OBJECT (window), "delete-event", G_CALLBACK (mousepad_window_delete_event), NULL);

  /* allocate a closure for the menu_item_deselected() callback */
  window->menu_item_deselected_closure = g_cclosure_new_object (G_CALLBACK (mousepad_window_menu_item_deselected), G_OBJECT (window));
  g_closure_ref (window->menu_item_deselected_closure);
  g_closure_sink (window->menu_item_deselected_closure);

  /* read settings from the preferences */
  g_object_get (G_OBJECT (window->preferences),
                "window-width", &width,
                "window-height", &height,
                "window-statusbar-visible", &statusbar_visible,
                NULL);

  /* set the default window size */
  gtk_window_set_default_size (GTK_WINDOW (window), width, height);

  /* the action group for this window */
  window->action_group = gtk_action_group_new ("MousepadWindow");
  gtk_action_group_set_translation_domain (window->action_group, GETTEXT_PACKAGE);
  gtk_action_group_add_actions (window->action_group, action_entries, G_N_ELEMENTS (action_entries), GTK_WIDGET (window));
  gtk_action_group_add_toggle_actions (window->action_group, toggle_action_entries, G_N_ELEMENTS (toggle_action_entries), GTK_WIDGET (window));

  /* create the ui manager and connect proxy signals for the statusbar */
  window->ui_manager = gtk_ui_manager_new ();
  g_signal_connect (G_OBJECT (window->ui_manager), "connect-proxy", G_CALLBACK (mousepad_window_connect_proxy), window);
  g_signal_connect (G_OBJECT (window->ui_manager), "disconnect-proxy", G_CALLBACK (mousepad_window_disconnect_proxy), window);
  gtk_ui_manager_insert_action_group (window->ui_manager, window->action_group, 0);
  gtk_ui_manager_add_ui_from_string (window->ui_manager, mousepad_window_ui, mousepad_window_ui_length, NULL);

  /* create the recent menu (idle) */
  mousepad_window_recent_menu (window);

  /* add tab size menu */
  mousepad_window_menu_tab_sizes (window);

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
      gtk_rc_parse_string ("style\"mousepad-window-root-style\"\n"
                           "  {\n"
                           "    bg[NORMAL]=\"#b4254b\"\n"
                           "    fg[NORMAL]=\"#fefefe\"\n"
                           "  }\n"
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

  /* set the group id */
#if GTK_CHECK_VERSION (2,12,0)
  gtk_notebook_set_group (GTK_NOTEBOOK (window->notebook), NOTEBOOK_GROUP);
#else
  gtk_notebook_set_group_id (GTK_NOTEBOOK (window->notebook), 1337);
#endif

  /* connect signals to the notebooks */
  g_signal_connect (G_OBJECT (window->notebook), "notify::page", G_CALLBACK (mousepad_window_notebook_notified), window);
  g_signal_connect (G_OBJECT (window->notebook), "page-reordered", G_CALLBACK (mousepad_window_notebook_reordered), window);
  g_signal_connect (G_OBJECT (window->notebook), "page-added", G_CALLBACK (mousepad_window_notebook_added), window);
  g_signal_connect (G_OBJECT (window->notebook), "page-removed", G_CALLBACK (mousepad_window_notebook_removed), window);
  g_signal_connect (G_OBJECT (window->notebook), "button-press-event", G_CALLBACK (mousepad_window_notebook_button_press_event), window);
#if GTK_CHECK_VERSION (2,12,0)
  g_signal_connect (G_OBJECT (window->notebook), "create-window", G_CALLBACK (mousepad_window_notebook_create_window), window);
#endif

  /* append and show the notebook */
  gtk_table_attach (GTK_TABLE (window->table), window->notebook, 0, 1, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_table_set_row_spacing (GTK_TABLE (window->table), 2, WINDOW_SPACING);
  gtk_widget_show (window->notebook);

  /* check if we should display the statusbar by default */
  action = gtk_action_group_get_action (window->action_group, "statusbar");
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), statusbar_visible);

  /* allow drops in the window */
  gtk_drag_dest_set (GTK_WIDGET (window), GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP, drop_targets, G_N_ELEMENTS (drop_targets), GDK_ACTION_COPY | GDK_ACTION_MOVE);
  g_signal_connect (G_OBJECT (window), "drag-data-received", G_CALLBACK (mousepad_window_drag_data_received), window);
}



static void
mousepad_window_dispose (GObject *object)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (object);

  /* disconnect recent manager signal */
  if (G_LIKELY (window->recent_manager))
    g_signal_handlers_disconnect_by_func (G_OBJECT (window->recent_manager), mousepad_window_recent_menu, window);

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

  /* release the action group */
  g_object_unref (G_OBJECT (window->action_group));

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

  /* let gtk+ handle the configure event */
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
  gint       id;

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
              id = gtk_statusbar_get_context_id (GTK_STATUSBAR (window->statusbar), "tooltip");
              gtk_statusbar_push (GTK_STATUSBAR (window->statusbar), id, tooltip);

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
  gint id;

  /* we can only undisplay tooltips if we have a statusbar */
  if (G_LIKELY (window->statusbar != NULL))
    {
      /* drop the last tooltip from the statusbar */
      id = gtk_statusbar_get_context_id (GTK_STATUSBAR (window->statusbar), "tooltip");
      gtk_statusbar_pop (GTK_STATUSBAR (window->statusbar), id);
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
                            "window-width", width,
                            "window-height", height, NULL);
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
static gboolean
mousepad_window_open_file (MousepadWindow *window,
                           const gchar    *filename,
                           const gchar    *encoding)
{
  MousepadDocument *document;
  GError           *error = NULL;
  gboolean          succeed = FALSE;
  gint              npages = 0, i;
  gint              response;
  const gchar      *new_encoding;
  const gchar      *opened_filename;
  GtkWidget        *dialog;

  /* get the number of page in the notebook */
  if (filename)
    npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

  /* walk though the tabs */
  for (i = 0; i < npages; i++)
    {
      document = MOUSEPAD_DOCUMENT (gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i));

      if (G_LIKELY (document))
        {
          /* get the filename */
          opened_filename = mousepad_file_get_filename (MOUSEPAD_DOCUMENT (document)->file);

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
    {
      /* set the filename */
      mousepad_file_set_filename (document->file, filename);

      /* set the passed encoding */
      mousepad_file_set_encoding (document->file, encoding);

      try_open_again:

      /* lock the undo manager */
      mousepad_undo_lock (document->undo);

      /* read the content into the buffer */
      succeed = mousepad_file_open (document->file, &error);

      /* release the lock */
      mousepad_undo_unlock (document->undo);

      if (G_LIKELY (succeed))
        {
          /* add the document to the notebook and connect some signals */
          mousepad_window_add (window, document);

          /* add to the recent history */
          mousepad_window_recent_add (window, document->file);
        }
      else if (error->domain == G_CONVERT_ERROR)
        {
          /* clear the error */
          g_clear_error (&error);

          /* run the encoding dialog */
          dialog = mousepad_encoding_dialog_new (GTK_WINDOW (window), document->file);

          /* run the dialog */
          response = gtk_dialog_run (GTK_DIALOG (dialog));

          if (response == GTK_RESPONSE_OK)
            {
              /* get the selected encoding */
              new_encoding = mousepad_encoding_dialog_get_encoding (MOUSEPAD_ENCODING_DIALOG (dialog));

              /* set the document encoding */
              mousepad_file_set_encoding (document->file, new_encoding);
            }

          /* destroy the dialog */
          gtk_widget_destroy (dialog);

          /* handle */
          if (response == GTK_RESPONSE_OK)
            goto try_open_again;
          else
            goto opening_failed;
        }
      else
        {
          opening_failed:

          /* something went wrong, remove the document */
          gtk_widget_destroy (GTK_WIDGET (document));

          if (error)
            {
              /* show the warning */
              mousepad_dialogs_show_error (GTK_WINDOW (window), error, _("Failed to open file"));

              g_error_free (error);
            }
        }
    }
  else
    {
      /* no filename, simple add the window and succeed */
      mousepad_window_add (window, document);

      succeed = TRUE;
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
  _mousepad_return_val_if_fail (filenames != NULL, FALSE);
  _mousepad_return_val_if_fail (*filenames != NULL, FALSE);

  /* block menu updates */
  lock_menu_updates++;

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
      mousepad_window_open_file (window, filename ? filename : filenames[n], NULL);

      /* cleanup */
      g_free (filename);
      filename = NULL;
    }

  /* allow menu updates again */
  lock_menu_updates--;

  /* check if the window contains tabs */
  npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));
  if (G_UNLIKELY (npages == 0))
    return FALSE;

  /* update the menus */
  mousepad_window_recent_menu (window);
  mousepad_window_update_gomenu (window);

  return TRUE;
}



void
mousepad_window_add (MousepadWindow   *window,
                     MousepadDocument *document)
{
  GtkWidget        *label;
  gint              page;
  MousepadDocument *prev_active;

  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  _mousepad_return_if_fail (GTK_IS_NOTEBOOK (window->notebook));

  /* create the tab label */
  label = mousepad_document_get_tab_label (document);

  /* get active page */
  page = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));

  /* insert the page right of the active tab */
  page = gtk_notebook_insert_page (GTK_NOTEBOOK (window->notebook), GTK_WIDGET (document), label, page + 1);

  /* allow tab reordering and detaching */
  gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (window->notebook), GTK_WIDGET (document), TRUE);
  gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (window->notebook), GTK_WIDGET (document), TRUE);

  /* show the document */
  gtk_widget_show (GTK_WIDGET (document));

  /* get the active tab before we switch to the new one */
  prev_active = window->active;

  /* switch to the new tab */
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), page);

  /* make sure the textview is focused in the new document */
  mousepad_document_focus_textview (document);

  /* destroy the previous tab if it was not modified, untitled and the new tab is not untitled */
  if (prev_active != NULL
      && gtk_text_buffer_get_modified (prev_active->buffer) == FALSE
      && mousepad_file_get_filename (prev_active->file) == NULL
      && mousepad_file_get_filename (document->file) != NULL)
    gtk_widget_destroy (GTK_WIDGET (prev_active));
}



static gboolean
mousepad_window_close_document (MousepadWindow   *window,
                                MousepadDocument *document)
{
  gboolean succeed = FALSE;
  gint     response;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);
  _mousepad_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), FALSE);

  /* check if the document has been modified */
  if (gtk_text_buffer_get_modified (document->buffer))
    {
      /* run save changes dialog */
      response = mousepad_dialogs_save_changes (GTK_WINDOW (window));

      switch (response)
        {
          case MOUSEPAD_RESPONSE_DONT_SAVE:
            /* don't save, only destroy the document */
            succeed = TRUE;
            break;

          case MOUSEPAD_RESPONSE_CANCEL:
            /* do nothing */
            break;

          case MOUSEPAD_RESPONSE_SAVE:
            succeed = mousepad_window_action_save_file (NULL, window);
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
mousepad_window_set_title (MousepadWindow *window)
{
  gchar            *string;
  const gchar      *title;
  gboolean          show_full_path;
  MousepadDocument *document = window->active;

  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* whether to show the full path */
  g_object_get (G_OBJECT (window->preferences), "misc-path-in-title", &show_full_path, NULL);

  /* name we display in the title */
  if (G_UNLIKELY (show_full_path && mousepad_document_get_filename (document)))
    title = mousepad_document_get_filename (document);
  else
    title = mousepad_document_get_basename (document);

  /* build the title */
  if (G_UNLIKELY (mousepad_file_get_read_only (document->file)))
    string = g_strdup_printf ("%s [%s] - %s", title, _("Read Only"), PACKAGE_NAME);
  else
    string = g_strdup_printf ("%s%s - %s", gtk_text_buffer_get_modified (document->buffer) ? "*" : "", title, PACKAGE_NAME);

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
mousepad_window_notebook_notified (GtkNotebook    *notebook,
                                   GParamSpec     *pspec,
                                   MousepadWindow *window)
{
  gint page_num;

  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* get the current page */
  page_num = gtk_notebook_get_current_page (notebook);

  /* get the new active document */
  if (G_LIKELY (page_num != -1))
    window->active = MOUSEPAD_DOCUMENT (gtk_notebook_get_nth_page (notebook, page_num));
  else
    g_assert_not_reached ();

  /* set the window title */
  mousepad_window_set_title (window);

  /* update the menu actions */
  mousepad_window_update_actions (window);

  /* update the statusbar */
  mousepad_document_send_statusbar_signals (window->active);
}



static void
mousepad_window_notebook_reordered (GtkNotebook     *notebook,
                                    GtkWidget       *page,
                                    guint            page_num,
                                    MousepadWindow  *window)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (page));

  /* update the go menu */
  mousepad_window_update_gomenu (window);
}



static void
mousepad_window_notebook_added (GtkNotebook     *notebook,
                                GtkWidget       *page,
                                guint            page_num,
                                MousepadWindow  *window)
{
  MousepadDocument *document = MOUSEPAD_DOCUMENT (page);
  gboolean          always_show_tabs;
  gint              npages;

  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* connect signals to the document for this window */
  g_signal_connect (G_OBJECT (page), "close-tab", G_CALLBACK (mousepad_window_button_close_tab), window);
  g_signal_connect (G_OBJECT (page), "selection-changed", G_CALLBACK (mousepad_window_selection_changed), window);
  g_signal_connect (G_OBJECT (page), "cursor-changed", G_CALLBACK (mousepad_window_cursor_changed), window);
  g_signal_connect (G_OBJECT (page), "overwrite-changed", G_CALLBACK (mousepad_window_overwrite_changed), window);
  g_signal_connect (G_OBJECT (page), "drag-data-received", G_CALLBACK (mousepad_window_drag_data_received), window);
  g_signal_connect_swapped (G_OBJECT (document->undo), "can-undo", G_CALLBACK (mousepad_window_can_undo), window);
  g_signal_connect_swapped (G_OBJECT (document->undo), "can-redo", G_CALLBACK (mousepad_window_can_redo), window);
  g_signal_connect_swapped (G_OBJECT (document->buffer), "modified-changed", G_CALLBACK (mousepad_window_modified_changed), window);

  /* get the number of pages */
  npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

  /* check tabs should always be visible */
  g_object_get (G_OBJECT (window->preferences), "misc-always-show-tabs", &always_show_tabs, NULL);

  /* change the visibility of the tabs accordingly */
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (window->notebook), always_show_tabs || (npages > 1));

  /* don't focus the notebook */
  GTK_WIDGET_UNSET_FLAGS (window->notebook, GTK_CAN_FOCUS);

  /* update the go menu */
  mousepad_window_update_gomenu (window);

  /* update the window actions */
  mousepad_window_update_actions (window);
}



static void
mousepad_window_notebook_removed (GtkNotebook     *notebook,
                                  GtkWidget       *page,
                                  guint            page_num,
                                  MousepadWindow  *window)
{
  gboolean          always_show_tabs;
  gint              npages;
  MousepadDocument *document = MOUSEPAD_DOCUMENT (page);

  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  _mousepad_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  /* disconnect the old document signals */
  g_signal_handlers_disconnect_by_func (G_OBJECT (page), mousepad_window_button_close_tab, window);
  g_signal_handlers_disconnect_by_func (G_OBJECT (page), mousepad_window_selection_changed, window);
  g_signal_handlers_disconnect_by_func (G_OBJECT (page), mousepad_window_cursor_changed, window);
  g_signal_handlers_disconnect_by_func (G_OBJECT (page), mousepad_window_overwrite_changed, window);
  g_signal_handlers_disconnect_by_func (G_OBJECT (page), mousepad_window_drag_data_received, window);
  g_signal_handlers_disconnect_by_func (G_OBJECT (document->undo), mousepad_window_can_undo, window);
  g_signal_handlers_disconnect_by_func (G_OBJECT (document->undo), mousepad_window_can_redo, window);
  g_signal_handlers_disconnect_by_func (G_OBJECT (document->buffer), mousepad_window_modified_changed, window);

  /* unset the go menu item (part of the old window) */
  g_object_set_data (G_OBJECT (page), I_("navigation-menu-action"), NULL);

  /* get the number of pages in this notebook */
  npages = gtk_notebook_get_n_pages (notebook);

  /* update the window */
  if (npages == 0)
    {
      /* window contains no tabs, destroy it */
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

      /* update the go menu */
      mousepad_window_update_gomenu (window);

      /* update the actions */
      mousepad_window_update_actions (window);
    }
}



static void
mousepad_window_notebook_menu_position (GtkMenu  *menu,
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
mousepad_window_notebook_button_press_event (GtkNotebook    *notebook,
                                             GdkEventButton *event,
                                             MousepadWindow *window)
{
  GtkWidget *page, *label;
  GtkWidget *menu;
  guint      page_num = 0;
  gint       x_root;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);

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
                              (GtkMenuPositionFunc) mousepad_window_notebook_menu_position, label,
                              event->button, event->time);

              /* we succeed */
              return TRUE;
            }

          /* try the next tab */
          ++page_num;
        }
    }
  else if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
    {
      /* open a new tab */
      mousepad_window_open_file (window, NULL, NULL);

      /* we succeed */
      return TRUE;
    }

  return FALSE;
}



#if GTK_CHECK_VERSION (2,12,0)
static GtkNotebook *
mousepad_window_notebook_create_window (GtkNotebook    *notebook,
                                        GtkWidget      *page,
                                        gint            x,
                                        gint            y,
                                        MousepadWindow *window)
{
  MousepadDocument *document;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), NULL);
  _mousepad_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (page), NULL);

  /* only create new window when there are more then 2 tabs */
  if (gtk_notebook_get_n_pages (notebook) >= 2)
    {
      /* get the document */
      document = MOUSEPAD_DOCUMENT (page);

      /* take a reference */
      g_object_ref (G_OBJECT (document));

      /* remove the document from the active window */
      gtk_container_remove (GTK_CONTAINER (window->notebook), page);

      /* emit the new window with document signal */
      g_signal_emit (G_OBJECT (window), window_signals[NEW_WINDOW_WITH_DOCUMENT], 0, document, x, y);

      /* release our reference */
      g_object_unref (G_OBJECT (document));
    }

  return NULL;
}
#endif



/**
 * Document Signals Functions
 **/
static void
mousepad_window_modified_changed (MousepadWindow   *window)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  mousepad_window_set_title (window);
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

  action = gtk_action_group_get_action (window->action_group, "cut");
  gtk_action_set_sensitive (action, selected);

  action = gtk_action_group_get_action (window->action_group, "copy");
  gtk_action_set_sensitive (action, selected);

  action = gtk_action_group_get_action (window->action_group, "delete");
  gtk_action_set_sensitive (action, selected);
}



static void
mousepad_window_can_undo (MousepadWindow *window,
                          gboolean        can_undo)
{
  GtkAction *action;

  action = gtk_action_group_get_action (window->action_group, "undo");
  gtk_action_set_sensitive (action, can_undo);
}



static void
mousepad_window_can_redo (MousepadWindow *window,
                          gboolean        can_redo)
{
  GtkAction *action;

  action = gtk_action_group_get_action (window->action_group, "redo");
  gtk_action_set_sensitive (action, can_redo);
}



/**
 * Menu Functions
 **/
static void
mousepad_window_menu_tab_sizes (MousepadWindow *window)
{
  GtkRadioAction   *action;
  GSList           *group = NULL;
  gint              i, size, merge_id;
  gchar            *name, *tmp;
  gchar           **tab_sizes;

  /* lock menu updates */
  lock_menu_updates++;

  /* get the default tab sizes and active tab size */
  g_object_get (G_OBJECT (window->preferences),
                "misc-default-tab-sizes", &tmp,
                NULL);

  /* get sizes array and free the temp string */
  tab_sizes = g_strsplit (tmp, ",", -1);
  g_free (tmp);

  /* create merge id */
  merge_id = gtk_ui_manager_new_merge_id (window->ui_manager);

  /* add the default sizes to the menu */
  for (i = 0; tab_sizes[i] != NULL; i++)
    {
      /* convert the string to a number */
      size = strtol (tab_sizes[i], NULL, 10);

      /* keep this in sync with the property limits */
      if (G_LIKELY (size > 0))
        {
          /* keep this in sync with the properties */
          size = CLAMP (size, 1, 32);

          /* create action name */
          name = g_strdup_printf ("tab-size-%d", size);

          action = gtk_radio_action_new (name, name + 9, NULL, NULL, size);
          gtk_radio_action_set_group (action, group);
          group = gtk_radio_action_get_group (action);
          g_signal_connect (G_OBJECT (action), "activate", G_CALLBACK (mousepad_window_action_tab_size), window);
          gtk_action_group_add_action_with_accel (window->action_group, GTK_ACTION (action), "");

          /* release the action */
          g_object_unref (G_OBJECT (action));

          /* add the action to the go menu */
          gtk_ui_manager_add_ui (window->ui_manager, merge_id,
                                 "/main-menu/document-menu/tab-size-menu/placeholder-tab-sizes",
                                 name, name, GTK_UI_MANAGER_MENUITEM, FALSE);

          /* cleanup */
          g_free (name);
        }
    }

  /* cleanup the array */
  g_strfreev (tab_sizes);

  /* create other action */
  action = gtk_radio_action_new ("tab-size-other", "", _("Set custom tab size"), NULL, 0);
  gtk_radio_action_set_group (action, group);
  g_signal_connect (G_OBJECT (action), "activate", G_CALLBACK (mousepad_window_action_tab_size), window);
  gtk_action_group_add_action_with_accel (window->action_group, GTK_ACTION (action), "");

  /* release the action */
  g_object_unref (G_OBJECT (action));

  /* add the action to the go menu */
  gtk_ui_manager_add_ui (window->ui_manager, merge_id,
                         "/main-menu/document-menu/tab-size-menu/placeholder-tab-sizes",
                         "tab-size-other", "tab-size-other", GTK_UI_MANAGER_MENUITEM, FALSE);

  /* unlock */
  lock_menu_updates--;
}



static void
mousepad_window_menu_tab_sizes_update (MousepadWindow  *window)
{
  gint       tab_size;
  gchar     *name, *label = NULL;
  GtkAction *action;

  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* avoid menu actions */
  lock_menu_updates++;

  /* get tab size of active document */
  tab_size = mousepad_view_get_tab_size (window->active->textview);

  /* check if there is a default item with this number */
  name = g_strdup_printf ("tab-size-%d", tab_size);
  action = gtk_action_group_get_action (window->action_group, name);
  g_free (name);

  if (action)
    {
      /* toggle the default action */
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
    }
  else
    {
      /* create suitable label */
      label = g_strdup_printf (_("Other (%d)..."), tab_size);
    }

  /* get other action */
  action = gtk_action_group_get_action (window->action_group, "tab-size-other");

  /* toggle other action if needed */
  if (label)
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

  /* set action label */
  g_object_set (G_OBJECT (action), "label", label ? label : _("Other..."), NULL);

  /* cleanup */
  g_free (label);

  /* allow menu actions again */
  lock_menu_updates--;
}




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

  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* active document */
  document = window->active;

  /* update the actions for the active document */
  if (G_LIKELY (document))
    {
      /* avoid menu actions */
      lock_menu_updates++;

      /* determine the number of pages and the current page number */
      n_pages = gtk_notebook_get_n_pages (notebook);
      page_num = gtk_notebook_page_num (notebook, GTK_WIDGET (document));

      /* whether we cycle tabs */
      g_object_get (G_OBJECT (window->preferences), "misc-cycle-tabs", &cycle_tabs, NULL);

      /* set the sensitivity of the back and forward buttons in the go menu */
      action = gtk_action_group_get_action (window->action_group, "back");
      gtk_action_set_sensitive (action, (cycle_tabs && n_pages > 1) || (page_num > 0));

      action = gtk_action_group_get_action (window->action_group, "forward");
      gtk_action_set_sensitive (action, (cycle_tabs && n_pages > 1 ) || (page_num < n_pages - 1));

      /* set the reload, detach and save sensitivity */
      action = gtk_action_group_get_action (window->action_group, "save-file");
      gtk_action_set_sensitive (action, !mousepad_file_get_read_only (document->file));

      action = gtk_action_group_get_action (window->action_group, "detach-tab");
      gtk_action_set_sensitive (action, (n_pages > 1));

      action = gtk_action_group_get_action (window->action_group, "reload");
      gtk_action_set_sensitive (action, mousepad_file_get_filename (document->file) != NULL);

      /* toggle the document settings */
      active = mousepad_document_get_word_wrap (document);
      action = gtk_action_group_get_action (window->action_group, "word-wrap");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);

      active = mousepad_view_get_line_numbers (document->textview);
      action = gtk_action_group_get_action (window->action_group, "line-numbers");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);

      active = mousepad_view_get_auto_indent (document->textview);
      action = gtk_action_group_get_action (window->action_group, "auto-indent");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);

      mousepad_window_menu_tab_sizes_update (window);

      active = mousepad_view_get_insert_spaces (document->textview);
      action = gtk_action_group_get_action (window->action_group, "insert-spaces");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);

      /* set the sensitivity of the undo and redo actions */
      mousepad_window_can_undo (window, mousepad_undo_can_undo (document->undo));
      mousepad_window_can_redo (window, mousepad_undo_can_redo (document->undo));

      /* set the sensitivity of the selection actions */
      has_selection = mousepad_view_get_has_selection (document->textview);
      mousepad_window_selection_changed (document, has_selection, window);

      /* active this tab in the go menu */
      action = g_object_get_data (G_OBJECT (document), I_("navigation-menu-action"));
      if (G_LIKELY (action != NULL))
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

      /* allow menu actions again */
      lock_menu_updates--;
    }
}



static gboolean
mousepad_window_update_gomenu_idle (gpointer user_data)
{
  MousepadDocument *document;
  MousepadWindow   *window;
  gint              npages;
  gint              n;
  gchar            *name;
  const gchar      *title;
  const gchar      *tooltip;
  gchar             accelerator[7];
  GtkRadioAction   *action;
  GSList           *group = NULL;
  GList            *actions, *li;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_WINDOW (user_data), FALSE);

  GDK_THREADS_ENTER ();

  /* get the window */
  window = MOUSEPAD_WINDOW (user_data);

  /* prevent menu updates */
  lock_menu_updates++;

  /* remove the old merge */
  if (window->gomenu_merge_id != 0)
    {
      gtk_ui_manager_remove_ui (window->ui_manager, window->gomenu_merge_id);

      /* drop all the previous actions from the action group */
      actions = gtk_action_group_list_actions (window->action_group);
      for (li = actions; li != NULL; li = li->next)
        if (strncmp (gtk_action_get_name (li->data), "mousepad-document-", 18) == 0)
          gtk_action_group_remove_action (window->action_group, GTK_ACTION (li->data));
      g_list_free (actions);
    }

  /* create a new merge id */
  window->gomenu_merge_id = gtk_ui_manager_new_merge_id (window->ui_manager);

  /* walk through the notebook pages */
  npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

  for (n = 0; n < npages; ++n)
    {
      document = MOUSEPAD_DOCUMENT (gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), n));

      /* create a new action name */
      name = g_strdup_printf ("mousepad-document-%d", n);

      /* get the name and file name */
      title = mousepad_document_get_basename (document);
      tooltip = mousepad_document_get_filename (document);

      /* create the radio action */
      action = gtk_radio_action_new (name, title, tooltip, NULL, n);
      gtk_radio_action_set_group (action, group);
      group = gtk_radio_action_get_group (action);
      g_signal_connect (G_OBJECT (action), "activate", G_CALLBACK (mousepad_window_action_goto), window->notebook);

      /* connect the action to the document to we can easily active it when the user switched from tab */
      g_object_set_data (G_OBJECT (document), I_("navigation-menu-action"), action);

      if (G_LIKELY (n < 9))
        {
          /* create an accelerator and add it to the menu */
          g_snprintf (accelerator, sizeof (accelerator), "<Alt>%d", n+1);
          gtk_action_group_add_action_with_accel (window->action_group, GTK_ACTION (action), accelerator);
        }
      else
        /* add a menu item without accelerator */
        gtk_action_group_add_action (window->action_group, GTK_ACTION (action));

      /* select the active entry */
      if (gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook)) == n)
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

      /* release the action */
      g_object_unref (G_OBJECT (action));

      /* add the action to the go menu */
      gtk_ui_manager_add_ui (window->ui_manager, window->gomenu_merge_id,
                             "/main-menu/navigation-menu/placeholder-documents",
                             name, name, GTK_UI_MANAGER_MENUITEM, FALSE);

      /* cleanup */
      g_free (name);
    }

  /* make sure the ui is up2date to avoid flickering */
  gtk_ui_manager_ensure_update (window->ui_manager);

  /* release our lock */
  lock_menu_updates--;

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
mousepad_window_update_gomenu_idle_destroy (gpointer user_data)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (user_data));

  MOUSEPAD_WINDOW (user_data)->update_go_menu_id = 0;
}



static void
mousepad_window_update_gomenu (MousepadWindow *window)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));

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
                            MousepadFile   *file)
{
  GtkRecentData  info;
  gchar         *uri;
  static gchar  *groups[] = { PACKAGE_NAME, NULL };
  gchar         *description;

  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  _mousepad_return_if_fail (MOUSEPAD_IS_FILE (file));

  /* build description */
  description = g_strdup_printf ("%s: %s", _("Encoding"), mousepad_file_get_encoding (file));

  /* create the recent data */
  info.display_name = NULL;
  info.description  = description;
  info.mime_type    = "text/plain";
  info.app_name     = PACKAGE_NAME;
  info.app_exec     = PACKAGE " %u";
  info.groups       = groups;
  info.is_private   = FALSE;

  /* create an uri from the filename */
  uri = mousepad_file_get_uri (file);

  if (G_LIKELY (uri != NULL))
    {
      /* make sure the recent manager is initialized */
      mousepad_window_recent_manager_init (window);

      /* add the new recent info to the recent manager */
      gtk_recent_manager_add_full (window->recent_manager, uri, &info);

      /* cleanup */
      g_free (uri);
    }

  /* cleanup */
  g_free (description);
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



static void
mousepad_window_recent_manager_init (MousepadWindow *window)
{
  /* set recent manager if not already done */
  if (G_UNLIKELY (window->recent_manager == NULL))
    {
      /* get the default manager */
      window->recent_manager = gtk_recent_manager_get_default ();

      /* connect changed signal */
      g_signal_connect_swapped (G_OBJECT (window->recent_manager), "changed", G_CALLBACK (mousepad_window_recent_menu), window);
    }
}



static gboolean
mousepad_window_recent_menu_idle (gpointer user_data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (user_data);
  GList          *items, *li, *actions;
  GList          *filtered = NULL;
  GtkRecentInfo  *info;
  const gchar    *display_name;
  gchar          *uri_display;
  gchar          *tooltip, *name, *label;
  GtkAction      *action;
  gint            n;

  GDK_THREADS_ENTER ();

  if (window->recent_merge_id != 0)
    {
      /* unmerge the ui controls from the previous update */
      gtk_ui_manager_remove_ui (window->ui_manager, window->recent_merge_id);

      /* drop all the previous actions from the action group */
      actions = gtk_action_group_list_actions (window->action_group);
      for (li = actions; li != NULL; li = li->next)
        if (strncmp (gtk_action_get_name (li->data), "recent-info-", 12) == 0)
          gtk_action_group_remove_action (window->action_group, li->data);
      g_list_free (actions);
    }

  /* create a new merge id */
  window->recent_merge_id = gtk_ui_manager_new_merge_id (window->ui_manager);

  /* make sure the recent manager is initialized */
  mousepad_window_recent_manager_init (window);

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
  g_object_get (G_OBJECT (window->preferences), "misc-recent-menu-items", &n, NULL);

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
      g_signal_connect (G_OBJECT (action), "activate", G_CALLBACK (mousepad_window_action_open_recent), window);

      /* add the action to the recent actions group */
      gtk_action_group_add_action (window->action_group, action);

      /* add the action to the menu */
      gtk_ui_manager_add_ui (window->ui_manager, window->recent_merge_id,
                             "/main-menu/file-menu/recent-menu/placeholder-recent-items",
                             name, name, GTK_UI_MANAGER_MENUITEM, FALSE);

      /* cleanup */
      g_free (name);
    }

  /* set the visibility of the 'No items found' action */
  action = gtk_action_group_get_action (window->action_group, "no-recent-items");
  gtk_action_set_visible (action, (filtered == NULL));
  gtk_action_set_sensitive (action, FALSE);

  /* set the sensitivity of the clear button */
  action = gtk_action_group_get_action (window->action_group, "clear-recent");
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
  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* leave when we're updating multiple files or there is this an idle function pending */
  if (lock_menu_updates > 0 || window->update_recent_menu_id != 0)
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

  /* make sure the recent manager is initialized */
  mousepad_window_recent_manager_init (window);

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
                                   _("Failed to remove an item from the documents history"));
      g_error_free (error);
    }
}



/**
 * Drag and drop functions
 **/
static void
mousepad_window_drag_data_received (GtkWidget        *widget,
                                    GdkDragContext   *context,
                                    gint              x,
                                    gint              y,
                                    GtkSelectionData *selection_data,
                                    guint             info,
                                    guint             time,
                                    MousepadWindow   *window)
{
  gchar     **uris;
  GtkWidget  *notebook, **document;
  GtkWidget  *child, *label;
  gint        i, n_pages;

  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  _mousepad_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  /* we only accept text/uri-list drops with format 8 and atleast one byte of data */
  if (info == TARGET_TEXT_URI_LIST && selection_data->format == 8 && selection_data->length > 0)
    {
      /* extract the uris from the data */
      uris = g_uri_list_extract_uris ((const gchar *)selection_data->data);

      /* open the files */
      mousepad_window_open_files (window, NULL, uris);

      /* cleanup */
      g_strfreev (uris);

      /* finish the drag (copy) */
      gtk_drag_finish (context, TRUE, FALSE, time);
    }
  else if (info == TARGET_GTK_NOTEBOOK_TAB)
    {
      /* get the source notebook */
      notebook = gtk_drag_get_source_widget (context);

      /* get the document that has been dragged */
      document = (GtkWidget **) selection_data->data;

      /* check */
      _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (*document));

      /* take a reference on the document before we remove it */
      g_object_ref (G_OBJECT (*document));

      /* remove the document from the source window */
      gtk_container_remove (GTK_CONTAINER (notebook), *document);

      /* get the number of pages in the notebook */
      n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

      /* figure out where to insert the tab in the notebook */
      for (i = 0; i < n_pages; i++)
        {
          /* get the child label */
          child = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i);
          label = gtk_notebook_get_tab_label (GTK_NOTEBOOK (window->notebook), child);

          /* break if we have a matching drop position */
          if (x < (label->allocation.x + label->allocation.width / 2))
            break;
        }

      /* add the document to the new window */
      mousepad_window_add (window, MOUSEPAD_DOCUMENT (*document));

      /* move the tab to the correct position */
      gtk_notebook_reorder_child (GTK_NOTEBOOK (window->notebook), *document, i);

      /* release our reference on the document */
      g_object_unref (G_OBJECT (*document));

      /* finish the drag (move) */
      gtk_drag_finish (context, TRUE, TRUE, time);
    }
}



static gint
mousepad_window_search (MousepadWindow      *window,
                        MousepadSearchFlags  flags,
                        const gchar         *string,
                        const gchar         *replacement)
{
  gint       nmatches = 0;
  gint       npages, i;
  GtkWidget *document;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), -1);

  if (flags & MOUSEPAD_SEARCH_FLAGS_ACTION_HIGHTLIGHT)
    {
      /* highlight all the matches */
      nmatches = mousepad_util_highlight (window->active->buffer, window->active->tag, string, flags);
    }
  else if (flags & MOUSEPAD_SEARCH_FLAGS_ALL_DOCUMENTS)
    {
      /* get the number of documents in this window */
      npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

      /* walk the pages */
      for (i = 0; i < npages; i++)
        {
          /* get the document */
          document = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i);

          /* replace the matches in the document */
          nmatches += mousepad_util_search (MOUSEPAD_DOCUMENT (document)->buffer, string, replacement, flags);
        }
    }
  else if (window->active != NULL)
    {
      /* search or replace in the active document */
      nmatches = mousepad_util_search (window->active->buffer, string, replacement, flags);

      /* make sure the selection is visible */
      if (flags & (MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT | MOUSEPAD_SEARCH_FLAGS_ACTION_REPLACE) && nmatches > 0)
        mousepad_view_put_cursor_on_screen (window->active->textview);
    }
  else
    {
      /* should never be reaches */
      _mousepad_assert_not_reached ();
    }

  return nmatches;
}



/**
 * Search Bar
 **/
static void
mousepad_window_hide_search_bar (MousepadWindow *window)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  _mousepad_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (window->search_bar));

  /* remove the highlight */
  //mousepad_search_bar_reset_highlight (MOUSEPAD_SEARCH_BAR (window->search_bar));
  /* TODO */

  /* hide the search bar */
  gtk_widget_hide (window->search_bar);
  gtk_table_set_row_spacing (GTK_TABLE (window->table), 3, 0);

  /* focus the active document's text view */
  if (G_LIKELY (window->active))
    mousepad_document_focus_textview (window->active);
}


/**
 * Menu Actions
 *
 * All those function should be sorted by the menu structure so it's
 * easy to find a function. Remember that some action functions can
 * still be triggered with keybindings, even when there are no tabs
 * opened. There for it should always check if the active document is
 * not NULL.
 **/
static void
mousepad_window_action_open_new_tab (GtkAction      *action,
                                     MousepadWindow *window)
{
  mousepad_window_open_file (window, NULL, NULL);
}



static void
mousepad_window_action_open_new_window (GtkAction      *action,
                                        MousepadWindow *window)
{
  /* emit the new window signal */
  g_signal_emit (G_OBJECT (window), window_signals[NEW_WINDOW], 0);
}



static void
mousepad_window_action_open_file (GtkAction      *action,
                                  MousepadWindow *window)
{
  GtkWidget        *chooser;
  gchar            *filename;
  const gchar      *active_filename;
  GSList           *filenames, *li;
  MousepadDocument *document = window->active;

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

  /* open the folder of the currently opened file */
  if (document)
    {
      /* get the filename of the active document */
      active_filename = mousepad_file_get_filename (document->file);

      /* set the current filename, if there is one */
      if (active_filename && g_file_test (active_filename, G_FILE_TEST_EXISTS))
        gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser), active_filename);
    }

  /* run the dialog */
  if (G_LIKELY (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT))
    {
      /* hide the dialog */
      gtk_widget_hide (chooser);

      /* open the new file */
      filenames = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (chooser));

      /* block menu updates */
      lock_menu_updates++;

      /* open the selected files */
      for (li = filenames; li != NULL; li = li->next)
        {
          filename = li->data;

          /* open the file in a new tab */
          mousepad_window_open_file (window, filename, NULL);

          /* cleanup */
          g_free (filename);
        }

      /* free the list */
      g_slist_free (filenames);

      /* allow menu updates again */
      lock_menu_updates--;

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
  const gchar   *uri, *description;
  const gchar   *encoding = NULL;
  GError        *error = NULL;
  gint           offset;
  gchar         *filename;
  gboolean       succeed = FALSE;
  GtkRecentInfo *info;

  /* get the info */
  info = g_object_get_data (G_OBJECT (action), I_("gtk-recent-info"));

  if (G_LIKELY (info != NULL))
    {
      /* get the file uri */
      uri = gtk_recent_info_get_uri (info);

      /* build a filename from the uri */
      filename = g_filename_from_uri (uri, NULL, NULL);

      if (G_LIKELY (filename != NULL))
        {
          /* open the file in a new tab if it exists */
          if (g_file_test (filename, G_FILE_TEST_EXISTS))
            {
              /* set text offset */
              offset = strlen (_("Encoding")) + 2;

              /* try to read the encoding we might has saved in the description ;) */
              description = gtk_recent_info_get_description (info);
              if (G_LIKELY (description && strlen (description) > offset))
                encoding = description + offset;

              succeed = mousepad_window_open_file (window, filename, encoding);
            }
          else
            {
              /* create a warning */
              g_set_error (&error,  G_FILE_ERROR, G_FILE_ERROR_IO,
                           _("Failed to open \"%s\" for reading. It will be removed from the document history"), filename);

              /* show the warning and cleanup */
              mousepad_dialogs_show_error (GTK_WINDOW (window), error, _("Failed to open file"));
              g_error_free (error);
            }

          /* cleanup */
          g_free (filename);

          /* update the document history */
          if (G_LIKELY (succeed))
            gtk_recent_manager_add_item (window->recent_manager, uri);
          else
            gtk_recent_manager_remove_item (window->recent_manager, uri, NULL);
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
      lock_menu_updates++;

      /* clear the document history */
      mousepad_window_recent_clear (window);

      /* allow menu updates again */
      lock_menu_updates--;

      /* update the recent menu */
      mousepad_window_recent_menu (window);
    }
}



static gboolean
mousepad_window_action_save_file (GtkAction      *action,
                                  MousepadWindow *window)
{
  MousepadDocument *document = window->active;
  GError           *error = NULL;
  gboolean          succeed = FALSE;

  _mousepad_return_val_if_fail (document != NULL, FALSE);
  _mousepad_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), FALSE);

  if (G_LIKELY (document))
    {
      /* save-as when there is no filename yet */
      if (mousepad_file_get_filename (document->file) == NULL)
        {
          mousepad_window_action_save_file_as (NULL, window);
        }
      else
        {
          /* save the document */
          succeed = mousepad_file_save (document->file, &error);

          if (G_UNLIKELY (succeed == FALSE))
            {
              /* show the warning */
              mousepad_dialogs_show_error (GTK_WINDOW (window), error, _("Failed to save the document"));
              g_error_free (error);
            }
        }
    }

  return succeed;
}



static gboolean
mousepad_window_action_save_file_as (GtkAction      *action,
                                     MousepadWindow *window)
{
  MousepadDocument *document = window->active;
  gchar            *filename;
  const gchar      *old_filename;
  GtkWidget        *dialog;
  gboolean          succeed = TRUE;

  if (G_LIKELY (document))
    {
      /* create the dialog */
      dialog = gtk_file_chooser_dialog_new (_("Save As"),
          GTK_WINDOW (window), GTK_FILE_CHOOSER_ACTION_SAVE,
          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
          GTK_STOCK_SAVE, GTK_RESPONSE_OK, NULL);
      gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), TRUE);
      gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
      gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

      /* set the current filename */
      old_filename = mousepad_file_get_filename (document->file);
      if (old_filename)
        gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), old_filename);

      /* run the dialog */
      if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
        {
          /* get the new filename */
          filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

          if (G_LIKELY (filename))
            {
              /* set the new filename */
              mousepad_file_set_filename (document->file, filename);

              /* cleanup */
              g_free (filename);

              /* save the file */
              succeed = mousepad_window_action_save_file (NULL, window);
            }
        }

      /* destroy the dialog */
      gtk_widget_destroy (dialog);
    }

  return succeed;
}



static void
mousepad_window_action_reload (GtkAction      *action,
                               MousepadWindow *window)
{
  MousepadDocument *document = window->active;
  GError           *error = NULL;
  gboolean          result;

  if (G_LIKELY (document))
    {
      /* TODO ask user if he/she really want this */

      /* lock the undo manager */
      mousepad_undo_lock (document->undo);

      /* clear the undo history */
      mousepad_undo_clear (document->undo);

      /* reload the file */
      result = mousepad_file_reload (document->file, &error);

      /* release the lock */
      mousepad_undo_unlock (document->undo);

      if (G_UNLIKELY (result == FALSE))
        {
          /* show the warning */
          mousepad_dialogs_show_error (GTK_WINDOW (window), error, _("Failed to reload the document"));
          g_error_free (error);
        }
    }
}



static void
mousepad_window_action_print (GtkAction      *action,
                              MousepadWindow *window)
{
  MousepadPrint    *print;
  MousepadDocument *document = window->active;
  GError           *error = NULL;
  gboolean          succeed;

  /* create new print operation */
  print = mousepad_print_new ();

  /* print the current document interactive */
  succeed = mousepad_print_document_interactive (print, document, GTK_WINDOW (window), &error);

  /* release the object */
  g_object_unref (G_OBJECT (print));
}



static void
mousepad_window_action_detach (GtkAction      *action,
                               MousepadWindow *window)
{
#if GTK_CHECK_VERSION (2,12,0)
  /* invoke function without cooridinates */
  mousepad_window_notebook_create_window (GTK_NOTEBOOK (window->notebook),
                                          GTK_WIDGET (window->active),
                                          -1, -1, window);
#else
  MousepadDocument *document = window->active;

  /* only detach when there are more then 2 tabs */
  if (document && gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook)) >= 2)
    {
      /* take a reference */
      g_object_ref (G_OBJECT (document));

      /* remove the document from the active window */
      gtk_container_remove (GTK_CONTAINER (window->notebook), GTK_WIDGET (document));

      /* emit the new window with document signal */
      g_signal_emit (G_OBJECT (window), window_signals[NEW_WINDOW_WITH_DOCUMENT], 0, document, -1, -1);

      /* release our reference */
      g_object_unref (G_OBJECT (document));
    }
#endif
}



static void
mousepad_window_action_close_tab (GtkAction      *action,
                                  MousepadWindow *window)
{
  MousepadDocument *document = window->active;

  if (G_LIKELY (document))
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
  lock_menu_updates++;

  /* ask what to do with the modified document in this window */
  for (i = npages; i >= 0; --i)
    {
      document = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i);

      if (G_LIKELY (document))
        {
          /* focus the tab we're going to close */
          gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), i);

          /* ask user what to do, break when he/she hits the cancel button */
          if (!mousepad_window_close_document (window, MOUSEPAD_DOCUMENT (document)))
            {
              /* update the go menu */
              mousepad_window_update_gomenu (window);

              /* leave the loop */
              break;
            }
        }
    }

  /* allow updates again */
  lock_menu_updates--;

  /* window will close it self when it contains to tabs */
}



static void
mousepad_window_action_undo (GtkAction      *action,
                             MousepadWindow *window)
{
  MousepadDocument *document = window->active;

  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* undo */
  mousepad_undo_do_undo (document->undo);

  /* scroll to visible area */
  mousepad_view_put_cursor_on_screen (document->textview);
}



static void
mousepad_window_action_redo (GtkAction      *action,
                             MousepadWindow *window)
{
  MousepadDocument *document = window->active;

  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* redo */
  mousepad_undo_do_redo (document->undo);

  /* scroll to visible area */
  mousepad_view_put_cursor_on_screen (document->textview);
}



static void
mousepad_window_action_cut (GtkAction      *action,
                            MousepadWindow *window)
{
  GtkEditable *entry;

  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get searchbar entry */
  entry = mousepad_search_bar_entry (MOUSEPAD_SEARCH_BAR (window->search_bar));

  if (entry)
    gtk_editable_cut_clipboard (entry);
  else
    mousepad_view_clipboard_cut (window->active->textview);
}



static void
mousepad_window_action_copy (GtkAction      *action,
                             MousepadWindow *window)
{
  GtkEditable *entry;

  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get searchbar entry */
  entry = mousepad_search_bar_entry (MOUSEPAD_SEARCH_BAR (window->search_bar));

  if (entry)
    gtk_editable_copy_clipboard (entry);
  else
    mousepad_view_clipboard_copy (window->active->textview);
}



static void
mousepad_window_action_paste (GtkAction      *action,
                              MousepadWindow *window)
{
  GtkEditable *entry;

  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get searchbar entry */
  entry = mousepad_search_bar_entry (MOUSEPAD_SEARCH_BAR (window->search_bar));

  if (entry)
    gtk_editable_paste_clipboard (entry);
  else
    mousepad_view_clipboard_paste (window->active->textview, FALSE);
}



static void
mousepad_window_action_paste_column (GtkAction      *action,
                                     MousepadWindow *window)
{
  MousepadDocument *document = window->active;

  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  mousepad_view_clipboard_paste (document->textview, TRUE);
}



static void
mousepad_window_action_delete (GtkAction      *action,
                               MousepadWindow *window)
{
  GtkEditable *entry;

  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get searchbar entry */
  entry = mousepad_search_bar_entry (MOUSEPAD_SEARCH_BAR (window->search_bar));

  if (entry)
    gtk_editable_delete_selection (entry);
  else
    mousepad_view_delete_selection (window->active->textview);
}



static void
mousepad_window_action_select_all (GtkAction      *action,
                                   MousepadWindow *window)
{
  MousepadDocument *document = window->active;

  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  mousepad_view_select_all (document->textview);
}



static void
mousepad_window_action_find (GtkAction      *action,
                             MousepadWindow *window)
{
  if (window->search_bar == NULL)
    {
      /* create a new toolbar */
      window->search_bar = mousepad_search_bar_new ();
      gtk_table_attach (GTK_TABLE (window->table), window->search_bar, 0, 1, 4, 5, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

      /* connect signals */
      g_signal_connect_swapped (G_OBJECT (window->search_bar), "hide-bar", G_CALLBACK (mousepad_window_hide_search_bar), window);
      g_signal_connect_swapped (G_OBJECT (window->search_bar), "search", G_CALLBACK (mousepad_window_search), window);
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
  MousepadDocument *document = window->active;

  /* only find the next occurence when the search bar is initialized */
  if (G_LIKELY (document && window->search_bar != NULL))
    mousepad_search_bar_find_next (MOUSEPAD_SEARCH_BAR (window->search_bar));
}



static void
mousepad_window_action_find_previous (GtkAction      *action,
                                      MousepadWindow *window)
{
  MousepadDocument *document = window->active;

  _mousepad_return_if_fail (document != NULL);
  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* only find the previous occurence when the search bar is initialized */
  if (G_LIKELY (document && window->search_bar != NULL))
    mousepad_search_bar_find_previous (MOUSEPAD_SEARCH_BAR (window->search_bar));
}



static void
mousepad_window_action_replace_destroy (MousepadWindow *window)
{
  /* reset the dialog variable */
  window->replace_dialog = NULL;
}



static void
mousepad_window_action_replace (GtkAction      *action,
                                MousepadWindow *window)
{
  if (!window->replace_dialog)
    {
      /* create a new dialog */
      window->replace_dialog = mousepad_replace_dialog_new ();

      /* popup the dialog */
      gtk_window_set_destroy_with_parent (GTK_WINDOW (window->replace_dialog), TRUE);
      gtk_window_set_transient_for (GTK_WINDOW (window->replace_dialog), GTK_WINDOW (window));
      gtk_widget_show (window->replace_dialog);

      /* connect signals */
      g_signal_connect_swapped (G_OBJECT (window->replace_dialog), "destroy", G_CALLBACK (mousepad_window_action_replace_destroy), window);
      g_signal_connect_swapped (G_OBJECT (window->replace_dialog), "search", G_CALLBACK (mousepad_window_search), window);
    }
}



static void
mousepad_window_action_select_font (GtkAction      *action,
                                    MousepadWindow *window)
{
  GtkWidget        *dialog;
  MousepadDocument *document;
  gchar            *font_name;
  guint             npages, i;

  dialog = gtk_font_selection_dialog_new (_("Choose Mousepad Font"));
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));

  /* get the current font */
  g_object_get (G_OBJECT (window->preferences), "view-font-name", &font_name, NULL);

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
      g_object_set (G_OBJECT (window->preferences), "view-font-name", font_name, NULL);

      /* set the font in all documents in this window */
      npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));
      for (i = 0; i < npages; i++)
        {
          /* get the document */
          document = MOUSEPAD_DOCUMENT (gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i));

          /* debug check */
          _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

          /* set the font */
          mousepad_document_set_font (document, font_name);

          /* update the tab array */
          mousepad_view_set_tab_size (document->textview, mousepad_view_get_tab_size (document->textview));
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
  gboolean show_statusbar;

  /* whether we show the statusbar */
  show_statusbar = gtk_toggle_action_get_active (action);

  /* check if we should drop the statusbar */
  if (!show_statusbar && window->statusbar != NULL)
    {
      /* just get rid of the statusbar */
      gtk_widget_destroy (window->statusbar);
      window->statusbar = NULL;
    }
  else if (show_statusbar && window->statusbar == NULL)
    {
      /* setup a new statusbar */
      window->statusbar = mousepad_statusbar_new ();
      gtk_table_attach (GTK_TABLE (window->table), window->statusbar, 0, 1, 5, 6, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
      gtk_widget_show (window->statusbar);

      /* overwrite toggle signal */
      g_signal_connect_swapped (G_OBJECT (window->statusbar), "enable-overwrite",
                                G_CALLBACK (mousepad_window_toggle_overwrite), window);

      /* update the statusbar items */
      if (window->active)
        mousepad_document_send_statusbar_signals (window->active);
    }

  /* set the spacing above the statusbar */
  gtk_table_set_row_spacing (GTK_TABLE (window->table), 4, show_statusbar ? WINDOW_SPACING : 0);

  /* remember the setting */
  g_object_set (G_OBJECT (window->preferences), "window-statusbar-visible", show_statusbar, NULL);
}



static void
mousepad_window_action_line_numbers (GtkToggleAction *action,
                                     MousepadWindow  *window)
{
  gboolean line_numbers;

  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* leave when menu updates are locked */
  if (lock_menu_updates)
    return;

  /* get the current state */
  line_numbers = gtk_toggle_action_get_active (action);

  /* save as the last used line number setting */
  g_object_set (G_OBJECT (window->preferences), "view-line-numbers", line_numbers, NULL);

  /* update the active document */
  mousepad_view_set_line_numbers (window->active->textview, line_numbers);
}



static void
mousepad_window_action_auto_indent (GtkToggleAction *action,
                                    MousepadWindow  *window)
{
  gboolean auto_indent;

  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* leave when menu updates are locked */
  if (lock_menu_updates)
    return;

  /* get the current state */
  auto_indent = gtk_toggle_action_get_active (action);

  /* save as the last auto indent mode */
  g_object_set (G_OBJECT (window->preferences), "view-auto-indent", auto_indent, NULL);

  /* update the active document */
  mousepad_view_set_auto_indent (window->active->textview, auto_indent);
}



static void
mousepad_window_action_word_wrap (GtkToggleAction *action,
                                  MousepadWindow  *window)
{
  gboolean word_wrap;

  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* leave when menu updates are locked */
  if (lock_menu_updates)
    return;

  /* get the current state */
  word_wrap = gtk_toggle_action_get_active (action);

  /* store this as the last used wrap mode */
  g_object_set (G_OBJECT (window->preferences), "view-word-wrap", word_wrap, NULL);

  /* set the wrapping mode of the current document */
  mousepad_document_set_word_wrap (window->active, word_wrap);
}



static void
mousepad_window_action_tab_size (GtkToggleAction *action,
                                 MousepadWindow  *window)
{
  gboolean tab_size;

  /* leave when menu updates are locked */
  if (lock_menu_updates)
    return;

  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  if (gtk_toggle_action_get_active (action))
    {
      /* get the tab size */
      tab_size = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));

      /* other... item clicked */
      if (tab_size == 0)
        {
          /* get tab size from document */
          tab_size = mousepad_view_get_tab_size (window->active->textview);

          /* select other size in dialog */
          tab_size = mousepad_dialogs_other_tab_size (GTK_WINDOW (window), tab_size);
        }

      /* store as last used value */
      g_object_set (G_OBJECT (window->preferences), "view-tab-size", tab_size, NULL);

      /* set the value */
      mousepad_view_set_tab_size (window->active->textview, tab_size);

      /* update menu */
      mousepad_window_menu_tab_sizes_update (window);
    }
}


static void
mousepad_window_action_insert_spaces (GtkToggleAction *action,
                                      MousepadWindow  *window)
{
  gboolean insert_spaces;

  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* leave when menu updates are locked */
  if (lock_menu_updates)
    return;

  /* get the current state */
  insert_spaces = gtk_toggle_action_get_active (action);

  /* save as the last auto indent mode */
  g_object_set (G_OBJECT (window->preferences), "view-insert-spaces", insert_spaces, NULL);

  /* update the active document */
  mousepad_view_set_insert_spaces (window->active->textview, insert_spaces);
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
  gint page;

  /* leave when the menu is locked */
  if (lock_menu_updates)
    return;

  /* get the page number from the action value */
  page = gtk_radio_action_get_current_value (action);

  /* set the page */
  gtk_notebook_set_current_page (notebook, page);
}



static void
mousepad_window_action_go_to_line (GtkAction      *action,
                                   MousepadWindow *window)
{
  MousepadDocument *document = window->active;
  gint              current_line, last_line, line;

  if (G_LIKELY (document))
    {
      /* get the current and last line number */
      mousepad_document_line_numbers (document, &current_line, &last_line);

      /* run the jump to dialog and wait for the response */
      line = mousepad_dialogs_go_to_line (GTK_WINDOW (window), current_line, last_line);

      if (G_LIKELY (line > 0))
        mousepad_document_go_to_line (document, line);
    }
}



static void
mousepad_window_action_contents (GtkAction      *action,
                                 MousepadWindow *window)
{

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
