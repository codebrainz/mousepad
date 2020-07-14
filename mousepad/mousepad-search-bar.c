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
#include <mousepad/mousepad-marshal.h>
#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-search-bar.h>
#include <mousepad/mousepad-util.h>
#include <mousepad/mousepad-window.h>

#include <gdk/gdk.h>



#define TOOL_BAR_ICON_SIZE  GTK_ICON_SIZE_MENU



static void      mousepad_search_bar_finalize                   (GObject                 *object);
static void      mousepad_search_bar_find_string                (MousepadSearchBar       *bar,
                                                                 MousepadSearchFlags   flags);
static void      mousepad_search_bar_hide_clicked               (MousepadSearchBar       *bar);
static void      mousepad_search_bar_entry_activate             (GtkWidget               *entry,
                                                                 MousepadSearchBar       *bar);
static void      mousepad_search_bar_entry_activate_backward    (GtkWidget               *entry,
                                                                 MousepadSearchBar       *bar);
static void      mousepad_search_bar_entry_changed              (GtkWidget               *entry,
                                                                 MousepadSearchBar       *bar);
static void      mousepad_search_bar_highlight_toggled          (GtkWidget               *button,
                                                                 MousepadSearchBar       *bar);
static void      mousepad_search_bar_match_case_toggled         (GtkWidget               *button,
                                                                 MousepadSearchBar       *bar);
static void      mousepad_search_bar_enable_regex_toggled       (GtkWidget               *button,
                                                                 MousepadSearchBar       *bar);



enum
{
  HIDE_BAR,
  SEARCH,
  LAST_SIGNAL
};

struct _MousepadSearchBarClass
{
  GtkToolbarClass __parent__;
};

struct _MousepadSearchBar
{
  GtkToolbar           __parent__;

  /* text entry */
  GtkWidget           *entry;

  /* menu entries */
  GtkWidget           *match_case_entry;
  GtkWidget           *enable_regex_entry;

  /* flags */
  guint                highlight_all : 1;
  guint                match_case : 1;
  guint                enable_regex : 1;
};



static guint search_bar_signals[LAST_SIGNAL];



GtkWidget *
mousepad_search_bar_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_SEARCH_BAR,
                       "toolbar-style", GTK_TOOLBAR_BOTH_HORIZ,
                       "icon-size", TOOL_BAR_ICON_SIZE,
                       NULL);
}



G_DEFINE_TYPE (MousepadSearchBar, mousepad_search_bar, GTK_TYPE_TOOLBAR)



static void
mousepad_search_bar_class_init (MousepadSearchBarClass *klass)
{
  GObjectClass  *gobject_class;
  GObjectClass  *entry_class;
  GtkBindingSet *binding_set;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_search_bar_finalize;

  /* signals */
  search_bar_signals[HIDE_BAR] =
    g_signal_new (I_("hide-bar"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  search_bar_signals[SEARCH] =
    g_signal_new (I_("search"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  _mousepad_marshal_INT__FLAGS_STRING_STRING,
                  G_TYPE_INT, 3,
                  MOUSEPAD_TYPE_SEARCH_FLAGS,
                  G_TYPE_STRING, G_TYPE_STRING);

  /* setup key bindings for the search bar */
  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "hide-bar", 0);

  /* In GTK3, gtkrc is deprecated */
#if GTK_CHECK_VERSION(3, 0, 0) && (__GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__ > 2)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

  /* hide the shadow around the toolbar */
  gtk_rc_parse_string ("style \"mousepad-search-bar-style\"\n"
                         "{\n"
                           "GtkToolbar::shadow-type = GTK_SHADOW_NONE\n"
                         "}\n"
                       "class \"MousepadSearchBar\" style \"mousepad-search-bar-style\"\n"

                       /* add 2px space between the toolbar buttons */
                       "style \"mousepad-button-style\"\n"
                         "{\n"
                           "GtkToolButton::icon-spacing = 2\n"
                         "}\n"
                       "widget \"MousepadWindow.*.Gtk*ToolButton\" style \"mousepad-button-style\"\n");

#if GTK_CHECK_VERSION(3, 0, 0) && (__GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__ > 2)
# pragma GCC diagnostic pop
#endif

  /* add an activate-backwards signal to GtkEntry */
  entry_class = g_type_class_ref (GTK_TYPE_ENTRY);
  if (G_LIKELY (g_signal_lookup("activate-backward", GTK_TYPE_ENTRY) == 0))
    {
      /* install the signal */
      g_signal_new("activate-backward",
                   GTK_TYPE_ENTRY,
                   G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                   0, NULL, NULL,
                   g_cclosure_marshal_VOID__VOID,
                   G_TYPE_NONE, 0);
      binding_set = gtk_binding_set_by_class(entry_class);
      gtk_binding_entry_add_signal(binding_set, GDK_KEY_Return, GDK_SHIFT_MASK, "activate-backward", 0);
      gtk_binding_entry_add_signal(binding_set, GDK_KEY_KP_Enter, GDK_SHIFT_MASK, "activate-backward", 0);
    }
  g_type_class_unref (entry_class);
}



static void
mousepad_search_bar_init (MousepadSearchBar *bar)
{
  GtkWidget   *label, *image, *check, *menuitem;
  GtkToolItem *item;

  /* load some saved state */
  bar->match_case = MOUSEPAD_SETTING_GET_BOOLEAN (SEARCH_MATCH_CASE);
  bar->enable_regex = MOUSEPAD_SETTING_GET_BOOLEAN (SEARCH_ENABLE_REGEX);
  bar->highlight_all = MOUSEPAD_SETTING_GET_BOOLEAN (SEARCH_HIGHLIGHT_ALL);

  /* the close button */
  item = gtk_tool_button_new_from_stock (GTK_STOCK_CLOSE);
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);
  g_signal_connect_swapped (G_OBJECT (item), "clicked", G_CALLBACK (mousepad_search_bar_hide_clicked), bar);
  gtk_widget_show (GTK_WIDGET (item));

  /* the find label */
  item = gtk_tool_item_new ();
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);
  gtk_widget_show (GTK_WIDGET (item));

  label = gtk_label_new_with_mnemonic (_("Fi_nd:"));
  gtk_container_add (GTK_CONTAINER (item), label);
  gtk_misc_set_padding (GTK_MISC (label), 2, 0);
  gtk_widget_show (label);

  /* the entry field */
  item = gtk_tool_item_new ();
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);
  gtk_widget_show (GTK_WIDGET (item));

  bar->entry = gtk_entry_new ();
  gtk_container_add (GTK_CONTAINER (item), bar->entry);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), bar->entry);
  g_signal_connect (G_OBJECT (bar->entry), "changed", G_CALLBACK (mousepad_search_bar_entry_changed), bar);
  g_signal_connect (G_OBJECT (bar->entry), "activate", G_CALLBACK (mousepad_search_bar_entry_activate), bar);
  g_signal_connect (G_OBJECT (bar->entry), "activate-backward", G_CALLBACK (mousepad_search_bar_entry_activate_backward), bar);
  gtk_widget_show (bar->entry);

  /* next button */
  image = gtk_image_new_from_icon_name ("go-down", TOOL_BAR_ICON_SIZE);
  gtk_widget_show (image);

  item = gtk_tool_button_new (image, _("_Next"));
  gtk_tool_item_set_is_important (item, TRUE);
  gtk_tool_button_set_use_underline (GTK_TOOL_BUTTON (item), TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);
  g_signal_connect_swapped (G_OBJECT (item), "clicked", G_CALLBACK (mousepad_search_bar_find_next), bar);
  gtk_widget_show (GTK_WIDGET (item));

  /* previous button */
  image = gtk_image_new_from_icon_name ("go-up", TOOL_BAR_ICON_SIZE);
  gtk_widget_show (image);

  item = gtk_tool_button_new (image, _("_Previous"));
  gtk_tool_item_set_is_important (item, TRUE);
  gtk_tool_button_set_use_underline (GTK_TOOL_BUTTON (item), TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);
  g_signal_connect_swapped (G_OBJECT (item), "clicked", G_CALLBACK (mousepad_search_bar_find_previous), bar);
  gtk_widget_show (GTK_WIDGET (item));

  /* highlight all */
  item = (GtkToolItem *) gtk_toggle_tool_button_new ();
  g_signal_connect_object (G_OBJECT (bar), "destroy", G_CALLBACK (gtk_widget_destroy), item, G_CONNECT_SWAPPED);
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), "edit-select-all");
  gtk_tool_button_set_label (GTK_TOOL_BUTTON (item), _("Highlight _All"));
  gtk_tool_item_set_is_important (item, TRUE);
  gtk_tool_button_set_use_underline (GTK_TOOL_BUTTON (item), TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);
  g_signal_connect (G_OBJECT (item), "clicked", G_CALLBACK (mousepad_search_bar_highlight_toggled), bar);
  gtk_widget_show (GTK_WIDGET (item));

  MOUSEPAD_SETTING_BIND (SEARCH_HIGHLIGHT_ALL, item, "active", G_SETTINGS_BIND_DEFAULT);

  /* check button for case sensitive, including the proxy menu item */
  item = gtk_tool_item_new ();
  g_signal_connect_object (G_OBJECT (bar), "destroy", G_CALLBACK (gtk_widget_destroy), item, G_CONNECT_SWAPPED);
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);
  gtk_widget_show (GTK_WIDGET (item));

  check = gtk_check_button_new_with_mnemonic (_("Mat_ch Case"));
  g_signal_connect_object (G_OBJECT (bar), "destroy", G_CALLBACK (gtk_widget_destroy), item, G_CONNECT_SWAPPED);
  gtk_container_add (GTK_CONTAINER (item), check);
  g_signal_connect (G_OBJECT (check), "toggled", G_CALLBACK (mousepad_search_bar_match_case_toggled), bar);
  gtk_widget_show (check);

  /* keep the widgets in sync with the GSettings */
  MOUSEPAD_SETTING_BIND (SEARCH_MATCH_CASE, check, "active", G_SETTINGS_BIND_DEFAULT);

  /* overflow menu item for when window is too narrow to show the tool bar item */
  bar->match_case_entry = menuitem = gtk_check_menu_item_new_with_mnemonic (_("Mat_ch Case"));
  g_signal_connect_object (G_OBJECT (bar), "destroy", G_CALLBACK (gtk_widget_destroy), item, G_CONNECT_SWAPPED);
  gtk_tool_item_set_proxy_menu_item (item, "case-sensitive", menuitem);

  /* Keep toolbar check button and overflow proxy menu item in sync */
  g_object_bind_property (check, "active", menuitem, "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_show (menuitem);

  /* check button for enabling regex, including the proxy menu item */
  item = gtk_tool_item_new ();
  g_signal_connect_object (G_OBJECT (bar), "destroy", G_CALLBACK (gtk_widget_destroy), item, G_CONNECT_SWAPPED);
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);
  gtk_widget_show (GTK_WIDGET (item));

  check = gtk_check_button_new_with_mnemonic (_("_Enable Regex"));
  g_signal_connect_object (G_OBJECT (bar), "destroy", G_CALLBACK (gtk_widget_destroy), item, G_CONNECT_SWAPPED);
  gtk_container_add (GTK_CONTAINER (item), check);
  g_signal_connect (G_OBJECT (check), "toggled", G_CALLBACK (mousepad_search_bar_enable_regex_toggled), bar);
  gtk_widget_show (check);

  /* keep the widgets in sync with the GSettings */
  MOUSEPAD_SETTING_BIND (SEARCH_ENABLE_REGEX, check, "active", G_SETTINGS_BIND_DEFAULT);

  /* overflow menu item for when window is too narrow to show the tool bar item */
  bar->enable_regex_entry = menuitem = gtk_check_menu_item_new_with_mnemonic (_("_Enable Regex"));
  g_signal_connect_object (G_OBJECT (bar), "destroy", G_CALLBACK (gtk_widget_destroy), item, G_CONNECT_SWAPPED);
  gtk_tool_item_set_proxy_menu_item (item, "enable-regex", menuitem);

  /* Keep toolbar check button and overflow proxy menu item in sync */
  g_object_bind_property (check, "active", menuitem, "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_show (menuitem);
}



static void
mousepad_search_bar_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (mousepad_search_bar_parent_class)->finalize) (object);
}



static void
mousepad_search_bar_find_string (MousepadSearchBar   *bar,
                                 MousepadSearchFlags  flags)
{
  const gchar *string;
  gint         nmatches;

  /* always true when using the search bar */
  flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT
           | MOUSEPAD_SEARCH_FLAGS_WRAP_AROUND;

  /* append the insensitive flags when needed */
  if (bar->match_case)
    flags |= MOUSEPAD_SEARCH_FLAGS_MATCH_CASE;

  /* append the regex flags when needed */
  if (bar->enable_regex)
    flags |= MOUSEPAD_SEARCH_FLAGS_ENABLE_REGEX;

  /* get the entry string */
  string = gtk_entry_get_text (GTK_ENTRY (bar->entry));

  /* emit signal */
  g_signal_emit (G_OBJECT (bar), search_bar_signals[SEARCH], 0, flags, string, NULL, &nmatches);

  /* do nothing with the entry error when triggered by highlight */
  if (! (flags & (MOUSEPAD_SEARCH_FLAGS_ACTION_HIGHLIGHT_ON
                  | MOUSEPAD_SEARCH_FLAGS_ACTION_HIGHLIGHT_OFF)))
    {
      /* make sure the search entry is not red when no text was typed */
      if (string == NULL || *string == '\0')
        nmatches = 1;

      /* change the entry style */
      mousepad_util_entry_error (bar->entry, nmatches < 1);
    }
}



static void
mousepad_search_bar_hide_clicked (MousepadSearchBar *bar)
{
  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* emit the signal */
  g_signal_emit (G_OBJECT (bar), search_bar_signals[HIDE_BAR], 0);
}



static void
mousepad_search_bar_entry_activate (GtkWidget         *entry,
                                    MousepadSearchBar *bar)
{
  mousepad_search_bar_find_next (bar);
}



static void
mousepad_search_bar_entry_activate_backward (GtkWidget         *entry,
                                             MousepadSearchBar *bar)
{
  mousepad_search_bar_find_previous (bar);
}



static void
mousepad_search_bar_entry_changed (GtkWidget         *entry,
                                   MousepadSearchBar *bar)
{
  MousepadSearchFlags flags;

  /* set the search flags */
  flags = MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START
          | MOUSEPAD_SEARCH_FLAGS_DIR_FORWARD;

  /* find */
  mousepad_search_bar_find_string (bar, flags);
}



static void
mousepad_search_bar_highlight_toggled (GtkWidget         *button,
                                       MousepadSearchBar *bar)
{
  MousepadSearchFlags flags;

  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* set the new state and search flags */
  bar->highlight_all = gtk_toggle_tool_button_get_active (GTK_TOGGLE_TOOL_BUTTON (button));
  if (bar->highlight_all)
    flags = MOUSEPAD_SEARCH_FLAGS_ACTION_HIGHLIGHT_ON;
  else
    flags = MOUSEPAD_SEARCH_FLAGS_ACTION_HIGHLIGHT_OFF;

  /* emit signal to set the highlight */
  mousepad_search_bar_find_string (bar, flags);
}



static void
mousepad_search_bar_match_case_toggled (GtkWidget         *button,
                                        MousepadSearchBar *bar)
{
  gboolean active;

  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* get the state of the toggle button */
  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  /* save the state */
  bar->match_case = active;

  /* search ahead with this new flags */
  mousepad_search_bar_entry_changed (NULL, bar);
}



static void
mousepad_search_bar_enable_regex_toggled (GtkWidget         *button,
                                          MousepadSearchBar *bar)
{
  gboolean active;

  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* get the state of the toggle button */
  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  /* save the state */
  bar->enable_regex = active;

  /* search ahead with this new flags */
  mousepad_search_bar_entry_changed (NULL, bar);
}



GtkEditable *
mousepad_search_bar_entry (MousepadSearchBar *bar)
{
  if (bar && gtk_widget_has_focus (bar->entry))
    return GTK_EDITABLE (bar->entry);
  else
    return NULL;
}



void
mousepad_search_bar_focus (MousepadSearchBar *bar)
{
  MousepadSearchFlags flags;

  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* focus the entry field */
  gtk_widget_grab_focus (bar->entry);

  /* select the entire entry */
  gtk_editable_select_region (GTK_EDITABLE (bar->entry), 0, -1);

  /* highlighting has been disabled by hiding the search bar */
  if (bar->highlight_all)
    {
      flags = MOUSEPAD_SEARCH_FLAGS_ACTION_HIGHLIGHT_ON;
      mousepad_search_bar_find_string (bar, flags);
    }
}



void
mousepad_search_bar_find_next (MousepadSearchBar *bar)
{
  MousepadSearchFlags flags;

  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* set search flags */
  flags = MOUSEPAD_SEARCH_FLAGS_ITER_SEL_END
          | MOUSEPAD_SEARCH_FLAGS_DIR_FORWARD;

  /* search */
  mousepad_search_bar_find_string (bar, flags);
}



void
mousepad_search_bar_find_previous (MousepadSearchBar *bar)
{
  MousepadSearchFlags flags;

  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* set search flags */
  flags = MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START
          | MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD;

  /* search */
  mousepad_search_bar_find_string (bar, flags);
}



void
mousepad_search_bar_set_text (MousepadSearchBar *bar, gchar *text)
{
  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  gtk_entry_set_text (GTK_ENTRY (bar->entry), text);
}
