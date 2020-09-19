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

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-settings.h>
#include <mousepad/mousepad-application.h>
#include <mousepad/mousepad-marshal.h>
#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-dialogs.h>
#include <mousepad/mousepad-replace-dialog.h>
#include <mousepad/mousepad-encoding-dialog.h>
#include <mousepad/mousepad-search-bar.h>
#include <mousepad/mousepad-statusbar.h>
#include <mousepad/mousepad-print.h>
#include <mousepad/mousepad-window.h>

#include <glib/gstdio.h>

#include <gtksourceview/gtksource.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif



#define PADDING                   (2)
#define PASTE_HISTORY_MENU_LENGTH (30)

static const gchar *NOTEBOOK_GROUP = "Mousepad";



enum
{
  NEW_WINDOW,
  NEW_WINDOW_WITH_DOCUMENT,
  LAST_SIGNAL
};



static void              mousepad_window_dispose                      (GObject                *object);
static void              mousepad_window_finalize                     (GObject                *object);
static gboolean          mousepad_window_configure_event              (GtkWidget              *widget,
                                                                       GdkEventConfigure      *event);

/* statusbar tooltips */
static void              mousepad_window_menu_set_tooltips_full       (MousepadWindow         *window,
                                                                       GtkWidget              *menu,
                                                                       GPtrArray              *tooltips,
                                                                       guint                  *index);
static void              mousepad_window_menubar_set_insertion_flags  (MousepadWindow         *window,
                                                                       GtkWidget              *menu,
                                                                       guint                  *index);
static GtkWidget        *mousepad_window_get_menubar_submenu          (MousepadWindow         *window,
                                                                       GtkWidget              *menu,
                                                                       const gchar            *flag);
static void              mousepad_window_menu_set_tooltips            (MousepadWindow         *window,
                                                                       GtkWidget              *menu,
                                                                       GPtrArray              *tooltips,
                                                                       guint                   n_tooltips,
                                                                       guint                   offset);
static void              mousepad_window_menu_item_selected           (GtkWidget              *menu_item,
                                                                       MousepadWindow         *window);
static void              mousepad_window_menu_item_deselected         (GtkWidget              *menu_item,
                                                                       MousepadWindow         *window);
static gboolean          mousepad_window_tool_item_enter_event        (GtkWidget              *tool_item,
                                                                       GdkEvent               *event,
                                                                       MousepadWindow         *window);
static gboolean          mousepad_window_tool_item_leave_event        (GtkWidget              *tool_item,
                                                                       GdkEvent               *event,
                                                                       MousepadWindow         *window);

/* save windows geometry */
static gboolean          mousepad_window_save_geometry_timer          (gpointer                user_data);
static void              mousepad_window_save_geometry_timer_destroy  (gpointer                user_data);

/* window functions */
static gboolean          mousepad_window_open_file                    (MousepadWindow         *window,
                                                                       const gchar            *filename,
                                                                       MousepadEncoding        encoding);
static gboolean          mousepad_window_close_document               (MousepadWindow         *window,
                                                                       MousepadDocument       *document);
static void              mousepad_window_set_title                    (MousepadWindow         *window);
static void              mousepad_window_create_statusbar             (MousepadWindow         *window);
static gboolean          mousepad_window_get_in_fullscreen            (MousepadWindow         *window);
static gboolean          mousepad_window_fullscreen_bars_timer        (gpointer                user_data);
static void              mousepad_window_update_main_widgets          (MousepadWindow         *window);

/* notebook signals */
static void              mousepad_window_notebook_switch_page         (GtkNotebook            *notebook,
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
#if !GTK_CHECK_VERSION (3, 22, 0)
static void              mousepad_window_notebook_menu_position       (GtkMenu                *menu,
                                                                       gint                   *x,
                                                                       gint                   *y,
                                                                       gboolean               *push_in,
                                                                       gpointer                user_data);
#endif
static gboolean          mousepad_window_notebook_button_release_event (GtkNotebook           *notebook,
                                                                        GdkEventButton        *event,
                                                                        MousepadWindow        *window);
static gboolean          mousepad_window_notebook_button_press_event  (GtkNotebook            *notebook,
                                                                       GdkEventButton         *event,
                                                                       MousepadWindow         *window);
static GtkNotebook      *mousepad_window_notebook_create_window       (GtkNotebook            *notebook,
                                                                       GtkWidget              *page,
                                                                       gint                    x,
                                                                       gint                    y,
                                                                       MousepadWindow         *window);

/* document signals */
static void              mousepad_window_modified_changed             (MousepadWindow         *window);
static void              mousepad_window_cursor_changed               (MousepadDocument       *document,
                                                                       gint                    line,
                                                                       gint                    column,
                                                                       gint                    selection,
                                                                       MousepadWindow         *window);
static void              mousepad_window_selection_changed            (MousepadDocument       *document,
                                                                       gint                    selection,
                                                                       MousepadWindow         *window);
static void              mousepad_window_overwrite_changed            (MousepadDocument       *document,
                                                                       gboolean                overwrite,
                                                                       MousepadWindow         *window);
static void              mousepad_window_buffer_language_changed      (MousepadDocument       *document,
                                                                       GtkSourceLanguage      *language,
                                                                       MousepadWindow         *window);
static void              mousepad_window_can_undo                     (MousepadWindow         *window,
                                                                       GParamSpec             *unused,
                                                                       GObject                *buffer);
static void              mousepad_window_can_redo                     (MousepadWindow         *window,
                                                                       GParamSpec             *unused,
                                                                       GObject                *buffer);

/* menu functions */
static void              mousepad_window_menu_templates_fill          (MousepadWindow         *window,
                                                                       GMenu                  *menu,
                                                                       const gchar            *path,
                                                                       GPtrArray              *tooltips);
static void              mousepad_window_menu_templates               (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_menu_tab_sizes_update        (MousepadWindow         *window);
static void              mousepad_window_menu_textview_deactivate     (GtkWidget              *menu,
                                                                       MousepadWindow         *window);
static void              mousepad_window_menu_textview_popup          (GtkTextView            *textview,
                                                                       GtkMenu                *old_menu,
                                                                       MousepadWindow         *window);
static void              mousepad_window_update_fullscreen_action     (MousepadWindow         *window);
static void              mousepad_window_update_line_numbers_action   (MousepadWindow         *window);
static void              mousepad_window_update_document_actions      (MousepadWindow         *window);
static void              mousepad_window_update_color_scheme_action   (MousepadWindow         *window);
static void              mousepad_window_update_actions               (MousepadWindow         *window);
static void              mousepad_window_update_gomenu                (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_update_tabs                  (MousepadWindow         *window,
                                                                       gchar                  *key,
                                                                       GSettings              *settings);

/* recent functions */
static void              mousepad_window_recent_add                   (MousepadWindow         *window,
                                                                       MousepadFile           *file);
static gint              mousepad_window_recent_sort                  (GtkRecentInfo          *a,
                                                                       GtkRecentInfo          *b);
static void              mousepad_window_recent_manager_init          (MousepadWindow         *window);
static void              mousepad_window_recent_menu                  (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static const gchar      *mousepad_window_recent_get_charset           (GtkRecentInfo          *info);
static void              mousepad_window_recent_clear                 (MousepadWindow         *window);

/* dnd */
static void              mousepad_window_drag_data_received           (GtkWidget              *widget,
                                                                       GdkDragContext         *context,
                                                                       gint                    x,
                                                                       gint                    y,
                                                                       GtkSelectionData       *selection_data,
                                                                       guint                   info,
                                                                       guint                   drag_time,
                                                                       MousepadWindow         *window);

/* search bar */
static void              mousepad_window_hide_search_bar              (MousepadWindow         *window);

/* history clipboard functions */
static void              mousepad_window_paste_history_add            (MousepadWindow         *window);
#if !GTK_CHECK_VERSION (3, 22, 0)
static void              mousepad_window_paste_history_menu_position  (GtkMenu                *menu,
                                                                       gint                   *x,
                                                                       gint                   *y,
                                                                       gboolean               *push_in,
                                                                       gpointer                user_data);
#endif
static void              mousepad_window_paste_history_activate       (GtkMenuItem            *item,
                                                                       MousepadWindow         *window);
static GtkWidget        *mousepad_window_paste_history_menu_item      (const gchar            *text,
                                                                       const gchar            *mnemonic);
static GtkWidget        *mousepad_window_paste_history_menu           (MousepadWindow         *window);

/* miscellaneous actions */
static void              mousepad_window_button_close_tab             (MousepadDocument       *document,
                                                                       MousepadWindow         *window);
static gboolean          mousepad_window_delete_event                 (MousepadWindow         *window,
                                                                       GdkEvent               *event);

/* actions */
static void              mousepad_window_action_new                   (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_new_window            (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_new_from_template     (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_open                  (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_open_recent           (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_clear_recent          (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_save                  (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_save_as               (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_save_all              (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_revert                (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_print                 (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_detach                (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_close                 (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_close_window          (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_undo                  (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_redo                  (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_cut                   (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_copy                  (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_paste                 (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_paste_history         (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_paste_column          (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_delete                (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_select_all            (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_preferences           (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_lowercase             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_uppercase             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_titlecase             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_opposite_case         (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_tabs_to_spaces        (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_spaces_to_tabs        (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_strip_trailing_spaces (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_transpose             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_move_line_up          (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_move_line_down        (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_duplicate             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_increase_indent       (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_decrease_indent       (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_find                  (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_find_next             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_find_previous         (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_replace_destroy       (MousepadWindow         *window);
static void              mousepad_window_action_replace               (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_go_to_position        (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_select_font           (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_line_numbers          (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_menubar               (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_toolbar               (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_statusbar_overwrite   (MousepadWindow         *window,
                                                                       gboolean                overwrite);
static void              mousepad_window_action_statusbar             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_fullscreen            (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_auto_indent           (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_line_ending           (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_tab_size              (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_color_scheme          (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_language              (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_insert_spaces         (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_word_wrap             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_write_bom             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_prev_tab              (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_next_tab              (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_go_to_tab             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_contents              (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_about                 (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);



struct _MousepadWindowClass
{
  GtkApplicationWindowClass __parent__;
};

struct _MousepadWindow
{
  GtkApplicationWindow __parent__;

  /* the current active document */
  MousepadDocument    *active;

  /* main window widgets */
  GtkWidget           *box;
  GtkWidget           *menubar_box;
  GtkWidget           *menubar;
  GtkWidget           *toolbar;
  GtkWidget           *notebook;
  GtkWidget           *search_bar;
  GtkWidget           *statusbar;
  GtkWidget           *replace_dialog;

  /* contextual gtkmenus created from the GtkBuilder */
  GtkWidget           *textview_menu;
  GtkWidget           *tab_menu;
  GtkWidget           *languages_menu;

  /* menubar related */
  GtkRecentManager    *recent_manager;

  /* fullscreen bars visibility switch */
  guint                fullscreen_bars_timer_id;

  /* success of file saving */
  /*
   * A possible TODO: handle this internally by a clever use of GAction parameter/state
   * Easy to think, difficult to write: see
   * https://developer.gnome.org/glib/stable/gvariant-format-strings.html
   * and the obscure interaction between a GActionEntry definition and a GtkBuilder XML file
   */
  gboolean             save_succeed;

  /* support to remember window geometry */
  guint                save_geometry_timer_id;
};



/* menubar actions */
static const GActionEntry action_entries[] =
{
  /* to make menu items insensitive, when needed */
  { "insensitive", NULL, NULL, NULL, NULL },

  /* additional action for the "Textview" menu to show the menubar when hidden */
  { "textview.menubar", NULL, NULL, "false", mousepad_window_action_menubar },

  /* "File" menu */
  { "file.new", mousepad_window_action_new, NULL, NULL, NULL },
  { "file.new-window", mousepad_window_action_new_window, NULL, NULL, NULL },
  { "file.new-from-template", NULL, NULL, "false", mousepad_window_menu_templates },
    { "file.new-from-template.new", mousepad_window_action_new_from_template, "s", NULL, NULL },

  { "file.open", mousepad_window_action_open, NULL, NULL, NULL },
  { "file.open-recent", NULL, NULL, "false", mousepad_window_recent_menu },
    { "file.open-recent.new", mousepad_window_action_open_recent, "s", NULL, NULL },
    { "file.open-recent.clear-history", mousepad_window_action_clear_recent, NULL, NULL, NULL },

  { "file.save", mousepad_window_action_save, NULL, NULL, NULL },
  { "file.save-as", mousepad_window_action_save_as, NULL, NULL, NULL },
  { "file.save-all", mousepad_window_action_save_all, NULL, NULL, NULL },
  { "file.revert", mousepad_window_action_revert, NULL, NULL, NULL },

  { "file.print", mousepad_window_action_print, NULL, NULL, NULL },

  { "file.detach-tab", mousepad_window_action_detach, NULL, NULL, NULL },

  { "file.close-tab", mousepad_window_action_close, NULL, NULL, NULL },
  { "file.close-window", mousepad_window_action_close_window, NULL, NULL, NULL },

  /* "Edit" menu */
  { "edit.undo", mousepad_window_action_undo, NULL, NULL, NULL },
  { "edit.redo", mousepad_window_action_redo, NULL, NULL, NULL },

  { "edit.cut", mousepad_window_action_cut, NULL, NULL, NULL },
  { "edit.copy", mousepad_window_action_copy, NULL, NULL, NULL },
  { "edit.paste", mousepad_window_action_paste, NULL, NULL, NULL },
  /* "Paste Special" submenu */
    { "edit.paste-special.paste-from-history", mousepad_window_action_paste_history, NULL, NULL, NULL },
    { "edit.paste-special.paste-as-column", mousepad_window_action_paste_column, NULL, NULL, NULL },
  { "edit.delete", mousepad_window_action_delete, NULL, NULL, NULL },

  { "edit.select-all", mousepad_window_action_select_all, NULL, NULL, NULL },

  /* "Convert" submenu */
    { "edit.convert.to-lowercase", mousepad_window_action_lowercase, NULL, NULL, NULL },
    { "edit.convert.to-uppercase", mousepad_window_action_uppercase, NULL, NULL, NULL },
    { "edit.convert.to-title-case", mousepad_window_action_titlecase, NULL, NULL, NULL },
    { "edit.convert.to-opposite-case", mousepad_window_action_opposite_case, NULL, NULL, NULL },

    { "edit.convert.tabs-to-spaces", mousepad_window_action_tabs_to_spaces, NULL, NULL, NULL },
    { "edit.convert.spaces-to-tabs", mousepad_window_action_spaces_to_tabs, NULL, NULL, NULL },

    { "edit.convert.strip-trailing-spaces", mousepad_window_action_strip_trailing_spaces, NULL, NULL, NULL },

    { "edit.convert.transpose", mousepad_window_action_transpose, NULL, NULL, NULL },
  /* "Move selection" submenu */
    { "edit.move-selection.line-up", mousepad_window_action_move_line_up, NULL, NULL, NULL },
    { "edit.move-selection.line-down", mousepad_window_action_move_line_down, NULL, NULL, NULL },
  { "edit.duplicate-line-selection", mousepad_window_action_duplicate, NULL, NULL, NULL },
  { "edit.increase-indent", mousepad_window_action_increase_indent, NULL, NULL, NULL },
  { "edit.decrease-indent", mousepad_window_action_decrease_indent, NULL, NULL, NULL },

  { "edit.preferences", mousepad_window_action_preferences, NULL, NULL, NULL },

  /* "Search" menu */
  { "search.find", mousepad_window_action_find, NULL, NULL, NULL },
  { "search.find-next", mousepad_window_action_find_next, NULL, NULL, NULL },
  { "search.find-previous", mousepad_window_action_find_previous, NULL, NULL, NULL },
  { "search.find-and-replace", mousepad_window_action_replace, NULL, NULL, NULL },

  { "search.go-to", mousepad_window_action_go_to_position, NULL, NULL, NULL },

  /* "View" menu */
  { "view.select-font", mousepad_window_action_select_font, NULL, NULL, NULL },

  /* "Color Scheme" submenu */
    { "view.color-scheme", mousepad_window_action_color_scheme, "s", "'none'", NULL },
  { "view.line-numbers", NULL, NULL, "false", mousepad_window_action_line_numbers },

  { "view.menubar", NULL, NULL, "false", mousepad_window_action_menubar },
  { "view.toolbar", NULL, NULL, "false", mousepad_window_action_toolbar },
  { "view.statusbar", NULL, NULL, "false", mousepad_window_action_statusbar },

  { "view.fullscreen", NULL, NULL, "false", mousepad_window_action_fullscreen },

  /* "Document" menu */
  { "document", NULL, NULL, "false", mousepad_window_update_gomenu },
    { "document.word-wrap", NULL, NULL, "false", mousepad_window_action_word_wrap },
    { "document.auto-indent", NULL, NULL, "false", mousepad_window_action_auto_indent },
    /* "Tab Size" submenu */
      { "document.tab.tab-size", mousepad_window_action_tab_size, "i", "2", NULL },
      { "document.tab.insert-spaces", NULL, NULL, "false", mousepad_window_action_insert_spaces },

    /* "Filetype" submenu */
      { "document.filetype", mousepad_window_action_language, "s", "'plain-text'", NULL },
    /* "Line Ending" submenu */
      { "document.line-ending", mousepad_window_action_line_ending, "i", "0", NULL },

    { "document.write-unicode-bom", NULL, NULL, "false", mousepad_window_action_write_bom },

    { "document.previous-tab", mousepad_window_action_prev_tab, NULL, NULL, NULL },
    { "document.next-tab", mousepad_window_action_next_tab, NULL, NULL, NULL },

    { "document.go-to-tab", mousepad_window_action_go_to_tab, "i", "0", NULL },

  /* "Help" menu */
  { "help.contents", mousepad_window_action_contents, NULL, NULL, NULL },
  { "help.about", mousepad_window_action_about, NULL, NULL, NULL }
};



/* menubar tooltips */
/*
 * Tooltips display in the status bar is based on the indices of the following array,
 * so if you change it, think to change also the pieces of code that use it.
 */
static const gchar *menubar_tooltips[] =
{
  /* "File" menu */
  NULL,
    "Create a new document", /* 1, toolbar item 1 */
    "Create a new document in a new window",
    NULL, /* 3, template menu insertion flag */

    "Open a file", /* 4, toolbar item 2 */
    NULL, /* 5, recent menu insertion flag */
      "Clear the recently used files history",

    "Save the current document", /* 7, tab menu start, toolbar item 3 */
    "Save current document as another file", /* 8, tab menu 2, toolbar item 4 */
    "Save all document in this window",
    "Revert to the saved version of the file", /* 10, tab menu 3, toolbar item 5 */

    "Print the current document",

    "Move the current document to a new window", /* 12, tab menu 4 */

    "Close the current document", /* 13, tab menu end, toolbar item 6 */
    "Close this window",

  /* "Edit" menu */
  NULL,
    "Undo the last action", /* 16, textview menu start, toolbar item 7 */
    "Redo the last undone action", /* 17, toolbar item 8 */

    "Cut the selection", /* 18, toolbar item 9 */
    "Copy the selection", /* 19, toolbar item 10 */
    "Paste the clipboard", /* 20, toolbar item 11 */
    /* "Paste Special" submenu */
    NULL,
      "Paste from the clipboard history",
      "Paste the clipboard text into a column",
    "Delete the current selection",

    "Select the text in the entire document",

    /* "Convert" submenu */
    NULL,
      "Change the case of the selection to lowercase",
      "Change the case of the selection to uppercase",
      "Change the case of the selection to title case",
      "Change the case of the selection opposite case",

      "Convert all tabs to spaces in the selection or document",
      "Convert all the leading spaces to tabs in the selected line(s) or document",

      "Remove all the trailing spaces from the selected line(s) or document",

      "Reverse the order of something",
    /* "Move selection" submenu */
    NULL,
      "Move the selection one line up",
      "Move the selection one line down",
    "Duplicate the current line or selection",
    "Increase the indentation of the selection or current line",
    "Decrease the indentation of the selection or current line", /* 40, textview menu end */

    "Show the preferences dialog",

  /* "Search" menu */
  NULL,
    "Search for text", /* 43, toolbar item 12 */
    "Search forwards for the same text",
    "Search backwards for the same text",
    "Search for and replace text", /* 46, toolbar item 13 */

    "Go to a specific location in the document", /* 47, toolbar item 14 */

  /* "View" menu */
  NULL, /* 48, view menu insertion flag */
    "Change the editor font",

    /* "Color Scheme" submenu */
    NULL, /* 50, style sheme menu insertion flag */
    "Show line numbers",

    "Change the visibility of the main menubar", /* 52, textview menu additional item */
    "Change the visibility of the toolbar",
    "Change the visibility of the statusbar",

    "Make the window fullscreen", /* 55, toolbar item 15 */

  /* "Document" menu */
  NULL, /* 56, document menu insertion flag */
    "Toggle breaking lines in between words",
    "Auto indent a new line",
    /* "Tab Size" submenu */
    NULL, /* 59, tab size menu insertion flag */
      NULL,
      NULL,
      NULL,
      NULL,
      "Set custom tab size", /* 64, custom tab size tooltip */

      "Insert spaces when the tab button is pressed",

    /* "Filetype" submenu */
    NULL, /* 66, languages menu insertion flag */
    /* "Line Ending" submenu */
    NULL,
      "Set the line ending of the document to Unix (LF)",
      "Set the line ending of the document to Mac (CR)",
      "Set the line ending of the document to DOS / Windows (CR LF)",

    "Store the byte-order mark in the file",

    "Select the previous tab",
    "Select the next tab",

  /* "Help" menu */
  NULL,
    "Display the Mousepad user manual",
    "About this application"
};



/* global variables */
static guint   window_signals[LAST_SIGNAL];
static gint    lock_menu_updates = 0;
static GSList *clipboard_history = NULL;
static guint   clipboard_history_ref_count = 0;
static gchar  *last_save_location = NULL;
static guint   last_save_location_ref_count = 0;



G_DEFINE_TYPE (MousepadWindow, mousepad_window, GTK_TYPE_APPLICATION_WINDOW)



GtkWidget *
mousepad_window_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_WINDOW, NULL);
}



static void
mousepad_window_class_init (MousepadWindowClass *klass)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *gtkwidget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = mousepad_window_dispose;
  gobject_class->finalize = mousepad_window_finalize;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->configure_event = mousepad_window_configure_event;

  window_signals[NEW_WINDOW] =
    g_signal_new (I_("new-window"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  window_signals[NEW_WINDOW_WITH_DOCUMENT] =
    g_signal_new (I_("new-window-with-document"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  _mousepad_marshal_VOID__OBJECT_INT_INT,
                  G_TYPE_NONE, 3,
                  G_TYPE_OBJECT,
                  G_TYPE_INT, G_TYPE_INT);
}



/* Called in response to any settings changed which affect the statusbar labels. */
static void
mousepad_window_update_statusbar_settings (MousepadWindow *window,
                                           gchar          *key,
                                           GSettings      *settings)
{
  if (G_LIKELY (MOUSEPAD_IS_DOCUMENT (window->active)))
    mousepad_document_send_signals (window->active);
}



/* Called in response to any setting changed which affects the window title. */
static void
mousepad_window_update_window_title (MousepadWindow *window,
                                     gchar          *key,
                                     GSettings      *settings)
{
  mousepad_window_set_title (window);
}



/* Called when always-show-tabs setting changes to update the UI. */
static void
mousepad_window_update_tabs (MousepadWindow *window,
                             gchar          *key,
                             GSettings      *settings)
{
  gint     n_pages;
  gboolean always_show;

  always_show = MOUSEPAD_SETTING_GET_BOOLEAN (ALWAYS_SHOW_TABS);

  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (window->notebook),
                              (n_pages > 1 || always_show) ? TRUE : FALSE);
}



static void
mousepad_window_update_toolbar (MousepadWindow *window,
                                gchar          *key,
                                GSettings      *settings)
{
  GtkToolbarStyle style;
  GtkIconSize     size;

  style = MOUSEPAD_SETTING_GET_ENUM (TOOLBAR_STYLE);
  size = MOUSEPAD_SETTING_GET_ENUM (TOOLBAR_ICON_SIZE);

  gtk_toolbar_set_style (GTK_TOOLBAR (window->toolbar), style);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (window->toolbar), size);
}



static void
mousepad_window_restore (MousepadWindow *window)
{
  gboolean remember_size, remember_position, remember_state;

  remember_size = MOUSEPAD_SETTING_GET_BOOLEAN (REMEMBER_SIZE);
  remember_position = MOUSEPAD_SETTING_GET_BOOLEAN (REMEMBER_POSITION);
  remember_state = MOUSEPAD_SETTING_GET_BOOLEAN (REMEMBER_STATE);

  /* first restore size */
  if (remember_size)
    {
      gint width, height;

      width = MOUSEPAD_SETTING_GET_INT (WINDOW_WIDTH);
      height = MOUSEPAD_SETTING_GET_INT (WINDOW_HEIGHT);

      gtk_window_set_default_size (GTK_WINDOW (window), width, height);
    }

  /* then restore position */
  if (remember_position)
    {
      gint left, top;

      left = MOUSEPAD_SETTING_GET_INT (WINDOW_LEFT);
      top = MOUSEPAD_SETTING_GET_INT (WINDOW_TOP);

      gtk_window_move (GTK_WINDOW (window), left, top);
    }

  /* finally restore window state */
  if (remember_state)
    {
      gboolean maximized, fullscreen;

      maximized = MOUSEPAD_SETTING_GET_BOOLEAN (WINDOW_MAXIMIZED);
      fullscreen = MOUSEPAD_SETTING_GET_BOOLEAN (WINDOW_FULLSCREEN);

      /* first restore if it was maximized */
      if (maximized)
        gtk_window_maximize (GTK_WINDOW (window));

      /* then restore if it was fullscreen */
      if (fullscreen)
        gtk_window_fullscreen (GTK_WINDOW (window));
    }
}



static void
mousepad_window_create_contextual_menus (MousepadWindow *window)
{
  GtkBuilder  *builder;
  GMenuModel  *model;
  GPtrArray   *tooltips;
  gint         textview_menu_indices[] = { 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                                           26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
                                           36, 37, 38, 39, 40, 52 };
  gint         tab_menu_indices[] = { 7, 8, 10, 12, 13 };
  guint        index;

  builder = mousepad_application_get_builder (MOUSEPAD_APPLICATION (
              gtk_window_get_application (GTK_WINDOW (window))));

  /* create textview menu */
  model = G_MENU_MODEL (gtk_builder_get_object (builder, "textview-menu"));
  window->textview_menu = gtk_menu_new_from_model (model);
  gtk_menu_attach_to_widget (GTK_MENU (window->textview_menu),
                             GTK_WIDGET (window), NULL);

  /* set textview menu tooltips */
  tooltips = g_ptr_array_new ();
  for (index = 0; index < G_N_ELEMENTS (textview_menu_indices); index++)
    g_ptr_array_add (tooltips, (gpointer) menubar_tooltips[index]);
  index = 0;
  mousepad_window_menu_set_tooltips_full (window, window->textview_menu, tooltips, &index);
  g_ptr_array_free (tooltips, TRUE);

  /* create tab menu */
  model = G_MENU_MODEL (gtk_builder_get_object (builder, "tab-menu"));
  window->tab_menu = gtk_menu_new_from_model (model);
  gtk_menu_attach_to_widget (GTK_MENU (window->tab_menu),
                             GTK_WIDGET (window), NULL);

  /* set tab menu tooltips */
  tooltips = g_ptr_array_new ();
  for (index = 0; index < G_N_ELEMENTS (tab_menu_indices); index++)
    g_ptr_array_add (tooltips, (gpointer) menubar_tooltips[index]);
  index = 0;
  mousepad_window_menu_set_tooltips_full (window, window->tab_menu, tooltips, &index);
  g_ptr_array_free (tooltips, TRUE);

  /* create languages menu */
  window->languages_menu = mousepad_window_get_menubar_submenu (window, window->menubar,
                                                                "languages-menu-flag");
}



void
mousepad_window_create_menubar (MousepadWindow *window)
{
  MousepadApplication  *application;
  GtkBuilder           *builder;
  GMenuModel           *model;
  GMenu                *menu;
  GAction              *action;
  GtkWidget            *gtkmenu, *languages_menu, *style_schemes_menu;
  GPtrArray            *tooltips;
  guint                 index, show_fullscreen;
  gboolean              show;

  /* create the base menubar from the gtkbuilder menu model */
  application = MOUSEPAD_APPLICATION (gtk_window_get_application (GTK_WINDOW (window)));
  builder = mousepad_application_get_builder (application);
  model = gtk_application_get_menubar (GTK_APPLICATION (application));
  window->menubar = gtk_menu_bar_new_from_model (model);

  /* empty the dynamical submenus to recover the original menubar tooltips numbering
   * if this window is not the first one */
  menu = G_MENU (gtk_builder_get_object (builder, "file.new-from-template"));
  g_menu_remove_all (menu);
  menu = G_MENU (gtk_builder_get_object (builder, "file.open-recent.list"));
  g_menu_remove_all (menu);
  menu = G_MENU (gtk_builder_get_object (builder, "document.go-to-tab"));
  g_menu_remove_all (menu);

  /* set insertion flags for some submenus to extract, move, or update subsequently */
  index = 0;
  mousepad_window_menubar_set_insertion_flags (window, window->menubar, &index);

  /* move out temporarily the languages and style schemes submenus to work on the basic menubar */
  gtkmenu = mousepad_window_get_menubar_submenu (window, window->menubar, "languages-menu-flag");
  languages_menu = gtk_menu_new ();
  mousepad_util_container_move_children (GTK_CONTAINER (gtkmenu), GTK_CONTAINER (languages_menu));

  gtkmenu = mousepad_window_get_menubar_submenu (window, window->menubar, "style-schemes-menu-flag");
  style_schemes_menu = gtk_menu_new ();
  mousepad_util_container_move_children (GTK_CONTAINER (gtkmenu), GTK_CONTAINER (style_schemes_menu));

  /* set tooltips and connect handlers to the basic menubar items signals */
  tooltips = g_ptr_array_new ();
  for (index = 0; index < G_N_ELEMENTS (menubar_tooltips); index++)
    g_ptr_array_add (tooltips, (gpointer) menubar_tooltips[index]);
  index = 0;
  mousepad_window_menu_set_tooltips_full (window, window->menubar, tooltips, &index);
  g_ptr_array_free (tooltips, TRUE);

  /* set the languages menu tooltips and put it back in place in the menubar */
  tooltips = mousepad_application_get_languages_tooltips (application);
  index = 0;
  mousepad_window_menu_set_tooltips_full (window, languages_menu, tooltips, &index);

  gtkmenu = mousepad_window_get_menubar_submenu (window, window->menubar, "languages-menu-flag");
  mousepad_util_container_move_children (GTK_CONTAINER (languages_menu), GTK_CONTAINER (gtkmenu));
  gtk_widget_destroy (languages_menu);

  /* set the style schemes menu tooltips and put it back in place in the menubar */
  tooltips = mousepad_application_get_style_schemes_tooltips (application);
  index = 0;
  mousepad_window_menu_set_tooltips_full (window, style_schemes_menu, tooltips, &index);

  gtkmenu = mousepad_window_get_menubar_submenu (window, window->menubar, "style-schemes-menu-flag");
  mousepad_util_container_move_children (GTK_CONTAINER (style_schemes_menu), GTK_CONTAINER (gtkmenu));
  gtk_widget_destroy (style_schemes_menu);

  /* insert the menubar in its previously reserved space */
  gtk_box_pack_start (GTK_BOX (window->menubar_box), window->menubar, TRUE, TRUE, 0);

  /* now we can crate contextual menus */
  mousepad_window_create_contextual_menus (window);

  /* sync the menubar visibility and action state to the setting */
  if (MOUSEPAD_SETTING_GET_BOOLEAN (WINDOW_FULLSCREEN))
    {
      show_fullscreen = MOUSEPAD_SETTING_GET_ENUM (MENUBAR_VISIBLE_FULLSCREEN);
      if (! show_fullscreen)
        show = MOUSEPAD_SETTING_GET_BOOLEAN (MENUBAR_VISIBLE);
      else
        show = (show_fullscreen == 2);
    }
  else
    show = MOUSEPAD_SETTING_GET_BOOLEAN (MENUBAR_VISIBLE);

  action = g_action_map_lookup_action (G_ACTION_MAP (window), "view.menubar");
  g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (show));
  action = g_action_map_lookup_action (G_ACTION_MAP (window), "textview.menubar");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), ! show);

  /* set the textview menu last tooltip */
  if (! show)
    {
      tooltips = g_ptr_array_new ();
      g_ptr_array_add (tooltips, (gpointer) menubar_tooltips[52]);
      mousepad_window_menu_set_tooltips (window, window->textview_menu, tooltips, 1, 0);
      g_ptr_array_free (tooltips, TRUE);
    }

  gtk_widget_set_visible (window->menubar, show);

  MOUSEPAD_SETTING_CONNECT_OBJECT (MENUBAR_VISIBLE,
                                   G_CALLBACK (mousepad_window_update_main_widgets),
                                   window, G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (MENUBAR_VISIBLE_FULLSCREEN,
                                   G_CALLBACK (mousepad_window_update_main_widgets),
                                   window, G_CONNECT_SWAPPED);
}



static void
mousepad_window_toolbar_insert (MousepadWindow *window,
                                const gchar    *label,
                                const gchar    *icon_name,
                                const gchar    *tooltip,
                                const gchar    *action_name)
{
  GtkToolItem *item;
  GtkWidget   *child;

  item = gtk_tool_button_new (NULL, label);
  gtk_tool_button_set_use_underline (GTK_TOOL_BUTTON (item), TRUE);
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), icon_name);
  gtk_tool_item_set_tooltip_text (item, tooltip);

  /* make the widget actionable just as the corresponding menu item */
  gtk_actionable_set_action_name (GTK_ACTIONABLE (item), action_name);

  /* tool items will have GtkButton or other widgets in them, we want the child */
  child = gtk_bin_get_child (GTK_BIN (item));

  /* get events for mouse enter/leave and focus in/out */
  gtk_widget_add_events (child,
                         GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK
                         | GDK_FOCUS_CHANGE_MASK);

  /* connect to signals for the events to show the tooltip in the status bar */
  g_signal_connect_object (child, "enter-notify-event",
                           G_CALLBACK (mousepad_window_tool_item_enter_event),
                           window, 0);
  g_signal_connect_object (child, "leave-notify-event",
                           G_CALLBACK (mousepad_window_tool_item_leave_event),
                           window, 0);
  g_signal_connect_object (child, "focus-in-event",
                           G_CALLBACK (mousepad_window_tool_item_enter_event),
                           window, 0);
  g_signal_connect_object (child, "focus-out-event",
                           G_CALLBACK (mousepad_window_tool_item_leave_event),
                           window, 0);

  /* append the item to the end of the toolbar */
  gtk_toolbar_insert (GTK_TOOLBAR (window->toolbar), item, -1);
}



static void
mousepad_window_create_toolbar (MousepadWindow *window)
{
  GtkToolItem *item;
  GAction     *action;
  gboolean     active;

  /* create the toolbar and set the main properties */
  window->toolbar = gtk_toolbar_new ();
  gtk_toolbar_set_style (GTK_TOOLBAR (window->toolbar), GTK_TOOLBAR_ICONS);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (window->toolbar), GTK_ICON_SIZE_SMALL_TOOLBAR);

  /* insert items */
  mousepad_window_toolbar_insert (window, _("_New"), "document-new",
                                  _(menubar_tooltips[1]), "win.file.new");
  mousepad_window_toolbar_insert (window, _("_Open..."), "document-open",
                                  _(menubar_tooltips[4]), "win.file.open");
  mousepad_window_toolbar_insert (window, _("_Save"), "document-save",
                                  _(menubar_tooltips[7]), "win.file.save");
  mousepad_window_toolbar_insert (window, _("Save _As..."), "document-save-as",
                                  _(menubar_tooltips[8]), "win.file.save-as");
  mousepad_window_toolbar_insert (window, _("Re_vert"), "document-revert",
                                  _(menubar_tooltips[10]), "win.file.revert");
  mousepad_window_toolbar_insert (window, _("Close _Tab"), "window-close",
                                  _(menubar_tooltips[13]), "win.file.close-tab");

  item = gtk_separator_tool_item_new ();
  gtk_toolbar_insert (GTK_TOOLBAR (window->toolbar), item, -1);

  mousepad_window_toolbar_insert (window, _("_Undo"), "edit-undo",
                                  _(menubar_tooltips[16]), "win.edit.undo");
  mousepad_window_toolbar_insert (window, _("_Redo"), "edit-redo",
                                  _(menubar_tooltips[17]), "win.edit.redo");
  mousepad_window_toolbar_insert (window, _("Cu_t"), "edit-cut",
                                  _(menubar_tooltips[18]), "win.edit.cut");
  mousepad_window_toolbar_insert (window, _("_Copy"), "edit-copy",
                                  _(menubar_tooltips[19]), "win.edit.copy");
  mousepad_window_toolbar_insert (window, _("_Paste"), "edit-paste",
                                  _(menubar_tooltips[20]), "win.edit.paste");

  item = gtk_separator_tool_item_new ();
  gtk_toolbar_insert (GTK_TOOLBAR (window->toolbar), item, -1);

  mousepad_window_toolbar_insert (window, _("_Find"), "edit-find",
                                  _(menubar_tooltips[43]), "win.search.find");
  mousepad_window_toolbar_insert (window, _("Find and Rep_lace..."), "edit-find-replace",
                                  _(menubar_tooltips[46]), "win.search.find-and-replace");
  mousepad_window_toolbar_insert (window, _("_Go to..."), "go-jump",
                                  _(menubar_tooltips[47]), "win.search.go-to");

  /* make the last toolbar separator so it expands properly */
  item = gtk_separator_tool_item_new ();
  gtk_toolbar_insert (GTK_TOOLBAR (window->toolbar), item, -1);
  gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (item), FALSE);
  gtk_tool_item_set_expand (item, TRUE);

  mousepad_window_toolbar_insert (window, _("_Fullscreen"), "view-fullscreen",
                                  _(menubar_tooltips[55]), "win.view.fullscreen");

  /* insert the toolbar in the main window box and show all widgets */
  gtk_box_pack_start (GTK_BOX (window->box), window->toolbar, FALSE, FALSE, 0);
  gtk_widget_show_all (window->toolbar);

  /* sync the toolbar visibility and action state to the setting */
  action = g_action_map_lookup_action (G_ACTION_MAP (window), "view.toolbar");
  if (MOUSEPAD_SETTING_GET_BOOLEAN (WINDOW_FULLSCREEN))
    {
      gint value = MOUSEPAD_SETTING_GET_ENUM (TOOLBAR_VISIBLE_FULLSCREEN);
      active = (value == 0) ? MOUSEPAD_SETTING_GET_BOOLEAN (TOOLBAR_VISIBLE) : (value == 2);
    }
  else
    active = MOUSEPAD_SETTING_GET_BOOLEAN (TOOLBAR_VISIBLE);
  g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (active));
  gtk_widget_set_visible (window->toolbar, active);

  /* update the toolbar with the settings */
  mousepad_window_update_toolbar (window, NULL, NULL);

  /* connect to some signals to keep in sync */
  MOUSEPAD_SETTING_CONNECT_OBJECT (TOOLBAR_VISIBLE,
                                   G_CALLBACK (mousepad_window_update_main_widgets),
                                   window, G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (TOOLBAR_VISIBLE_FULLSCREEN,
                                   G_CALLBACK (mousepad_window_update_main_widgets),
                                   window, G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (TOOLBAR_STYLE,
                                   G_CALLBACK (mousepad_window_update_toolbar),
                                   window, G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (TOOLBAR_ICON_SIZE,
                                   G_CALLBACK (mousepad_window_update_toolbar),
                                   window, G_CONNECT_SWAPPED);
}



static void
mousepad_window_create_root_warning (MousepadWindow *window)
{
  /* check if we need to add the root warning */
  if (G_UNLIKELY (geteuid () == 0))
    {
      GtkWidget *ebox, *label, *separator;

  /* In GTK3, gtkrc is deprecated */
#if G_GNUC_CHECK_VERSION (4, 3)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

      /* install default settings for the root warning text box */
      gtk_rc_parse_string ("style\"mousepad-window-root-style\"\n"
                             "{\n"
                               "bg[NORMAL]=\"#b4254b\"\n"
                               "fg[NORMAL]=\"#fefefe\"\n"
                             "}\n"
                           "widget\"MousepadWindow.*.root-warning\"style\"mousepad-window-root-style\"\n"
                           "widget\"MousepadWindow.*.root-warning.GtkLabel\"style\"mousepad-window-root-style\"\n");

#if G_GNUC_CHECK_VERSION (4, 3)
# pragma GCC diagnostic pop
#endif

      /* add the box for the root warning */
      ebox = gtk_event_box_new ();
      gtk_widget_set_name (ebox, "root-warning");
      gtk_box_pack_start (GTK_BOX (window->box), ebox, FALSE, FALSE, 0);
      gtk_widget_show (ebox);

      /* add the label with the root warning */
      label = gtk_label_new (_("Warning: you are using the root account. You may harm your system."));
      gtk_widget_set_margin_start (label, 6);
      gtk_widget_set_margin_end (label, 6);
      gtk_widget_set_margin_top (label, 3);
      gtk_widget_set_margin_bottom (label, 3);
      gtk_container_add (GTK_CONTAINER (ebox), label);
      gtk_widget_show (label);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_pack_start (GTK_BOX (window->box), separator, FALSE, FALSE, 0);
      gtk_widget_show (separator);
    }
}



static void
mousepad_window_create_notebook (MousepadWindow *window)
{
  window->notebook = g_object_new (GTK_TYPE_NOTEBOOK,
                                   "scrollable", TRUE,
                                   "show-border", FALSE,
                                   "show-tabs", FALSE,
                                   NULL);

  /* set the group id */
  gtk_notebook_set_group_name (GTK_NOTEBOOK (window->notebook), NOTEBOOK_GROUP);

  /* connect signals to the notebooks */
  g_signal_connect (G_OBJECT (window->notebook), "switch-page",
                    G_CALLBACK (mousepad_window_notebook_switch_page), window);
  g_signal_connect (G_OBJECT (window->notebook), "page-added",
                    G_CALLBACK (mousepad_window_notebook_added), window);
  g_signal_connect (G_OBJECT (window->notebook), "page-removed",
                    G_CALLBACK (mousepad_window_notebook_removed), window);
  g_signal_connect (G_OBJECT (window->notebook), "button-press-event",
                    G_CALLBACK (mousepad_window_notebook_button_press_event), window);
  g_signal_connect (G_OBJECT (window->notebook), "button-release-event",
                    G_CALLBACK (mousepad_window_notebook_button_release_event), window);
  g_signal_connect (G_OBJECT (window->notebook), "create-window",
                    G_CALLBACK (mousepad_window_notebook_create_window), window);

  /* append and show the notebook */
  gtk_box_pack_start (GTK_BOX (window->box), window->notebook, TRUE, TRUE, PADDING);
  gtk_widget_show (window->notebook);
}



static void
mousepad_window_create_statusbar (MousepadWindow *window)
{
  GAction  *action;
  gboolean  active;

  /* setup a new statusbar */
  window->statusbar = mousepad_statusbar_new ();

  /* sync the statusbar visibility and action state to the setting */
  action = g_action_map_lookup_action (G_ACTION_MAP (window), "view.statusbar");
  if (MOUSEPAD_SETTING_GET_BOOLEAN (WINDOW_FULLSCREEN))
    {
      gint value = MOUSEPAD_SETTING_GET_ENUM (STATUSBAR_VISIBLE_FULLSCREEN);
      active = (value == 0) ? MOUSEPAD_SETTING_GET_BOOLEAN (STATUSBAR_VISIBLE) : (value == 2);
    }
  else
    active = MOUSEPAD_SETTING_GET_BOOLEAN (STATUSBAR_VISIBLE);
  g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (active));
  gtk_widget_set_visible (window->statusbar, active);

#if !GTK_CHECK_VERSION (4, 0, 0)
  /* make the statusbar smaller */
  /*
   * To fix hard-coded, oversized GTK+3 status bar padding, see:
   * https://gitlab.gnome.org/GNOME/gtk/-/commit/94ebe2106817f6bc7aaf868bd00d0fc381d33e7e
   * Fixed in GTK+4:
   * https://gitlab.gnome.org/GNOME/gtk/-/commit/1a7cbddbd4e98e4641e690035013abbfaec130b0
   */
  gtk_widget_set_margin_top (window->statusbar, 0);
  gtk_widget_set_margin_bottom (window->statusbar, 0);
#endif

  /* pack the statusbar into the window UI */
  gtk_box_pack_end (GTK_BOX (window->box), window->statusbar, FALSE, FALSE, 0);

  /* overwrite toggle signal */
  g_signal_connect_swapped (G_OBJECT (window->statusbar), "enable-overwrite",
                            G_CALLBACK (mousepad_window_action_statusbar_overwrite), window);

  /* update the statusbar items */
  if (MOUSEPAD_IS_DOCUMENT (window->active))
    mousepad_document_send_signals (window->active);

  /* update the statusbar with certain settings */
  MOUSEPAD_SETTING_CONNECT_OBJECT (TAB_WIDTH,
                                   G_CALLBACK (mousepad_window_update_statusbar_settings),
                                   window, G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (INSERT_SPACES,
                                   G_CALLBACK (mousepad_window_update_statusbar_settings),
                                   window, G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (STATUSBAR_VISIBLE,
                                   G_CALLBACK (mousepad_window_update_main_widgets),
                                   window, G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (STATUSBAR_VISIBLE_FULLSCREEN,
                                   G_CALLBACK (mousepad_window_update_main_widgets),
                                   window, G_CONNECT_SWAPPED);
}



static void
mousepad_window_init (MousepadWindow *window)
{
  GAction *action;

  /* initialize stuff */
  window->save_geometry_timer_id = 0;
  window->fullscreen_bars_timer_id = 0;
  window->search_bar = NULL;
  window->statusbar = NULL;
  window->replace_dialog = NULL;
  window->active = NULL;
  window->recent_manager = NULL;

  /* increase clipboard history ref count */
  clipboard_history_ref_count++;

  /* increase last save location ref count */
  last_save_location_ref_count++;

  /* signal for handling the window delete event */
  g_signal_connect (G_OBJECT (window), "delete-event",
                    G_CALLBACK (mousepad_window_delete_event), NULL);

  /* restore window settings */
  mousepad_window_restore (window);

  /* match the window to its actions */
  g_action_map_add_action_entries (G_ACTION_MAP (window), action_entries,
                                   G_N_ELEMENTS (action_entries), window);

  /* disable the "insensitive" action to make menu items that use it insensitive */
  action = g_action_map_lookup_action (G_ACTION_MAP (window), "insensitive");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

  /* create the main table */
  window->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), window->box);
  gtk_widget_show (window->box);

  /* keep a place for the menubar created from the gtkbuilder later */
  window->menubar_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (window->box), window->menubar_box, FALSE, FALSE, 0);
  gtk_widget_show (window->menubar_box);

  /* create the toolbar */
  mousepad_window_create_toolbar (window);

  /* create the root-warning bar (if needed) */
  mousepad_window_create_root_warning (window);

  /* create the notebook */
  mousepad_window_create_notebook (window);

  /* create the statusbar */
  mousepad_window_create_statusbar (window);

  /* allow drops in the window */
  gtk_drag_dest_set (GTK_WIDGET (window),
                     GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
                     drop_targets,
                     G_N_ELEMENTS (drop_targets), GDK_ACTION_COPY | GDK_ACTION_MOVE);
  g_signal_connect (G_OBJECT (window), "drag-data-received",
                    G_CALLBACK (mousepad_window_drag_data_received), window);

  /* update the window title when 'path-in-title' setting changes */
  MOUSEPAD_SETTING_CONNECT_OBJECT (PATH_IN_TITLE,
                                   G_CALLBACK (mousepad_window_update_window_title),
                                   window, G_CONNECT_SWAPPED);

  /* update the tabs when 'always-show-tabs' setting changes */
  MOUSEPAD_SETTING_CONNECT_OBJECT (ALWAYS_SHOW_TABS,
                                   G_CALLBACK (mousepad_window_update_tabs),
                                   window, G_CONNECT_SWAPPED);

  /* sync the fullscreen action state to its setting */
  mousepad_window_update_fullscreen_action (window);

  /* sync the view action states to their settings */
  MOUSEPAD_SETTING_CONNECT_OBJECT (SHOW_LINE_NUMBERS,
                                   G_CALLBACK (mousepad_window_update_line_numbers_action),
                                   window, G_CONNECT_SWAPPED);

  /* sync the document action states to their settings */
  MOUSEPAD_SETTING_CONNECT_OBJECT (WORD_WRAP,
                                   G_CALLBACK (mousepad_window_update_document_actions),
                                   window, G_CONNECT_SWAPPED);
  MOUSEPAD_SETTING_CONNECT_OBJECT (AUTO_INDENT,
                                   G_CALLBACK (mousepad_window_update_document_actions),
                                   window, G_CONNECT_SWAPPED);
  MOUSEPAD_SETTING_CONNECT_OBJECT (TAB_WIDTH,
                                   G_CALLBACK (mousepad_window_update_document_actions),
                                   window, G_CONNECT_SWAPPED);
  MOUSEPAD_SETTING_CONNECT_OBJECT (INSERT_SPACES,
                                   G_CALLBACK (mousepad_window_update_document_actions),
                                   window, G_CONNECT_SWAPPED);

  /* set the initial style scheme from the setting */
  mousepad_window_update_color_scheme_action (window);

  /* update the colour scheme when the setting changes */
  MOUSEPAD_SETTING_CONNECT_OBJECT (COLOR_SCHEME,
                                   G_CALLBACK (mousepad_window_update_color_scheme_action),
                                   window, G_CONNECT_SWAPPED);
}



static void
mousepad_window_dispose (GObject *object)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (object);

  /* destroy the save geometry timer source */
  if (G_UNLIKELY (window->save_geometry_timer_id != 0))
    g_source_remove (window->save_geometry_timer_id);

  /* destroy the fullscreen bars timer source */
  if (G_UNLIKELY (window->fullscreen_bars_timer_id != 0))
    g_source_remove (window->fullscreen_bars_timer_id);

  (*G_OBJECT_CLASS (mousepad_window_parent_class)->dispose) (object);
}



static void
mousepad_window_finalize (GObject *object)
{
  /* decrease history clipboard ref count */
  clipboard_history_ref_count--;

  /* decrease last save location ref count */
  last_save_location_ref_count--;

  /* free clipboard history if needed */
  if (clipboard_history_ref_count == 0 && clipboard_history != NULL)
    g_slist_free_full (clipboard_history, g_free);

  /* free last save location if needed */
  if (last_save_location_ref_count == 0 && last_save_location != NULL)
    {
      g_free (last_save_location);
      last_save_location = NULL;
    }

  (*G_OBJECT_CLASS (mousepad_window_parent_class)->finalize) (object);
}



static gboolean
mousepad_window_configure_event (GtkWidget         *widget,
                                 GdkEventConfigure *event)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (widget);
  GtkAllocation   alloc = { 0, 0, 0, 0 };

  gtk_widget_get_allocation (widget, &alloc);

  /* check if we have a new dimension here */
  if (alloc.width != event->width ||
      alloc.height != event->height ||
      alloc.x != event->x ||
      alloc.y != event->y)
    {
      /* drop any previous timer source */
      if (window->save_geometry_timer_id > 0)
        g_source_remove (window->save_geometry_timer_id);

      /* check if we should schedule another save timer */
      if (gtk_widget_get_visible (widget))
        {
          /* save the geometry one second after the last configure event */
          window->save_geometry_timer_id = g_timeout_add_full (G_PRIORITY_LOW, 1000,
                                                               mousepad_window_save_geometry_timer,
                                                               window,
                                                               mousepad_window_save_geometry_timer_destroy);
        }
    }

  /* let gtk+ handle the configure event */
  return (*GTK_WIDGET_CLASS (mousepad_window_parent_class)->configure_event) (widget, event);
}



/**
 * Statusbar Tooltip Functions
 **/
static void
mousepad_window_menu_item_selected (GtkWidget      *menu_item,
                                    MousepadWindow *window)
{
  gchar *tooltip;

  tooltip = gtk_widget_get_tooltip_text (menu_item);
  mousepad_statusbar_push_tooltip (MOUSEPAD_STATUSBAR (window->statusbar), tooltip);
  g_free (tooltip);
}



static void
mousepad_window_menu_item_deselected (GtkWidget      *menu_item,
                                      MousepadWindow *window)
{
  mousepad_statusbar_pop_tooltip (MOUSEPAD_STATUSBAR (window->statusbar));
}



static gboolean
mousepad_window_tool_item_enter_event (GtkWidget      *tool_item,
                                       GdkEvent       *event,
                                       MousepadWindow *window)
{
  gchar *tooltip;

  tooltip = gtk_widget_get_tooltip_text (tool_item);
  mousepad_statusbar_push_tooltip (MOUSEPAD_STATUSBAR (window->statusbar), tooltip);
  g_free (tooltip);

  return FALSE;
}



static gboolean
mousepad_window_tool_item_leave_event (GtkWidget      *tool_item,
                                       GdkEvent       *event,
                                       MousepadWindow *window)
{
  mousepad_statusbar_pop_tooltip (MOUSEPAD_STATUSBAR (window->statusbar));

  return FALSE;
}



static void
mousepad_window_menu_set_tooltips_full (MousepadWindow  *window,
                                        GtkWidget       *menu,
                                        GPtrArray       *tooltips,
                                        guint           *index)
{
  GtkWidget   *submenu;
  GList       *children, *child;
  const gchar *tooltip;

  children = gtk_container_get_children (GTK_CONTAINER (menu));

  for (child = children; child != NULL; child = child->next)
    {
      /* set tooltip and connect handlers to the menu item select/deselect signals */
      if (! GTK_IS_SEPARATOR_MENU_ITEM (child->data))
        {
          tooltip = _((const gchar*) g_ptr_array_index (tooltips, (*index)++));
          gtk_widget_set_tooltip_text (child->data, tooltip);
          gtk_widget_set_has_tooltip (child->data, FALSE);
          g_signal_connect_object (child->data, "select",
                                   G_CALLBACK (mousepad_window_menu_item_selected),
                                   window, 0);
          g_signal_connect_object (child->data, "deselect",
                                   G_CALLBACK (mousepad_window_menu_item_deselected),
                                   window, 0);
        }

      /* go ahead recursively */
      submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (child->data));
      if (submenu)
        mousepad_window_menu_set_tooltips_full (window, submenu, tooltips, index);
    }

  g_list_free (children);
}



static void
mousepad_window_menubar_set_insertion_flags (MousepadWindow  *window,
                                             GtkWidget       *menu,
                                             guint           *index)
{
  MousepadApplication *application;
  GtkWidget           *submenu;
  GList               *children, *child;
  guint                n_style_schemes, document_menu_index,
                       tab_size_menu_index, languages_menu_index;

  /* take into account the style schemes menu insertion in the basic menubar */
  application = MOUSEPAD_APPLICATION (gtk_window_get_application (GTK_WINDOW (window)));
  n_style_schemes = mousepad_application_get_n_style_schemes (application);
  document_menu_index = 56 + n_style_schemes;
  tab_size_menu_index = 59 + n_style_schemes;
  languages_menu_index = 66 + n_style_schemes;

  children = gtk_container_get_children (GTK_CONTAINER (menu));

  for (child = children; child != NULL; child = child->next)
    {
      if (! GTK_IS_SEPARATOR_MENU_ITEM (child->data))
        {
          /* set insertion flag using the "name" widget property */
          if (*index == 3)
            gtk_widget_set_name (child->data, "template-menu-flag");
          else if (*index == 5)
            gtk_widget_set_name (child->data, "recent-menu-flag");
          else if (*index == 48)
            gtk_widget_set_name (child->data, "view-menu-flag");
          else if (*index == 50)
            gtk_widget_set_name (child->data, "style-schemes-menu-flag");
          else if (*index == document_menu_index)
            gtk_widget_set_name (child->data, "document-menu-flag");
          else if (*index == tab_size_menu_index)
            gtk_widget_set_name (child->data, "tab-size-menu-flag");
          else if (*index == languages_menu_index)
            gtk_widget_set_name (child->data, "languages-menu-flag");
          else
            gtk_widget_set_name (child->data, NULL);

          (*index)++;
        }

      /* go ahead recursively */
      submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (child->data));
      if (submenu)
        mousepad_window_menubar_set_insertion_flags (window, submenu, index);
    }

  g_list_free (children);
}



static GtkWidget *
mousepad_window_get_menubar_submenu (MousepadWindow *window,
                                     GtkWidget      *menu,
                                     const gchar    *flag)
{
  GtkWidget   *submenu = NULL, *next_menu;
  GList       *children, *child;

  children = gtk_container_get_children (GTK_CONTAINER (menu));
  flag = g_intern_string (flag);

  for (child = children; child != NULL; child = child->next)
    {
      if (g_intern_string (gtk_widget_get_name (child->data)) == flag)
        {
          submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (child->data));
          break;
        }

      /* go ahead recursively */
      next_menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (child->data));
      if (next_menu)
        submenu = mousepad_window_get_menubar_submenu (window, next_menu, flag);
      if (submenu)
        break;
    }

  g_list_free (children);

  return submenu;
}



static void
mousepad_window_menu_set_tooltips (MousepadWindow  *window,
                                   GtkWidget       *menu,
                                   GPtrArray       *tooltips,
                                   guint            n_tooltips,
                                   guint            offset)
{
  GList       *children, *child;
  const gchar *tooltip;
  gint         n;

  children = gtk_container_get_children (GTK_CONTAINER (menu));
  child = g_list_last (children);

  while (offset--)
    child = child->prev;

  /* from bottom to top */
  for (n = n_tooltips - 1; n >= 0; n--)
    {
      tooltip = _((const gchar*) g_ptr_array_index (tooltips, n));
      gtk_widget_set_tooltip_text (child->data, tooltip);
      gtk_widget_set_has_tooltip (child->data, FALSE);
      g_signal_connect_object (child->data, "select",
                               G_CALLBACK (mousepad_window_menu_item_selected),
                               window, 0);
      g_signal_connect_object (child->data, "deselect",
                               G_CALLBACK (mousepad_window_menu_item_deselected),
                               window, 0);
      child = child->prev;
    }

  g_list_free (children);

}



/**
 * Save Geometry Functions
 **/
static gboolean
mousepad_window_save_geometry_timer (gpointer user_data)
{
  GdkWindowState   state;
  MousepadWindow  *window = MOUSEPAD_WINDOW (user_data);
  gboolean         remember_size, remember_position, remember_state;

  /* check if we should remember the window geometry */
  remember_size = MOUSEPAD_SETTING_GET_BOOLEAN (REMEMBER_SIZE);
  remember_position = MOUSEPAD_SETTING_GET_BOOLEAN (REMEMBER_POSITION);
  remember_state = MOUSEPAD_SETTING_GET_BOOLEAN (REMEMBER_STATE);

  if (remember_size || remember_position || remember_state)
    {
      /* check if the window is still visible */
      if (gtk_widget_get_visible (GTK_WIDGET (window)))
        {
          /* determine the current state of the window */
          state = gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window)));

          /* don't save geometry for maximized or fullscreen windows */
          if ((state & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)) == 0)
            {
              if (remember_size)
                {
                  gint width, height;

                  /* determine the current width/height of the window... */
                  gtk_window_get_size (GTK_WINDOW (window), &width, &height);

                  /* ...and remember them as default for new windows */
                  MOUSEPAD_SETTING_SET_INT (WINDOW_WIDTH, width);
                  MOUSEPAD_SETTING_SET_INT (WINDOW_HEIGHT, height);
                }

              if (remember_position)
                {
                  gint left, top;

                  /* determine the current left/top position of the window */
                  gtk_window_get_position (GTK_WINDOW (window), &left, &top);

                  /* and then remember it for next startup */
                  MOUSEPAD_SETTING_SET_INT (WINDOW_LEFT, left);
                  MOUSEPAD_SETTING_SET_INT (WINDOW_TOP, top);
                }
            }

          if (remember_state)
            {
              /* remember whether the window is maximized or full screen or not */
              MOUSEPAD_SETTING_SET_BOOLEAN (WINDOW_MAXIMIZED, (state & GDK_WINDOW_STATE_MAXIMIZED));
              MOUSEPAD_SETTING_SET_BOOLEAN (WINDOW_FULLSCREEN, (state & GDK_WINDOW_STATE_FULLSCREEN));
            }
        }
    }

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
mousepad_window_open_file (MousepadWindow   *window,
                           const gchar      *filename,
                           MousepadEncoding  encoding)
{
  MousepadDocument *document;
  GError           *error = NULL;
  gint              result;
  gint              npages = 0, i;
  gint              response;
  const gchar      *charset;
  const gchar      *opened_filename;
  GtkWidget        *dialog;
  gboolean          encoding_from_recent = FALSE;
  gchar            *uri;
  GtkRecentInfo    *info;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (filename != NULL && *filename != '\0', FALSE);

  /* check if the file is already openend */
  npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));
  for (i = 0; i < npages; i++)
    {
      document = MOUSEPAD_DOCUMENT (gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i));

      /* debug check */
      g_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), FALSE);

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

  /* make sure it's not a floating object */
  g_object_ref_sink (G_OBJECT (document));

  /* set the filename */
  mousepad_file_set_filename (document->file, filename);

  /* set the passed encoding */
  mousepad_file_set_encoding (document->file, encoding);

  retry:

  /* lock the undo manager */
  gtk_source_buffer_begin_not_undoable_action (GTK_SOURCE_BUFFER (document->buffer));

  /* read the content into the buffer */
  result = mousepad_file_open (document->file, NULL, &error);

  /* release the lock */
  gtk_source_buffer_end_not_undoable_action (GTK_SOURCE_BUFFER (document->buffer));

  switch (result)
    {
      case 0:
        /* add the document to the window */
        mousepad_window_add (window, document);

        /* insert in the recent history */
        mousepad_window_recent_add (window, document->file);
        break;

      case ERROR_CONVERTING_FAILED:
      case ERROR_NOT_UTF8_VALID:
        /* clear the error */
        g_clear_error (&error);

        /* try to lookup the encoding from the recent history */
        if (encoding_from_recent == FALSE)
          {
            /* we only try this once */
            encoding_from_recent = TRUE;

            /* make sure the recent manager is initialized */
            mousepad_window_recent_manager_init (window);

            /* build uri */
            uri = g_filename_to_uri (filename, NULL, NULL);

            /* try to lookup the recent item */
            if (G_LIKELY (uri))
              {
                info = gtk_recent_manager_lookup_item (window->recent_manager, uri, NULL);

                /* cleanup */
                g_free (uri);

                if (info)
                  {
                    /* try to find the encoding */
                    charset = mousepad_window_recent_get_charset (info);

                    encoding = mousepad_encoding_find (charset);

                    /* set the new encoding */
                    mousepad_file_set_encoding (document->file, encoding);

                    /* release */
                    gtk_recent_info_unref (info);

                    /* try to open again with the last used encoding */
                    if (G_LIKELY (encoding))
                      goto retry;
                  }
              }
          }

        /* run the encoding dialog */
        dialog = mousepad_encoding_dialog_new (GTK_WINDOW (window), document->file);

        /* run the dialog */
        response = gtk_dialog_run (GTK_DIALOG (dialog));

        if (response == GTK_RESPONSE_OK)
          {
            /* set the new encoding */
            encoding = mousepad_encoding_dialog_get_encoding (MOUSEPAD_ENCODING_DIALOG (dialog));

            /* set encoding */
            mousepad_file_set_encoding (document->file, encoding);
          }

        /* destroy the dialog */
        gtk_widget_destroy (dialog);

        /* handle */
        if (response == GTK_RESPONSE_OK)
          goto retry;
        else
          g_object_unref (G_OBJECT (document));

        break;

      default:
        /* something went wrong, release the document */
        g_object_unref (G_OBJECT (document));

        if (G_LIKELY (error))
          {
            /* show the warning */
            mousepad_dialogs_show_error (GTK_WINDOW (window), error, _("Failed to open the document"));

            /* cleanup */
            g_error_free (error);
          }

        break;
    }

  return (result == 0);
}



gboolean
mousepad_window_open_files (MousepadWindow  *window,
                            const gchar     *working_directory,
                            gchar          **filenames)
{
  guint  n;
  gchar *filename;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (working_directory != NULL, FALSE);
  g_return_val_if_fail (filenames != NULL, FALSE);
  g_return_val_if_fail (*filenames != NULL, FALSE);

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
#if GLIB_CHECK_VERSION (2, 58, 0)
          /* better, if we can:
           * see https://gitlab.xfce.org/apps/mousepad/-/merge_requests/19 */
          filename = g_canonicalize_filename (filenames[n], working_directory);
#else
          filename = g_build_filename (working_directory, filenames[n], NULL);
#endif
        }
      else
        {
          /* looks like a valid filename */
          filename = NULL;
        }

      /* open a new tab with the file */
      mousepad_window_open_file (window, filename ? filename : filenames[n], MOUSEPAD_ENCODING_UTF_8);

      /* cleanup */
      g_free (filename);
    }

  /* allow menu updates again */
  lock_menu_updates--;

  /* check if the window contains tabs */
  if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook)) == 0)
    return FALSE;

  return TRUE;
}



void
mousepad_window_add (MousepadWindow   *window,
                     MousepadDocument *document)
{
  GtkWidget        *label;
  gint              page;
  MousepadDocument *prev_active = window->active;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (GTK_IS_NOTEBOOK (window->notebook));

  /* create the tab label */
  label = mousepad_document_get_tab_label (document);

  /* get active page */
  page = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));

  /* insert the page right of the active tab */
  page = gtk_notebook_insert_page (GTK_NOTEBOOK (window->notebook), GTK_WIDGET (document), label, page + 1);

  /* set tab child properties */
  gtk_container_child_set (GTK_CONTAINER (window->notebook), GTK_WIDGET (document), "tab-expand", TRUE, NULL);
  gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (window->notebook), GTK_WIDGET (document), TRUE);
  gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (window->notebook), GTK_WIDGET (document), TRUE);

  /* show the document */
  gtk_widget_show (GTK_WIDGET (document));

  /* don't bother about this when there was no previous active page (startup) */
  if (G_LIKELY (prev_active != NULL))
    {
      /* switch to the new tab */
      gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), page);

      /* destroy the previous tab if it was not modified, untitled and the new tab is not untitled */
      if (gtk_text_buffer_get_modified (prev_active->buffer) == FALSE
          && mousepad_file_get_filename (prev_active->file) == NULL
          && mousepad_file_get_filename (document->file) != NULL)
        gtk_widget_destroy (GTK_WIDGET (prev_active));
    }

  /* make sure the textview is focused in the new document */
  mousepad_document_focus_textview (document);
}



static gboolean
mousepad_window_close_document (MousepadWindow   *window,
                                MousepadDocument *document)
{
  GAction  *action;
  gboolean  succeed = FALSE, readonly;
  gint      response;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), FALSE);

  /* check if the document has been modified */
  if (gtk_text_buffer_get_modified (document->buffer))
    {
      /* whether the file is readonly */
      readonly = mousepad_file_get_read_only (document->file);

      /* run save changes dialog */
      response = mousepad_dialogs_save_changes (GTK_WINDOW (window), readonly);

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
            action = g_action_map_lookup_action (G_ACTION_MAP (window), "file.save");
            g_action_activate (action, NULL);
            succeed = window->save_succeed;
            break;

          case MOUSEPAD_RESPONSE_SAVE_AS:
            action = g_action_map_lookup_action (G_ACTION_MAP (window), "file.save-as");
            g_action_activate (action, NULL);
            succeed = window->save_succeed;
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

  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* whether to show the full path */
  show_full_path = MOUSEPAD_SETTING_GET_BOOLEAN (PATH_IN_TITLE);

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



/* give the statusbar a languages menu created from the gtkbuilder */
GtkWidget *
mousepad_window_get_languages_menu (MousepadWindow *window)
{
  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), NULL);

  return window->languages_menu;
}



static gboolean
mousepad_window_get_in_fullscreen (MousepadWindow *window)
{
  if (GTK_IS_WIDGET (window) && gtk_widget_get_visible (GTK_WIDGET (window)))
    {
      GdkWindow     *win = gtk_widget_get_window (GTK_WIDGET (window));
      GdkWindowState state = gdk_window_get_state (win);
      return (state & GDK_WINDOW_STATE_FULLSCREEN);
    }

  return FALSE;
}



/**
 * Notebook Signal Functions
 **/
static void
mousepad_window_notebook_switch_page (GtkNotebook     *notebook,
                                      GtkWidget       *page,
                                      guint            page_num,
                                      MousepadWindow  *window)
{
  MousepadDocument  *document;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  /* get the new active document */
  document = MOUSEPAD_DOCUMENT (gtk_notebook_get_nth_page (notebook, page_num));

  /* only update when really changed */
  if (G_LIKELY (window->active != document))
    {
      /* set new active document */
      window->active = document;

      /* set the window title */
      mousepad_window_set_title (window);

      /* update the menu actions */
      mousepad_window_update_actions (window);

      /* update the statusbar */
      mousepad_document_send_signals (window->active);
    }
}



static void
mousepad_window_notebook_added (GtkNotebook     *notebook,
                                GtkWidget       *page,
                                guint            page_num,
                                MousepadWindow  *window)
{
  MousepadDocument *document = MOUSEPAD_DOCUMENT (page);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* connect signals to the document for this window */
  g_signal_connect (G_OBJECT (page), "close-tab",
                    G_CALLBACK (mousepad_window_button_close_tab), window);
  g_signal_connect (G_OBJECT (page), "cursor-changed",
                    G_CALLBACK (mousepad_window_cursor_changed), window);
  g_signal_connect (G_OBJECT (page), "selection-changed",
                    G_CALLBACK (mousepad_window_selection_changed), window);
  g_signal_connect (G_OBJECT (page), "overwrite-changed",
                    G_CALLBACK (mousepad_window_overwrite_changed), window);
  g_signal_connect (G_OBJECT (page), "language-changed",
                    G_CALLBACK (mousepad_window_buffer_language_changed), window);
  g_signal_connect (G_OBJECT (page), "drag-data-received",
                    G_CALLBACK (mousepad_window_drag_data_received), window);
  g_signal_connect_swapped (G_OBJECT (document->buffer), "notify::can-undo",
                            G_CALLBACK (mousepad_window_can_undo), window);
  g_signal_connect_swapped (G_OBJECT (document->buffer), "notify::can-redo",
                            G_CALLBACK (mousepad_window_can_redo), window);
  g_signal_connect_swapped (G_OBJECT (document->buffer), "modified-changed",
                            G_CALLBACK (mousepad_window_modified_changed), window);
  g_signal_connect (G_OBJECT (document->textview), "populate-popup",
                    G_CALLBACK (mousepad_window_menu_textview_popup), window);

  /* change the visibility of the tabs accordingly */
  mousepad_window_update_tabs (window, NULL, NULL);
}



static void
mousepad_window_notebook_removed (GtkNotebook     *notebook,
                                  GtkWidget       *page,
                                  guint            page_num,
                                  MousepadWindow  *window)
{
  gint              npages;
  MousepadDocument *document = MOUSEPAD_DOCUMENT (page);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  /* disconnect the old document signals */
  mousepad_disconnect_by_func (G_OBJECT (page), mousepad_window_button_close_tab, window);
  mousepad_disconnect_by_func (G_OBJECT (page), mousepad_window_cursor_changed, window);
  mousepad_disconnect_by_func (G_OBJECT (page), mousepad_window_selection_changed, window);
  mousepad_disconnect_by_func (G_OBJECT (page), mousepad_window_overwrite_changed, window);
  mousepad_disconnect_by_func (G_OBJECT (page), mousepad_window_buffer_language_changed, window);
  mousepad_disconnect_by_func (G_OBJECT (page), mousepad_window_drag_data_received, window);
  mousepad_disconnect_by_func (G_OBJECT (document->buffer), mousepad_window_can_undo, window);
  mousepad_disconnect_by_func (G_OBJECT (document->buffer), mousepad_window_can_redo, window);
  mousepad_disconnect_by_func (G_OBJECT (document->buffer),
                               mousepad_window_modified_changed, window);
  mousepad_disconnect_by_func (G_OBJECT (document->textview),
                               mousepad_window_menu_textview_popup, window);

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
      /* change the visibility of the tabs accordingly */
      mousepad_window_update_tabs (window, NULL, NULL);

      /* update action entries */
      mousepad_window_update_actions (window);
    }
}



#if !GTK_CHECK_VERSION (3, 22, 0)
static void
mousepad_window_notebook_menu_position (GtkMenu  *menu,
                                        gint     *x,
                                        gint     *y,
                                        gboolean *push_in,
                                        gpointer  user_data)
{
  GtkWidget    *widget = GTK_WIDGET (user_data);
  GtkAllocation alloc = { 0, 0, 0, 0 };

  gdk_window_get_origin (gtk_widget_get_window (widget), x, y);
  gtk_widget_get_allocation (widget, &alloc);

  *x += alloc.x;
  *y += alloc.y + alloc.height;

  *push_in = TRUE;
}
#endif


/* stolen from Geany notebook.c */
static gboolean
mousepad_window_is_position_on_tab_bar (GtkNotebook *notebook, GdkEventButton *event)
{
  GtkWidget      *page, *tab, *nb;
  GtkPositionType tab_pos;
  gint            scroll_arrow_hlength, scroll_arrow_vlength;
  gdouble         x, y;

  page = gtk_notebook_get_nth_page (notebook, 0);
  g_return_val_if_fail (page != NULL, FALSE);

  tab = gtk_notebook_get_tab_label (notebook, page);
  g_return_val_if_fail (tab != NULL, FALSE);

  tab_pos = gtk_notebook_get_tab_pos (notebook);
  nb = GTK_WIDGET (notebook);

  gtk_widget_style_get (GTK_WIDGET (notebook),
                        "scroll-arrow-hlength", &scroll_arrow_hlength,
                        "scroll-arrow-vlength", &scroll_arrow_vlength,
                        NULL);

  if (! gdk_event_get_coords ((GdkEvent*) event, &x, &y))
    {
      x = event->x;
      y = event->y;
    }

  switch (tab_pos)
    {
    case GTK_POS_TOP:
    case GTK_POS_BOTTOM:
      if (event->y >= 0 && event->y <= gtk_widget_get_allocated_height (tab))
        {
          if (! gtk_notebook_get_scrollable (notebook) || (
            x > scroll_arrow_hlength &&
            x < gtk_widget_get_allocated_width (nb) - scroll_arrow_hlength))
          {
            return TRUE;
          }
        }
      break;
    case GTK_POS_LEFT:
    case GTK_POS_RIGHT:
        if (event->x >= 0 && event->x <= gtk_widget_get_allocated_width (tab))
          {
            if (! gtk_notebook_get_scrollable (notebook) || (
                y > scroll_arrow_vlength &&
                y < gtk_widget_get_allocated_height (nb) - scroll_arrow_vlength))
              {
                return TRUE;
              }
          }
    }

  return FALSE;
}



static gboolean
mousepad_window_notebook_button_press_event (GtkNotebook    *notebook,
                                             GdkEventButton *event,
                                             MousepadWindow *window)
{
  GtkWidget  *page, *label;
  GAction    *action;
  guint       page_num = 0;
  gint        x_root;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);

  if (event->type == GDK_BUTTON_PRESS && (event->button == 3 || event->button == 2))
    {
      /* walk through the tabs and look for the tab under the cursor */
      while ((page = gtk_notebook_get_nth_page (notebook, page_num)) != NULL)
        {
          GtkAllocation alloc = { 0, 0, 0, 0 };

          label = gtk_notebook_get_tab_label (notebook, page);

          /* get the origin of the label */
          gdk_window_get_origin (gtk_widget_get_window (label), &x_root, NULL);
          gtk_widget_get_allocation (label, &alloc);
          x_root = x_root + alloc.x;

          /* check if the cursor is inside this label */
          if (event->x_root >= x_root && event->x_root <= (x_root + alloc.width))
            {
              /* switch to this tab */
              gtk_notebook_set_current_page (notebook, page_num);

              /* handle the button action */
              if (event->button == 3)
                {
                  /* show the menu */
#if GTK_CHECK_VERSION (3, 22, 0)
                  gtk_menu_popup_at_widget (GTK_MENU (window->tab_menu), label,
                                            GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST,
                                            (GdkEvent*) event);
#else

#if G_GNUC_CHECK_VERSION (4, 3)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

                  gtk_menu_popup (GTK_MENU (window->tab_menu), NULL, NULL,
                                  mousepad_window_notebook_menu_position, label,
                                  event->button, event->time);

#if G_GNUC_CHECK_VERSION (4, 3)
# pragma GCC diagnostic pop
#endif

#endif
                }
              else if (event->button == 2)
                {
                  /* close the document */
                  action = g_action_map_lookup_action (G_ACTION_MAP (window), "file.close-tab");
                  g_action_activate (action, NULL);
                }

              /* we succeed */
              return TRUE;
            }

          /* try the next tab */
          ++page_num;
        }
    }
  else if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
    {
      GtkWidget   *ev_widget, *nb_child;

      ev_widget = gtk_get_event_widget ((GdkEvent*) event);
      nb_child = gtk_notebook_get_nth_page (notebook,
                                            gtk_notebook_get_current_page (notebook));
      if (ev_widget == NULL || ev_widget == nb_child || gtk_widget_is_ancestor (ev_widget, nb_child))
        return FALSE;

      /* check if the event window is the notebook event window (not a tab) */
      if (mousepad_window_is_position_on_tab_bar (notebook, event))
        {
          /* create new document */
          action = g_action_map_lookup_action (G_ACTION_MAP (window), "file.new");
          g_action_activate (action, NULL);

          /* we succeed */
          return TRUE;
        }
    }

  return FALSE;
}



static gboolean
mousepad_window_notebook_button_release_event (GtkNotebook    *notebook,
                                               GdkEventButton *event,
                                               MousepadWindow *window)
{
  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (window->active), FALSE);

  /* focus the active textview */
  mousepad_document_focus_textview (window->active);

  return FALSE;
}



static GtkNotebook *
mousepad_window_notebook_create_window (GtkNotebook    *notebook,
                                        GtkWidget      *page,
                                        gint            x,
                                        gint            y,
                                        MousepadWindow *window)
{
  MousepadDocument *document;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), NULL);
  g_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (page), NULL);

  /* only create new window when there are more than 2 tabs */
  if (gtk_notebook_get_n_pages (notebook) >= 2)
    {
      /* get the document */
      document = MOUSEPAD_DOCUMENT (page);

      /* take a reference */
      g_object_ref (G_OBJECT (document));

      /* remove the document from the active window */
      gtk_notebook_detach_tab (GTK_NOTEBOOK (window->notebook), page);

      /* emit the new window with document signal */
      g_signal_emit (G_OBJECT (window), window_signals[NEW_WINDOW_WITH_DOCUMENT], 0, document, x, y);

      /* release our reference */
      g_object_unref (G_OBJECT (document));
    }

  return NULL;
}



/**
 * Document Signals Functions
 **/
static void
mousepad_window_modified_changed (MousepadWindow   *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  mousepad_window_set_title (window);
}



static void
mousepad_window_cursor_changed (MousepadDocument *document,
                                gint              line,
                                gint              column,
                                gint              selection,
                                MousepadWindow   *window)
{


  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  if (window->statusbar)
    {
      /* set the new statusbar cursor position and selection length */
      mousepad_statusbar_set_cursor_position (MOUSEPAD_STATUSBAR (window->statusbar), line, column, selection);
    }
}



static void
mousepad_window_selection_changed (MousepadDocument *document,
                                   gint              selection,
                                   MousepadWindow   *window)
{
  GtkToolItem *button;
  GAction     *action;
  guint        i;
  gboolean     sensitive;
  const gchar *action_names_1[] = { "edit.convert.tabs-to-spaces",
                                    "edit.convert.spaces-to-tabs",
                                    "edit.duplicate-line-selection",
                                    "edit.convert.strip-trailing-spaces" };
  const gchar *action_names_2[] = { "edit.move-selection.line-up",
                                    "edit.move-selection.line-down" };
  const gchar *action_names_3[] = { "edit.cut",
                                    "edit.copy",
                                    "edit.delete",
                                    "edit.convert.to-lowercase",
                                    "edit.convert.to-uppercase",
                                    "edit.convert.to-title-case",
                                    "edit.convert.to-opposite-case" };

  /* actions that are unsensitive during a column selection */
  sensitive = (selection == 0 || selection == 1);
  for (i = 0; i < G_N_ELEMENTS (action_names_1); i++)
    {
      action = g_action_map_lookup_action (G_ACTION_MAP (window), action_names_1[i]);
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), sensitive);
    }

  /* action that are only sensitive for normal selections */
  sensitive = (selection == 1);
  for (i = 0; i < G_N_ELEMENTS (action_names_2); i++)
    {
      action = g_action_map_lookup_action (G_ACTION_MAP (window), action_names_2[i]);
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), sensitive);
    }

  /* actions that are sensitive for all selections with content */
  sensitive = (selection > 0);
  for (i = 0; i < G_N_ELEMENTS (action_names_3); i++)
    {
      action = g_action_map_lookup_action (G_ACTION_MAP (window), action_names_3[i]);
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), sensitive);
    }

  /* set the sensitivity of the "Cut" and "Copy" toolbar buttons */
  button = gtk_toolbar_get_nth_item (GTK_TOOLBAR (window->toolbar), 9);
  gtk_widget_set_sensitive (GTK_WIDGET (button), sensitive);
  button = gtk_toolbar_get_nth_item (GTK_TOOLBAR (window->toolbar), 10);
  gtk_widget_set_sensitive (GTK_WIDGET (button), sensitive);
}



static void
mousepad_window_overwrite_changed (MousepadDocument *document,
                                   gboolean          overwrite,
                                   MousepadWindow   *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* set the new overwrite mode in the statusbar */
  if (window->statusbar)
    mousepad_statusbar_set_overwrite (MOUSEPAD_STATUSBAR (window->statusbar), overwrite);
}



static void
mousepad_window_buffer_language_changed (MousepadDocument  *document,
                                         GtkSourceLanguage *language,
                                         MousepadWindow    *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* update the filetype shown in the statusbar */
  mousepad_statusbar_set_language (MOUSEPAD_STATUSBAR (window->statusbar), language);
}



static void
mousepad_window_can_undo (MousepadWindow *window,
                          GParamSpec     *unused,
                          GObject        *buffer)
{
  GtkToolItem *button;
  GAction     *action;
  gboolean     can_undo;

  can_undo = gtk_source_buffer_can_undo (GTK_SOURCE_BUFFER (buffer));

  action = g_action_map_lookup_action (G_ACTION_MAP (window), "edit.undo");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_undo);

  /* set the sensitivity of the toolbar button */
  button = gtk_toolbar_get_nth_item (GTK_TOOLBAR (window->toolbar), 7);
  gtk_widget_set_sensitive (GTK_WIDGET (button), can_undo);
}



static void
mousepad_window_can_redo (MousepadWindow *window,
                          GParamSpec     *unused,
                          GObject        *buffer)
{
  GtkToolItem *button;
  GAction     *action;
  gboolean     can_redo;

  can_redo = gtk_source_buffer_can_redo (GTK_SOURCE_BUFFER (buffer));

  action = g_action_map_lookup_action (G_ACTION_MAP (window), "edit.redo");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_redo);

  /* set the sensitivity of the toolbar button */
  button = gtk_toolbar_get_nth_item (GTK_TOOLBAR (window->toolbar), 8);
  gtk_widget_set_sensitive (GTK_WIDGET (button), can_redo);
}



/**
 * Menu Functions
 **/
static void
mousepad_window_menu_templates_fill (MousepadWindow *window,
                                     GMenu          *menu,
                                     const gchar    *path,
                                     GPtrArray      *tooltips)
{
  GDir         *dir;
  GSList       *files_list = NULL, *dirs_list = NULL, *li;
  gchar        *absolute_path, *label, *dot, *message,
               *action_name, *filename_utf8, *tooltip;
  const gchar  *name;
  gboolean      files_added = FALSE;
  GIcon        *icon;
  GMenu        *submenu;
  GMenuItem    *item;

  /* open the directory */
  dir = g_dir_open (path, 0, NULL);

  /* read the directory */
  if (G_LIKELY (dir))
    {
      /* walk the directory */
      for (;;)
        {
          /* read the filename of the next file */
          name = g_dir_read_name (dir);

          /* break when we reached the last file */
          if (G_UNLIKELY (name == NULL))
            break;

          /* skip hidden files */
          if (name[0] == '.')
            continue;

          /* build absolute path */
          absolute_path = g_build_path (G_DIR_SEPARATOR_S, path, name, NULL);

          /* check if the file is a regular file or directory */
          if (g_file_test (absolute_path, G_FILE_TEST_IS_DIR))
            dirs_list = g_slist_insert_sorted (dirs_list, absolute_path, (GCompareFunc) strcmp);
          else if (g_file_test (absolute_path, G_FILE_TEST_IS_REGULAR))
            files_list = g_slist_insert_sorted (files_list, absolute_path, (GCompareFunc) strcmp);
          else
            g_free (absolute_path);
        }

      /* close the directory */
      g_dir_close (dir);
    }

  /* append the directories */
  for (li = dirs_list; li != NULL; li = li->next)
    {
      /* create a new submenu for the directory */
      submenu = g_menu_new ();
      g_ptr_array_add (tooltips, NULL);

      /* fill the menu */
      mousepad_window_menu_templates_fill (window, submenu, li->data, tooltips);

      /* check if the submenu contains items */
      if (g_menu_model_get_n_items (G_MENU_MODEL (submenu)))
        {
          /* append the menu */
          label = g_filename_display_basename (li->data);
          item = g_menu_item_new (label, NULL);
          g_free (label);

          /* set submenu icon */
          /* TODO: is there a way to apply an icon to a submenu?
           * (the documentation says yes, the "folder" icon name is valid, but…) */
          icon = g_icon_new_for_string ("folder", NULL);
          g_menu_item_set_icon (item, icon);
          g_object_unref (G_OBJECT (icon));

          g_menu_item_set_submenu (item, G_MENU_MODEL (submenu));
          g_menu_append_item (menu, item);
          g_object_unref (item);
        }

      /* cleanup */
      g_free (li->data);
    }

  /* append the files */
  for (li = files_list; li != NULL; li = li->next)
    {
      /* create directory label */
      label = g_filename_display_basename (li->data);

      /* strip the extension from the label */
      dot = g_utf8_strrchr (label, -1, '.');
      if (dot != NULL)
        *dot = '\0';

      /* create the menu item */
      action_name = g_strconcat ("win.file.new-from-template.new('", li->data, "')", NULL);
      item = g_menu_item_new (label, action_name);
      g_free (action_name);

      /* create an utf-8 valid version of the filename for the tooltip */
      filename_utf8 = g_filename_to_utf8 (li->data, -1, NULL, NULL, NULL);
      tooltip = g_strdup_printf (_("Use '%s' as template"), filename_utf8);
      g_free (filename_utf8);
      g_ptr_array_add (tooltips, tooltip);

      /* set item icon */
      icon = g_icon_new_for_string ("text-x-generic", NULL);
      g_menu_item_set_icon (item, icon);
      g_object_unref (G_OBJECT (icon));

      /* append item to the menu */
      g_menu_append_item (menu, item);
      g_object_unref (item);

      /* disable the menu item telling the user there's no templates */
      files_added = TRUE;

      /* cleanup */
      g_free (label);
      g_free (li->data);
    }

  /* cleanup */
  g_slist_free (dirs_list);
  g_slist_free (files_list);

  if (! files_added)
    {
      g_ptr_array_add (tooltips, NULL);
      message = g_strdup_printf (_("No template files found in\n'%s'"), path);
      item = g_menu_item_new (message, "insensitive");
      g_free (message);
      g_menu_append_item (menu, item);
      g_object_unref (item);
    }
}



static void
mousepad_window_menu_templates (GSimpleAction *action,
                                GVariant      *value,
                                gpointer       data)
{
  MousepadWindow   *window = MOUSEPAD_WINDOW (data);
  GMenu            *menu;
  GMenuItem        *item;
  GtkBuilder       *builder;
  GtkWidget        *gtkmenu;
  const gchar      *homedir;
  gchar            *templates_path, *message;
  GPtrArray        *tooltips;
  guint             n;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* opening the menu */
  if (g_variant_get_boolean (value))
    {
      /* lock menu updates */
      lock_menu_updates++;

      /* get the templates path */
      templates_path = g_strdup (g_get_user_special_dir (G_USER_DIRECTORY_TEMPLATES));

      /* check if the templates directory is valid and is not home */
      homedir = g_get_home_dir ();

      if (G_UNLIKELY (templates_path == NULL) || g_strcmp0 (templates_path, homedir) == 0)
        {
          /* if not, fall back to "~/Templates" */
          templates_path = g_build_filename (homedir, "Templates", NULL);
        }

      /* get and empty the "Templates" submenu */
      builder = mousepad_application_get_builder (MOUSEPAD_APPLICATION (
                  gtk_window_get_application (GTK_WINDOW (window))));
      menu = G_MENU (gtk_builder_get_object (builder, "file.new-from-template"));
      g_menu_remove_all (menu);

      /* check if the directory exists */
      if (g_file_test (templates_path, G_FILE_TEST_IS_DIR))
        {
          /* fill the menu */
          tooltips = g_ptr_array_new_with_free_func (g_free);
          mousepad_window_menu_templates_fill (window, menu, templates_path, tooltips);

          /* set the tooltips */
          gtkmenu = mousepad_window_get_menubar_submenu (window, window->menubar,
                                                         "template-menu-flag");
          n = 0;
          mousepad_window_menu_set_tooltips_full (window, gtkmenu, tooltips, &n);

          g_ptr_array_free (tooltips, TRUE);
        }
      else
        {
          message = g_strdup_printf (_("Missing Templates directory\n'%s'"), templates_path);
          item = g_menu_item_new (message, "insensitive");
          g_free (message);
          g_menu_append_item (menu, item);
          g_object_unref (item);
        }

      /* cleanup */
      g_free (templates_path);

      /* unlock */
      lock_menu_updates--;
    }
}



static void
mousepad_window_menu_tab_sizes_update (MousepadWindow *window)
{
  gint32      tab_size;
  gint        tab_size_n, nitem, nitems;
  gchar      *text = NULL;
  GtkWidget  *gtkmenu;
  GAction    *action;
  GMenuModel *model;
  GMenuItem  *item;
  GtkBuilder *builder;
  GPtrArray  *tooltips;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* avoid menu actions */
  lock_menu_updates++;

  /* get tab size of active document */
  tab_size = MOUSEPAD_SETTING_GET_INT (TAB_WIDTH);

  /* get the number of items in the tab-size submenu */
  builder = mousepad_application_get_builder (MOUSEPAD_APPLICATION (
              gtk_window_get_application (GTK_WINDOW (window))));
  model = G_MENU_MODEL (gtk_builder_get_object (builder, "document.tab.tab-size"));
  nitems = g_menu_model_get_n_items (model);

  /* check if there is a default item with this tab size */
  for (nitem = 0; nitem < nitems; nitem++)
    {
      tab_size_n = atoi (g_variant_get_string (
                     g_menu_model_get_item_attribute_value (model, nitem, "label", NULL), NULL));
      if (tab_size_n == tab_size)
        break;
    }

  if (nitem == nitems)
    {
      /* create suitable text label for the "Other" menu */
      text = g_strdup_printf (_("Ot_her (%d)..."), tab_size);
      tab_size = 0;
    }

  /* toggle the action */
  action = g_action_map_lookup_action (G_ACTION_MAP (window), "document.tab.tab-size");
  g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_int32 (tab_size));

  /* set the "Other" menu label */
  item = g_menu_item_new_from_model (model, nitems - 1);
  g_menu_item_set_label (item, text ? text : _("Ot_her..."));
  g_menu_remove (G_MENU (model), nitems - 1);
  g_menu_append_item (G_MENU (model), item);
  g_object_unref (item);

  /* set the "Other" menu tooltip */
  gtkmenu = mousepad_window_get_menubar_submenu (window, window->menubar, "tab-size-menu-flag");
  tooltips = g_ptr_array_new ();
  g_ptr_array_add (tooltips, (gpointer) menubar_tooltips[64]);
  mousepad_window_menu_set_tooltips (window, gtkmenu, tooltips, 1, 2);
  g_ptr_array_free (tooltips, TRUE);

  /* cleanup */
  g_free (text);

  /* allow menu actions again */
  lock_menu_updates--;
}



static void
mousepad_window_menu_textview_shown (GtkMenu        *menu,
                                     MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* disconnect this signal */
  mousepad_disconnect_by_func (menu, mousepad_window_menu_textview_shown, window);

  /* empty the original menu */
  mousepad_util_container_clear (GTK_CONTAINER (menu));

  /* move the textview menu children into the other menu */
  mousepad_util_container_move_children (GTK_CONTAINER (window->textview_menu),
                                         GTK_CONTAINER (menu));
}



static void
mousepad_window_menu_textview_deactivate (GtkWidget      *menu,
                                          MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* disconnect this signal */
  mousepad_disconnect_by_func (G_OBJECT (menu), mousepad_window_menu_textview_deactivate, window);

  /* copy the menu children back into the textview menu */
  mousepad_util_container_move_children (GTK_CONTAINER (menu),
                                         GTK_CONTAINER (window->textview_menu));
}



static void
mousepad_window_menu_textview_popup (GtkTextView    *textview,
                                     GtkMenu        *menu,
                                     MousepadWindow *window)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (textview));
  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* connect signal */
  g_signal_connect (G_OBJECT (menu), "show",
                    G_CALLBACK (mousepad_window_menu_textview_shown), window);
  g_signal_connect (G_OBJECT (menu), "deactivate",
                    G_CALLBACK (mousepad_window_menu_textview_deactivate), window);
}



static void
mousepad_window_update_fullscreen_action (MousepadWindow *window)
{
  GAction  *action;
  gboolean  active;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* avoid menu actions */
  lock_menu_updates++;

  /* toggle the fullscreen setting */
  active = MOUSEPAD_SETTING_GET_BOOLEAN (WINDOW_FULLSCREEN);
  action = g_action_map_lookup_action (G_ACTION_MAP (window), "view.fullscreen");;
  g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (active));

  /* allow menu actions again */
  lock_menu_updates--;
}



static void
mousepad_window_update_line_numbers_action (MousepadWindow *window)
{
  GAction  *action;
  gboolean  active;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* avoid menu actions */
  lock_menu_updates++;

  /* toggle the line numbers setting */
  active = MOUSEPAD_SETTING_GET_BOOLEAN (SHOW_LINE_NUMBERS);
  action = g_action_map_lookup_action (G_ACTION_MAP (window), "view.line-numbers");
  g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (active));

  /* allow menu actions again */
  lock_menu_updates--;
}



static void
mousepad_window_update_document_actions (MousepadWindow *window)
{
  GAction  *action;
  gboolean  active;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* update the actions for the active document */
  if (G_LIKELY (window->active))
    {
      /* avoid menu actions */
      lock_menu_updates++;

      /* toggle the document settings */
      active = MOUSEPAD_SETTING_GET_BOOLEAN (WORD_WRAP);
      action = g_action_map_lookup_action (G_ACTION_MAP (window), "document.word-wrap");
      g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (active));

      active = MOUSEPAD_SETTING_GET_BOOLEAN (AUTO_INDENT);
      action = g_action_map_lookup_action (G_ACTION_MAP (window), "document.auto-indent");
      g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (active));

      /* update the tabs size menu */
      mousepad_window_menu_tab_sizes_update (window);

      active = MOUSEPAD_SETTING_GET_BOOLEAN (INSERT_SPACES);
      action = g_action_map_lookup_action (G_ACTION_MAP (window), "document.tab.insert-spaces");
      g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (active));

      /* allow menu actions again */
      lock_menu_updates--;
    }
}



static void
mousepad_window_update_color_scheme_action (MousepadWindow *window)
{
  GAction  *action;
  gchar    *scheme_id;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* avoid menu actions */
  lock_menu_updates++;

  scheme_id = MOUSEPAD_SETTING_GET_STRING (COLOR_SCHEME);
  action = g_action_map_lookup_action (G_ACTION_MAP (window), "view.color-scheme");
  g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_string (scheme_id));
  g_free (scheme_id);

  /* allow menu actions again */
  lock_menu_updates--;
}



static void
mousepad_window_update_actions (MousepadWindow *window)
{
  GAction            *action;
  GtkNotebook        *notebook;
  MousepadDocument   *document;
  GtkSourceLanguage  *language;
  MousepadLineEnding  line_ending;
  gboolean            cycle_tabs, sensitive, value;
  gint                n_pages, page_num;
  const gchar        *language_id;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  notebook = GTK_NOTEBOOK (window->notebook);
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
      cycle_tabs = MOUSEPAD_SETTING_GET_BOOLEAN (CYCLE_TABS);

      /* set the sensitivity of the back and forward buttons in the go menu */
      action = g_action_map_lookup_action (G_ACTION_MAP (window), "document.previous-tab");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                   (cycle_tabs && n_pages > 1) || page_num > 0);

      action = g_action_map_lookup_action (G_ACTION_MAP (window), "document.next-tab");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                   (cycle_tabs && n_pages > 1 ) || page_num < n_pages - 1);

      /* set the reload, detach and save sensitivity */
      action = g_action_map_lookup_action (G_ACTION_MAP (window), "file.save");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                   ! mousepad_file_get_read_only (document->file));

      action = g_action_map_lookup_action (G_ACTION_MAP (window), "file.detach-tab");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), n_pages > 1);

      action = g_action_map_lookup_action (G_ACTION_MAP (window), "file.revert");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                   mousepad_file_get_filename (document->file) != NULL);

      /* set the current line ending type */
      line_ending = mousepad_file_get_line_ending (document->file);
      action = g_action_map_lookup_action (G_ACTION_MAP (window), "document.line-ending");
      g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_int32 (line_ending));

      /* write bom */
      action = g_action_map_lookup_action (G_ACTION_MAP (window), "document.write-unicode-bom");
      value = mousepad_file_get_write_bom (document->file, &sensitive);
      g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (value));
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), sensitive);

      /* toggle the document settings */
      mousepad_window_update_document_actions (window);

      /* set the sensitivity of the undo and redo actions */
      mousepad_window_can_undo (window, NULL, G_OBJECT (document->buffer));
      mousepad_window_can_redo (window, NULL, G_OBJECT (document->buffer));

      /* active this tab in the go menu */
      action = g_action_map_lookup_action (G_ACTION_MAP (window), "document.go-to-tab");
      g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_int32 (page_num));

      /* update the currently active language */
      language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (window->active->buffer));
      language_id = language ? gtk_source_language_get_id (language) : "plain-text";
      action = g_action_map_lookup_action (G_ACTION_MAP (window), "document.filetype");
      g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_string (language_id));

      /* allow menu actions again */
      lock_menu_updates--;
    }
}



static void
mousepad_window_update_gomenu (GSimpleAction *action,
                               GVariant      *value,
                               gpointer       data)
{
  MousepadWindow   *window = MOUSEPAD_WINDOW (data);
  MousepadDocument *document;
  GtkBuilder       *builder;
  GtkWidget        *gtkmenu;
  GMenu            *menu;
  GMenuItem        *item;
  GAction          *subaction;
  GPtrArray        *tooltips;
  const gchar      *label;
  gchar            *action_name, *accelerator;
  guint             n_pages, n_tooltips, n;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* opening the menu */
  if (g_variant_get_boolean (value))
    {
      /* prevent menu updates */
      lock_menu_updates++;

      /* get and empty the "Go to tab" submenu */
      builder = mousepad_application_get_builder (MOUSEPAD_APPLICATION (
                  gtk_window_get_application (GTK_WINDOW (window))));
      menu = G_MENU (gtk_builder_get_object (builder, "document.go-to-tab"));
      g_menu_remove_all (menu);

      /* walk through the notebook pages */
      n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));
      tooltips = g_ptr_array_new ();
      n_tooltips = 0;

      for (n = 0; n < n_pages; ++n)
        {
          document = MOUSEPAD_DOCUMENT (gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), n));

          /* add the item to the "Go to Tab" submenu */
          g_ptr_array_add (tooltips, (gchar*) mousepad_document_get_filename (document));
          n_tooltips++;
          label = mousepad_document_get_basename (document);
          action_name = g_strdup_printf ("win.document.go-to-tab(%d)", n);
          item = g_menu_item_new (label, action_name);
          g_free (action_name);

          if (G_LIKELY (n < 9))
            {
              /* create an accelerator and add it to the menu item */
              accelerator = g_strdup_printf ("<Alt>%d", n + 1);
              g_menu_item_set_attribute_value (item, "accel", g_variant_new_string (accelerator));
              g_free (accelerator);
            }

          /* add the menu item */
          g_menu_append_item (menu, item);
          g_object_unref (item);

          /* select the active entry */
          if ((guint) gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook)) == n)
            {
              subaction = g_action_map_lookup_action (G_ACTION_MAP (window), "document.go-to-tab");
              g_simple_action_set_state (G_SIMPLE_ACTION (subaction), g_variant_new_int32 (n));
            }
        }

      /* update the tooltips */
      gtkmenu = mousepad_window_get_menubar_submenu (window, window->menubar, "document-menu-flag");
      mousepad_window_menu_set_tooltips (window, gtkmenu, tooltips, n_tooltips, 0);
      g_ptr_array_free (tooltips, TRUE);

      /* release our lock */
      lock_menu_updates--;
    }
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
  gchar         *description;
  const gchar   *charset;
  static gchar  *groups[] = { (gchar *) PACKAGE_NAME, NULL };

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_FILE (file));

  /* get the charset */
  charset = mousepad_encoding_get_charset (mousepad_file_get_encoding (file));

  /* build description */
  description = g_strdup_printf ("%s: %s", _("Charset"), charset);

  /* create the recent data */
  info.display_name = NULL;
  info.description  = (gchar *) description;
  info.mime_type    = (gchar *) "text/plain";
  info.app_name     = (gchar *) PACKAGE_NAME;
  info.app_exec     = (gchar *) PACKAGE " %u";
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
    window->recent_manager = gtk_recent_manager_get_default ();
}



static void
mousepad_window_recent_menu (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       data)
{
  MousepadWindow   *window = MOUSEPAD_WINDOW (data);
  GtkBuilder       *builder;
  GtkWidget        *gtkmenu;
  GMenu            *menu;
  GMenuItem        *menu_item;
  GAction          *subaction;
  GtkRecentInfo    *info;
  GList            *items, *li, *filtered = NULL;
  const gchar      *uri, *display_name;
  gchar            *label, *filename, *filename_utf8, *tooltip, *action_name;
  GPtrArray        *tooltips;
  guint             n_tooltips, n;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* opening the menu */
  if (g_variant_get_boolean (value))
    {
      /* avoid updating the menu */
      lock_menu_updates++;

      /* get and empty the "Recent" submenu */
      builder = mousepad_application_get_builder (MOUSEPAD_APPLICATION (
                  gtk_window_get_application (GTK_WINDOW (window))));
      menu = G_MENU (gtk_builder_get_object (builder, "file.open-recent.list"));
      g_menu_remove_all (menu);

      /* make sure the recent manager is initialized */
      mousepad_window_recent_manager_init (window);

      /* get all the items in the manager */
      items = gtk_recent_manager_get_items (window->recent_manager);

      /* walk through the items in the manager and pick the ones that are in the mousepad group */
      for (li = items; li != NULL; li = li->next)
        {
          /* check if the item is in the Mousepad group */
          if (! gtk_recent_info_has_group (li->data, PACKAGE_NAME))
            continue;

          /* insert the list, sorted by date */
          filtered = g_list_insert_sorted (filtered, li->data,
                                           (GCompareFunc) mousepad_window_recent_sort);
        }

      /* get the recent menu limit number */
      n = MOUSEPAD_SETTING_GET_INT (RECENT_MENU_ITEMS);
      tooltips = g_ptr_array_new_with_free_func (g_free);
      n_tooltips = 0;

      /* append the items to the menu */
      for (li = filtered; n > 0 && li != NULL; li = li->next)
        {
          info = li->data;

          /* get the filename */
          uri = gtk_recent_info_get_uri (info);
          filename = g_filename_from_uri (uri, NULL, NULL);

          /* append to the menu if the file exists, else remove it from the history */
          if (filename && g_file_test (filename, G_FILE_TEST_EXISTS))
            {
              /* link the info data to the action via the unique detailed action name */
              action_name = g_strconcat ("win.file.open-recent.new('", filename, "')", NULL);
              subaction = g_action_map_lookup_action (G_ACTION_MAP (window), "file.open-recent.new");
              mousepad_object_set_data_full (G_OBJECT (subaction), g_intern_string (action_name),
                                             gtk_recent_info_ref (info), gtk_recent_info_unref);

              /* get the name of the item and escape the underscores */
              display_name = gtk_recent_info_get_display_name (info);
              label = mousepad_util_escape_underscores (display_name);

              /* create an utf-8 valid version of the filename for the tooltip */
              filename_utf8 = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
              tooltip = g_strdup_printf (_("Open '%s'"), filename_utf8);
              g_free (filename_utf8);
              g_ptr_array_add (tooltips, tooltip);
              n_tooltips++;

              /* append menu item */
              menu_item = g_menu_item_new (label, action_name);
              g_menu_append_item (menu, menu_item);
              g_object_unref (menu_item);
              g_free (label);
              g_free (action_name);

              /* update couters */
              n--;
            }
          else
            {
              /* remove the item. don't both the user if this fails */
              gtk_recent_manager_remove_item (window->recent_manager, uri, NULL);
            }

          /* cleanup */
          g_free (filename);
        }

      /* add the "No items found" insensitive menu item */
      if (! filtered)
        {
          menu_item = g_menu_item_new (_("No items found"), "insensitive");
          g_menu_append_item (menu, menu_item);
          g_object_unref (menu_item);
        }
      /* set the tooltips */
      else
        {
          gtkmenu = mousepad_window_get_menubar_submenu (window, window->menubar,
                                                         "recent-menu-flag");
          mousepad_window_menu_set_tooltips (window, gtkmenu, tooltips, n_tooltips, 2);
        }

      /* set the sensitivity of the clear button */
      subaction = g_action_map_lookup_action (G_ACTION_MAP (window),
                                              "file.open-recent.clear-history");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (subaction), filtered != NULL);

      /* cleanup */
      g_list_free_full (items, (GDestroyNotify) gtk_recent_info_unref);
      g_list_free (filtered);
      g_ptr_array_free (tooltips, TRUE);

      /* allow menu updates again */
      lock_menu_updates--;
    }
}



static const gchar *
mousepad_window_recent_get_charset (GtkRecentInfo *info)
{
  const gchar *description;
  const gchar *charset = NULL;
  guint        offset;

  /* get the description */
  description = gtk_recent_info_get_description (info);
  if (G_LIKELY (description))
    {
      /* get the offset length: 'Encoding: ' */
      offset = strlen (_("Charset")) + 2;

      /* check if the encoding string looks valid, if so, set it */
      if (G_LIKELY (strlen (description) > offset))
        charset = description + offset;
    }

  return charset;
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
  g_list_free_full (items, (GDestroyNotify) gtk_recent_info_unref);

  /* print a warning is there is one */
  if (G_UNLIKELY (error != NULL))
    {
      mousepad_dialogs_show_error (GTK_WINDOW (window), error, _("Failed to clear the recent history"));
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
                                    guint             drag_time,
                                    MousepadWindow   *window)
{
  gchar     **uris;
  gchar      *working_directory;
  GtkWidget  *notebook, **document;
  GtkWidget  *child, *label;
  gint        i, n_pages;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  /* we only accept text/uri-list drops with format 8 and atleast one byte of data */
  if (info == TARGET_TEXT_URI_LIST &&
      gtk_selection_data_get_format (selection_data) == 8 &&
      gtk_selection_data_get_length (selection_data) > 0)
    {
      /* extract the uris from the data */
      uris = g_uri_list_extract_uris ((const gchar *) gtk_selection_data_get_data (selection_data));

      /* get working directory */
      working_directory = g_get_current_dir ();

      /* open the files */
      mousepad_window_open_files (window, working_directory, uris);

      /* cleanup */
      g_free (working_directory);
      g_strfreev (uris);

      /* finish the drag (copy) */
      gtk_drag_finish (context, TRUE, FALSE, drag_time);
    }
  else if (info == TARGET_GTK_NOTEBOOK_TAB)
    {
      /* get the source notebook */
      notebook = gtk_drag_get_source_widget (context);

      /* get the document that has been dragged */
      document = (GtkWidget **) gtk_selection_data_get_data (selection_data);

      /* check */
      g_return_if_fail (MOUSEPAD_IS_DOCUMENT (*document));

      /* take a reference on the document before we remove it */
      g_object_ref (G_OBJECT (*document));

      /* remove the document from the source window */
      gtk_container_remove (GTK_CONTAINER (notebook), *document);

      /* get the number of pages in the notebook */
      n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

      /* figure out where to insert the tab in the notebook */
      for (i = 0; i < n_pages; i++)
        {
          GtkAllocation alloc = { 0, 0, 0, 0 };

          /* get the child label */
          child = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i);
          label = gtk_notebook_get_tab_label (GTK_NOTEBOOK (window->notebook), child);

          gtk_widget_get_allocation (label, &alloc);

          /* break if we have a matching drop position */
          if (x < (alloc.x + alloc.width / 2))
            break;
        }

      /* add the document to the new window */
      mousepad_window_add (window, MOUSEPAD_DOCUMENT (*document));

      /* move the tab to the correct position */
      gtk_notebook_reorder_child (GTK_NOTEBOOK (window->notebook), *document, i);

      /* release our reference on the document */
      g_object_unref (G_OBJECT (*document));

      /* finish the drag (move) */
      gtk_drag_finish (context, TRUE, TRUE, drag_time);
    }
}



/**
 * Find and replace
 **/
static gboolean
mousepad_window_scroll_to_cursor (MousepadWindow *window)
{
  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);

  if (window->active != NULL)
    mousepad_view_scroll_to_cursor (window->active->textview);

  return FALSE;
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

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), -1);

  if (flags & MOUSEPAD_SEARCH_FLAGS_ACTION_HIGHLIGHT_ON)
    gtk_source_search_context_set_highlight (window->active->search_context, TRUE);
  else if (flags & MOUSEPAD_SEARCH_FLAGS_ACTION_HIGHLIGHT_OFF)
    gtk_source_search_context_set_highlight (window->active->search_context, FALSE);
  else if (flags & MOUSEPAD_SEARCH_FLAGS_AREA_ALL_DOCUMENTS)
    {
      /* get the number of documents in this window */
      npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

      /* walk the pages */
      for (i = 0; i < npages; i++)
        {
          /* get the document */
          document = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i);

          /* replace the matches in the document */
          nmatches += mousepad_util_search (MOUSEPAD_DOCUMENT (document)->search_context, string,
                                            replacement, flags);
        }
    }
  else if (window->active != NULL)
    {
      /* search or replace in the active document whenever idle */
      nmatches = mousepad_util_search (window->active->search_context, string, replacement, flags);

      /* make sure the selection is visible */
      if (flags & (MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT | MOUSEPAD_SEARCH_FLAGS_ACTION_REPLACE)
          && nmatches > 0)
        g_idle_add (G_SOURCE_FUNC (mousepad_window_scroll_to_cursor), window);
    }
  else
    {
      /* should never be reaches */
      g_assert_not_reached ();
    }

  return nmatches;
}



/**
 * Search Bar
 **/
static void
mousepad_window_hide_search_bar (MousepadWindow *window)
{
  MousepadSearchFlags flags;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));
  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (window->search_bar));

  /* setup flags */
  flags = MOUSEPAD_SEARCH_FLAGS_ACTION_HIGHLIGHT_OFF;

  /* remove the highlight */
  mousepad_window_search (window, flags, NULL, NULL);

  /* hide the search bar */
  gtk_widget_hide (window->search_bar);

  /* focus the active document's text view */
  mousepad_document_focus_textview (window->active);
}



/**
 * Paste from History
 **/
static void
mousepad_window_paste_history_add (MousepadWindow *window)
{
  GtkClipboard *clipboard;
  gchar        *text;
  GSList       *li;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* get the current clipboard text */
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window), GDK_SELECTION_CLIPBOARD);
  text = gtk_clipboard_wait_for_text (clipboard);

  /* leave when there is no text */
  if (G_UNLIKELY (text == NULL))
    return;

  /* check if the item is already in the history */
  for (li = clipboard_history; li != NULL; li = li->next)
    if (strcmp (li->data, text) == 0)
      break;

  /* append the item or remove it */
  if (G_LIKELY (li == NULL))
    {
      /* add to the list */
      clipboard_history = g_slist_prepend (clipboard_history, text);

      /* get the 10th item from the list and remove it if it exists */
      li = g_slist_nth (clipboard_history, 10);
      if (li != NULL)
        {
          /* cleanup */
          g_free (li->data);
          clipboard_history = g_slist_delete_link (clipboard_history, li);
        }
    }
  else
    {
      /* already in the history, remove it */
      g_free (text);
    }
}



#if !GTK_CHECK_VERSION (3, 22, 0)
static void
mousepad_window_paste_history_menu_position (GtkMenu  *menu,
                                             gint     *x,
                                             gint     *y,
                                             gboolean *push_in,
                                             gpointer  user_data)
{
  MousepadWindow   *window = MOUSEPAD_WINDOW (user_data);
  MousepadDocument *document = window->active;
  GtkTextIter       iter;
  GtkTextMark      *mark;
  GdkRectangle      location;
  gint              iter_x, iter_y;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (GTK_IS_TEXT_VIEW (document->textview));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (document->buffer));

  /* get the root coordinates of the texview widget */
  gdk_window_get_origin (gtk_text_view_get_window (GTK_TEXT_VIEW (document->textview), GTK_TEXT_WINDOW_TEXT), x, y);

  /* get the cursor iter */
  mark = gtk_text_buffer_get_insert (document->buffer);
  gtk_text_buffer_get_iter_at_mark (document->buffer, &iter, mark);

  /* get iter location */
  gtk_text_view_get_iter_location (GTK_TEXT_VIEW (document->textview), &iter, &location);

  /* convert to textview coordinates */
  gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (document->textview), GTK_TEXT_WINDOW_TEXT,
                                         location.x, location.y, &iter_x, &iter_y);

  /* add the iter coordinates to the menu popup position */
  *x += iter_x;
  *y += iter_y + location.height;
}
#endif



static void
mousepad_window_paste_history_activate (GtkMenuItem    *item,
                                        MousepadWindow *window)
{
  const gchar *text;

  g_return_if_fail (GTK_IS_MENU_ITEM (item));
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));
  g_return_if_fail (MOUSEPAD_IS_VIEW (window->active->textview));

  /* get the menu item text */
  text = mousepad_object_get_data (G_OBJECT (item), "history-pointer");

  /* paste the text */
  if (G_LIKELY (text))
    mousepad_view_clipboard_paste (window->active->textview, text, FALSE);
}



static GtkWidget *
mousepad_window_paste_history_menu_item (const gchar *text,
                                         const gchar *mnemonic)
{
  GtkWidget   *item;
  GtkWidget   *label;
  GtkWidget   *hbox;
  const gchar *s;
  gchar       *label_str;
  GString     *string;

  /* create new label string */
  string = g_string_sized_new (PASTE_HISTORY_MENU_LENGTH);

  /* get the first 30 chars of the clipboard text */
  if (g_utf8_strlen (text, -1) > PASTE_HISTORY_MENU_LENGTH)
    {
      /* append the first 30 chars */
      s = g_utf8_offset_to_pointer (text, PASTE_HISTORY_MENU_LENGTH);
      string = g_string_append_len (string, text, s - text);

      /* make it look like a ellipsized string */
      string = g_string_append (string, "...");
    }
  else
    {
      /* append the entire string */
      string = g_string_append (string, text);
    }

  /* get the string */
  label_str = g_string_free (string, FALSE);

  /* replace tab and new lines with spaces */
  label_str = g_strdelimit (label_str, "\n\t\r", ' ');

  /* create a new item */
  item = gtk_menu_item_new ();

  /* create a hbox */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 14);
  gtk_container_add (GTK_CONTAINER (item), hbox);
  gtk_widget_show (hbox);

  /* create the clipboard label */
  label = gtk_label_new (label_str);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.5);
  gtk_widget_show (label);

  /* create the mnemonic label */
  label = gtk_label_new_with_mnemonic (mnemonic);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.5);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), item);
  gtk_widget_show (label);

  /* cleanup */
  g_free (label_str);

  return item;
}



static GtkWidget *
mousepad_window_paste_history_menu (MousepadWindow *window)
{
  GSList       *li;
  gchar        *text;
  gpointer      list_data = NULL;
  GtkWidget    *item;
  GtkWidget    *menu;
  GtkClipboard *clipboard;
  gchar         mnemonic[4];
  gint          n;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), NULL);

  /* create new menu and set the screen */
  menu = gtk_menu_new ();
  g_object_ref_sink (G_OBJECT (menu));
  g_signal_connect (G_OBJECT (menu), "deactivate", G_CALLBACK (g_object_unref), NULL);
  gtk_menu_set_screen (GTK_MENU (menu), gtk_widget_get_screen (GTK_WIDGET (window)));

  /* get the current clipboard text */
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window), GDK_SELECTION_CLIPBOARD);
  text = gtk_clipboard_wait_for_text (clipboard);

  /* append the history items */
  for (li = clipboard_history, n = 1; li != NULL; li = li->next)
    {
      /* skip the active clipboard item */
      if (G_UNLIKELY (list_data == NULL && text && strcmp (li->data, text) == 0))
        {
          /* store the pointer so we can attach it at the end of the menu */
          list_data = li->data;
        }
      else
        {
          /* create mnemonic string */
          g_snprintf (mnemonic, sizeof (mnemonic), "_%d", n++);

          /* create menu item */
          item = mousepad_window_paste_history_menu_item (li->data, mnemonic);
          mousepad_object_set_data (G_OBJECT (item), "history-pointer", li->data);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (mousepad_window_paste_history_activate), window);
          gtk_widget_show (item);
        }
    }

  /* cleanup */
  g_free (text);

  if (list_data != NULL)
    {
      /* add separator between history and active menu items */
      if (mousepad_util_container_has_children (GTK_CONTAINER (menu)))
        {
          item = gtk_separator_menu_item_new ();
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_show (item);
        }

      /* create menu item for current clipboard text */
      item = mousepad_window_paste_history_menu_item (list_data, "_0");
      mousepad_object_set_data (G_OBJECT (item), "history-pointer", list_data);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      g_signal_connect (G_OBJECT (item), "activate",
                        G_CALLBACK (mousepad_window_paste_history_activate), window);
      gtk_widget_show (item);
    }
  else if (! mousepad_util_container_has_children (GTK_CONTAINER (menu)))
    {
      /* create an item to inform the user */
      item = gtk_menu_item_new_with_label (_("No clipboard data"));
      gtk_widget_set_sensitive (item, FALSE);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
    }

  return menu;
}



/**
 * Miscellaneous Actions
 **/
static void
mousepad_window_button_close_tab (MousepadDocument *document,
                                  MousepadWindow   *window)
{
  gint page_num;

  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

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
  GAction *action;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);

  /* try to close the window */
  action = g_action_map_lookup_action (G_ACTION_MAP (window), "file.close-window");
  g_action_activate (action, NULL);

  /* we will close the window when all the tabs are closed */
  return TRUE;
}



/**
 * Menu Actions
 *
 * All those function should be sorted by the menu structure so it's
 * easy to find a function. The function can always use window->active, since
 * we can assume there is always an active document inside a window.
 **/
static void
mousepad_window_action_new (GSimpleAction *action,
                            GVariant      *value,
                            gpointer       data)
{
  MousepadDocument *document;
  MousepadWindow   *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* create new document */
  document = mousepad_document_new ();

  /* add the document to the window */
  mousepad_window_add (window, document);
}



static void
mousepad_window_action_new_window (GSimpleAction *action,
                                   GVariant      *value,
                                   gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* emit the new window signal */
  g_signal_emit (G_OBJECT (window), window_signals[NEW_WINDOW], 0);
}



static void
mousepad_window_action_new_from_template (GSimpleAction *action,
                                          GVariant      *value,
                                          gpointer       data)
{
  MousepadWindow    *window = MOUSEPAD_WINDOW (data);
  MousepadDocument  *document;
  GtkSourceLanguage *language;
  const gchar       *filename, *message;
  GError            *error = NULL;
  gint               result;


  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* get the language from the filename */
  filename = g_variant_get_string (value, NULL);
  language = gtk_source_language_manager_guess_language (
               gtk_source_language_manager_get_default (), filename, NULL);

  /* test if the file exists */
  if (G_LIKELY (filename))
    {
      /* create new document */
      document = mousepad_document_new ();

      /* sink floating object */
      g_object_ref_sink (G_OBJECT (document));

      /* lock the undo manager */
      gtk_source_buffer_begin_not_undoable_action (GTK_SOURCE_BUFFER (document->buffer));

      /* try to load the template into the buffer */
      result = mousepad_file_open (document->file, filename, &error);

      /* release the lock */
      gtk_source_buffer_end_not_undoable_action (GTK_SOURCE_BUFFER (document->buffer));

      /* handle the result */
      if (G_LIKELY (result == 0))
        {
          /* no errors, insert the document */
          mousepad_window_add (window, document);
          mousepad_file_set_language (window->active->file, language);
        }
      else
        {
          /* release the document */
          g_object_unref (G_OBJECT (document));

          /* handle the error */
          switch (result)
            {
              case ERROR_NOT_UTF8_VALID:
              case ERROR_CONVERTING_FAILED:
                /* set error message */
                message = _("Templates should be UTF-8 valid");
                break;

              case ERROR_READING_FAILED:
                /* set error message */
                message = _("Reading the template failed");
                break;

              default:
                /* set error message */
                message = _("Loading the template failed");
                break;
            }

          /* show the error */
          mousepad_dialogs_show_error (GTK_WINDOW (window), error, message);
          g_clear_error (&error);
        }
    }
}



static void
mousepad_window_action_open (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkWidget      *chooser;
  GtkWidget      *hbox, *label, *combobox, *button;
  const gchar    *filename;
  GSList         *filenames, *li;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* create new file chooser dialog */
  chooser = gtk_file_chooser_dialog_new (_("Open File"),
                                         GTK_WINDOW (window),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         _("_Cancel"), GTK_RESPONSE_CANCEL,
                                         NULL);
  button = mousepad_util_image_button ("document-open", _("_Open"));
  gtk_widget_set_can_default (button, TRUE);
  gtk_dialog_add_action_widget (GTK_DIALOG (chooser), button, GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_default_response (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);
  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (chooser), TRUE);

  /* encoding selector */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
  gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (chooser), hbox);
  gtk_widget_show (hbox);

  label = gtk_label_new_with_mnemonic (_("_Encoding:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.5);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  combobox = gtk_combo_box_new ();
  gtk_box_pack_start (GTK_BOX (hbox), combobox, FALSE, FALSE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combobox);
  gtk_widget_show (combobox);

  /* select the active document in the file chooser */
  filename = mousepad_file_get_filename (window->active->file);
  if (filename && g_file_test (filename, G_FILE_TEST_EXISTS))
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser), filename);

  /* run the dialog */
  if (G_LIKELY (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT))
    {
      /* hide the dialog */
      gtk_widget_hide (chooser);

      /* get a list of selected filenames */
      filenames = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (chooser));

      /* lock menu updates */
      lock_menu_updates++;

      /* open all the selected filenames in a new tab */
      for (li = filenames; li != NULL; li = li->next)
        {
          /* open the file */
          mousepad_window_open_file (window, li->data, MOUSEPAD_ENCODING_UTF_8);

          /* cleanup */
          g_free (li->data);
        }

      /* cleanup */
      g_slist_free (filenames);

      /* allow menu updates again */
      lock_menu_updates--;
    }

  /* destroy dialog */
  gtk_widget_destroy (chooser);
}



static void
mousepad_window_action_open_recent (GSimpleAction *action,
                                    GVariant      *value,
                                    gpointer       data)
{
  MousepadWindow   *window = MOUSEPAD_WINDOW (data);
  const gchar      *uri, *charset;
  MousepadEncoding  encoding;
  GError           *error = NULL;
  gchar            *filename, *action_name;
  gboolean          succeed = FALSE;
  GtkRecentInfo    *info;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* get the info */
  action_name = g_strconcat ("win.file.open-recent.new('",
                             g_variant_get_string (value, NULL),
                             "')", NULL);
  info = mousepad_object_get_data (G_OBJECT (action), action_name);
  g_free (action_name);

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
              /* try to get the charset from the description */
              charset = mousepad_window_recent_get_charset (info);

              /* lookup the encoding */
              encoding = mousepad_encoding_find (charset);

              /* try to open the file */
              succeed = mousepad_window_open_file (window, filename, encoding);
            }
          else
            {
              /* create an error */
              g_set_error (&error,  G_FILE_ERROR, G_FILE_ERROR_IO,
                           _("Failed to open \"%s\" for reading. It will be "
                             "removed from the document history"), filename);

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
mousepad_window_action_clear_recent (GSimpleAction *action,
                                     GVariant      *value,
                                     gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* ask the user if he really want to clear the history */
  if (mousepad_dialogs_clear_recent (GTK_WINDOW (window)))
    {
      /* avoid updating the menu */
      lock_menu_updates++;

      /* clear the document history */
      mousepad_window_recent_clear (window);

      /* allow menu updates again */
      lock_menu_updates--;
    }
}



static void
mousepad_window_action_save (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       data)
{
  MousepadWindow   *window = MOUSEPAD_WINDOW (data);
  MousepadDocument *document = window->active;
  GAction          *action_save_as;
  GError           *error = NULL;
  gboolean          modified;
  gint              response;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  action_save_as = g_action_map_lookup_action (G_ACTION_MAP (window), "file.save-as");
  window->save_succeed = FALSE;

  if (mousepad_file_get_filename (document->file) == NULL)
    {
      /* file has no filename yet, open the save as dialog */
      g_action_activate (action_save_as, NULL);
    }
  else
    {
      /* check whether the file is externally modified */
      modified = mousepad_file_get_externally_modified (document->file, &error);
      if (G_UNLIKELY (error != NULL))
        goto showerror;

      if (modified)
        {
          /* ask the user what to do */
          response = mousepad_dialogs_externally_modified (GTK_WINDOW (window));
        }
      else
        {
          /* save */
          response = MOUSEPAD_RESPONSE_SAVE;
        }

      switch (response)
        {
          case MOUSEPAD_RESPONSE_CANCEL:
            /* do nothing */
            window->save_succeed = FALSE;
            return;

          case MOUSEPAD_RESPONSE_SAVE_AS:
            /* run save as dialog */
            g_action_activate (action_save_as, NULL);
            break;

          case MOUSEPAD_RESPONSE_SAVE:
            /* save the document */
            window->save_succeed = mousepad_file_save (document->file, &error);
            break;
        }

      if (G_LIKELY (window->save_succeed))
        {
          /* update the window title */
          mousepad_window_set_title (window);
        }
      else if (error != NULL)
        {
          showerror:

          /* show the error */
          mousepad_dialogs_show_error (GTK_WINDOW (window), error, _("Failed to save the document"));
          g_error_free (error);
        }
    }
}



static void
mousepad_window_action_save_as (GSimpleAction *action,
                                GVariant      *value,
                                gpointer       data)
{
  MousepadWindow   *window = MOUSEPAD_WINDOW (data);
  MousepadDocument *document = window->active;
  gchar            *filename;
  const gchar      *current_filename;
  GtkWidget        *dialog;
  GtkWidget        *button;
  GAction          *action_save;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* create the dialog */
  dialog = gtk_file_chooser_dialog_new (_("Save As"),
                                        GTK_WINDOW (window), GTK_FILE_CHOOSER_ACTION_SAVE,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        NULL);
  button = mousepad_util_image_button ("document-save", _("_Save"));
  gtk_widget_set_can_default (button, TRUE);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), TRUE);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  /* set the current filename if there is one, or use the last save location */
  current_filename = mousepad_file_get_filename (document->file);
  if (current_filename)
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), current_filename);
  else if (last_save_location)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), last_save_location);

  action_save = g_action_map_lookup_action (G_ACTION_MAP (window), "file.save");
  window->save_succeed = FALSE;

  /* run the dialog */
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
      /* get the new filename */
      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

      if (G_LIKELY (filename))
        {
          /* set the new filename */
          mousepad_file_set_filename (document->file, filename);

          /* save the file with the function above */
          g_action_activate (action_save, NULL);

          if (G_LIKELY (window->save_succeed))
            {
              /* add to the recent history if saving succeeded */
              mousepad_window_recent_add (window, document->file);

              /* update last save location */
              g_free (last_save_location);
              last_save_location = g_path_get_dirname (filename);
            }

          /* cleanup */
          g_free (filename);
        }
    }

  /* destroy the dialog */
  gtk_widget_destroy (dialog);
}



static void
mousepad_window_action_save_all (GSimpleAction *action,
                                 GVariant      *value,
                                 gpointer       data)
{
  MousepadWindow   *window = MOUSEPAD_WINDOW (data);
  MousepadDocument *document;
  GAction          *action_save, *action_save_as;
  GSList           *li, *documents = NULL;
  GError           *error = NULL;
  gboolean          succeed = TRUE;
  gint              i, current, page_num;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get the current active tab */
  current = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));

  /* walk though all the document in the window */
  for (i = 0; i < gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook)); i++)
    {
      /* get the document */
      document = MOUSEPAD_DOCUMENT (gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i));

      /* debug check */
      g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

      /* continue if the document is not modified */
      if (! gtk_text_buffer_get_modified (document->buffer))
        continue;

      /* we try to quickly save files, without bothering the user */
      if (mousepad_file_get_filename (document->file) != NULL
          && mousepad_file_get_read_only (document->file) == FALSE
          && mousepad_file_get_externally_modified (document->file, NULL) == FALSE)
        {
          /* try to quickly save the file */
          succeed = mousepad_file_save (document->file, &error);

          /* break on problems */
          if (G_UNLIKELY (!succeed))
            break;
        }
      else
        {
          /* add the document to a queue to bother the user later */
          documents = g_slist_prepend (documents, document);
        }
    }

  if (G_UNLIKELY (succeed == FALSE))
    {
      /* focus the tab that triggered the problem */
      gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), i);

      /* show the error */
      mousepad_dialogs_show_error (GTK_WINDOW (window), error, _("Failed to save the document"));

      /* free error */
      if (error != NULL)
        g_error_free (error);
    }
  else
    {
      /* open a save as dialog for all the unnamed files */
      for (li = documents; li != NULL; li = li->next)
        {
          document = MOUSEPAD_DOCUMENT (li->data);

          /* get the documents page number */
          page_num = gtk_notebook_page_num (GTK_NOTEBOOK (window->notebook), GTK_WIDGET (li->data));

          if (G_LIKELY (page_num > -1))
            {
              /* focus the tab we're going to save */
              gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), page_num);

              if (mousepad_file_get_filename (document->file) == NULL
                  || mousepad_file_get_read_only (document->file))
                {
                  /* trigger the save as function */
                  action_save_as = g_action_map_lookup_action (G_ACTION_MAP (window), "file.save-as");
                  g_action_activate (action_save_as, NULL);
                }
              else
                {
                  /* trigger the save function (externally modified document) */
                  action_save = g_action_map_lookup_action (G_ACTION_MAP (window), "file.save");
                  g_action_activate (action_save, NULL);
                }
            }
        }

      /* focus the origional doc if everything went fine */
      if (G_LIKELY (li == NULL))
        gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), current);
    }

  /* cleanup */
  g_slist_free (documents);
}



static void
mousepad_window_action_revert (GSimpleAction *action,
                               GVariant      *value,
                               gpointer       data)
{
  MousepadWindow   *window = MOUSEPAD_WINDOW (data);
  MousepadDocument *document = window->active;
  GAction          *action_save_as;
  GError           *error = NULL;
  gint              response;
  gboolean          succeed;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* ask the user if he really wants to do this when the file is modified */
  if (gtk_text_buffer_get_modified (document->buffer))
    {
      /* ask the user if he really wants to revert */
      response = mousepad_dialogs_revert (GTK_WINDOW (window));

      if (response == MOUSEPAD_RESPONSE_SAVE_AS)
        {
          /* open the save as dialog, leave when use user did not save (or it failed) */
          action_save_as = g_action_map_lookup_action (G_ACTION_MAP (window), "file.save-as");
          g_action_activate (action_save_as, NULL);
          if (! window->save_succeed)
            return;
        }
      else if (response == MOUSEPAD_RESPONSE_CANCEL)
        {
          /* meh, first click revert and then cancel... pussy... */
          return;
        }

      /* small check for debug builds */
      g_return_if_fail (response == MOUSEPAD_RESPONSE_REVERT);
    }

  /* lock the undo manager */
  gtk_source_buffer_begin_not_undoable_action (GTK_SOURCE_BUFFER (document->buffer));

  /* reload the file */
  succeed = mousepad_file_reload (document->file, &error);

  /* release the lock */
  gtk_source_buffer_end_not_undoable_action (GTK_SOURCE_BUFFER (document->buffer));

  if (G_UNLIKELY (succeed == FALSE))
    {
      /* show the error */
      mousepad_dialogs_show_error (GTK_WINDOW (window), error, _("Failed to reload the document"));
      g_error_free (error);
    }
}



static void
mousepad_window_action_print (GSimpleAction *action,
                              GVariant      *value,
                              gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  MousepadPrint  *print;
  GError         *error = NULL;
  gboolean        succeed;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* create new print operation */
  print = mousepad_print_new ();

  /* print the current document */
  succeed = mousepad_print_document_interactive (print, window->active, GTK_WINDOW (window), &error);

  if (G_UNLIKELY (succeed == FALSE))
    {
      /* show the error */
      mousepad_dialogs_show_error (GTK_WINDOW (window), error, _("Failed to print the document"));
      g_error_free (error);
    }

  /* release the object */
  g_object_unref (G_OBJECT (print));
}



static void
mousepad_window_action_detach (GSimpleAction *action,
                               GVariant      *value,
                               gpointer       data)
{
  MousepadWindow   *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* invoke function without cooridinates */
  mousepad_window_notebook_create_window (GTK_NOTEBOOK (window->notebook),
                                          GTK_WIDGET (window->active),
                                          -1, -1, window);
}



static void
mousepad_window_action_close (GSimpleAction *action,
                              GVariant      *value,
                              gpointer       data)
{
  MousepadWindow   *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* close active document */
  mousepad_window_close_document (window, window->active);
}



static void
mousepad_window_action_close_window (GSimpleAction *action,
                                     GVariant      *value,
                                     gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkWidget      *document;
  gint            npages, i;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get the number of page in the notebook */
  npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook)) - 1;

  /* prevent menu updates */
  lock_menu_updates++;

  /* ask what to do with the modified document in this window */
  for (i = npages; i >= 0; --i)
    {
      /* get the document */
      document = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i);

      /* check for debug builds */
      g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

      /* focus the tab we're going to close */
      gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), i);

      /* close each document */
      if (! mousepad_window_close_document (window, MOUSEPAD_DOCUMENT (document)))
        {
          /* closing cancelled, release menu lock */
          lock_menu_updates--;

          /* leave function */
          return;
        }
    }

  /* release lock */
  lock_menu_updates--;
}



static void
mousepad_window_action_undo (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* undo */
  gtk_source_buffer_undo (GTK_SOURCE_BUFFER (window->active->buffer));

  /* scroll to visible area */
  mousepad_view_scroll_to_cursor (window->active->textview);
}



static void
mousepad_window_action_redo (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* redo */
  gtk_source_buffer_redo (GTK_SOURCE_BUFFER (window->active->buffer));

  /* scroll to visible area */
  mousepad_view_scroll_to_cursor (window->active->textview);
}



static void
mousepad_window_action_cut (GSimpleAction *action,
                            GVariant      *value,
                            gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkEditable *entry;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get searchbar entry */
  entry = mousepad_search_bar_entry (MOUSEPAD_SEARCH_BAR (window->search_bar));

  /* cut from search bar entry or textview */
  if (G_UNLIKELY (entry))
    gtk_editable_cut_clipboard (entry);
  else
    mousepad_view_clipboard_cut (window->active->textview);

  /* update the history */
  mousepad_window_paste_history_add (window);
}



static void
mousepad_window_action_copy (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkEditable *entry;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get searchbar entry */
  entry = mousepad_search_bar_entry (MOUSEPAD_SEARCH_BAR (window->search_bar));

  /* copy from search bar entry or textview */
  if (G_UNLIKELY (entry))
    gtk_editable_copy_clipboard (entry);
  else
    mousepad_view_clipboard_copy (window->active->textview);

  /* update the history */
  mousepad_window_paste_history_add (window);
}



static void
mousepad_window_action_paste (GSimpleAction *action,
                              GVariant      *value,
                              gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkEditable *entry;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get searchbar entry */
  entry = mousepad_search_bar_entry (MOUSEPAD_SEARCH_BAR (window->search_bar));

  /* paste in search bar entry or textview */
  if (G_UNLIKELY (entry))
    gtk_editable_paste_clipboard (entry);
  else
    mousepad_view_clipboard_paste (window->active->textview, NULL, FALSE);
}



static void
mousepad_window_action_paste_history (GSimpleAction *action,
                                      GVariant      *value,
                                      gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkWidget *menu;
#if GTK_CHECK_VERSION (3, 22, 0)
  GdkRectangle  location;
#endif

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get the history menu */
  menu = mousepad_window_paste_history_menu (window);

  /* select the first item in the menu */
  gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), TRUE);

#if GTK_CHECK_VERSION (3, 22, 0)
  /* get cursor location in textview coordinates */
  gtk_text_view_get_cursor_locations (GTK_TEXT_VIEW (window->active->textview), NULL, &location, NULL);
  gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (window->active->textview),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         location.x, location.y,
                                         &(location.x), &(location.y));

  /* popup the menu */
  gtk_menu_popup_at_rect (GTK_MENU (menu),
                          gtk_widget_get_parent_window (GTK_WIDGET (window->active->textview)),
                          &location, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
#else

#if G_GNUC_CHECK_VERSION (4, 3)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

  /* popup the menu */
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                  mousepad_window_paste_history_menu_position,
                  window, 0, gtk_get_current_event_time ());

#if G_GNUC_CHECK_VERSION (4, 3)
# pragma GCC diagnostic pop
#endif

#endif
}



static void
mousepad_window_action_paste_column (GSimpleAction *action,
                                     GVariant      *value,
                                     gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* paste the clipboard into a column */
  mousepad_view_clipboard_paste (window->active->textview, NULL, TRUE);
}



static void
mousepad_window_action_delete (GSimpleAction *action,
                               GVariant      *value,
                               gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkEditable *entry;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get searchbar entry */
  entry = mousepad_search_bar_entry (MOUSEPAD_SEARCH_BAR (window->search_bar));

  /* delete selection in search bar entry or textview */
  if (G_UNLIKELY (entry))
    gtk_editable_delete_selection (entry);
  else
    mousepad_view_delete_selection (window->active->textview);
}



static void
mousepad_window_action_select_all (GSimpleAction *action,
                                   GVariant      *value,
                                   gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* select everything in the document */
  mousepad_view_select_all (window->active->textview);
}



static void
mousepad_window_action_preferences (GSimpleAction *action,
                                    GVariant      *value,
                                    gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  mousepad_window_show_preferences (window);
}



static void
mousepad_window_action_lowercase (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert selection to lowercase */
  mousepad_view_convert_selection_case (window->active->textview, LOWERCASE);
}



static void
mousepad_window_action_uppercase (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert selection to uppercase */
  mousepad_view_convert_selection_case (window->active->textview, UPPERCASE);
}



static void
mousepad_window_action_titlecase (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert selection to titlecase */
  mousepad_view_convert_selection_case (window->active->textview, TITLECASE);
}



static void
mousepad_window_action_opposite_case (GSimpleAction *action,
                                      GVariant      *value,
                                      gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert selection to opposite case */
  mousepad_view_convert_selection_case (window->active->textview, OPPOSITE_CASE);
}



static void
mousepad_window_action_tabs_to_spaces (GSimpleAction *action,
                                       GVariant      *value,
                                       gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert tabs to spaces */
  mousepad_view_convert_spaces_and_tabs (window->active->textview, TABS_TO_SPACES);
}



static void
mousepad_window_action_spaces_to_tabs (GSimpleAction *action,
                                       GVariant      *value,
                                       gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert spaces to tabs */
  mousepad_view_convert_spaces_and_tabs (window->active->textview, SPACES_TO_TABS);
}



static void
mousepad_window_action_strip_trailing_spaces (GSimpleAction *action,
                                              GVariant      *value,
                                              gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert spaces to tabs */
  mousepad_view_strip_trailing_spaces (window->active->textview);
}



static void
mousepad_window_action_transpose (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* transpose */
  mousepad_view_transpose (window->active->textview);
}



static void
mousepad_window_action_move_line_up (GSimpleAction *action,
                                     GVariant      *value,
                                     gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* move the selection on line up */
  mousepad_view_move_selection (window->active->textview, MOVE_LINE_UP);
}



static void
mousepad_window_action_move_line_down (GSimpleAction *action,
                                       GVariant      *value,
                                       gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* move the selection on line down */
  mousepad_view_move_selection (window->active->textview, MOVE_LINE_DOWN);
}



static void
mousepad_window_action_duplicate (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* dupplicate */
  mousepad_view_duplicate (window->active->textview);
}



static void
mousepad_window_action_increase_indent (GSimpleAction *action,
                                        GVariant      *value,
                                        gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* increase the indent */
  mousepad_view_indent (window->active->textview, INCREASE_INDENT);
}



static void
mousepad_window_action_decrease_indent (GSimpleAction *action,
                                        GVariant      *value,
                                        gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* decrease the indent */
  mousepad_view_indent (window->active->textview, DECREASE_INDENT);
}



static void
mousepad_window_action_find (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkTextIter     selection_start, selection_end;
  gchar          *selection;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* create a new search bar is needed */
  if (window->search_bar == NULL)
    {
      /* create a new toolbar and pack it into the box */
      window->search_bar = mousepad_search_bar_new ();
      gtk_box_pack_start (GTK_BOX (window->box), window->search_bar, FALSE, FALSE, PADDING);

      /* connect signals */
      g_signal_connect_swapped (G_OBJECT (window->search_bar), "hide-bar",
                                G_CALLBACK (mousepad_window_hide_search_bar), window);
      g_signal_connect_swapped (G_OBJECT (window->search_bar), "search",
                                G_CALLBACK (mousepad_window_search), window);
    }

  /* set the search entry text */
  if (gtk_text_buffer_get_has_selection (window->active->buffer) == TRUE)
    {
      gtk_text_buffer_get_selection_bounds (window->active->buffer, &selection_start, &selection_end);
      selection = gtk_text_buffer_get_text (window->active->buffer, &selection_start, &selection_end, 0);

      /* selection should be one line */
      if (g_strrstr (selection, "\n") == NULL && g_strrstr (selection, "\r") == NULL)
        mousepad_search_bar_set_text (MOUSEPAD_SEARCH_BAR (window->search_bar), selection);

      g_free (selection);
    }

  /* show the search bar */
  gtk_widget_show (window->search_bar);

  /* focus the search entry */
  mousepad_search_bar_focus (MOUSEPAD_SEARCH_BAR (window->search_bar));
}



static void
mousepad_window_action_find_next (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* find the next occurrence */
  if (G_LIKELY (window->search_bar != NULL))
    mousepad_search_bar_find_next (MOUSEPAD_SEARCH_BAR (window->search_bar));
}



static void
mousepad_window_action_find_previous (GSimpleAction *action,
                                      GVariant      *value,
                                      gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* find the previous occurrence */
  if (G_LIKELY (window->search_bar != NULL))
    mousepad_search_bar_find_previous (MOUSEPAD_SEARCH_BAR (window->search_bar));
}



static void
mousepad_window_action_replace_switch_page (MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_REPLACE_DIALOG (window->replace_dialog));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* page switched */
  mousepad_replace_dialog_page_switched (MOUSEPAD_REPLACE_DIALOG (window->replace_dialog));
}


static void
mousepad_window_action_replace_destroy (MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* disconnect tab switch signal */
  mousepad_disconnect_by_func (G_OBJECT (window->notebook),
                               mousepad_window_action_replace_switch_page,
                               window);

  /* reset the dialog variable */
  window->replace_dialog = NULL;
}


static void
mousepad_window_action_replace (GSimpleAction *action,
                                GVariant      *value,
                                gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkTextIter     selection_start, selection_end;
  gchar          *selection;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  if (window->replace_dialog == NULL)
    {
      /* create a new dialog */
      window->replace_dialog = mousepad_replace_dialog_new ();

      /* popup the dialog */
      gtk_window_set_destroy_with_parent (GTK_WINDOW (window->replace_dialog), TRUE);
      gtk_window_set_transient_for (GTK_WINDOW (window->replace_dialog), GTK_WINDOW (window));
      gtk_widget_show (window->replace_dialog);

      /* connect signals */
      g_signal_connect_swapped (G_OBJECT (window->replace_dialog), "destroy",
                                G_CALLBACK (mousepad_window_action_replace_destroy), window);
      g_signal_connect_swapped (G_OBJECT (window->replace_dialog), "search",
                                G_CALLBACK (mousepad_window_search), window);
      g_signal_connect_swapped (G_OBJECT (window->notebook), "switch-page",
                                G_CALLBACK (mousepad_window_action_replace_switch_page), window);
    }
  else
    {
      /* focus the existing dialog */
      gtk_window_present (GTK_WINDOW (window->replace_dialog));
    }

  /* set the search entry text */
  if (gtk_text_buffer_get_has_selection (window->active->buffer) == TRUE)
    {
      gtk_text_buffer_get_selection_bounds (window->active->buffer, &selection_start, &selection_end);
      selection = gtk_text_buffer_get_text (window->active->buffer, &selection_start, &selection_end, 0);

      /* selection should be one line */
      if (g_strrstr (selection, "\n") == NULL && g_strrstr (selection, "\r") == NULL)
        mousepad_replace_dialog_set_text (MOUSEPAD_REPLACE_DIALOG (window->replace_dialog), selection);

      g_free (selection);
    }
}



static void
mousepad_window_action_go_to_position (GSimpleAction *action,
                                       GVariant      *value,
                                       gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (window->active->buffer));

  /* run jump dialog */
  if (mousepad_dialogs_go_to (GTK_WINDOW (window), window->active->buffer))
    {
      /* put the cursor on screen */
      mousepad_view_scroll_to_cursor (window->active->textview);
    }
}



static void
mousepad_window_action_select_font (GSimpleAction *action,
                                    GVariant      *value,
                                    gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkWidget      *dialog;
  gchar          *font_name;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  dialog = gtk_font_chooser_dialog_new (_("Choose Mousepad Font"), GTK_WINDOW (window));

  /* set the current font name */
  font_name = MOUSEPAD_SETTING_GET_STRING (FONT_NAME);

  if (G_LIKELY (font_name))
    {
      gtk_font_chooser_set_font (GTK_FONT_CHOOSER (dialog), font_name);
      g_free (font_name);
    }

  /* run the dialog */
  if (G_LIKELY (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK))
    {
      /* get the selected font from the dialog */
      font_name = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (dialog));

      /* store the font in the preferences */
      MOUSEPAD_SETTING_SET_STRING (FONT_NAME, font_name);
      /* stop using default font */
      MOUSEPAD_SETTING_SET_BOOLEAN (USE_DEFAULT_FONT, FALSE);

      /* cleanup */
      g_free (font_name);
    }

  /* destroy dialog */
  gtk_widget_destroy (dialog);
}



static void
mousepad_window_action_line_numbers (GSimpleAction *action,
                                     GVariant      *value,
                                     gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* save as the last used line number setting */
  MOUSEPAD_SETTING_SET_BOOLEAN (SHOW_LINE_NUMBERS, g_variant_get_boolean (value));
}



static void
mousepad_window_action_menubar (GSimpleAction *action,
                                GVariant      *value,
                                gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  gboolean        active;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  active = g_variant_get_boolean (value);

  if (mousepad_window_get_in_fullscreen (window))
    MOUSEPAD_SETTING_SET_ENUM (MENUBAR_VISIBLE_FULLSCREEN, (active ? 2 : 1));
  else
    MOUSEPAD_SETTING_SET_BOOLEAN (MENUBAR_VISIBLE, active);
}



static void
mousepad_window_action_toolbar (GSimpleAction *action,
                                GVariant      *value,
                                gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  gboolean        active;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  active = g_variant_get_boolean (value);

  if (mousepad_window_get_in_fullscreen (window))
    MOUSEPAD_SETTING_SET_ENUM (TOOLBAR_VISIBLE_FULLSCREEN, (active ? 2 : 1));
  else
    MOUSEPAD_SETTING_SET_BOOLEAN (TOOLBAR_VISIBLE, active);
}



static void
mousepad_window_action_statusbar_overwrite (MousepadWindow *window,
                                            gboolean        overwrite)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* set the new overwrite mode */
  mousepad_document_set_overwrite (window->active, overwrite);
}



static void
mousepad_window_action_statusbar (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  gboolean        active;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  active = g_variant_get_boolean (value);

  if (mousepad_window_get_in_fullscreen (window))
    MOUSEPAD_SETTING_SET_ENUM (STATUSBAR_VISIBLE_FULLSCREEN, (active ? 2 : 1));
  else
    MOUSEPAD_SETTING_SET_BOOLEAN (STATUSBAR_VISIBLE, active);
}



static gboolean
mousepad_window_fullscreen_bars_timer (gpointer user_data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (user_data);

  mousepad_window_update_main_widgets (window);

  MOUSEPAD_WINDOW (user_data)->fullscreen_bars_timer_id = 0;

  return FALSE;
}



static void
mousepad_window_update_main_widgets (MousepadWindow *window)
{
  GAction   *action;
  GPtrArray *tooltips;
  gboolean   fullscreen, mb_visible, tb_visible, sb_visible,
             mb_active, tb_active, sb_active;
  gint       mb_visible_fs, tb_visible_fs, sb_visible_fs;

  if (! gtk_widget_get_visible (GTK_WIDGET (window)))
    return;

  fullscreen = mousepad_window_get_in_fullscreen (window);

  /* get the non-fullscreen settings */
  mb_visible = MOUSEPAD_SETTING_GET_BOOLEAN (MENUBAR_VISIBLE);
  tb_visible = MOUSEPAD_SETTING_GET_BOOLEAN (TOOLBAR_VISIBLE);
  sb_visible = MOUSEPAD_SETTING_GET_BOOLEAN (STATUSBAR_VISIBLE);

  /* get the fullscreen settings */
  mb_visible_fs = MOUSEPAD_SETTING_GET_ENUM (MENUBAR_VISIBLE_FULLSCREEN);
  tb_visible_fs = MOUSEPAD_SETTING_GET_ENUM (TOOLBAR_VISIBLE_FULLSCREEN);
  sb_visible_fs = MOUSEPAD_SETTING_GET_ENUM (STATUSBAR_VISIBLE_FULLSCREEN);

  /* set to true or false based on fullscreen setting */
  mb_visible_fs = (mb_visible_fs == 0) ? mb_visible : (mb_visible_fs == 2);
  tb_visible_fs = (tb_visible_fs == 0) ? tb_visible : (tb_visible_fs == 2);
  sb_visible_fs = (sb_visible_fs == 0) ? sb_visible : (sb_visible_fs == 2);

  mb_active = fullscreen ? mb_visible_fs : mb_visible;
  tb_active = fullscreen ? tb_visible_fs : tb_visible;
  sb_active = fullscreen ? sb_visible_fs : sb_visible;

  /* update the widgets' visibility */
  gtk_widget_set_visible (window->menubar, mb_active);
  gtk_widget_set_visible (window->toolbar, tb_active);
  gtk_widget_set_visible (window->statusbar, sb_active);

  /* update the toolbar with the settings */
  mousepad_window_update_toolbar (window, NULL, NULL);

  /* avoid menu actions */
  lock_menu_updates++;

  /* sync the action states to the settings */
  action = g_action_map_lookup_action (G_ACTION_MAP (window), "view.menubar");
  g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (mb_active));
  action = g_action_map_lookup_action (G_ACTION_MAP (window), "view.toolbar");
  g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (tb_active));
  action = g_action_map_lookup_action (G_ACTION_MAP (window), "view.statusbar");
  g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (sb_active));
  action = g_action_map_lookup_action (G_ACTION_MAP (window), "textview.menubar");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), ! mb_active);

  /* set the textview menu last tooltip */
  if (! mb_active)
    {
      tooltips = g_ptr_array_new ();
      g_ptr_array_add (tooltips, (gpointer) menubar_tooltips[52]);
      mousepad_window_menu_set_tooltips (window, window->textview_menu, tooltips, 1, 0);
      g_ptr_array_free (tooltips, TRUE);
    }

  /* allow menu actions again */
  lock_menu_updates--;
}



static void
mousepad_window_action_fullscreen (GSimpleAction *action,
                                   GVariant      *value,
                                   gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkBuilder     *builder;
  GtkWidget      *gtkmenu;
  GtkToolItem    *tool_item;
  GMenu          *menu;
  GMenuItem      *item;
  GIcon          *icon = NULL;
  GPtrArray      *tooltips;
  gboolean        fullscreen;
  const gchar    *tooltip;

  if (! gtk_widget_get_visible (GTK_WIDGET (window)))
    return;

  /* avoid menu actions */
  lock_menu_updates++;

  fullscreen = g_variant_get_boolean (value);
  g_simple_action_set_state (action, value);

  /* entering fullscreen mode */
  if (fullscreen && ! mousepad_window_get_in_fullscreen (window))
    {
      gtk_window_fullscreen (GTK_WINDOW (window));
      icon = g_icon_new_for_string ("view-restore", NULL);
      tooltip = "Leave fullscreen mode";
    }
  /* leaving fullscreen mode */
  else if (mousepad_window_get_in_fullscreen (window))
    {
      gtk_window_unfullscreen (GTK_WINDOW (window));
      icon = g_icon_new_for_string ("view-fullscreen", NULL);
      tooltip = "Make the window fullscreen";
    }

  /* update the menu item icon */
  if (icon)
    {
      builder = mousepad_application_get_builder (MOUSEPAD_APPLICATION (
                  gtk_window_get_application (GTK_WINDOW (window))));
      menu = G_MENU (gtk_builder_get_object (builder, "view.fullscreen"));
      item = g_menu_item_new_from_model (G_MENU_MODEL (menu), 0);
      g_menu_item_set_icon (item, icon);
      g_object_unref (icon);
      g_menu_remove (menu, 0);
      g_menu_prepend_item (menu, item);
      g_object_unref (item);
    }

  /* update the tooltip */
  gtkmenu = mousepad_window_get_menubar_submenu (window, window->menubar, "view-menu-flag");
  tooltips = g_ptr_array_new ();
  g_ptr_array_add (tooltips, (gpointer) tooltip);
  mousepad_window_menu_set_tooltips (window, gtkmenu, tooltips, 1, 0);
  g_ptr_array_free (tooltips, TRUE);

  tool_item = gtk_toolbar_get_nth_item (GTK_TOOLBAR (window->toolbar),
                                        gtk_toolbar_get_n_items (GTK_TOOLBAR (window->toolbar)) - 1);
  gtk_tool_item_set_tooltip_text (tool_item, _(tooltip));

  /* update the widgets based on whether in fullscreen mode or not */
  if (window->fullscreen_bars_timer_id == 0)
    window->fullscreen_bars_timer_id = g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE, 50,
                                                           mousepad_window_fullscreen_bars_timer,
                                                           window, NULL);

  /* allow menu updates again */
  lock_menu_updates--;
}



static void
mousepad_window_action_auto_indent (GSimpleAction *action,
                                    GVariant      *value,
                                    gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* save as the last auto indent mode */
  MOUSEPAD_SETTING_SET_BOOLEAN (AUTO_INDENT, g_variant_get_boolean (value));
}



static void
mousepad_window_action_line_ending (GSimpleAction *action,
                                    GVariant      *value,
                                    gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* leave when menu updates are locked */
  if (lock_menu_updates == 0)
    {
      /* avoid menu actions */
      lock_menu_updates++;

      /* set the current state and the new line ending on the file */
      g_simple_action_set_state (action, value);
      mousepad_file_set_line_ending (window->active->file, g_variant_get_int32 (value));

      /* make buffer as modified to show the user the change is not saved */
      gtk_text_buffer_set_modified (window->active->buffer, TRUE);

      /* allow menu actions again */
      lock_menu_updates--;
    }
}



static void
mousepad_window_action_tab_size (GSimpleAction *action,
                                 GVariant      *value,
                                 gpointer       data)
{
  gint            tab_size;
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* leave when menu updates are locked */
  if (lock_menu_updates == 0)
    {
      /* get the tab size */
      tab_size = g_variant_get_int32 (value);

      /* whether the other item was clicked */
      if (tab_size == 0)
        {
          /* get tab size from document */
          tab_size = MOUSEPAD_SETTING_GET_INT (TAB_WIDTH);

          /* select other size in dialog */
          tab_size = mousepad_dialogs_other_tab_size (GTK_WINDOW (window), tab_size);
        }

      /* store as last used value */
      MOUSEPAD_SETTING_SET_INT (TAB_WIDTH, tab_size);
    }
}



static void
mousepad_window_action_color_scheme (GSimpleAction *action,
                                     GVariant      *value,
                                     gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* leave when menu updates are locked */
  if (lock_menu_updates == 0)
    MOUSEPAD_SETTING_SET_STRING (COLOR_SCHEME, g_variant_get_string (value, NULL));
}



static void
mousepad_window_action_language (GSimpleAction *action,
                                 GVariant      *value,
                                 gpointer       data)
{
  MousepadWindow    *window = MOUSEPAD_WINDOW (data);
  GtkSourceLanguage *language;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* leave when menu updates are locked */
  if (lock_menu_updates == 0)
    {
      /* avoid menu actions */
      lock_menu_updates++;

      g_simple_action_set_state (action, value);
      language = gtk_source_language_manager_get_language (gtk_source_language_manager_get_default (),
                                                           g_variant_get_string (value, NULL));
      mousepad_file_set_language (window->active->file, language);

      /* mark the file as having its language chosen explicitly by the user
       * so we don't clobber their choice by guessing ourselves */
      mousepad_file_set_user_set_language (window->active->file, TRUE);

      /* allow menu actions again */
      lock_menu_updates--;
    }
}



static void
mousepad_window_action_insert_spaces (GSimpleAction *action,
                                      GVariant      *value,
                                      gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* leave when menu updates are locked */
  if (lock_menu_updates == 0)
    {
      /* save as the last auto indent mode */
      MOUSEPAD_SETTING_SET_BOOLEAN (INSERT_SPACES, g_variant_get_boolean (value));
    }
}



static void
mousepad_window_action_word_wrap (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* leave when menu updates are locked */
  if (lock_menu_updates == 0)
    MOUSEPAD_SETTING_SET_BOOLEAN (WORD_WRAP, g_variant_get_boolean (value));
}



static void
mousepad_window_action_write_bom (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* leave when menu updates are locked */
  if (lock_menu_updates == 0)
    {
      /* avoid menu actions */
      lock_menu_updates++;

      /* set the current state */
      g_simple_action_set_state (action, value);

      /* set new value */
      mousepad_file_set_write_bom (window->active->file, g_variant_get_boolean (value));

      /* make buffer as modified to show the user the change is not saved */
      gtk_text_buffer_set_modified (window->active->buffer, TRUE);

      /* allow menu actions again */
      lock_menu_updates--;
    }
}



static void
mousepad_window_action_prev_tab (GSimpleAction *action,
                                 GVariant      *value,
                                 gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  gint page_num, n_pages;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* get notebook info */
  page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));
  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

  /* switch to the previous tab or cycle to the last tab */
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), (page_num - 1) % n_pages);
}



static void
mousepad_window_action_next_tab (GSimpleAction *action,
                                 GVariant      *value,
                                 gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  gint page_num, n_pages;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* get notebook info */
  page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));
  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

  /* switch to the next tab or cycle to the first tab */
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), (page_num + 1) % n_pages);
}



static void
mousepad_window_action_go_to_tab (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* leave when the menu is locked */
  if (lock_menu_updates == 0)
    {
      /* avoid menu actions */
      lock_menu_updates++;

      gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), g_variant_get_int32 (value));
      g_simple_action_set_state (action, value);

      /* allow menu actions again */
      lock_menu_updates--;
    }
}



static void
mousepad_window_action_contents (GSimpleAction *action,
                                 GVariant      *value,
                                 gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* show help */
  mousepad_dialogs_show_help (GTK_WINDOW (window), NULL, NULL);
}



static void
mousepad_window_action_about (GSimpleAction *action,
                              GVariant      *value,
                              gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* show about dialog */
  mousepad_dialogs_show_about (GTK_WINDOW (window));
}



void
mousepad_window_show_preferences (MousepadWindow  *window)
{
  MousepadApplication *application;
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  application = mousepad_application_get ();
  mousepad_application_show_preferences (application, GTK_WINDOW (window));
}
