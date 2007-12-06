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

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-marshal.h>
#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-search-bar.h>
#include <mousepad/mousepad-preferences.h>
#include <mousepad/mousepad-util.h>
#include <mousepad/mousepad-window.h>



#define TOOL_BAR_ICON_SIZE  GTK_ICON_SIZE_MENU
#define HIGHTLIGHT_TIMEOUT  225



static void      mousepad_search_bar_class_init                 (MousepadSearchBarClass  *klass);
static void      mousepad_search_bar_init                       (MousepadSearchBar       *bar);
static void      mousepad_search_bar_finalize                   (GObject                 *object);
static void      mousepad_search_bar_find_string                (MousepadSearchBar       *bar,
                                                                 MousepadSearchFlags   flags);
static void      mousepad_search_bar_hide_clicked               (MousepadSearchBar       *bar);
static void      mousepad_search_bar_entry_changed              (GtkWidget               *entry,
                                                                 MousepadSearchBar       *bar);
static void      mousepad_search_bar_highlight_toggled          (GtkWidget               *button,
                                                                 MousepadSearchBar       *bar);
static void      mousepad_search_bar_match_case_toggled         (GtkWidget               *button,
                                                                 MousepadSearchBar       *bar);
static void      mousepad_search_bar_menuitem_toggled           (GtkCheckMenuItem        *item,
                                                                 GtkToggleButton         *button);
static void      mousepad_search_bar_highlight_schedule         (MousepadSearchBar       *bar);
static gboolean  mousepad_search_bar_highlight_timeout          (gpointer                 user_data);
static void      mousepad_search_bar_highlight_timeout_destroy  (gpointer                 user_data);



enum
{
  HIDE_BAR,
  SEARCH,
  LAST_SIGNAL,
};

struct _MousepadSearchBarClass
{
  GtkToolbarClass __parent__;
};

struct _MousepadSearchBar
{
  GtkToolbar           __parent__;

  /* preferences */
  MousepadPreferences *preferences;

  /* text entry */
  GtkWidget           *entry;

  /* menu entries */
  GtkWidget           *match_case_entry;

  /* flags */
  guint                highlight_all : 1;
  guint                match_case : 1;

  /* highlight id */
  guint                highlight_id;
};



static GObjectClass *mousepad_search_bar_parent_class;
static guint         search_bar_signals[LAST_SIGNAL];



GtkWidget *
mousepad_search_bar_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_SEARCH_BAR,
                       "toolbar-style", GTK_TOOLBAR_BOTH_HORIZ,
                       "icon-size", TOOL_BAR_ICON_SIZE,
                       NULL);
}



GType
mousepad_search_bar_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_type_register_static_simple (GTK_TYPE_TOOLBAR,
                                            I_("MousepadSearchBar"),
                                            sizeof (MousepadSearchBarClass),
                                            (GClassInitFunc) mousepad_search_bar_class_init,
                                            sizeof (MousepadSearchBar),
                                            (GInstanceInitFunc) mousepad_search_bar_init,
                                            0);
    }

  return type;
}



static void
mousepad_search_bar_class_init (MousepadSearchBarClass *klass)
{
  GObjectClass  *gobject_class;
  GtkBindingSet *binding_set;

  mousepad_search_bar_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_search_bar_finalize;

  /* signals */
  search_bar_signals[HIDE_BAR] =
    g_signal_new (I_("hide-bar"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_NO_HOOKS | G_SIGNAL_ACTION,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  search_bar_signals[SEARCH] =
    g_signal_new (I_("search"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_NO_HOOKS ,
                  0, NULL, NULL,
                  _mousepad_marshal_INT__FLAGS_STRING_STRING,
                  G_TYPE_INT, 3,
                  MOUSEPAD_TYPE_SEARCH_FLAGS,
                  G_TYPE_STRING, G_TYPE_STRING);

  /* setup key bindings for the search bar */
  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_Escape, 0, "hide-bar", 0);

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
}



static void
mousepad_search_bar_init (MousepadSearchBar *bar)
{
  GtkWidget   *label, *image, *check, *menuitem;
  GtkToolItem *item;
  gboolean     match_case;

  /* preferences */
  bar->preferences = mousepad_preferences_get ();

  /* load some preferences */
  g_object_get (G_OBJECT (bar->preferences), "search-match-case", &match_case, NULL);

  /* init variables */
  bar->highlight_id = 0;
  bar->match_case = match_case;

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
  gtk_widget_show (bar->entry);

  /* next button */
  image = gtk_image_new_from_stock (GTK_STOCK_GO_DOWN, TOOL_BAR_ICON_SIZE);
  gtk_widget_show (image);

  item = gtk_tool_button_new (image, _("_Next"));
  gtk_tool_item_set_is_important (item, TRUE);
  gtk_tool_button_set_use_underline (GTK_TOOL_BUTTON (item), TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);
  g_signal_connect_swapped (G_OBJECT (item), "clicked", G_CALLBACK (mousepad_search_bar_find_next), bar);
  gtk_widget_show (GTK_WIDGET (item));

  /* previous button */
  image = gtk_image_new_from_stock (GTK_STOCK_GO_UP, TOOL_BAR_ICON_SIZE);
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
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), GTK_STOCK_SELECT_ALL);
  gtk_tool_button_set_label (GTK_TOOL_BUTTON (item), _("Highlight _All"));
  gtk_tool_item_set_is_important (item, TRUE);
  gtk_tool_button_set_use_underline (GTK_TOOL_BUTTON (item), TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);
  g_signal_connect (G_OBJECT (item), "clicked", G_CALLBACK (mousepad_search_bar_highlight_toggled), bar);
  gtk_widget_show (GTK_WIDGET (item));

  /* check button for case sensitive, including the proxy menu item */
  item = gtk_tool_item_new ();
  g_signal_connect_object (G_OBJECT (bar), "destroy", G_CALLBACK (gtk_widget_destroy), item, G_CONNECT_SWAPPED);
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);
  gtk_widget_show (GTK_WIDGET (item));

  check = gtk_check_button_new_with_mnemonic (_("Mat_ch Case"));
  g_signal_connect_object (G_OBJECT (bar), "destroy", G_CALLBACK (gtk_widget_destroy), item, G_CONNECT_SWAPPED);
  gtk_container_add (GTK_CONTAINER (item), check);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), match_case);
  g_signal_connect (G_OBJECT (check), "toggled", G_CALLBACK (mousepad_search_bar_match_case_toggled), bar);
  gtk_widget_show (check);

  bar->match_case_entry = menuitem = gtk_check_menu_item_new_with_mnemonic (_("Mat_ch Case"));
  g_signal_connect_object (G_OBJECT (bar), "destroy", G_CALLBACK (gtk_widget_destroy), item, G_CONNECT_SWAPPED);
  gtk_tool_item_set_proxy_menu_item (item, "case-sensitive", menuitem);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem), match_case);
  g_signal_connect (G_OBJECT (menuitem), "toggled", G_CALLBACK (mousepad_search_bar_menuitem_toggled), check);
  gtk_widget_show (menuitem);
}



static void
mousepad_search_bar_finalize (GObject *object)
{
  MousepadSearchBar *bar = MOUSEPAD_SEARCH_BAR (object);

  /* release the preferences */
  g_object_unref (G_OBJECT (bar->preferences));

  /* stop a running highlight timeout */
  if (bar->highlight_id != 0)
    g_source_remove (bar->highlight_id);

  (*G_OBJECT_CLASS (mousepad_search_bar_parent_class)->finalize) (object);
}



static void
mousepad_search_bar_find_string (MousepadSearchBar   *bar,
                                 MousepadSearchFlags  flags)
{
  const gchar *string;
  gint         nmatches;

  /* search the entire document */
  flags |= MOUSEPAD_SEARCH_FLAGS_AREA_DOCUMENT;

  /* if we don't hightlight, we select with wrapping */
  if ((flags & MOUSEPAD_SEARCH_FLAGS_ACTION_HIGHTLIGHT) == 0)
    flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT
             | MOUSEPAD_SEARCH_FLAGS_WRAP_AROUND;

  /* append the insensitive flags when needed */
  if (bar->match_case)
    flags |= MOUSEPAD_SEARCH_FLAGS_MATCH_CASE;

  /* get the entry string */
  string = gtk_entry_get_text (GTK_ENTRY (bar->entry));

  /* emit signal */
  g_signal_emit (G_OBJECT (bar), search_bar_signals[SEARCH], 0, flags, string, NULL, &nmatches);

  /* make sure the search entry is not red when no text was typed */
  if (string == NULL || *string == '\0')
    nmatches = 1;

  /* change the entry style */
  mousepad_util_entry_error (bar->entry, nmatches < 1);
}



static void
mousepad_search_bar_hide_clicked (MousepadSearchBar *bar)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* emit the signal */
  g_signal_emit (G_OBJECT (bar), search_bar_signals[HIDE_BAR], 0);
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

  /* schedule a new highlight */
  if (bar->highlight_all)
    mousepad_search_bar_highlight_schedule (bar);
}



static void
mousepad_search_bar_highlight_toggled (GtkWidget         *button,
                                       MousepadSearchBar *bar)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* set the new state */
  bar->highlight_all = gtk_toggle_tool_button_get_active (GTK_TOGGLE_TOOL_BUTTON (button));

  /* reschedule the highlight */
  mousepad_search_bar_highlight_schedule (bar);
}



static void
mousepad_search_bar_match_case_toggled (GtkWidget         *button,
                                        MousepadSearchBar *bar)
{
  gboolean active;

  _mousepad_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* get the state of the toggle button */
  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  /* set the state of the menu item */
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (bar->match_case_entry), active);

  /* save the state */
  bar->match_case = active;

  /* save the setting */
  g_object_set (G_OBJECT (bar->preferences), "search-match-case", active, NULL);

  /* search ahead with this new flags */
  mousepad_search_bar_entry_changed (NULL, bar);

  /* schedule a new hightlight */
  if (bar->highlight_all)
    mousepad_search_bar_highlight_schedule (bar);
}



static void
mousepad_search_bar_menuitem_toggled (GtkCheckMenuItem *item,
                                      GtkToggleButton  *button)
{
  gboolean active;

  _mousepad_return_if_fail (GTK_IS_CHECK_MENU_ITEM (item));
  _mousepad_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));

  /* toggle the menubar item, he/she will send the signal */
  active = gtk_check_menu_item_get_active (item);
  gtk_toggle_button_set_active (button, active);
}



static void
mousepad_search_bar_highlight_schedule (MousepadSearchBar *bar)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* stop a pending timeout */
  if (bar->highlight_id != 0)
    g_source_remove (bar->highlight_id);

  /* schedule a new timeout */
  bar->highlight_id = g_timeout_add_full (G_PRIORITY_LOW, HIGHTLIGHT_TIMEOUT, mousepad_search_bar_highlight_timeout,
                                          bar, mousepad_search_bar_highlight_timeout_destroy);
}



static void
mousepad_search_bar_highlight_timeout_destroy (gpointer user_data)
{
  MOUSEPAD_SEARCH_BAR (user_data)->highlight_id = 0;
}



static gboolean
mousepad_search_bar_highlight_timeout (gpointer user_data)
{
  MousepadSearchBar   *bar = MOUSEPAD_SEARCH_BAR (user_data);
  MousepadSearchFlags  flags;

  GDK_THREADS_ENTER ();

  /* set search flags */
  flags = MOUSEPAD_SEARCH_FLAGS_DIR_FORWARD
          | MOUSEPAD_SEARCH_FLAGS_ITER_AREA_START
          | MOUSEPAD_SEARCH_FLAGS_ACTION_HIGHTLIGHT;

  /* only clear when there is no text */
  if (!bar->highlight_all)
    flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_CLEANUP;

  /* hightlight */
  mousepad_search_bar_find_string (bar, flags);

  GDK_THREADS_LEAVE ();

  /* stop the timeout */
  return FALSE;
}



GtkEditable *
mousepad_search_bar_entry (MousepadSearchBar *bar)
{
  if (bar && GTK_WIDGET_HAS_FOCUS (bar->entry))
    return GTK_EDITABLE (bar->entry);
  else
    return NULL;
}



void
mousepad_search_bar_focus (MousepadSearchBar *bar)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* focus the entry field */
  gtk_widget_grab_focus (bar->entry);

  /* update the highlight */
  if (bar->highlight_all)
    mousepad_search_bar_highlight_schedule (bar);
}



void
mousepad_search_bar_find_next (MousepadSearchBar *bar)
{
  MousepadSearchFlags flags;

  _mousepad_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

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

  _mousepad_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* set search flags */
  flags = MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START
          | MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD;

  /* search */
  mousepad_search_bar_find_string (bar, flags);
}
