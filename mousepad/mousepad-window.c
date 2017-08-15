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
#include <mousepad/mousepad-action-group.h>
#include <mousepad/mousepad-application.h>
#include <mousepad/mousepad-marshal.h>
#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-dialogs.h>
#include <mousepad/mousepad-gtkcompat.h>
#include <mousepad/mousepad-replace-dialog.h>
#include <mousepad/mousepad-encoding-dialog.h>
#include <mousepad/mousepad-search-bar.h>
#include <mousepad/mousepad-statusbar.h>
#include <mousepad/mousepad-print.h>
#include <mousepad/mousepad-window.h>
#include <mousepad/mousepad-window-ui.h>

#include <glib/gstdio.h>
#include <gtksourceview/gtksourcelanguage.h>
#include <gtksourceview/gtksourcelanguagemanager.h>
#include <gtksourceview/gtksourcebuffer.h>

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
static GtkWidget        *mousepad_window_provide_languages_menu       (MousepadWindow         *window,
                                                                       MousepadStatusbar      *statusbar);
static void              mousepad_window_create_statusbar             (MousepadWindow         *window);
static gboolean          mousepad_window_get_in_fullscreen            (MousepadWindow         *window);
static void              mousepad_window_update_main_widgets          (MousepadWindow         *window);

/* notebook signals */
static void              mousepad_window_notebook_switch_page         (GtkNotebook            *notebook,
                                                                       GtkWidget              *page,
                                                                       guint                   page_num,
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
                                                                       GtkWidget              *menu,
                                                                       const gchar            *path);
static void              mousepad_window_menu_templates               (GtkWidget              *item,
                                                                       MousepadWindow         *window);
static void              mousepad_window_menu_tab_sizes               (MousepadWindow         *window);
static void              mousepad_window_menu_tab_sizes_update        (MousepadWindow         *window);
static void              mousepad_window_menu_textview_deactivate     (GtkWidget              *menu,
                                                                       MousepadWindow         *window);
static void              mousepad_window_menu_textview_popup          (GtkTextView            *textview,
                                                                       GtkMenu                *old_menu,
                                                                       MousepadWindow         *window);
static void              mousepad_window_update_actions               (MousepadWindow         *window);
static gboolean          mousepad_window_update_gomenu_idle           (gpointer                user_data);
static void              mousepad_window_update_gomenu_idle_destroy   (gpointer                user_data);
static void              mousepad_window_update_gomenu                (MousepadWindow         *window);
static void              mousepad_window_update_tabs                  (MousepadWindow         *window,
                                                                       gchar                  *key,
                                                                       GSettings              *settings);

/* recent functions */
static void              mousepad_window_recent_add                   (MousepadWindow         *window,
                                                                       MousepadFile           *file);
static gint              mousepad_window_recent_sort                  (GtkRecentInfo          *a,
                                                                       GtkRecentInfo          *b);
static void              mousepad_window_recent_manager_init          (MousepadWindow         *window);
static gboolean          mousepad_window_recent_menu_idle             (gpointer                user_data);
static void              mousepad_window_recent_menu_idle_destroy     (gpointer                user_data);
static void              mousepad_window_recent_menu                  (MousepadWindow         *window);
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
static void              mousepad_window_paste_history_menu_position  (GtkMenu                *menu,
                                                                       gint                   *x,
                                                                       gint                   *y,
                                                                       gboolean               *push_in,
                                                                       gpointer                user_data);
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
static void              mousepad_window_action_new                   (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_new_window            (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_new_from_template     (GtkMenuItem            *item,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_open                  (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_open_recent           (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_clear_recent          (GtkAction              *action,
                                                                       MousepadWindow         *window);
static gboolean          mousepad_window_action_save                  (GtkAction              *action,
                                                                       MousepadWindow         *window);
static gboolean          mousepad_window_action_save_as               (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_save_all              (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_revert                (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_print                 (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_detach                (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_close                 (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_close_window          (GtkAction              *action,
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
static void              mousepad_window_action_paste_history         (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_paste_column          (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_delete                (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_select_all            (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_change_selection      (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_preferences           (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_lowercase             (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_uppercase             (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_titlecase             (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_opposite_case         (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_tabs_to_spaces        (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_spaces_to_tabs        (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_strip_trailing_spaces (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_transpose             (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_move_line_up          (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_move_line_down        (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_duplicate             (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_increase_indent       (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_decrease_indent       (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_find                  (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_find_next             (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_find_previous         (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_replace_destroy       (MousepadWindow         *window);
static void              mousepad_window_action_replace               (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_go_to_position        (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_select_font           (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_line_numbers          (GtkToggleAction        *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_menubar               (GtkToggleAction        *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_toolbar               (GtkToggleAction        *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_statusbar_overwrite   (MousepadWindow         *window,
                                                                       gboolean                overwrite);
static void              mousepad_window_action_statusbar             (GtkToggleAction        *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_fullscreen            (GtkToggleAction        *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_auto_indent           (GtkToggleAction        *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_line_ending           (GtkRadioAction         *action,
                                                                       GtkRadioAction         *current,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_tab_size              (GtkToggleAction        *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_insert_spaces         (GtkToggleAction        *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_word_wrap             (GtkToggleAction        *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_write_bom             (GtkToggleAction        *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_prev_tab              (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_next_tab              (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_go_to_tab             (GtkRadioAction         *action,
                                                                       GtkNotebook            *notebook);
static void              mousepad_window_action_contents              (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_about                 (GtkAction              *action,
                                                                       MousepadWindow         *window);



struct _MousepadWindowClass
{
  GtkWindowClass __parent__;
};

struct _MousepadWindow
{
  GtkWindow __parent__;

  /* the current active document */
  MousepadDocument    *active;

  /* action groups */
  GtkActionGroup      *action_group;

  /* recent manager */
  GtkRecentManager    *recent_manager;

  /* UI manager */
  GtkUIManager        *ui_manager;
  guint                gomenu_merge_id;
  guint                recent_merge_id;

  /* main window widgets */
  GtkWidget           *box;
  GtkWidget           *notebook;
  GtkWidget           *search_bar;
  GtkWidget           *statusbar;
  GtkWidget           *replace_dialog;
  GtkWidget           *toolbar;
  GtkWidget           *menubar;

  /* support to remember window geometry */
  guint                save_geometry_timer_id;

  /* idle update functions for the recent and go menu */
  guint                update_recent_menu_id;
  guint                update_go_menu_id;
};



static const GtkActionEntry action_entries[] =
{
  { "file-menu", NULL, N_("_File"), NULL, NULL, NULL, },
    { "new", GTK_STOCK_NEW, N_("_New"), "<control>N", N_("Create a new document"), G_CALLBACK (mousepad_window_action_new), },
    { "new-window", NULL, N_("New _Window"), "<shift><control>N", N_("Create a new document in a new window"), G_CALLBACK (mousepad_window_action_new_window), },
    { "template-menu", NULL, N_("New From Te_mplate"), NULL, NULL, NULL, },
    { "open", GTK_STOCK_OPEN, N_("_Open..."), NULL, N_("Open a file"), G_CALLBACK (mousepad_window_action_open), },
    { "recent-menu", NULL, N_("Op_en Recent"), NULL, NULL, NULL, },
      { "no-recent-items", NULL, N_("No items found"), NULL, NULL, NULL, },
      { "clear-recent", GTK_STOCK_CLEAR, N_("Clear _History"), NULL, N_("Clear the recently used files history"), G_CALLBACK (mousepad_window_action_clear_recent), },
    { "save", GTK_STOCK_SAVE, NULL, "<control>S", N_("Save the current document"), G_CALLBACK (mousepad_window_action_save), },
    { "save-as", GTK_STOCK_SAVE_AS, N_("Save _As..."), "<shift><control>S", N_("Save current document as another file"), G_CALLBACK (mousepad_window_action_save_as), },
    { "save-all", NULL, N_("Save A_ll"), NULL, N_("Save all document in this window"), G_CALLBACK (mousepad_window_action_save_all), },
    { "revert", GTK_STOCK_REVERT_TO_SAVED, N_("Re_vert"), NULL, N_("Revert to the saved version of the file"), G_CALLBACK (mousepad_window_action_revert), },
    { "print", GTK_STOCK_PRINT, N_("_Print..."), "<control>P", N_("Print the current document"), G_CALLBACK (mousepad_window_action_print), },
    { "detach", NULL, N_("_Detach Tab"), "<control>D", N_("Move the current document to a new window"), G_CALLBACK (mousepad_window_action_detach), },
    { "close", GTK_STOCK_CLOSE, N_("Close _Tab"), "<control>W", N_("Close the current document"), G_CALLBACK (mousepad_window_action_close), },
    { "close-window", GTK_STOCK_QUIT, N_("_Close Window"), "<control>Q", N_("Close this window"), G_CALLBACK (mousepad_window_action_close_window), },

  { "edit-menu", NULL, N_("_Edit"), NULL, NULL, NULL, },
    { "undo", GTK_STOCK_UNDO, NULL, "<control>Z", N_("Undo the last action"), G_CALLBACK (mousepad_window_action_undo), },
    { "redo", GTK_STOCK_REDO, NULL, "<control>Y", N_("Redo the last undone action"), G_CALLBACK (mousepad_window_action_redo), },
    { "cut", GTK_STOCK_CUT, NULL, NULL, N_("Cut the selection"), G_CALLBACK (mousepad_window_action_cut), },
    { "copy", GTK_STOCK_COPY, NULL, NULL, N_("Copy the selection"), G_CALLBACK (mousepad_window_action_copy), },
    { "paste", GTK_STOCK_PASTE, NULL, NULL, N_("Paste the clipboard"), G_CALLBACK (mousepad_window_action_paste), },
    { "paste-menu", NULL, N_("Paste _Special"), NULL, NULL, NULL, },
      { "paste-history", NULL, N_("Paste from _History"), NULL, N_("Paste from the clipboard history"), G_CALLBACK (mousepad_window_action_paste_history), },
      { "paste-column", NULL, N_("Paste as _Column"), NULL, N_("Paste the clipboard text into a column"), G_CALLBACK (mousepad_window_action_paste_column), },
    { "delete", GTK_STOCK_DELETE, NULL, NULL, N_("Delete the current selection"), G_CALLBACK (mousepad_window_action_delete), },
    { "select-all", GTK_STOCK_SELECT_ALL, NULL, NULL, N_("Select the text in the entire document"), G_CALLBACK (mousepad_window_action_select_all), },
    { "change-selection", NULL, N_("Change the selection"), NULL, N_("Change a normal selection into a column selection and vice versa"), G_CALLBACK (mousepad_window_action_change_selection), },
    { "convert-menu", NULL, N_("Conve_rt"), NULL, NULL, NULL, },
      { "uppercase", NULL, N_("To _Uppercase"), NULL, N_("Change the case of the selection to uppercase"), G_CALLBACK (mousepad_window_action_uppercase), },
      { "lowercase", NULL, N_("To _Lowercase"), NULL, N_("Change the case of the selection to lowercase"), G_CALLBACK (mousepad_window_action_lowercase), },
      { "titlecase", NULL, N_("To _Title Case"), NULL, N_("Change the case of the selection to title case"), G_CALLBACK (mousepad_window_action_titlecase), },
      { "opposite-case", NULL, N_("To _Opposite Case"), NULL, N_("Change the case of the selection opposite case"), G_CALLBACK (mousepad_window_action_opposite_case), },
      { "tabs-to-spaces", NULL, N_("_Tabs to Spaces"), NULL, N_("Convert all tabs to spaces in the selection or document"), G_CALLBACK (mousepad_window_action_tabs_to_spaces), },
      { "spaces-to-tabs", NULL, N_("_Spaces to Tabs"), NULL, N_("Convert all the leading spaces to tabs in the selected line(s) or document"), G_CALLBACK (mousepad_window_action_spaces_to_tabs), },
      { "strip-trailing", NULL, N_("St_rip Trailing Spaces"), NULL, N_("Remove all the trailing spaces from the selected line(s) or document"), G_CALLBACK (mousepad_window_action_strip_trailing_spaces), },
      { "transpose", NULL, N_("_Transpose"), "<control>T", N_("Reverse the order of something"), G_CALLBACK (mousepad_window_action_transpose), },
    { "move-menu", NULL, N_("_Move Selection"), NULL, NULL, NULL, },
      { "line-up", NULL, N_("Line _Up"), NULL, N_("Move the selection one line up"), G_CALLBACK (mousepad_window_action_move_line_up), },
      { "line-down", NULL, N_("Line _Down"), NULL, N_("Move the selection one line down"), G_CALLBACK (mousepad_window_action_move_line_down), },
    { "duplicate", NULL, N_("Dup_licate Line / Selection"), NULL, N_("Duplicate the current line or selection"), G_CALLBACK (mousepad_window_action_duplicate), },
    { "increase-indent", GTK_STOCK_INDENT, N_("_Increase Indent"), NULL, N_("Increase the indentation of the selection or current line"), G_CALLBACK (mousepad_window_action_increase_indent), },
    { "decrease-indent", GTK_STOCK_UNINDENT, N_("_Decrease Indent"), NULL, N_("Decrease the indentation of the selection or current line"), G_CALLBACK (mousepad_window_action_decrease_indent), },
    { "preferences", GTK_STOCK_PREFERENCES, N_("Preferences"), NULL, N_("Show the preferences dialog"), G_CALLBACK (mousepad_window_action_preferences), },

  { "search-menu", NULL, N_("_Search"), NULL, NULL, NULL, },
    { "find", GTK_STOCK_FIND, NULL, NULL, N_("Search for text"), G_CALLBACK (mousepad_window_action_find), },
    { "find-next", NULL, N_("Find _Next"), "<control>g", N_("Search forwards for the same text"), G_CALLBACK (mousepad_window_action_find_next), },
    { "find-previous", NULL, N_("Find _Previous"), "<shift><control>g", N_("Search backwards for the same text"), G_CALLBACK (mousepad_window_action_find_previous), },
    { "replace", GTK_STOCK_FIND_AND_REPLACE, N_("Find and Rep_lace..."), NULL, N_("Search for and replace text"), G_CALLBACK (mousepad_window_action_replace), },
    { "go-to", GTK_STOCK_JUMP_TO, N_("_Go to..."), "<control>l", N_("Go to a specific location in the document"), G_CALLBACK (mousepad_window_action_go_to_position), },

  { "view-menu", NULL, N_("_View"), NULL, NULL, NULL, },
    { "font", GTK_STOCK_SELECT_FONT, N_("Select F_ont..."), NULL, N_("Change the editor font"), G_CALLBACK (mousepad_window_action_select_font), },
    { "color-scheme-menu", NULL, N_("_Color Scheme"), NULL, NULL, NULL, },

  { "document-menu", NULL, N_("_Document"), NULL, NULL, NULL, },
    { "eol-menu", NULL, N_("Line E_nding"), NULL, NULL, NULL, },
    { "tab-size-menu", NULL, N_("Tab _Size"), NULL, NULL, NULL, },
    { "language-menu", NULL, N_("_Filetype"), NULL, NULL, NULL, },
    { "back", GTK_STOCK_GO_BACK, N_("_Previous Tab"), "<control>Page_Up", N_("Select the previous tab"), G_CALLBACK (mousepad_window_action_prev_tab), },
    { "forward", GTK_STOCK_GO_FORWARD, N_("_Next Tab"), "<control>Page_Down", N_("Select the next tab"), G_CALLBACK (mousepad_window_action_next_tab), },

  { "help-menu", NULL, N_("_Help"), NULL, NULL, NULL },
    { "contents", GTK_STOCK_HELP, N_ ("_Contents"), "F1", N_("Display the Mousepad user manual"), G_CALLBACK (mousepad_window_action_contents), },
    { "about", GTK_STOCK_ABOUT, NULL, NULL, N_("About this application"), G_CALLBACK (mousepad_window_action_about), }
};

static const GtkToggleActionEntry toggle_action_entries[] =
{
  { "line-numbers", NULL, N_("Line N_umbers"), NULL, N_("Show line numbers"), G_CALLBACK (mousepad_window_action_line_numbers), FALSE, },
  { "menubar", NULL, N_("_Menubar"), NULL, N_("Change the visibility of the main menubar"), G_CALLBACK (mousepad_window_action_menubar), FALSE, },
  { "toolbar", NULL, N_("_Toolbar"), NULL, N_("Change the visibility of the toolbar"), G_CALLBACK (mousepad_window_action_toolbar), FALSE, },
  { "statusbar", NULL, N_("St_atusbar"), NULL, N_("Change the visibility of the statusbar"), G_CALLBACK (mousepad_window_action_statusbar), FALSE, },
  { "fullscreen", GTK_STOCK_FULLSCREEN, N_("_Fullscreen"), "F11", N_("Make the window fullscreen"), G_CALLBACK (mousepad_window_action_fullscreen), FALSE, },
  { "auto-indent", NULL, N_("_Auto Indent"), NULL, N_("Auto indent a new line"), G_CALLBACK (mousepad_window_action_auto_indent), FALSE, },
  { "insert-spaces", NULL, N_("Insert _Spaces"), NULL, N_("Insert spaces when the tab button is pressed"), G_CALLBACK (mousepad_window_action_insert_spaces), FALSE, },
  { "word-wrap", NULL, N_("_Word Wrap"), NULL, N_("Toggle breaking lines in between words"), G_CALLBACK (mousepad_window_action_word_wrap), FALSE, },
  { "write-bom", NULL, N_("Write Unicode _BOM"), NULL, N_("Store the byte-order mark in the file"), G_CALLBACK (mousepad_window_action_write_bom), FALSE, }
};

static const GtkRadioActionEntry radio_action_entries[] =
{
  { "unix", NULL, N_("Unix (_LF)"), NULL, N_("Set the line ending of the document to Unix (LF)"), MOUSEPAD_EOL_UNIX, },
  { "mac", NULL, N_("Mac (_CR)"), NULL, N_("Set the line ending of the document to Mac (CR)"), MOUSEPAD_EOL_MAC, },
  { "dos", NULL, N_("DOS / Windows (C_R LF)"), NULL, N_("Set the line ending of the document to DOS / Windows (CR LF)"), MOUSEPAD_EOL_DOS, }
};



/* global variables */
static guint   window_signals[LAST_SIGNAL];
static gint    lock_menu_updates = 0;
static GSList *clipboard_history = NULL;
static guint   clipboard_history_ref_count = 0;



G_DEFINE_TYPE (MousepadWindow, mousepad_window, GTK_TYPE_WINDOW)



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



/* Called when 'window-recent-menu-items' setting changes to update the UI. */
static void
mousepad_window_update_recent_menu (MousepadWindow *window,
                                    gchar          *key,
                                    GSettings      *settings)
{
  mousepad_window_recent_menu (window);
}



static void
mousepad_window_update_toolbar (MousepadWindow *window,
                                gchar          *key,
                                GSettings      *settings)
{
  gboolean visible;
  GtkIconSize size;
  GtkToolbarStyle style;

  visible = MOUSEPAD_SETTING_GET_BOOLEAN (TOOLBAR_VISIBLE);
  size = MOUSEPAD_SETTING_GET_ENUM (TOOLBAR_ICON_SIZE);
  style = MOUSEPAD_SETTING_GET_ENUM (TOOLBAR_STYLE);

  gtk_widget_set_visible (window->toolbar, visible);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (window->toolbar), size);
  gtk_toolbar_set_style (GTK_TOOLBAR (window->toolbar), style);
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
mousepad_window_action_group_language_changed (MousepadWindow      *window,
                                               GParamSpec          *pspec,
                                               MousepadActionGroup *group)
{
  GtkSourceLanguage *language;

  /* get the new active language */
  language = mousepad_action_group_get_active_language (group);

  /* update the language on the active file */
  mousepad_file_set_language (window->active->file, language);

  /* update the filetype shown in the statusbar */
  mousepad_statusbar_set_language (MOUSEPAD_STATUSBAR (window->statusbar), language);
}



static void
mousepad_window_create_languages_menu (MousepadWindow *window)
{
  GtkWidget           *menu, *item;
  MousepadActionGroup *group;
  static const gchar  *menu_path = "/main-menu/document-menu/language-menu";

  /* create the languages menu and add it to the placeholder */
  group = MOUSEPAD_ACTION_GROUP (window->action_group);
  menu = mousepad_action_group_create_language_menu (group);
  item = gtk_ui_manager_get_widget (window->ui_manager, menu_path);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
  gtk_widget_show_all (menu);
  gtk_widget_show (item);

  /* watch for activations of the language actions */
  g_signal_connect_object (window->action_group,
                           "notify::active-language",
                           G_CALLBACK (mousepad_window_action_group_language_changed),
                           window,
                           G_CONNECT_SWAPPED);
}



static void
mousepad_window_create_style_schemes_menu (MousepadWindow *window)
{
  GtkWidget           *menu, *item;
  MousepadActionGroup *group;
  static const gchar  *menu_path = "/main-menu/view-menu/color-scheme-menu";
  
  /* create the color schemes menu and add it to the placeholder */
  group = MOUSEPAD_ACTION_GROUP (window->action_group);
  menu = mousepad_action_group_create_style_scheme_menu (group);
  item = gtk_ui_manager_get_widget (window->ui_manager, menu_path);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
  gtk_widget_show_all (menu);
  gtk_widget_show (item);
}



static void
mousepad_window_create_menubar (MousepadWindow *window)
{
  GtkAction *action;
  gboolean   active;

  window->menubar = gtk_ui_manager_get_widget (window->ui_manager, "/main-menu");
  gtk_box_pack_start (GTK_BOX (window->box), window->menubar, FALSE, FALSE, 0);

  /* add the filetypes menu */
  mousepad_window_create_languages_menu (window);

  /* add the colour schemes menu */
  mousepad_window_create_style_schemes_menu (window);

  /* sync the menubar visibility and action state to the setting */
  action = gtk_action_group_get_action (window->action_group, "menubar");
  if (MOUSEPAD_SETTING_GET_BOOLEAN (WINDOW_FULLSCREEN))
    {
      gint value = MOUSEPAD_SETTING_GET_BOOLEAN (MENUBAR_VISIBLE_FULLSCREEN);
      active = (value == 0) ? MOUSEPAD_SETTING_GET_BOOLEAN (MENUBAR_VISIBLE) : (value == 2);
    }
  else
    active = MOUSEPAD_SETTING_GET_BOOLEAN (MENUBAR_VISIBLE);
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);
  gtk_widget_set_visible (window->menubar, active);

  MOUSEPAD_SETTING_CONNECT_OBJECT (MENUBAR_VISIBLE,
                                  G_CALLBACK (mousepad_window_update_main_widgets),
                                  window,
                                  G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (MENUBAR_VISIBLE_FULLSCREEN,
                                  G_CALLBACK (mousepad_window_update_main_widgets),
                                  window,
                                  G_CONNECT_SWAPPED);
}



static void
mousepad_window_create_toolbar (MousepadWindow *window)
{
  GtkWidget *item;
  GtkAction *action;
  gboolean   active;

  window->toolbar = gtk_ui_manager_get_widget (window->ui_manager, "/main-toolbar");
  gtk_box_pack_start (GTK_BOX (window->box), window->toolbar, FALSE, FALSE, 0);

  /* make the last toolbar separator so it expands and is invisible */
  item = gtk_ui_manager_get_widget (window->ui_manager, "/main-toolbar/spacer");
  gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (item), FALSE);
  gtk_tool_item_set_expand (GTK_TOOL_ITEM (item), TRUE);

  /* sync the toolbar visibility and action state to the setting */
  action = gtk_action_group_get_action (window->action_group, "toolbar");
  if (MOUSEPAD_SETTING_GET_BOOLEAN (WINDOW_FULLSCREEN))
    {
      gint value = MOUSEPAD_SETTING_GET_BOOLEAN (TOOLBAR_VISIBLE_FULLSCREEN);
      active = (value == 0) ? MOUSEPAD_SETTING_GET_BOOLEAN (TOOLBAR_VISIBLE) : (value == 2);
    }
  else
    active = MOUSEPAD_SETTING_GET_BOOLEAN (TOOLBAR_VISIBLE);
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);
  gtk_widget_set_visible (window->toolbar, active);

  /* update the toolbar with the settings */
  mousepad_window_update_toolbar (window, NULL, NULL);

  /* connect to some signals to keep in sync */
  MOUSEPAD_SETTING_CONNECT_OBJECT (TOOLBAR_VISIBLE,
                                   G_CALLBACK (mousepad_window_update_toolbar),
                                   window,
                                   G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (TOOLBAR_VISIBLE_FULLSCREEN,
                                   G_CALLBACK (mousepad_window_update_main_widgets),
                                   window,
                                   G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (TOOLBAR_STYLE,
                                   G_CALLBACK (mousepad_window_update_toolbar),
                                   window,
                                   G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (TOOLBAR_ICON_SIZE,
                                   G_CALLBACK (mousepad_window_update_toolbar),
                                   window,
                                   G_CONNECT_SWAPPED);
}



static void
mousepad_window_create_root_warning (MousepadWindow *window)
{
  /* check if we need to add the root warning */
  if (G_UNLIKELY (geteuid () == 0))
    {
      GtkWidget *ebox, *label, *separator;

  /* In GTK3, gtkrc is deprecated */
#if GTK_CHECK_VERSION(3, 0, 0) && (__GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__ > 2)
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

#if GTK_CHECK_VERSION(3, 0, 0) && (__GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__ > 2)
# pragma GCC diagnostic pop
#endif

      /* add the box for the root warning */
      ebox = gtk_event_box_new ();
      gtk_widget_set_name (ebox, "root-warning");
      gtk_box_pack_start (GTK_BOX (window->box), ebox, FALSE, FALSE, 0);
      gtk_widget_show (ebox);

      /* add the label with the root warning */
      label = gtk_label_new (_("Warning, you are using the root account, you may harm your system."));
      gtk_misc_set_padding (GTK_MISC (label), 6, 3);
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
#if ! GTK_CHECK_VERSION(3, 0, 0)
                                   "tab-hborder", 0,
                                   "tab-vborder", 0,
#endif
                                   NULL);

  /* set the group id */
  gtk_notebook_set_group_name (GTK_NOTEBOOK (window->notebook), NOTEBOOK_GROUP);

  /* connect signals to the notebooks */
  g_signal_connect (G_OBJECT (window->notebook), "switch-page", G_CALLBACK (mousepad_window_notebook_switch_page), window);
  g_signal_connect (G_OBJECT (window->notebook), "page-reordered", G_CALLBACK (mousepad_window_notebook_reordered), window);
  g_signal_connect (G_OBJECT (window->notebook), "page-added", G_CALLBACK (mousepad_window_notebook_added), window);
  g_signal_connect (G_OBJECT (window->notebook), "page-removed", G_CALLBACK (mousepad_window_notebook_removed), window);
  g_signal_connect (G_OBJECT (window->notebook), "button-press-event", G_CALLBACK (mousepad_window_notebook_button_press_event), window);
  g_signal_connect (G_OBJECT (window->notebook), "button-release-event", G_CALLBACK (mousepad_window_notebook_button_release_event), window);
  g_signal_connect (G_OBJECT (window->notebook), "create-window", G_CALLBACK (mousepad_window_notebook_create_window), window);

  /* append and show the notebook */
  gtk_box_pack_start (GTK_BOX (window->box), window->notebook, TRUE, TRUE, PADDING);
  gtk_widget_show (window->notebook);
}



static void
mousepad_window_create_statusbar (MousepadWindow *window)
{
  GtkAction *action;
  gboolean   active;

  /* setup a new statusbar */
  window->statusbar = mousepad_statusbar_new ();

  /* sync the statusbar visibility and action state to the setting */
  action = gtk_action_group_get_action (window->action_group, "statusbar");
  if (MOUSEPAD_SETTING_GET_BOOLEAN (WINDOW_FULLSCREEN))
    {
      gint value = MOUSEPAD_SETTING_GET_BOOLEAN (STATUSBAR_VISIBLE_FULLSCREEN);
      active = (value == 0) ? MOUSEPAD_SETTING_GET_BOOLEAN (STATUSBAR_VISIBLE) : (value == 2);
    }
  else
    active = MOUSEPAD_SETTING_GET_BOOLEAN (STATUSBAR_VISIBLE);
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);
  gtk_widget_set_visible (window->statusbar, active);

  /* pack the statusbar into the window UI */
  gtk_box_pack_end (GTK_BOX (window->box), window->statusbar, FALSE, FALSE, 0);

  /* overwrite toggle signal */
  g_signal_connect_swapped (G_OBJECT (window->statusbar), "enable-overwrite",
                            G_CALLBACK (mousepad_window_action_statusbar_overwrite), window);

  /* populate filetype popup menu signal */
  g_signal_connect_swapped (G_OBJECT (window->statusbar), "provide-languages-menu",
                            G_CALLBACK (mousepad_window_provide_languages_menu), window);

  /* update the statusbar items */
  if (MOUSEPAD_IS_DOCUMENT (window->active))
    mousepad_document_send_signals (window->active);

  /* update the statusbar with certain settings */
  MOUSEPAD_SETTING_CONNECT_OBJECT (TAB_WIDTH,
                                   G_CALLBACK (mousepad_window_update_statusbar_settings),
                                   window,
                                   G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (INSERT_SPACES,
                                   G_CALLBACK (mousepad_window_update_statusbar_settings),
                                   window,
                                   G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (STATUSBAR_VISIBLE,
                                   G_CALLBACK (mousepad_window_update_main_widgets),
                                   window,
                                   G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (STATUSBAR_VISIBLE_FULLSCREEN,
                                   G_CALLBACK (mousepad_window_update_main_widgets),
                                   window,
                                   G_CONNECT_SWAPPED);
}



static void
mousepad_window_user_set_language (MousepadWindow      *window,
                                   GtkSourceLanguage   *language,
                                   MousepadActionGroup *group)
{
  /* mark the file as having its language chosen explicitly by the user
   * so we don't clobber their choice by guessing ourselves */
  mousepad_file_set_user_set_language (window->active->file, TRUE);
}



static void
mousepad_window_init (MousepadWindow *window)
{
  GtkAccelGroup *accel_group;
  GtkWidget     *item;

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

#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_window_set_has_resize_grip (GTK_WINDOW (window), TRUE);
#endif

  /* increase clipboard history ref count */
  clipboard_history_ref_count++;

  /* signal for handling the window delete event */
  g_signal_connect (G_OBJECT (window), "delete-event", G_CALLBACK (mousepad_window_delete_event), NULL);

  /* restore window settings */
  mousepad_window_restore (window);

  /* the action group for this window */
  window->action_group = mousepad_action_group_new ();
  gtk_action_group_set_translation_domain (window->action_group, GETTEXT_PACKAGE);
  gtk_action_group_add_actions (window->action_group, action_entries, G_N_ELEMENTS (action_entries), GTK_WIDGET (window));
  gtk_action_group_add_toggle_actions (window->action_group, toggle_action_entries, G_N_ELEMENTS (toggle_action_entries), GTK_WIDGET (window));
  gtk_action_group_add_radio_actions (window->action_group, radio_action_entries, G_N_ELEMENTS (radio_action_entries), -1, G_CALLBACK (mousepad_window_action_line_ending), GTK_WIDGET (window));
  g_signal_connect_object (window->action_group, "user-set-language", G_CALLBACK (mousepad_window_user_set_language), window, G_CONNECT_SWAPPED);

  /* create the ui manager and connect proxy signals for the statusbar */
  window->ui_manager = gtk_ui_manager_new ();
  g_signal_connect (G_OBJECT (window->ui_manager), "connect-proxy", G_CALLBACK (mousepad_window_connect_proxy), window);
  g_signal_connect (G_OBJECT (window->ui_manager), "disconnect-proxy", G_CALLBACK (mousepad_window_disconnect_proxy), window);
  gtk_ui_manager_insert_action_group (window->ui_manager, window->action_group, 0);
  gtk_ui_manager_add_ui_from_string (window->ui_manager, mousepad_window_ui, mousepad_window_ui_length, NULL);

  /* build the templates menu when the item is shown for the first time */
  /* from here we also trigger the idle build of the recent menu */
  item = gtk_ui_manager_get_widget (window->ui_manager, "/main-menu/file-menu/template-menu");
  g_signal_connect (G_OBJECT (item), "map", G_CALLBACK (mousepad_window_menu_templates), window);

  /* add tab size menu */
  mousepad_window_menu_tab_sizes (window);

  /* set accel group for the window */
  accel_group = gtk_ui_manager_get_accel_group (window->ui_manager);
  gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

  /* create the main table */
  window->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), window->box);
  gtk_widget_show (window->box);

  /* create the main menu from the ui manager */
  mousepad_window_create_menubar (window);

  /* create the toolbar from the ui manager */
  mousepad_window_create_toolbar (window);

  /* create the root-warning bar (if needed) */
  mousepad_window_create_root_warning (window);

  /* create the notebook */
  mousepad_window_create_notebook (window);

  /* create the statusbar */
  mousepad_window_create_statusbar (window);

  /* allow drops in the window */
  gtk_drag_dest_set (GTK_WIDGET (window), GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP, drop_targets, G_N_ELEMENTS (drop_targets), GDK_ACTION_COPY | GDK_ACTION_MOVE);
  g_signal_connect (G_OBJECT (window), "drag-data-received", G_CALLBACK (mousepad_window_drag_data_received), window);

  /* Add menubar action to the textview menu when the menubar is hidden */
  item = gtk_ui_manager_get_widget (window->ui_manager, "/textview-menu/menubar-visible-separator");
  g_object_bind_property (window->menubar, "visible",
                          item,            "visible",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  item = gtk_ui_manager_get_widget (window->ui_manager, "/textview-menu/menubar-visible-item");
  g_object_bind_property (window->menubar, "visible",
                          item,            "visible",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  /* update the window title when 'path-in-title' setting changes */
  MOUSEPAD_SETTING_CONNECT_OBJECT (PATH_IN_TITLE,
                                   G_CALLBACK (mousepad_window_update_window_title),
                                   window,
                                   G_CONNECT_SWAPPED);

  /* update the tabs when 'always-show-tabs' setting changes */
  MOUSEPAD_SETTING_CONNECT_OBJECT (ALWAYS_SHOW_TABS,
                                   G_CALLBACK (mousepad_window_update_tabs),
                                   window,
                                   G_CONNECT_SWAPPED);

  /* update the recent items menu when 'window-recent-menu-items' setting changes */
  MOUSEPAD_SETTING_CONNECT_OBJECT (RECENT_MENU_ITEMS,
                                   G_CALLBACK (mousepad_window_update_recent_menu),
                                   window,
                                   G_CONNECT_SWAPPED);
}



static void
mousepad_window_dispose (GObject *object)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (object);

  /* disconnect recent manager signal */
  if (G_LIKELY (window->recent_manager))
    mousepad_disconnect_by_func (G_OBJECT (window->recent_manager), mousepad_window_recent_menu, window);

  /* destroy the save geometry timer source */
  if (G_UNLIKELY (window->save_geometry_timer_id != 0))
    g_source_remove (window->save_geometry_timer_id);

  (*G_OBJECT_CLASS (mousepad_window_parent_class)->dispose) (object);
}



static void
mousepad_window_finalize (GObject *object)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (object);

  /* decrease history clipboard ref count */
  clipboard_history_ref_count--;

  /* cancel a scheduled recent menu update */
  if (G_UNLIKELY (window->update_recent_menu_id != 0))
    g_source_remove (window->update_recent_menu_id);

  /* cancel a scheduled go menu update */
  if (G_UNLIKELY (window->update_go_menu_id != 0))
    g_source_remove (window->update_go_menu_id);

  /* release the ui manager */
  g_signal_handlers_disconnect_matched (G_OBJECT (window->ui_manager), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, window);
  g_object_unref (G_OBJECT (window->ui_manager));

  /* release the action groups */
  g_object_unref (G_OBJECT (window->action_group));

  /* free clipboard history if needed */
  if (clipboard_history_ref_count == 0 && clipboard_history != NULL)
    {
      g_slist_foreach (clipboard_history, (GFunc) g_free, NULL);
      g_slist_free (clipboard_history);
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
  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (GTK_IS_MENU_ITEM (proxy) || GTK_IS_TOOL_ITEM (proxy));
  g_return_if_fail (GTK_IS_UI_MANAGER (manager));

  if (GTK_IS_MENU_ITEM (proxy))
    {
      /* menu item's don't work with gdk window events */
      g_signal_connect_object (proxy, "select", G_CALLBACK (mousepad_window_menu_item_selected), window, 0);
      g_signal_connect_object (proxy, "deselect", G_CALLBACK (mousepad_window_menu_item_deselected), window, 0);
    }
  else if (GTK_IS_TOOL_ITEM (proxy))
    {
      GtkWidget *child;

      /* tool items will have GtkButton or other widgets in them, we want the child */
      child = gtk_bin_get_child (GTK_BIN (proxy));

      /* get events for mouse enter/leave and focus in/out */
      gtk_widget_add_events (child, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_FOCUS_CHANGE_MASK);

      /* connect to signals for the events */
      g_signal_connect_object (child, "enter-notify-event", G_CALLBACK (mousepad_window_tool_item_enter_event), window, 0);
      g_signal_connect_object (child, "leave-notify-event", G_CALLBACK (mousepad_window_tool_item_leave_event), window, 0);
      g_signal_connect_object (child, "focus-in-event", G_CALLBACK (mousepad_window_tool_item_enter_event), window, 0);
      g_signal_connect_object (child, "focus-out-event", G_CALLBACK (mousepad_window_tool_item_leave_event), window, 0);
    }
}



static void
mousepad_window_disconnect_proxy (GtkUIManager   *manager,
                                  GtkAction      *action,
                                  GtkWidget      *proxy,
                                  MousepadWindow *window)
{
  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (GTK_IS_MENU_ITEM (proxy) || GTK_IS_TOOL_ITEM (proxy));
  g_return_if_fail (GTK_IS_UI_MANAGER (manager));

  if (GTK_IS_MENU_ITEM (proxy))
    {
      /* disconnect from the select/deselect signals */
      mousepad_disconnect_by_func (proxy, mousepad_window_menu_item_selected, window);
      mousepad_disconnect_by_func (proxy, mousepad_window_menu_item_deselected, window);
    }
  else if (GTK_IS_TOOL_ITEM (proxy))
    {
      GtkWidget *child;

      /* tool items will have GtkButton or other widgets in them, we want the child */
      child = gtk_bin_get_child (GTK_BIN (proxy));

      /* disconnect from the gdk window event signals */
      mousepad_disconnect_by_func (child, mousepad_window_tool_item_enter_event, window);
      mousepad_disconnect_by_func (child, mousepad_window_tool_item_leave_event, window);
      mousepad_disconnect_by_func (child, mousepad_window_tool_item_enter_event, window);
      mousepad_disconnect_by_func (child, mousepad_window_tool_item_leave_event, window);
    }
}



static void
mousepad_window_menu_item_selected (GtkWidget      *menu_item,
                                    MousepadWindow *window)
{
  mousepad_statusbar_push_tooltip (MOUSEPAD_STATUSBAR (window->statusbar), menu_item);
}



static void
mousepad_window_menu_item_deselected (GtkWidget      *menu_item,
                                      MousepadWindow *window)
{
  mousepad_statusbar_pop_tooltip (MOUSEPAD_STATUSBAR (window->statusbar), menu_item);
}



static gboolean
mousepad_window_tool_item_enter_event (GtkWidget      *tool_item,
                                       GdkEvent       *event,
                                       MousepadWindow *window)
{
  mousepad_statusbar_push_tooltip (MOUSEPAD_STATUSBAR (window->statusbar), tool_item);

  return FALSE;
}



static gboolean
mousepad_window_tool_item_leave_event (GtkWidget      *tool_item,
                                       GdkEvent       *event,
                                       MousepadWindow *window)
{
  mousepad_statusbar_pop_tooltip (MOUSEPAD_STATUSBAR (window->statusbar), tool_item);

  return FALSE;
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
      if (gtk_widget_get_visible (GTK_WIDGET(window)))
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

            /* build uri */
            uri = g_filename_to_uri (filename, NULL, NULL);
            if (G_LIKELY (uri))
              {
                /* try to lookup the recent item */
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
          filename = g_build_filename (working_directory, filenames[n], NULL);
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

  /* allow tab reordering and detaching */
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
  gboolean succeed = FALSE;
  gint     response;
  gboolean readonly;

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
            succeed = mousepad_window_action_save (NULL, window);
            break;

          case MOUSEPAD_RESPONSE_SAVE_AS:
            succeed = mousepad_window_action_save_as (NULL, window);
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



/* give the statusbar a languages menu created from our action group */
static GtkWidget *
mousepad_window_provide_languages_menu (MousepadWindow    *window,
                                        MousepadStatusbar *statusbar)
{
  MousepadActionGroup *group;
  
  group = MOUSEPAD_ACTION_GROUP (window->action_group);
  return mousepad_action_group_create_language_menu (group);
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
mousepad_window_notebook_reordered (GtkNotebook     *notebook,
                                    GtkWidget       *page,
                                    guint            page_num,
                                    MousepadWindow  *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (page));

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

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* connect signals to the document for this window */
  g_signal_connect (G_OBJECT (page), "close-tab", G_CALLBACK (mousepad_window_button_close_tab), window);
  g_signal_connect (G_OBJECT (page), "cursor-changed", G_CALLBACK (mousepad_window_cursor_changed), window);
  g_signal_connect (G_OBJECT (page), "selection-changed", G_CALLBACK (mousepad_window_selection_changed), window);
  g_signal_connect (G_OBJECT (page), "overwrite-changed", G_CALLBACK (mousepad_window_overwrite_changed), window);
  g_signal_connect (G_OBJECT (page), "language-changed", G_CALLBACK (mousepad_window_buffer_language_changed), window);
  g_signal_connect (G_OBJECT (page), "drag-data-received", G_CALLBACK (mousepad_window_drag_data_received), window);
  g_signal_connect_swapped (G_OBJECT (document->buffer), "notify::can-undo", G_CALLBACK (mousepad_window_can_undo), window);
  g_signal_connect_swapped (G_OBJECT (document->buffer), "notify::can-redo", G_CALLBACK (mousepad_window_can_redo), window);
  g_signal_connect_swapped (G_OBJECT (document->buffer), "modified-changed", G_CALLBACK (mousepad_window_modified_changed), window);
  g_signal_connect (G_OBJECT (document->textview), "populate-popup", G_CALLBACK (mousepad_window_menu_textview_popup), window);

  /* change the visibility of the tabs accordingly */
  mousepad_window_update_tabs (window, NULL, NULL);

  /* update the go menu */
  mousepad_window_update_gomenu (window);
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
  mousepad_disconnect_by_func (G_OBJECT (document->buffer), mousepad_window_modified_changed, window);
  mousepad_disconnect_by_func (G_OBJECT (document->textview), mousepad_window_menu_textview_popup, window);

  /* unset the go menu item (part of the old window) */
  mousepad_object_set_data (G_OBJECT (page), "document-menu-action", NULL);

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

      /* update the go menu */
      mousepad_window_update_gomenu (window);

      /* update action entries */
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
  GtkWidget    *widget = GTK_WIDGET (user_data);
  GtkAllocation alloc = { 0, 0, 0, 0 };

  gdk_window_get_origin (gtk_widget_get_window (widget), x, y);
  gtk_widget_get_allocation (widget, &alloc);

  *x += alloc.x;
  *y += alloc.y + alloc.height;

  *push_in = TRUE;
}



/* stolen from Geany notebook.c */
static gboolean
mousepad_window_is_position_on_tab_bar(GtkNotebook *notebook, GdkEventButton *event)
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
  GtkWidget *page, *label;
  GtkWidget *menu;
  guint      page_num = 0;
  gint       x_root;

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
                  /* get the menu */
                  menu = gtk_ui_manager_get_widget (window->ui_manager, "/tab-menu");

                  /* show it */
                  gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                                  mousepad_window_notebook_menu_position, label,
                                  event->button, event->time);
                }
              else if (event->button == 2)
                {
                  /* close the document */
                  mousepad_window_action_close (NULL, window);
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
          mousepad_window_action_new (NULL, window);

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
  GtkAction   *action;
  guint        i;
  const gchar *action_names1[] = { "tabs-to-spaces", "spaces-to-tabs", "duplicate", "strip-trailing" };
  const gchar *action_names2[] = { "line-up", "line-down" };
  const gchar *action_names3[] = { "cut", "copy", "delete", "lowercase", "uppercase", "titlecase", "opposite-case" };

  /* sensitivity of the change selection action */
  action = gtk_action_group_get_action (window->action_group, "change-selection");
  gtk_action_set_sensitive (action, selection != 0);

  /* actions that are unsensitive during a column selection */
  for (i = 0; i < G_N_ELEMENTS (action_names1); i++)
    {
      action = gtk_action_group_get_action (window->action_group, action_names1[i]);
      gtk_action_set_sensitive (action, selection == 0 || selection == 1);
    }

  /* action that are only sensitive for normal selections */
  for (i = 0; i < G_N_ELEMENTS (action_names2); i++)
    {
      action = gtk_action_group_get_action (window->action_group, action_names2[i]);
      gtk_action_set_sensitive (action, selection == 1);
    }

  /* actions that are sensitive for all selections with content */
  for (i = 0; i < G_N_ELEMENTS (action_names3); i++)
    {
      action = gtk_action_group_get_action (window->action_group, action_names3[i]);
      gtk_action_set_sensitive (action, selection > 0);
    }
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
  MousepadActionGroup *group;

  /* activate the action for the new buffer language */
  group = MOUSEPAD_ACTION_GROUP (window->action_group);
  mousepad_action_group_set_active_language (group, language);
}



static void
mousepad_window_can_undo (MousepadWindow *window,
                          GParamSpec     *unused,
                          GObject        *buffer)
{
  GtkAction *action;
  gboolean   can_undo;

  can_undo = gtk_source_buffer_can_undo (GTK_SOURCE_BUFFER (buffer));

  action = gtk_action_group_get_action (window->action_group, "undo");
  gtk_action_set_sensitive (action, can_undo);
}



static void
mousepad_window_can_redo (MousepadWindow *window,
                          GParamSpec     *unused,
                          GObject        *buffer)
{
  GtkAction *action;
  gboolean   can_redo;

  can_redo = gtk_source_buffer_can_redo (GTK_SOURCE_BUFFER (buffer));

  action = gtk_action_group_get_action (window->action_group, "redo");
  gtk_action_set_sensitive (action, can_redo);
}



/**
 * Menu Functions
 **/
static void
mousepad_window_menu_templates_fill (MousepadWindow *window,
                                     GtkWidget      *menu,
                                     const gchar    *path)
{
  GDir        *dir;
  GSList      *files_list = NULL;
  GSList      *dirs_list = NULL;
  GSList      *li;
  gchar       *absolute_path;
  gchar       *label, *dot;
  const gchar *name;
  gboolean     files_added = FALSE;
  GtkWidget   *item, *image, *submenu;

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
      /* create a newsub menu for the directory */
      submenu = gtk_menu_new ();
      g_object_ref_sink (G_OBJECT (submenu));
      gtk_menu_set_screen (GTK_MENU (submenu), gtk_widget_get_screen (menu));

      /* fill the menu */
      mousepad_window_menu_templates_fill (window, submenu, li->data);

      /* check if the sub menu contains items */
      if (mousepad_util_container_has_children (GTK_CONTAINER (submenu)))
        {
          /* create directory label */
          label = g_filename_display_basename (li->data);

          /* append the menu */
          item = gtk_image_menu_item_new_with_label (label);
          gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_show (item);

          /* cleanup */
          g_free (label);

          /* set menu image */
          image = gtk_image_new_from_icon_name (GTK_STOCK_DIRECTORY, GTK_ICON_SIZE_MENU);
          gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
          gtk_widget_show (image);
        }

      /* cleanup */
      g_free (li->data);
      g_object_unref (G_OBJECT (submenu));
    }

  /* append the files */
  for (li = files_list; li != NULL; li = li->next)
    {
      GtkSourceLanguage *language;

      language = gtk_source_language_manager_guess_language (
                    gtk_source_language_manager_get_default (), li->data, NULL);

      /* create directory label */
      label = g_filename_display_basename (li->data);

      /* strip the extension from the label */
      dot = g_utf8_strrchr (label, -1, '.');
      if (dot != NULL)
        *dot = '\0';

      /* create menu item */
      item = gtk_image_menu_item_new_with_label (label);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      mousepad_object_set_data_full (G_OBJECT (item), "filename", li->data, g_free);
      mousepad_object_set_data_full (G_OBJECT (item), "language", g_object_ref (language), g_object_unref);
      g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (mousepad_window_action_new_from_template), window);
      gtk_widget_show (item);

      /* set menu image */
      image = gtk_image_new_from_icon_name (GTK_STOCK_FILE, GTK_ICON_SIZE_MENU);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
      gtk_widget_show (image);

      /* disable the menu item telling the user there's no templates */
      files_added = TRUE;

      /* cleanup */
      g_free (label);
    }

  /* cleanup */
  g_slist_free (dirs_list);
  g_slist_free (files_list);

  if (! files_added)
    {
      gchar *msg;
      
      msg = g_strdup_printf (_("No template files found in\n'%s'"), path);
      item = gtk_menu_item_new_with_label (msg);
      g_free (msg);
      
      gtk_widget_set_sensitive (item, FALSE);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
    }
}



static void
mousepad_window_menu_templates (GtkWidget      *item,
                                MousepadWindow *window)
{
  GtkWidget   *submenu;
  const gchar *homedir;
  gchar       *templates_path;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (GTK_IS_MENU_ITEM (item));

  /* schedule the idle build of the recent menu */
  mousepad_window_recent_menu (window);

  /* get the home directory */
  homedir = g_getenv ("HOME");
  if (G_UNLIKELY (homedir == NULL))
    homedir = g_get_home_dir ();

  /* get the templates path */
  templates_path = g_build_filename (homedir, "Templates", NULL);

  /* check if the directory exists */
  if (g_file_test (templates_path, G_FILE_TEST_IS_DIR))
    {
      /* create submenu */
      submenu = gtk_menu_new ();
      g_object_ref_sink (G_OBJECT (submenu));
      gtk_menu_set_screen (GTK_MENU (submenu), gtk_widget_get_screen (item));

      /* fill the menu */
      mousepad_window_menu_templates_fill (window, submenu, templates_path);

      /* set the submenu */
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);

      /* release */
      g_object_unref (G_OBJECT (submenu));
    }
  else
    {
      /* hide the templates menu item */
      gtk_widget_hide (item);
    }

  /* cleanup */
  g_free (templates_path);
}



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
  tmp = MOUSEPAD_SETTING_GET_STRING (DEFAULT_TAB_SIZES);

  /* get sizes array and free the temp string */
  tab_sizes = g_strsplit (tmp, ",", -1);
  g_free (tmp);

  /* create merge id */
  merge_id = gtk_ui_manager_new_merge_id (window->ui_manager);

  /* add the default sizes to the menu */
  for (i = 0; tab_sizes[i] != NULL; i++)
    {
      /* convert the string to a number */
      size = CLAMP (atoi (tab_sizes[i]), 1, 32);

      /* create action name */
      name = g_strdup_printf ("tab-size_%d", size);

      action = gtk_radio_action_new (name, name + 8, NULL, NULL, size);
      gtk_radio_action_set_group (action, group);
      group = gtk_radio_action_get_group (action);
      g_signal_connect (G_OBJECT (action), "activate", G_CALLBACK (mousepad_window_action_tab_size), window);
      gtk_action_group_add_action_with_accel (window->action_group, GTK_ACTION (action), "");

      /* release the action */
      g_object_unref (G_OBJECT (action));

      /* add the action to the go menu */
      gtk_ui_manager_add_ui (window->ui_manager, merge_id,
                             "/main-menu/document-menu/tab-size-menu/placeholder-tab-items",
                             name, name, GTK_UI_MANAGER_MENUITEM, FALSE);

      /* cleanup */
      g_free (name);
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
                         "/main-menu/document-menu/tab-size-menu/placeholder-tab-items",
                         "tab-size-other", "tab-size-other", GTK_UI_MANAGER_MENUITEM, FALSE);

  /* unlock */
  lock_menu_updates--;
}



static void
mousepad_window_menu_tab_sizes_update (MousepadWindow *window)
{
  gint       tab_size;
  gchar     *name, *label = NULL;
  GtkAction *action;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* avoid menu actions */
  lock_menu_updates++;

  /* get tab size of active document */
  tab_size = MOUSEPAD_SETTING_GET_INT (TAB_WIDTH);

  /* check if there is a default item with this number */
  name = g_strdup_printf ("tab-size_%d", tab_size);
  action = gtk_action_group_get_action (window->action_group, name);
  g_free (name);

  if (action)
    {
      /* toggle the default action */
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
    }
  else
    {
      /* create suitable label for the other menu */
      label = g_strdup_printf (_("Ot_her (%d)..."), tab_size);
    }

  /* get other action */
  action = gtk_action_group_get_action (window->action_group, "tab-size-other");

  /* toggle other action if needed */
  if (label)
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

  /* set action label */
  g_object_set (G_OBJECT (action), "label", label ? label : _("Ot_her..."), NULL);

  /* cleanup */
  g_free (label);

  /* allow menu actions again */
  lock_menu_updates--;
}



static void
mousepad_window_menu_textview_shown (GtkMenu        *menu,
                                     MousepadWindow *window)
{
  GtkWidget *our_menu;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* disconnect this signal */
  mousepad_disconnect_by_func (menu, mousepad_window_menu_textview_shown, window);

  /* empty the original menu */
  mousepad_util_container_clear (GTK_CONTAINER (menu));

  /* get the ui manager menu and move its children into the other menu */
  our_menu = gtk_ui_manager_get_widget (window->ui_manager, "/textview-menu");
  mousepad_util_container_move_children (GTK_CONTAINER (our_menu), GTK_CONTAINER (menu));
}



static void
mousepad_window_menu_textview_deactivate (GtkWidget      *menu,
                                          MousepadWindow *window)
{
  GtkWidget *our_menu;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* disconnect this signal */
  mousepad_disconnect_by_func (G_OBJECT (menu), mousepad_window_menu_textview_deactivate, window);

  /* copy the menus children back into the ui manager menu */
  our_menu = gtk_ui_manager_get_widget (window->ui_manager, "/textview-menu");
  mousepad_util_container_move_children (GTK_CONTAINER (menu), GTK_CONTAINER (our_menu));
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
  g_signal_connect (G_OBJECT (menu), "show", G_CALLBACK (mousepad_window_menu_textview_shown), window);
  g_signal_connect (G_OBJECT (menu), "deactivate", G_CALLBACK (mousepad_window_menu_textview_deactivate), window);
}



static void
mousepad_window_update_actions (MousepadWindow *window)
{
  GtkAction          *action;
  GtkNotebook        *notebook = GTK_NOTEBOOK (window->notebook);
  MousepadDocument   *document = window->active;
  gboolean            cycle_tabs;
  gint                n_pages;
  gint                page_num;
  gboolean            active, sensitive;
  MousepadLineEnding  line_ending;
  const gchar        *action_name;
  GtkSourceLanguage  *language;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* update the actions for the active document */
  if (G_LIKELY (document))
    {
      MousepadActionGroup *group;

      /* avoid menu actions */
      lock_menu_updates++;

      /* determine the number of pages and the current page number */
      n_pages = gtk_notebook_get_n_pages (notebook);
      page_num = gtk_notebook_page_num (notebook, GTK_WIDGET (document));

      /* whether we cycle tabs */
      cycle_tabs = MOUSEPAD_SETTING_GET_BOOLEAN (CYCLE_TABS);

      /* set the sensitivity of the back and forward buttons in the go menu */
      action = gtk_action_group_get_action (window->action_group, "back");
      gtk_action_set_sensitive (action, (cycle_tabs && n_pages > 1) || (page_num > 0));

      action = gtk_action_group_get_action (window->action_group, "forward");
      gtk_action_set_sensitive (action, (cycle_tabs && n_pages > 1 ) || (page_num < n_pages - 1));

      /* set the reload, detach and save sensitivity */
      action = gtk_action_group_get_action (window->action_group, "save");
      gtk_action_set_sensitive (action, !mousepad_file_get_read_only (document->file));

      action = gtk_action_group_get_action (window->action_group, "detach");
      gtk_action_set_sensitive (action, (n_pages > 1));

      action = gtk_action_group_get_action (window->action_group, "revert");
      gtk_action_set_sensitive (action, mousepad_file_get_filename (document->file) != NULL);

      /* line ending type */
      line_ending = mousepad_file_get_line_ending (document->file);
      if (G_UNLIKELY (line_ending == MOUSEPAD_EOL_MAC))
        action_name = "mac";
      else if (G_UNLIKELY (line_ending == MOUSEPAD_EOL_DOS))
        action_name = "dos";
      else
        action_name = "unix";

      /* set the corrent line ending type */
      action = gtk_action_group_get_action (window->action_group, action_name);
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

      /* write bom */
      action = gtk_action_group_get_action (window->action_group, "write-bom");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), mousepad_file_get_write_bom (document->file, &sensitive));
      gtk_action_set_sensitive (action, sensitive);

      /* toggle the document settings */
      active = MOUSEPAD_SETTING_GET_BOOLEAN (WORD_WRAP);
      action = gtk_action_group_get_action (window->action_group, "word-wrap");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);

      active = MOUSEPAD_SETTING_GET_BOOLEAN (SHOW_LINE_NUMBERS);
      action = gtk_action_group_get_action (window->action_group, "line-numbers");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);

      active = MOUSEPAD_SETTING_GET_BOOLEAN (AUTO_INDENT);
      action = gtk_action_group_get_action (window->action_group, "auto-indent");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);

      /* update the tabs size menu */
      mousepad_window_menu_tab_sizes_update (window);

      active = MOUSEPAD_SETTING_GET_BOOLEAN (INSERT_SPACES);
      action = gtk_action_group_get_action (window->action_group, "insert-spaces");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);

      /* set the sensitivity of the undo and redo actions */
      mousepad_window_can_undo (window, NULL, G_OBJECT (document->buffer));
      mousepad_window_can_redo (window, NULL, G_OBJECT (document->buffer));

      /* active this tab in the go menu */
      action = mousepad_object_get_data (G_OBJECT (document), "document-menu-action");
      if (G_LIKELY (action != NULL))
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

      /* update the currently active language */
      language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (window->active->buffer));
      group = MOUSEPAD_ACTION_GROUP (window->action_group);
      mousepad_action_group_set_active_language (group, language);

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
  gchar             name[32];
  const gchar      *title;
  const gchar      *tooltip;
  gchar             accelerator[7];
  GtkRadioAction   *radio_action;
  GSList           *group = NULL;
  GList            *actions, *iter;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (user_data), FALSE);

  /* get the window */
  window = MOUSEPAD_WINDOW (user_data);

  /* prevent menu updates */
  lock_menu_updates++;

  /* remove the old merge */
  if (window->gomenu_merge_id != 0)
    {
      gtk_ui_manager_remove_ui (window->ui_manager, window->gomenu_merge_id);

      /* drop all the old recent items from the menu */
      actions = gtk_action_group_list_actions (window->action_group);
      for (iter = actions; iter != NULL; iter = g_list_next (iter))
        {
          /* match only actions starting with "mousepad-tab-" */
          if (g_str_has_prefix (gtk_action_get_name (iter->data), "mousepad-tab-"))
            gtk_action_group_remove_action (window->action_group, iter->data);
        }
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
      g_snprintf (name, sizeof (name), "mousepad-tab-%d", n);

      /* get the name and file name */
      title = mousepad_document_get_basename (document);
      tooltip = mousepad_document_get_filename (document);

      /* create the radio action */
      radio_action = gtk_radio_action_new (name, title, tooltip, NULL, n);
      gtk_radio_action_set_group (radio_action, group);
      group = gtk_radio_action_get_group (radio_action);
      g_signal_connect (G_OBJECT (radio_action), "activate", G_CALLBACK (mousepad_window_action_go_to_tab), window->notebook);

      /* connect the action to the document to we can easily active it when the user switched from tab */
      mousepad_object_set_data (G_OBJECT (document), "document-menu-action", radio_action);

      if (G_LIKELY (n < 9))
        {
          /* create an accelerator and add it to the menu */
          g_snprintf (accelerator, sizeof (accelerator), "<Alt>%d", n + 1);
          gtk_action_group_add_action_with_accel (window->action_group, GTK_ACTION (radio_action), accelerator);
        }
      else
        /* add a menu item without accelerator */
        gtk_action_group_add_action (window->action_group, GTK_ACTION (radio_action));

      /* select the active entry */
      if (gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook)) == n)
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (radio_action), TRUE);

      /* release the action */
      g_object_unref (G_OBJECT (radio_action));

      /* add the action to the go menu */
      gtk_ui_manager_add_ui (window->ui_manager, window->gomenu_merge_id,
                             "/main-menu/document-menu/placeholder-file-items",
                             name, name, GTK_UI_MANAGER_MENUITEM, FALSE);
    }

  /* release our lock */
  lock_menu_updates--;

  return FALSE;
}



static void
mousepad_window_update_gomenu_idle_destroy (gpointer user_data)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (user_data));

  MOUSEPAD_WINDOW (user_data)->update_go_menu_id = 0;
}



static void
mousepad_window_update_gomenu (MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

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
  GList          *items, *li;
  GList          *filtered = NULL;
  GtkRecentInfo  *info;
  const gchar    *uri;
  const gchar    *display_name;
  gchar          *tooltip, *label;
  gchar          *filename, *filename_utf8;
  GtkAction      *action;
  gchar           name[32];
  gint            n, i;

  if (window->recent_merge_id != 0)
    {
      /* unmerge the ui controls from the previous update */
      gtk_ui_manager_remove_ui (window->ui_manager, window->recent_merge_id);

      /* drop all the old recent items from the menu */
      for (i = 1; i < 100 /* arbitrary */; i++)
        {
          g_snprintf (name, sizeof (name), "recent-info-%d", i);
          action = gtk_action_group_get_action (window->action_group, name);
          if (G_UNLIKELY (action == NULL))
            break;
          gtk_action_group_remove_action (window->action_group, action);
        }
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

      /* insert the the list, sorted by date */
      filtered = g_list_insert_sorted (filtered, li->data, (GCompareFunc) mousepad_window_recent_sort);
    }

  /* get the recent menu limit number */
  n = MOUSEPAD_SETTING_GET_INT (RECENT_MENU_ITEMS);

  /* append the items to the menu */
  for (li = filtered, i = 1; n > 0 && li != NULL; li = li->next)
    {
      info = li->data;

      /* get the filename */
      uri = gtk_recent_info_get_uri (info);
      filename = g_filename_from_uri (uri, NULL, NULL);

      /* append to the menu if the file exists, else remove it from the history */
      if (filename && g_file_test (filename, G_FILE_TEST_EXISTS))
        {
          /* create the action name */
          g_snprintf (name, sizeof (name), "recent-info-%d", i);

          /* get the name of the item and escape the underscores */
          display_name = gtk_recent_info_get_display_name (info);
          label = mousepad_util_escape_underscores (display_name);

          /* create and utf-8 valid version of the filename */
          filename_utf8 = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
          tooltip = g_strdup_printf (_("Open '%s'"), filename_utf8);
          g_free (filename_utf8);

          /* create the action */
          action = gtk_action_new (name, label, tooltip, NULL);

          /* cleanup */
          g_free (tooltip);
          g_free (label);

          /* add the info data and connect a menu signal */
          mousepad_object_set_data_full (G_OBJECT (action), "gtk-recent-info", gtk_recent_info_ref (info), gtk_recent_info_unref);
          g_signal_connect (G_OBJECT (action), "activate", G_CALLBACK (mousepad_window_action_open_recent), window);

          /* add the action to the recent actions group */
          gtk_action_group_add_action (window->action_group, action);

          /* release the action */
          g_object_unref (G_OBJECT (action));

          /* add the action to the menu */
          gtk_ui_manager_add_ui (window->ui_manager, window->recent_merge_id,
                                 "/main-menu/file-menu/recent-menu/placeholder-recent-items",
                                 name, name, GTK_UI_MANAGER_MENUITEM, FALSE);

          /* update couters */
          n--;
          i++;
        }
      else
        {
          /* remove the item. don't both the user if this fails */
          gtk_recent_manager_remove_item (window->recent_manager, uri, NULL);
        }

      /* cleanup */
      g_free (filename);
    }

  /* set the visibility of the 'no items found' action */
  action = gtk_action_group_get_action (window->action_group, "no-recent-items");
  gtk_action_set_visible (action, (filtered == NULL));
  gtk_action_set_sensitive (action, FALSE);

  /* set the sensitivity of the clear button */
  action = gtk_action_group_get_action (window->action_group, "clear-recent");
  gtk_action_set_sensitive (action, (filtered != NULL));

  /* cleanup */
  g_list_foreach (items, (GFunc) gtk_recent_info_unref, NULL);
  g_list_free (items);
  g_list_free (filtered);

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
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* leave when we're updating multiple files or there is this an idle function pending */
  if (lock_menu_updates > 0 || window->update_recent_menu_id != 0)
    return;

  /* schedule a recent menu update */
  window->update_recent_menu_id = g_idle_add_full (G_PRIORITY_LOW, mousepad_window_recent_menu_idle,
                                                   window, mousepad_window_recent_menu_idle_destroy);
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
  g_list_foreach (items, (GFunc) gtk_recent_info_unref, NULL);
  g_list_free (items);

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
        mousepad_view_scroll_to_cursor (window->active->textview);
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
  flags = MOUSEPAD_SEARCH_FLAGS_ACTION_HIGHTLIGHT
          | MOUSEPAD_SEARCH_FLAGS_ACTION_CLEANUP;

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
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);

  /* create the mnemonic label */
  label = gtk_label_new_with_mnemonic (mnemonic);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
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
      g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (mousepad_window_paste_history_activate), window);
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
  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);

  /* try to close the window */
  mousepad_window_action_close_window (NULL, window);

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
mousepad_window_action_new (GtkAction      *action,
                            MousepadWindow *window)
{
  MousepadDocument *document;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* create new document */
  document = mousepad_document_new ();

  /* add the document to the window */
  mousepad_window_add (window, document);
}



static void
mousepad_window_action_new_window (GtkAction      *action,
                                   MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* emit the new window signal */
  g_signal_emit (G_OBJECT (window), window_signals[NEW_WINDOW], 0);
}



static void
mousepad_window_action_new_from_template (GtkMenuItem    *item,
                                          MousepadWindow *window)
{
  const gchar       *filename;
  GError            *error = NULL;
  gint               result;
  MousepadDocument  *document;
  const gchar       *message;
  GtkSourceLanguage *language;


  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (GTK_IS_MENU_ITEM (item));

  /* get the filename from the menu item */
  filename = mousepad_object_get_data (G_OBJECT (item), "filename");
  language = mousepad_object_get_data (G_OBJECT (item), "language");

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
                /* destroy the menu item */
                gtk_widget_destroy (GTK_WIDGET (item));

                /* set error message */
                message = _("Reading the template failed, the menu item has been removed");
                break;

              default:
                /* set error message */
                message = _("Loading the template failed");
                break;
            }

          /* show the error */
          mousepad_dialogs_show_error (GTK_WINDOW (window), error, message);
        }
    }
}



static void
mousepad_window_action_open (GtkAction      *action,
                             MousepadWindow *window)
{
  GtkWidget   *chooser;
  GtkWidget    *hbox, *label, *combobox;
  const gchar *filename;
  GSList      *filenames, *li;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* create new file chooser dialog */
  chooser = gtk_file_chooser_dialog_new (_("Open File"),
                                         GTK_WINDOW (window),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                         NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);
  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (chooser), TRUE);

  /* encoding selector */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
  gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (chooser), hbox);
  gtk_widget_show (hbox);

  label = gtk_label_new_with_mnemonic ("_Encoding:");
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
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
  const gchar      *uri, *charset;
  MousepadEncoding  encoding;
  GError           *error = NULL;
  gchar            *filename;
  gboolean          succeed = FALSE;
  GtkRecentInfo    *info;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* get the info */
  info = mousepad_object_get_data (G_OBJECT (action), "gtk-recent-info");

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
mousepad_window_action_clear_recent (GtkAction      *action,
                                     MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

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
mousepad_window_action_save (GtkAction      *action,
                             MousepadWindow *window)
{
  MousepadDocument *document = window->active;
  GError           *error = NULL;
  gboolean          succeed = FALSE;
  gboolean          modified;
  gint              response;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (window->active), FALSE);

  if (mousepad_file_get_filename (document->file) == NULL)
    {
      /* file has no filename yet, open the save as dialog */
      succeed = mousepad_window_action_save_as (NULL, window);
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
            return FALSE;

          case MOUSEPAD_RESPONSE_SAVE_AS:
            /* run save as dialog */
            succeed = mousepad_window_action_save_as (NULL, window);
            break;

          case MOUSEPAD_RESPONSE_SAVE:
            /* save the document */
            succeed = mousepad_file_save (document->file, &error);
            break;
        }

      if (G_LIKELY (succeed))
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

  return succeed;
}



static gboolean
mousepad_window_action_save_as (GtkAction      *action,
                                MousepadWindow *window)
{
  MousepadDocument *document = window->active;
  gchar            *filename;
  const gchar      *current_filename;
  GtkWidget        *dialog;
  gboolean          succeed = FALSE;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (window->active), FALSE);

  /* create the dialog */
  dialog = gtk_file_chooser_dialog_new (_("Save As"),
                                        GTK_WINDOW (window), GTK_FILE_CHOOSER_ACTION_SAVE,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_SAVE, GTK_RESPONSE_OK, NULL);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), TRUE);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  /* set the current filename if there is one */
  current_filename = mousepad_file_get_filename (document->file);
  if (current_filename)
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), current_filename);

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

          /* save the file with the function above */
          succeed = mousepad_window_action_save (NULL, window);

          /* add to the recent history is saving succeeded */
          if (G_LIKELY (succeed))
            mousepad_window_recent_add (window, document->file);
        }
    }

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  return succeed;
}



static void
mousepad_window_action_save_all (GtkAction      *action,
                                 MousepadWindow *window)
{
  gint              i, current;
  gint              page_num;
  MousepadDocument *document;
  GSList           *li, *documents = NULL;
  gboolean          succeed = TRUE;
  GError           *error = NULL;

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
      if (!gtk_text_buffer_get_modified (document->buffer))
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
                  mousepad_window_action_save_as (NULL, window);
                }
              else
                {
                  /* trigger the save function (externally modified document) */
                  mousepad_window_action_save (NULL, window);
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
mousepad_window_action_revert (GtkAction      *action,
                               MousepadWindow *window)
{
  MousepadDocument *document = window->active;
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
          if (!mousepad_window_action_save_as (NULL, window))
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
mousepad_window_action_print (GtkAction      *action,
                              MousepadWindow *window)
{
  MousepadPrint    *print;
  GError           *error = NULL;
  gboolean          succeed;

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
mousepad_window_action_detach (GtkAction      *action,
                               MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* invoke function without cooridinates */
  mousepad_window_notebook_create_window (GTK_NOTEBOOK (window->notebook),
                                          GTK_WIDGET (window->active),
                                          -1, -1, window);
}



static void
mousepad_window_action_close (GtkAction      *action,
                              MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* close active document */
  mousepad_window_close_document (window, window->active);
}



static void
mousepad_window_action_close_window (GtkAction      *action,
                                     MousepadWindow *window)
{
  gint       npages, i;
  GtkWidget *document;

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
      if (!mousepad_window_close_document (window, MOUSEPAD_DOCUMENT (document)))
        {
          /* closing cancelled, release menu lock */
          lock_menu_updates--;

          /* rebuild go menu */
          mousepad_window_update_gomenu (window);

          /* leave function */
          return;
        }
    }

  /* release lock */
  lock_menu_updates--;
}



static void
mousepad_window_action_undo (GtkAction      *action,
                             MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* undo */
  gtk_source_buffer_undo (GTK_SOURCE_BUFFER (window->active->buffer));

  /* scroll to visible area */
  mousepad_view_scroll_to_cursor (window->active->textview);
}



static void
mousepad_window_action_redo (GtkAction      *action,
                             MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* redo */
  gtk_source_buffer_redo (GTK_SOURCE_BUFFER (window->active->buffer));

  /* scroll to visible area */
  mousepad_view_scroll_to_cursor (window->active->textview);
}



static void
mousepad_window_action_cut (GtkAction      *action,
                            MousepadWindow *window)
{
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
mousepad_window_action_copy (GtkAction      *action,
                             MousepadWindow *window)
{
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
mousepad_window_action_paste (GtkAction      *action,
                              MousepadWindow *window)
{
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
mousepad_window_action_paste_history (GtkAction      *action,
                                      MousepadWindow *window)
{
  GtkWidget *menu;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get the history menu */
  menu = mousepad_window_paste_history_menu (window);

  /* select the first item in the menu */
  gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), TRUE);

  /* popup the menu */
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                  mousepad_window_paste_history_menu_position,
                  window, 0, gtk_get_current_event_time ());
}



static void
mousepad_window_action_paste_column (GtkAction      *action,
                                     MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* paste the clipboard into a column */
  mousepad_view_clipboard_paste (window->active->textview, NULL, TRUE);
}



static void
mousepad_window_action_delete (GtkAction      *action,
                               MousepadWindow *window)
{
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
mousepad_window_action_select_all (GtkAction      *action,
                                   MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* select everything in the document */
  mousepad_view_select_all (window->active->textview);
}



static void
mousepad_window_action_change_selection (GtkAction      *action,
                                         MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* change the selection */
  mousepad_view_change_selection (window->active->textview);
}



static void
mousepad_window_action_preferences (GtkAction      *action,
                                    MousepadWindow *window)
{
  mousepad_window_show_preferences (window);
}



static void
mousepad_window_action_lowercase (GtkAction      *action,
                                  MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert selection to lowercase */
  mousepad_view_convert_selection_case (window->active->textview, LOWERCASE);
}



static void
mousepad_window_action_uppercase (GtkAction      *action,
                                  MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert selection to uppercase */
  mousepad_view_convert_selection_case (window->active->textview, UPPERCASE);
}



static void
mousepad_window_action_titlecase (GtkAction      *action,
                                  MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert selection to titlecase */
  mousepad_view_convert_selection_case (window->active->textview, TITLECASE);
}



static void
mousepad_window_action_opposite_case (GtkAction      *action,
                                      MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert selection to opposite case */
  mousepad_view_convert_selection_case (window->active->textview, OPPOSITE_CASE);
}



static void
mousepad_window_action_tabs_to_spaces (GtkAction      *action,
                                       MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert tabs to spaces */
  mousepad_view_convert_spaces_and_tabs (window->active->textview, TABS_TO_SPACES);
}



static void
mousepad_window_action_spaces_to_tabs (GtkAction      *action,
                                       MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert spaces to tabs */
  mousepad_view_convert_spaces_and_tabs (window->active->textview, SPACES_TO_TABS);
}



static void
mousepad_window_action_strip_trailing_spaces (GtkAction      *action,
                                              MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert spaces to tabs */
  mousepad_view_strip_trailing_spaces (window->active->textview);
}



static void
mousepad_window_action_transpose (GtkAction      *action,
                                  MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* transpose */
  mousepad_view_transpose (window->active->textview);
}



static void
mousepad_window_action_move_line_up (GtkAction      *action,
                                     MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* move the selection on line up */
  mousepad_view_move_selection (window->active->textview, MOVE_LINE_UP);
}



static void
mousepad_window_action_move_line_down (GtkAction      *action,
                                       MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* move the selection on line down */
  mousepad_view_move_selection (window->active->textview, MOVE_LINE_DOWN);
}



static void
mousepad_window_action_duplicate (GtkAction      *action,
                                  MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* dupplicate */
  mousepad_view_duplicate (window->active->textview);
}



static void
mousepad_window_action_increase_indent (GtkAction      *action,
                                        MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* increase the indent */
  mousepad_view_indent (window->active->textview, INCREASE_INDENT);
}



static void
mousepad_window_action_decrease_indent (GtkAction      *action,
                                        MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* decrease the indent */
  mousepad_view_indent (window->active->textview, DECREASE_INDENT);
}



static void
mousepad_window_action_find (GtkAction      *action,
                             MousepadWindow *window)
{
  GtkTextIter selection_start;
  GtkTextIter selection_end;
  gchar       *selection;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* create a new search bar is needed */
  if (window->search_bar == NULL)
    {
      /* create a new toolbar and pack it into the box */
      window->search_bar = mousepad_search_bar_new ();
      gtk_box_pack_start (GTK_BOX (window->box), window->search_bar, FALSE, FALSE, PADDING);

      /* connect signals */
      g_signal_connect_swapped (G_OBJECT (window->search_bar), "hide-bar", G_CALLBACK (mousepad_window_hide_search_bar), window);
      g_signal_connect_swapped (G_OBJECT (window->search_bar), "search", G_CALLBACK (mousepad_window_search), window);
    }

  /* set the search entry text if the search bar is hidden*/
  if (gtk_widget_get_visible (window->search_bar) == FALSE)
    {
      if (gtk_text_buffer_get_has_selection (window->active->buffer) == TRUE)
        {
          gtk_text_buffer_get_selection_bounds (window->active->buffer, &selection_start, &selection_end);
          selection = gtk_text_buffer_get_text (window->active->buffer, &selection_start, &selection_end, 0);

          /* selection should be one line */
          if (g_strrstr (selection, "\n") == NULL && g_strrstr (selection, "\r") == NULL)
            mousepad_search_bar_set_text (MOUSEPAD_SEARCH_BAR (window->search_bar), selection);

          g_free (selection);
        }
    }

  /* show the search bar */
  gtk_widget_show (window->search_bar);

  /* focus the search entry */
  mousepad_search_bar_focus (MOUSEPAD_SEARCH_BAR (window->search_bar));
}



static void
mousepad_window_action_find_next (GtkAction      *action,
                                  MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* find the next occurence */
  if (G_LIKELY (window->search_bar != NULL))
    mousepad_search_bar_find_next (MOUSEPAD_SEARCH_BAR (window->search_bar));
}



static void
mousepad_window_action_find_previous (GtkAction      *action,
                                      MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* find the previous occurence */
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
  mousepad_disconnect_by_func (G_OBJECT (window->notebook), mousepad_window_action_replace_switch_page, window);

  /* reset the dialog variable */
  window->replace_dialog = NULL;
}


static void
mousepad_window_action_replace (GtkAction      *action,
                                MousepadWindow *window)
{
  GtkTextIter selection_start;
  GtkTextIter selection_end;
  gchar       *selection;

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

      /* set the search entry text */
      if (gtk_text_buffer_get_has_selection (window->active->buffer) == TRUE)
        {
          gtk_text_buffer_get_selection_bounds (window->active->buffer, &selection_start, &selection_end);
          selection = gtk_text_buffer_get_text (window->active->buffer, &selection_start, &selection_end, 0);

          /* selection should be one line */
          if (g_strrstr(selection, "\n") == NULL && g_strrstr(selection, "\r") == NULL)
            mousepad_replace_dialog_set_text (MOUSEPAD_REPLACE_DIALOG (window->replace_dialog), selection);

          g_free (selection);
        }

      /* connect signals */
      g_signal_connect_swapped (G_OBJECT (window->replace_dialog), "destroy", G_CALLBACK (mousepad_window_action_replace_destroy), window);
      g_signal_connect_swapped (G_OBJECT (window->replace_dialog), "search", G_CALLBACK (mousepad_window_search), window);
      g_signal_connect_swapped (G_OBJECT (window->notebook), "switch-page", G_CALLBACK (mousepad_window_action_replace_switch_page), window);
    }
  else
    {
      /* focus the existing dialog */
      gtk_window_present (GTK_WINDOW (window->replace_dialog));
    }
}



static void
mousepad_window_action_go_to_position (GtkAction      *action,
                                       MousepadWindow *window)
{
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
mousepad_window_action_select_font (GtkAction      *action,
                                    MousepadWindow *window)
{
  GtkWidget        *dialog;
  gchar            *font_name;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  dialog = gtk_font_chooser_dialog_new (_("Choose Mousepad Font"), GTK_WINDOW (window));

  /* set the current font name */
  font_name = MOUSEPAD_SETTING_GET_STRING (FONT_NAME);

  if (G_LIKELY (font_name))
    {
      gtk_font_chooser_dialog_set_font_name (GTK_FONT_CHOOSER_DIALOG (dialog), font_name);
      g_free (font_name);
    }

  /* run the dialog */
  if (G_LIKELY (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK))
    {
      /* get the selected font from the dialog */
      font_name = gtk_font_chooser_dialog_get_font_name (GTK_FONT_CHOOSER_DIALOG (dialog));

      /* store the font in the preferences */
      MOUSEPAD_SETTING_SET_STRING (FONT_NAME, font_name);

      /* cleanup */
      g_free (font_name);
    }

  /* destroy dialog */
  gtk_widget_destroy (dialog);
}



static void
mousepad_window_action_line_numbers (GtkToggleAction *action,
                                     MousepadWindow  *window)
{
  gboolean active;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get the current state */
  active = gtk_toggle_action_get_active (action);

  /* save as the last used line number setting */
  MOUSEPAD_SETTING_SET_BOOLEAN (SHOW_LINE_NUMBERS, active);
}



static void
mousepad_window_action_menubar (GtkToggleAction *action,
                                MousepadWindow  *window)
{
  gboolean active;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  active = gtk_toggle_action_get_active (action);

  if (mousepad_window_get_in_fullscreen (window))
    MOUSEPAD_SETTING_SET_ENUM (MENUBAR_VISIBLE_FULLSCREEN, (active ? 2 : 1));
  else
    MOUSEPAD_SETTING_SET_BOOLEAN (MENUBAR_VISIBLE, active);
}



static void
mousepad_window_action_toolbar (GtkToggleAction *action,
                                MousepadWindow  *window)
{
  gboolean active;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  active = gtk_toggle_action_get_active (action);

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
mousepad_window_action_statusbar (GtkToggleAction *action,
                                  MousepadWindow  *window)
{
  gboolean active;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  active = gtk_toggle_action_get_active (action);

  if (mousepad_window_get_in_fullscreen (window))
    MOUSEPAD_SETTING_SET_ENUM (STATUSBAR_VISIBLE_FULLSCREEN, (active ? 2 : 1));
  else
    MOUSEPAD_SETTING_SET_BOOLEAN (STATUSBAR_VISIBLE, active);
}



static void
mousepad_window_update_main_widgets (MousepadWindow *window)
{
  gboolean       fullscreen, mb_visible, tb_visible, sb_visible;
  gint           mb_visible_fs, tb_visible_fs, sb_visible_fs;

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

  /* update the widgets' visibility */
  gtk_widget_set_visible (window->menubar, fullscreen ? mb_visible_fs : mb_visible);
  gtk_widget_set_visible (window->toolbar, fullscreen ? tb_visible_fs : tb_visible);
  gtk_widget_set_visible (window->statusbar, fullscreen ? sb_visible_fs : sb_visible);
}



static void
mousepad_window_action_fullscreen (GtkToggleAction *action,
                                   MousepadWindow  *window)
{
  gboolean fullscreen;

  if (! gtk_widget_get_visible (GTK_WIDGET (window)))
    return;

  fullscreen = gtk_toggle_action_get_active (action);

  /* entering fullscreen mode */
  if (fullscreen && ! mousepad_window_get_in_fullscreen (window))
    {
      gtk_window_fullscreen (GTK_WINDOW (window));
      gtk_action_set_stock_id (GTK_ACTION (action), GTK_STOCK_LEAVE_FULLSCREEN);
      gtk_action_set_tooltip (GTK_ACTION (action), _("Leave fullscreen mode"));
    }
  /* leaving fullscreen mode */
  else if (mousepad_window_get_in_fullscreen (window))
    {
      gtk_window_unfullscreen (GTK_WINDOW (window));
      gtk_action_set_stock_id (GTK_ACTION (action), GTK_STOCK_FULLSCREEN);
      gtk_action_set_tooltip (GTK_ACTION (action), _("Make the window fullscreen"));
    }

  /* update the widgets based on whether in fullscreen mode or not */
  mousepad_window_update_main_widgets (window);
}



static void
mousepad_window_action_auto_indent (GtkToggleAction *action,
                                    MousepadWindow  *window)
{
  gboolean active;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get the current state */
  active = gtk_toggle_action_get_active (action);

  /* save as the last auto indent mode */
  MOUSEPAD_SETTING_SET_BOOLEAN (AUTO_INDENT, active);
}



static void
mousepad_window_action_line_ending (GtkRadioAction *action,
                                    GtkRadioAction *current,
                                    MousepadWindow *window)
{
  MousepadLineEnding eol;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));
  g_return_if_fail (MOUSEPAD_IS_FILE (window->active->file));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (window->active->buffer));

  /* leave when menu updates are locked */
  if (lock_menu_updates == 0)
    {
      /* get selected line ending */
      eol = gtk_radio_action_get_current_value (current);

      /* set the new line ending on the file */
      mousepad_file_set_line_ending (window->active->file, eol);

      /* make buffer as modified to show the user the change is not saved */
      gtk_text_buffer_set_modified (window->active->buffer, TRUE);
    }
}



static void
mousepad_window_action_tab_size (GtkToggleAction *action,
                                 MousepadWindow  *window)
{
  gboolean tab_size;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* leave when menu updates are locked */
  if (lock_menu_updates == 0 && gtk_toggle_action_get_active (action))
    {
      g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

      /* get the tab size */
      tab_size = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));

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

      /* update menu */
      mousepad_window_menu_tab_sizes_update (window);
    }
}



static void
mousepad_window_action_insert_spaces (GtkToggleAction *action,
                                      MousepadWindow  *window)
{
  gboolean insert_spaces;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* leave when menu updates are locked */
  if (lock_menu_updates == 0)
    {
      /* get the current state */
      insert_spaces = gtk_toggle_action_get_active (action);

      /* save as the last auto indent mode */
      MOUSEPAD_SETTING_SET_BOOLEAN (INSERT_SPACES, insert_spaces);
    }
}



static void
mousepad_window_action_word_wrap (GtkToggleAction *action,
                                  MousepadWindow  *window)
{
  gboolean active;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* leave when menu updates are locked */
  if (lock_menu_updates == 0)
    {
      /* get the current state */
      active = gtk_toggle_action_get_active (action);

      /* store this as the last used wrap mode */
      MOUSEPAD_SETTING_SET_BOOLEAN (WORD_WRAP, active);
    }
}



static void
mousepad_window_action_write_bom (GtkToggleAction *action,
                                  MousepadWindow  *window)
{
  gboolean active;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* leave when menu updates are locked */
  if (lock_menu_updates == 0)
    {
      /* get the current state */
      active = gtk_toggle_action_get_active (action);

      /* set new value */
      mousepad_file_set_write_bom (window->active->file, active);

      /* make buffer as modified to show the user the change is not saved */
      gtk_text_buffer_set_modified (window->active->buffer, TRUE);
    }
}



static void
mousepad_window_action_prev_tab (GtkAction      *action,
                                 MousepadWindow *window)
{
  gint page_num;
  gint n_pages;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* get notebook info */
  page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));
  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

  /* switch to the previous tab or cycle to the last tab */
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), (page_num - 1) % n_pages);
}



static void
mousepad_window_action_next_tab (GtkAction      *action,
                                 MousepadWindow *window)
{
  gint page_num;
  gint n_pages;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* get notebook info */
  page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));
  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

  /* switch to the next tab or cycle to the first tab */
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), (page_num + 1) % n_pages);
}



static void
mousepad_window_action_go_to_tab (GtkRadioAction *action,
                                  GtkNotebook    *notebook)
{
  gint page;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_RADIO_ACTION (action));
  g_return_if_fail (GTK_IS_TOGGLE_ACTION (action));

  /* leave when the menu is locked or this is not the active radio button */
  if (lock_menu_updates == 0
      && gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
    {
      /* get the page number from the action value */
      page = gtk_radio_action_get_current_value (action);

      /* set the page */
      gtk_notebook_set_current_page (notebook, page);
    }
}



static void
mousepad_window_action_contents (GtkAction      *action,
                                 MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* show help */
  mousepad_dialogs_show_help (GTK_WINDOW (window), NULL, NULL);
}



static void
mousepad_window_action_about (GtkAction      *action,
                              MousepadWindow *window)
{
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
