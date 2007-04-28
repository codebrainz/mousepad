/* $Id$ */
/*
 * Copyright (c) 2007 Nick Schermer <nick@xfce.org>
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

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-marshal.h>
#include <mousepad/mousepad-types.h>
#include <mousepad/mousepad-enum-types.h>
#include <mousepad/mousepad-search-bar.h>
#include <mousepad/mousepad-preferences.h>
#include <mousepad/mousepad-window.h>



#define TOOL_BAR_ICON_SIZE  GTK_ICON_SIZE_MENU
#define HIGHTLIGHT_TIMEOUT  500



static void      mousepad_search_bar_class_init                 (MousepadSearchBarClass  *klass);
static void      mousepad_search_bar_init                       (MousepadSearchBar       *search_bar);
static void      mousepad_search_bar_finalize                   (GObject                 *object);
static void      mousepad_search_bar_find_string                (MousepadSearchBar       *search_bar,
                                                                 MousepadSearchFlags      flags);
static void      mousepad_search_hide_clicked                   (MousepadSearchBar       *search_bar);
static void      mousepad_search_bar_entry_changed              (GtkWidget               *entry,
                                                                 MousepadSearchBar       *search_bar);
static void      mousepad_search_bar_highlight_toggled          (GtkWidget               *button,
                                                                 MousepadSearchBar       *search_bar);
static void      mousepad_search_bar_match_case_toggled         (GtkWidget               *button,
                                                                 MousepadSearchBar       *search_bar);
static void      mousepad_search_bar_wrap_around_toggled        (GtkWidget               *button,
                                                                 MousepadSearchBar       *search_bar);
static void      mousepad_search_bar_menuitem_toggled           (GtkCheckMenuItem        *item,
                                                                 GtkToggleButton         *button);
static gboolean  mousepad_search_bar_highlight_timeout          (gpointer                 user_data);
static void      mousepad_search_bar_highlight_timeout_destroy  (gpointer                 user_data);
static void      mousepad_search_bar_nothing_found              (MousepadSearchBar       *search_bar,
                                                                 gboolean                 nothing_found);


enum
{
  HIDE_BAR,
  FIND_STRING,
  HIGHLIGHT_ALL,
  LAST_SIGNAL,
};


struct _MousepadSearchBarClass
{
  GtkToolbarClass __parent__;
};

struct _MousepadSearchBar
{
  GtkToolbar           __parent__;

  MousepadPreferences *preferences;

  /* text entry */
  GtkWidget           *entry;

  /* menu entries */
  GtkWidget           *match_case_entry;
  GtkWidget           *wrap_around_entry;

  /* if something was found */
  guint                nothing_found : 1;

  /* settings */
  guint                highlight_all : 1;
  guint                match_case : 1;
  guint                wrap_around : 1;

  /* timeout for highlighting while typing */
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
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  search_bar_signals[FIND_STRING] =
    g_signal_new (I_("find-string"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  _mousepad_marshal_BOOLEAN__STRING_FLAGS,
                  G_TYPE_BOOLEAN, 2,
                  G_TYPE_STRING, MOUSEPAD_TYPE_SEARCH_FLAGS);

  search_bar_signals[HIGHLIGHT_ALL] =
    g_signal_new (I_("highlight-all"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  _mousepad_marshal_BOOLEAN__STRING_FLAGS,
                  G_TYPE_BOOLEAN, 2,
                  G_TYPE_STRING, MOUSEPAD_TYPE_SEARCH_FLAGS);

  /* setup key bindings for the search bar */
  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_Escape, 0, "hide-bar", 0);

  /* hide the shadow around the toolbar */
  gtk_rc_parse_string ("style \"mousepad-search-bar-style\"\n"
                       "  {\n"
                       "    GtkToolbar::shadow-type = GTK_SHADOW_NONE\n"
                       "  }\n"
                       "class \"MousepadSearchBar\" style \"mousepad-search-bar-style\"\n"

                       /* add 2px space between the toolbar buttons */
                       "style \"mousepad-button-style\"\n"
                       "  {\n"
                       "    GtkToolButton::icon-spacing = 2\n"
                       "  }\n"
                       "widget \"MousepadWindow.*.Gtk*ToolButton\" style \"mousepad-button-style\"\n");
}



static void
mousepad_search_bar_init (MousepadSearchBar *search_bar)
{
  GtkWidget   *label, *image, *check, *menuitem;
  GtkToolItem *item;
  gboolean     match_case, wrap_around;

  /* preferences */
  search_bar->preferences = mousepad_preferences_get ();

  /* load some properties */
  g_object_get (G_OBJECT (search_bar->preferences),
                "last-match-case", &match_case,
                "last-wrap-around", &wrap_around,
                NULL);

  /* init variables */
  search_bar->nothing_found = FALSE;
  search_bar->highlight_id = 0;
  search_bar->match_case = match_case;
  search_bar->wrap_around = wrap_around;

  /* the close button */
  item = gtk_tool_button_new_from_stock (GTK_STOCK_CLOSE);
  gtk_toolbar_insert (GTK_TOOLBAR (search_bar), item, -1);
  gtk_widget_show (GTK_WIDGET (item));
  g_signal_connect_swapped (G_OBJECT (item), "clicked",
                            G_CALLBACK (mousepad_search_hide_clicked), search_bar);

  /* the find label */
  item = gtk_tool_item_new ();
  gtk_toolbar_insert (GTK_TOOLBAR (search_bar), item, -1);
  gtk_widget_show (GTK_WIDGET (item));

  label = gtk_label_new_with_mnemonic (_("Fi_nd:"));
  gtk_container_add (GTK_CONTAINER (item), label);
  gtk_misc_set_padding (GTK_MISC (label), 2, 0);
  gtk_widget_show (label);

  /* the entry field */
  item = gtk_tool_item_new ();
  gtk_toolbar_insert (GTK_TOOLBAR (search_bar), item, -1);
  gtk_widget_show (GTK_WIDGET (item));

  search_bar->entry = gtk_entry_new ();
  gtk_container_add (GTK_CONTAINER (item), search_bar->entry);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), search_bar->entry);
  gtk_widget_show (search_bar->entry);
  g_signal_connect (G_OBJECT (search_bar->entry), "changed",
                    G_CALLBACK (mousepad_search_bar_entry_changed), search_bar);

  /* next button */
  image = gtk_image_new_from_stock (GTK_STOCK_GO_DOWN, TOOL_BAR_ICON_SIZE);
  gtk_widget_show (image);

  item = gtk_tool_button_new (image, _("_Next"));
  gtk_tool_item_set_is_important (item, TRUE);
  gtk_tool_button_set_use_underline (GTK_TOOL_BUTTON (item), TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (search_bar), item, -1);
  gtk_widget_show (GTK_WIDGET (item));
  g_signal_connect_swapped (G_OBJECT (item), "clicked",
                            G_CALLBACK (mousepad_search_bar_find_next), search_bar);

  /* previous button */
  image = gtk_image_new_from_stock (GTK_STOCK_GO_UP, TOOL_BAR_ICON_SIZE);
  gtk_widget_show (image);

  item = gtk_tool_button_new (image, _("_Previous"));
  gtk_tool_item_set_is_important (item, TRUE);
  gtk_tool_button_set_use_underline (GTK_TOOL_BUTTON (item), TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (search_bar), item, -1);
  gtk_widget_show (GTK_WIDGET (item));
  g_signal_connect_swapped (G_OBJECT (item), "clicked",
                            G_CALLBACK (mousepad_search_bar_find_previous), search_bar);

  /* highlight all */
  item = (GtkToolItem *) gtk_toggle_tool_button_new ();
  g_signal_connect_object (G_OBJECT (search_bar), "destroy", G_CALLBACK (gtk_widget_destroy), item, G_CONNECT_SWAPPED);
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), GTK_STOCK_SELECT_ALL);
  gtk_tool_button_set_label (GTK_TOOL_BUTTON (item), _("Highlight _All"));
  gtk_tool_item_set_is_important (item, TRUE);
  gtk_tool_button_set_use_underline (GTK_TOOL_BUTTON (item), TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (search_bar), item, -1);
  gtk_widget_show (GTK_WIDGET (item));
  g_signal_connect (G_OBJECT (item), "clicked",
                    G_CALLBACK (mousepad_search_bar_highlight_toggled), search_bar);

  /* check button for case sensitive, including the proxy menu item */
  item = gtk_tool_item_new ();
  g_signal_connect_object (G_OBJECT (search_bar), "destroy", G_CALLBACK (gtk_widget_destroy), item, G_CONNECT_SWAPPED);
  gtk_toolbar_insert (GTK_TOOLBAR (search_bar), item, -1);
  gtk_widget_show (GTK_WIDGET (item));

  check = gtk_check_button_new_with_mnemonic (_("Mat_ch Case"));
  g_signal_connect_object (G_OBJECT (search_bar), "destroy", G_CALLBACK (gtk_widget_destroy), item, G_CONNECT_SWAPPED);
  gtk_container_add (GTK_CONTAINER (item), check);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), match_case);
  gtk_widget_show (check);
  g_signal_connect (G_OBJECT (check), "toggled",
                    G_CALLBACK (mousepad_search_bar_match_case_toggled), search_bar);

  search_bar->match_case_entry = menuitem = gtk_check_menu_item_new_with_mnemonic (_("Mat_ch Case"));
  g_signal_connect_object (G_OBJECT (search_bar), "destroy", G_CALLBACK (gtk_widget_destroy), item, G_CONNECT_SWAPPED);
  gtk_tool_item_set_proxy_menu_item (item, "case-sensitive", menuitem);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem), match_case);
  gtk_widget_show (menuitem);
  g_signal_connect (G_OBJECT (menuitem), "toggled",
                    G_CALLBACK (mousepad_search_bar_menuitem_toggled), check);

  /* check button for wrap around, including the proxy menu item */
  item = gtk_tool_item_new ();
  g_signal_connect_object (G_OBJECT (search_bar), "destroy", G_CALLBACK (gtk_widget_destroy), item, G_CONNECT_SWAPPED);
  gtk_toolbar_insert (GTK_TOOLBAR (search_bar), item, -1);
  gtk_widget_show (GTK_WIDGET (item));

  check = gtk_check_button_new_with_mnemonic (_("W_rap Around"));
  g_signal_connect_object (G_OBJECT (search_bar), "destroy", G_CALLBACK (gtk_widget_destroy), item, G_CONNECT_SWAPPED);
  gtk_container_add (GTK_CONTAINER (item), check);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), wrap_around);
  gtk_widget_show (check);
  g_signal_connect (G_OBJECT (check), "toggled",
                    G_CALLBACK (mousepad_search_bar_wrap_around_toggled), search_bar);

  search_bar->wrap_around_entry = menuitem = gtk_check_menu_item_new_with_mnemonic (_("W_rap Around"));
  g_signal_connect_object (G_OBJECT (search_bar), "destroy", G_CALLBACK (gtk_widget_destroy), item, G_CONNECT_SWAPPED);
  gtk_tool_item_set_proxy_menu_item (item, "case-sensitive", menuitem);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem), wrap_around);
  gtk_widget_show (menuitem);
  g_signal_connect (G_OBJECT (menuitem), "toggled",
                    G_CALLBACK (mousepad_search_bar_menuitem_toggled), check);
}



static void
mousepad_search_bar_finalize (GObject *object)
{
  MousepadSearchBar *search_bar = MOUSEPAD_SEARCH_BAR (object);

  /* release the preferences */
  g_object_unref (G_OBJECT (search_bar->preferences));

  /* stop a running highlight timeout */
  if (search_bar->highlight_id != 0)
    g_source_remove (search_bar->highlight_id);

  (*G_OBJECT_CLASS (mousepad_search_bar_parent_class)->finalize) (object);
}



static void
mousepad_search_bar_find_string (MousepadSearchBar *search_bar,
                                 MousepadSearchFlags flags)
{
  const gchar *string;
  gboolean     result;

  /* append the insensitive flags when needed */
  if (!search_bar->match_case)
    flags |= MOUSEPAD_SEARCH_CASE_INSENSITIVE;

  /* wrap around flag */
  if (search_bar->wrap_around)
    flags |= MOUSEPAD_SEARCH_WRAP_AROUND;

  /* get the entry string */
  string = gtk_entry_get_text (GTK_ENTRY (search_bar->entry));

  /* send the signal and wait for the result */
  g_signal_emit (G_OBJECT (search_bar), search_bar_signals[FIND_STRING], 0,
                 string, flags, &result);

  /* make sure the search entry is not red when no text was typed */
  if (string == NULL || *string == '\0')
    result = TRUE;

  /* change the entry style */
  mousepad_search_bar_nothing_found (search_bar, !result);
}



static void
mousepad_search_hide_clicked (MousepadSearchBar *search_bar)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (search_bar));

  /* emit the signal */
  g_signal_emit (G_OBJECT (search_bar), search_bar_signals[HIDE_BAR], 0);
}



static void
mousepad_search_bar_entry_changed (GtkWidget         *entry,
                                   MousepadSearchBar *search_bar)
{
  /* stop a running highlight timeout */
  if (search_bar->highlight_id != 0)
    g_source_remove (search_bar->highlight_id);

  if (search_bar->highlight_all)
    {
      /* start a new highlight timeout */
      search_bar->highlight_id = g_timeout_add_full (G_PRIORITY_LOW, HIGHTLIGHT_TIMEOUT,
                                                           mousepad_search_bar_highlight_timeout, search_bar,
                                                           mousepad_search_bar_highlight_timeout_destroy);
    }

  /* find */
  mousepad_search_bar_find_string (search_bar, 0);
}



static void
mousepad_search_bar_highlight_toggled (GtkWidget         *button,
                                       MousepadSearchBar *search_bar)
{
  gboolean active;

  _mousepad_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (search_bar));

  /* get the state of the toggle button */
  active = gtk_toggle_tool_button_get_active (GTK_TOGGLE_TOOL_BUTTON (button));

  /* stop a running highlight timeout if the button is inactive*/
  if (search_bar->highlight_id != 0 && !active)
    g_source_remove (search_bar->highlight_id);

  /* save the state */
  search_bar->highlight_all = active;

  /* invoke the highlight function to update the buffer */
  search_bar->highlight_id = g_idle_add_full (G_PRIORITY_LOW, mousepad_search_bar_highlight_timeout,
                                              search_bar, mousepad_search_bar_highlight_timeout_destroy);
}



static void
mousepad_search_bar_match_case_toggled (GtkWidget         *button,
                                        MousepadSearchBar *search_bar)
{
  gboolean active;

  _mousepad_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (search_bar));

  /* get the state of the toggle button */
  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  /* set the state of the menu item */
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (search_bar->match_case_entry), active);

  /* save the state */
  search_bar->match_case = active;

  /* save the setting */
  g_object_set (G_OBJECT (search_bar->preferences), "last-match-case", active, NULL);

  if (search_bar->highlight_all)
    {
      /* invoke the highlight function to update the buffer */
      search_bar->highlight_id = g_idle_add_full (G_PRIORITY_LOW, mousepad_search_bar_highlight_timeout,
                                                  search_bar, mousepad_search_bar_highlight_timeout_destroy);
    }
}



static void
mousepad_search_bar_wrap_around_toggled (GtkWidget         *button,
                                         MousepadSearchBar *search_bar)
{
  gboolean active;

  _mousepad_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (search_bar));

  /* get the state of the toggle button */
  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  /* set the state of the menu item */
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (search_bar->wrap_around_entry), active);

  /* save the state */
  search_bar->wrap_around = active;

  /* save the setting */
  g_object_set (G_OBJECT (search_bar->preferences), "last-wrap-around", active, NULL);
}



static void
mousepad_search_bar_menuitem_toggled (GtkCheckMenuItem *item,
                                      GtkToggleButton  *button)
{
  gboolean active;

  active = gtk_check_menu_item_get_active (item);
  gtk_toggle_button_set_active (button, active);
}



static gboolean
mousepad_search_bar_highlight_timeout (gpointer user_data)
{
  MousepadSearchBar   *search_bar = MOUSEPAD_SEARCH_BAR (user_data);
  const gchar         *string = NULL;
  gboolean             dummy;
  MousepadSearchFlags  flags = 0;

  GDK_THREADS_ENTER ();

  /* append the insensitive case flag if needed */
  if (!search_bar->match_case)
    flags |= MOUSEPAD_SEARCH_CASE_INSENSITIVE;

  /* get the string if highlighting is enabled */
  if (search_bar->highlight_all)
    {
      /* get the entry string */
      string = gtk_entry_get_text (GTK_ENTRY (search_bar->entry));
      if (string == NULL || *string == '\0')
        string = NULL;
    }

  /* send the signal and wait for the result */
  g_signal_emit (G_OBJECT (search_bar), search_bar_signals[HIGHLIGHT_ALL], 0,
                 string, flags, &dummy);

  GDK_THREADS_LEAVE ();

  /* stop the timeout */
  return FALSE;
}



static void
mousepad_search_bar_highlight_timeout_destroy (gpointer user_data)
{
  MOUSEPAD_SEARCH_BAR (user_data)->highlight_id = 0;
}



static void
mousepad_search_bar_nothing_found (MousepadSearchBar *search_bar,
                                   gboolean           nothing_found)
{
  const GdkColor red   = {0, 0xffff, 0x6666, 0x6666};
  const GdkColor white = {0, 0xffff, 0xffff, 0xffff};

  _mousepad_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (search_bar));

  /* only update the style if really needed */
  if (search_bar->nothing_found != nothing_found)
    {
      /* (re)set the base and font color */
      gtk_widget_modify_base (search_bar->entry, GTK_STATE_NORMAL, nothing_found ? &red : NULL);
      gtk_widget_modify_text (search_bar->entry, GTK_STATE_NORMAL, nothing_found ? &white : NULL);

      search_bar->nothing_found = nothing_found;
    }
}



void
mousepad_search_bar_focus (MousepadSearchBar *search_bar)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (search_bar));

  /* focus the entry field */
  gtk_widget_grab_focus (search_bar->entry);
}



void
mousepad_search_bar_find_next (MousepadSearchBar *search_bar)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (search_bar));

  /* find */
  mousepad_search_bar_find_string (search_bar, MOUSEPAD_SEARCH_FORWARDS);
}



void
mousepad_search_bar_find_previous (MousepadSearchBar *search_bar)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (search_bar));

  /* find backwards */
  mousepad_search_bar_find_string (search_bar, MOUSEPAD_SEARCH_BACKWARDS);
}
