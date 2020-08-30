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
#include <mousepad/mousepad-prefs-dialog.h>
#include <mousepad/mousepad-prefs-dialog-ui.h>
#include <mousepad/mousepad-settings.h>
#include <mousepad/mousepad-util.h>

#include <gtksourceview/gtksource.h>



#define WID_NOTEBOOK                        "/prefs/main-notebook"

/* View page */
#define WID_SHOW_LINE_NUMBERS_CHECK         "/prefs/view/display/show-line-numbers-check"
#define WID_SHOW_WHITESPACE_CHECK           "/prefs/view/display/display-whitespace-check"
#define WID_SHOW_LINE_ENDINGS_CHECK         "/prefs/view/display/display-line-endings-check"
#define WID_SHOW_RIGHT_MARGIN_CHECK         "/prefs/view/display/long-line-check"
#define WID_RIGHT_MARGIN_SPIN               "/prefs/view/display/long-line-spin"
#define WID_HIGHLIGHT_CURRENT_LINE_CHECK    "/prefs/view/display/highlight-current-line-check"
#define WID_MATCH_BRACES_CHECK              "/prefs/view/display/match-braces-check"
#define WID_WORD_WRAP_CHECK                 "/prefs/view/display/word-wrap-check"

#define WID_USE_DEFAULT_FONT_CHECK          "/prefs/view/font/default-check"
#define WID_FONT_BUTTON                     "/prefs/view/font/chooser-button"
#define WID_SCHEME_COMBO                    "/prefs/view/color-scheme-combo"
#define WID_SCHEME_MODEL                    "/prefs/view/color-scheme-model"

/* Editor page */
#define WID_TAB_WIDTH_SPIN                  "/prefs/editor/tab-width-spin"
#define WID_TAB_MODE_COMBO                  "/prefs/editor/tab-mode-combo"
#define WID_AUTO_INDENT_CHECK               "/prefs/editor/auto-indent-check"
#define WID_SMART_HOME_END_COMBO            "/prefs/editor/smart-home-end-combo"

/* Window page */
#define WID_STATUSBAR_VISIBLE_CHECK         "/prefs/window/general/show-statusbar-check"
#define WID_PATH_IN_TITLE_CHECK             "/prefs/window/general/show-path-in-title-check"
#define WID_REMEMBER_SIZE_CHECK             "/prefs/window/general/remember-window-size-check"
#define WID_REMEMBER_POSITION_CHECK         "/prefs/window/general/remember-window-position-check"
#define WID_REMEMBER_STATE_CHECK            "/prefs/window/general/remember-window-state-check"
#define WID_ALWAYS_SHOW_TABS_CHECK          "/prefs/window/notebook/always-show-tabs-check"
#define WID_CYCLE_TABS_CHECK                "/prefs/window/notebook/cycle-tabs-check"
#define WID_TOOLBAR_VISIBLE_CHECK           "/prefs/window/toolbar/visible-check"
#define WID_TOOLBAR_STYLE_COMBO             "/prefs/window/toolbar/style-combo"
#define WID_TOOLBAR_STYLE_LABEL             "/prefs/window/toolbar/style-label"
#define WID_TOOLBAR_ICON_SIZE_COMBO         "/prefs/window/toolbar/icon-size-combo"
#define WID_TOOLBAR_ICON_SIZE_LABEL         "/prefs/window/toolbar/icon-size-label"



enum
{
  COLUMN_ID,
  COLUMN_NAME,
};



struct MousepadPrefsDialog_
{
  GtkDialog   parent;
  GtkBuilder *builder;
  gboolean    blocked;
};

struct MousepadPrefsDialogClass_
{
  GtkDialogClass parent_class;
};



static void mousepad_prefs_dialog_finalize (GObject *object);



G_DEFINE_TYPE (MousepadPrefsDialog, mousepad_prefs_dialog, GTK_TYPE_DIALOG)



static void
mousepad_prefs_dialog_class_init (MousepadPrefsDialogClass *klass)
{
  GObjectClass *g_object_class;

  g_object_class = G_OBJECT_CLASS (klass);

  g_object_class->finalize = mousepad_prefs_dialog_finalize;
}



static void
mousepad_prefs_dialog_finalize (GObject *object)
{
  MousepadPrefsDialog *self;

  g_return_if_fail (MOUSEPAD_IS_PREFS_DIALOG (object));

  self = MOUSEPAD_PREFS_DIALOG (object);

  /* destroy the GtkBuilder instance */
  if (G_IS_OBJECT (self->builder))
    g_object_unref (self->builder);

  G_OBJECT_CLASS (mousepad_prefs_dialog_parent_class)->finalize (object);
}



/* update the color scheme when the prefs dialog widget changes */
static void
mousepad_prefs_dialog_color_scheme_changed (MousepadPrefsDialog *self,
                                            GtkComboBox         *combo)
{
  GtkListStore *store;
  GtkTreeIter   iter;
  gchar        *scheme_id = NULL;

  store = GTK_LIST_STORE (gtk_builder_get_object (self->builder, WID_SCHEME_MODEL));

  gtk_combo_box_get_active_iter (combo, &iter);

  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, COLUMN_ID, &scheme_id,-1);

  self->blocked = TRUE;
  MOUSEPAD_SETTING_SET_STRING (COLOR_SCHEME, scheme_id);
  self->blocked = FALSE;

  g_free (scheme_id);
}



/* udpate the color schemes combo when the setting changes */
static void
mousepad_prefs_dialog_color_scheme_setting_changed (MousepadPrefsDialog *self,
                                                    gchar               *key,
                                                    GSettings           *settings)
{
  gchar        *new_id;
  GtkTreeIter   iter;
  gboolean      iter_valid;
  GtkComboBox  *combo;
  GtkTreeModel *model;

  /* don't do anything when the combo box is itself updating the setting */
  if (self->blocked)
    return;

  new_id = MOUSEPAD_SETTING_GET_STRING (COLOR_SCHEME);

  combo = GTK_COMBO_BOX (gtk_builder_get_object (self->builder, WID_SCHEME_COMBO));
  model = GTK_TREE_MODEL (gtk_builder_get_object (self->builder, WID_SCHEME_MODEL));

  iter_valid = gtk_tree_model_get_iter_first (model, &iter);
  while (iter_valid)
    {
      gchar   *id = NULL;
      gboolean equal;

      gtk_tree_model_get (model, &iter, COLUMN_ID, &id, -1);
      equal = (g_strcmp0 (id, new_id) == 0);
      g_free (id);

      if (equal)
        {
          gtk_combo_box_set_active_iter (combo, &iter);
          break;
        }

      iter_valid = gtk_tree_model_iter_next (model, &iter);
    }

  g_free (new_id);
}



/* add available color schemes to the combo box model */
static void
mousepad_prefs_dialog_setup_color_schemes_combo (MousepadPrefsDialog *self)
{
  GtkSourceStyleSchemeManager *manager;
  const gchar *const          *ids;
  const gchar *const          *id_iter;
  GtkListStore                *store;
  GtkTreeIter                  iter;

  store = GTK_LIST_STORE (gtk_builder_get_object (self->builder, WID_SCHEME_MODEL));
  manager = gtk_source_style_scheme_manager_get_default ();
  ids = gtk_source_style_scheme_manager_get_scheme_ids (manager);

  for (id_iter = ids; *id_iter; id_iter++)
    {
      GtkSourceStyleScheme *scheme;

      scheme = gtk_source_style_scheme_manager_get_scheme (manager, *id_iter);
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          COLUMN_ID, gtk_source_style_scheme_get_id (scheme),
                          COLUMN_NAME, gtk_source_style_scheme_get_name (scheme),
                          -1);
    }

  /* set the active item from the settings */
  mousepad_prefs_dialog_color_scheme_setting_changed (self, NULL, NULL);

  g_signal_connect_swapped (gtk_builder_get_object (self->builder, WID_SCHEME_COMBO),
                            "changed",
                            G_CALLBACK (mousepad_prefs_dialog_color_scheme_changed),
                            self);

  MOUSEPAD_SETTING_CONNECT_OBJECT (COLOR_SCHEME,
                                   G_CALLBACK (mousepad_prefs_dialog_color_scheme_setting_changed),
                                   self,
                                   G_CONNECT_SWAPPED);
}



/* update the setting when the prefs dialog widget changes */
static void
mousepad_prefs_dialog_tab_mode_changed (MousepadPrefsDialog *self,
                                        GtkComboBox         *combo)
{
  self->blocked = TRUE;
  /* in the combo box, id 0 is tabs, and 1 is spaces */
  MOUSEPAD_SETTING_SET_BOOLEAN (INSERT_SPACES,
                                (gtk_combo_box_get_active (combo) == 1));

  self->blocked = FALSE;
}



/* update the combo box when the setting changes */
static void
mousepad_prefs_dialog_tab_mode_setting_changed (MousepadPrefsDialog *self,
                                                gchar               *key,
                                                GSettings           *settings)
{
  gboolean     insert_spaces;
  GtkComboBox *combo;

  /* don't do anything when the combo box is itself updating the setting */
  if (self->blocked)
    return;

  insert_spaces = MOUSEPAD_SETTING_GET_BOOLEAN (INSERT_SPACES);

  combo = GTK_COMBO_BOX (gtk_builder_get_object (self->builder, WID_TAB_MODE_COMBO));

  /* in the combo box, id 0 is tabs, and 1 is spaces */
  gtk_combo_box_set_active (combo, insert_spaces ? 1 : 0);
}



/* update the home/end key setting when the combo changes */
static void
mousepad_prefs_dialog_home_end_changed (MousepadPrefsDialog *self,
                                        GtkComboBox         *combo)
{
  self->blocked = TRUE;
  MOUSEPAD_SETTING_SET_ENUM (SMART_HOME_END, gtk_combo_box_get_active (combo));
  self->blocked = FALSE;
}



/* update the combo when the setting changes */
static void
mousepad_prefs_dialog_home_end_setting_changed (MousepadPrefsDialog *self,
                                                gchar               *key,
                                                GSettings           *settings)
{
  GtkComboBox *combo;

  /* don't do anything when the combo box is itself updating the setting */
  if (self->blocked)
    return;

  combo = GTK_COMBO_BOX (gtk_builder_get_object (self->builder, WID_SMART_HOME_END_COMBO));

  gtk_combo_box_set_active (combo, MOUSEPAD_SETTING_GET_ENUM (SMART_HOME_END));
}



/* update toolbar style setting when combo changes */
static void
mousepad_prefs_dialog_toolbar_style_changed (MousepadPrefsDialog *self,
                                             GtkComboBox         *combo)
{
  self->blocked = TRUE;
  MOUSEPAD_SETTING_SET_ENUM (TOOLBAR_STYLE, gtk_combo_box_get_active (combo));
  self->blocked = FALSE;
}



/* update the combo when the setting changes */
static void
mousepad_prefs_dialog_toolbar_style_setting_changed (MousepadPrefsDialog *self,
                                                     gchar               *key,
                                                     GSettings           *settings)
{
  GtkComboBox *combo;

  /* don't do anything when the combo box is itself updating the setting */
  if (self->blocked)
    return;

  combo = GTK_COMBO_BOX (gtk_builder_get_object (self->builder, WID_TOOLBAR_STYLE_COMBO));

  gtk_combo_box_set_active (combo, MOUSEPAD_SETTING_GET_ENUM (TOOLBAR_STYLE));
}



/* update toolbar icon size setting when combo changes */
static void
mousepad_prefs_dialog_toolbar_icon_size_changed (MousepadPrefsDialog *self,
                                                 GtkComboBox         *combo)
{
  GtkTreeIter iter;

  if (gtk_combo_box_get_active_iter (combo, &iter))
    {
      GtkTreeModel *model;
      gint          icon_size = 0;

      model = gtk_combo_box_get_model (combo);

      gtk_tree_model_get (model, &iter, 0, &icon_size, -1);

      self->blocked = TRUE;
      MOUSEPAD_SETTING_SET_ENUM (TOOLBAR_ICON_SIZE, icon_size);
      self->blocked = FALSE;
    }
}



/* update the combo when the setting changes */
static void
mousepad_prefs_dialog_toolbar_icon_size_setting_changed (MousepadPrefsDialog *self,
                                                         gchar               *key,
                                                         GSettings           *settings)
{
  GtkComboBox  *combo;
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gint          icon_size;
  gboolean      valid;

  /* don't do anything when the combo box is itself updating the setting */
  if (self->blocked)
    return;

  icon_size = MOUSEPAD_SETTING_GET_ENUM (TOOLBAR_ICON_SIZE);
  combo = GTK_COMBO_BOX (gtk_builder_get_object (self->builder, WID_TOOLBAR_ICON_SIZE_COMBO));
  model = gtk_combo_box_get_model (combo);
  valid = gtk_tree_model_get_iter_first (model, &iter);

  while (valid)
    {
      gint size = 0;
      gtk_tree_model_get (model, &iter, 0, &size, -1);
      if (size == icon_size)
        {
          gtk_combo_box_set_active_iter (combo, &iter);
          break;
        }
      valid = gtk_tree_model_iter_next (model, &iter);
    }
}



#define mousepad_builder_get_widget(builder, name) \
  GTK_WIDGET (gtk_builder_get_object (builder, name))



static void
mousepad_prefs_dialog_init (MousepadPrefsDialog *self)
{
  GError *error = NULL;
  GtkWidget *notebook;
  GtkWidget *content_area;
  GtkWidget *button;
  GtkWidget *check, *widget;

  self->builder = gtk_builder_new ();

  /* load the gtkbuilder xml that is compiled into the binary, or else die */
  if (! gtk_builder_add_from_string (self->builder,
                                     mousepad_prefs_dialog_ui,
                                     mousepad_prefs_dialog_ui_length,
                                     &error))
    {
      g_error ("Failed to load the internal preferences dialog: %s", error->message);
      /* not reached */
      g_error_free (error);
    }

  /* add the Glade/GtkBuilder notebook into this dialog */
  notebook = mousepad_builder_get_widget (self->builder, WID_NOTEBOOK);
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (self));
  gtk_container_add (GTK_CONTAINER (content_area), notebook);
  gtk_widget_show (notebook);

  /* add the close button */
  button = mousepad_util_image_button ("window-close", _("_Close"));
  gtk_widget_set_can_default (button, TRUE);
  gtk_dialog_add_action_widget (GTK_DIALOG (self), button, GTK_RESPONSE_CLOSE);
  gtk_dialog_set_default_response (GTK_DIALOG (self), GTK_RESPONSE_CLOSE);

  /* setup the window properties */
  gtk_window_set_title (GTK_WINDOW (self), _("Preferences"));
  gtk_window_set_icon_name (GTK_WINDOW (self), "preferences-desktop");

  /* enable/disable right margin spin button when checkbox is changed */
  check = mousepad_builder_get_widget (self->builder, WID_SHOW_RIGHT_MARGIN_CHECK);
  widget = mousepad_builder_get_widget (self->builder, WID_RIGHT_MARGIN_SPIN);
  g_object_bind_property (check, "active", widget, "sensitive", G_BINDING_SYNC_CREATE);

  /* enable/disable font chooser button when the default font checkbox is changed */
  check = mousepad_builder_get_widget (self->builder, WID_USE_DEFAULT_FONT_CHECK);
  widget = mousepad_builder_get_widget (self->builder, WID_FONT_BUTTON);
  g_object_bind_property (check, "active", widget, "sensitive", G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  /* setup tab mode combo box */
  widget = mousepad_builder_get_widget (self->builder, WID_TAB_MODE_COMBO);
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), MOUSEPAD_SETTING_GET_BOOLEAN (INSERT_SPACES));

  /* setup home/end keys combo box */
  widget = mousepad_builder_get_widget (self->builder, WID_SMART_HOME_END_COMBO);
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), MOUSEPAD_SETTING_GET_ENUM (SMART_HOME_END));

  /* enable/disable toolbar-related widgets when checkbox changes */
  check = mousepad_builder_get_widget (self->builder, WID_TOOLBAR_VISIBLE_CHECK);

  widget = mousepad_builder_get_widget (self->builder, WID_TOOLBAR_STYLE_LABEL);
  g_object_bind_property (check, "active", widget, "sensitive", G_BINDING_SYNC_CREATE);
  widget = mousepad_builder_get_widget (self->builder, WID_TOOLBAR_STYLE_COMBO);
  g_object_bind_property (check, "active", widget, "sensitive", G_BINDING_SYNC_CREATE);
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), MOUSEPAD_SETTING_GET_ENUM (TOOLBAR_STYLE));

  widget = mousepad_builder_get_widget (self->builder, WID_TOOLBAR_ICON_SIZE_LABEL);
  g_object_bind_property (check, "active", widget, "sensitive", G_BINDING_SYNC_CREATE);
  widget = mousepad_builder_get_widget (self->builder, WID_TOOLBAR_ICON_SIZE_COMBO);
  g_object_bind_property (check, "active", widget, "sensitive", G_BINDING_SYNC_CREATE);
  mousepad_prefs_dialog_toolbar_icon_size_setting_changed (self, NULL, NULL);

  /* bind checkboxes to settings */
#define BIND_CHECKBOX(setting)                                           \
  MOUSEPAD_SETTING_BIND (setting,                                        \
                         gtk_builder_get_object (self->builder,          \
                                                 WID_##setting##_CHECK), \
                         "active",                                       \
                         G_SETTINGS_BIND_DEFAULT)

  /* View */
  BIND_CHECKBOX (SHOW_LINE_NUMBERS);
  BIND_CHECKBOX (SHOW_WHITESPACE);
  BIND_CHECKBOX (SHOW_LINE_ENDINGS);
  BIND_CHECKBOX (SHOW_RIGHT_MARGIN);
  BIND_CHECKBOX (HIGHLIGHT_CURRENT_LINE);
  BIND_CHECKBOX (WORD_WRAP);
  BIND_CHECKBOX (USE_DEFAULT_FONT);
  BIND_CHECKBOX (MATCH_BRACES);

  /* Editor */
  BIND_CHECKBOX (AUTO_INDENT);

  /* Window */
  BIND_CHECKBOX (STATUSBAR_VISIBLE);
  BIND_CHECKBOX (PATH_IN_TITLE);
  BIND_CHECKBOX (REMEMBER_SIZE);
  BIND_CHECKBOX (REMEMBER_POSITION);
  BIND_CHECKBOX (REMEMBER_STATE);
  BIND_CHECKBOX (ALWAYS_SHOW_TABS);
  BIND_CHECKBOX (CYCLE_TABS);
  BIND_CHECKBOX (TOOLBAR_VISIBLE);

#undef BIND_CHECKBOX

  /* bind the right-margin-position setting to the spin button */
  MOUSEPAD_SETTING_BIND (RIGHT_MARGIN_POSITION,
                         gtk_builder_get_object (self->builder, WID_RIGHT_MARGIN_SPIN),
                         "value",
                         G_SETTINGS_BIND_DEFAULT);

  /* bind the font button font-name to the font-name setting */
  MOUSEPAD_SETTING_BIND (FONT_NAME,
                         gtk_builder_get_object (self->builder, WID_FONT_BUTTON),
                         "font-name",
                         G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

  mousepad_prefs_dialog_setup_color_schemes_combo (self);


  /* bind the tab-width spin button to the setting */
  MOUSEPAD_SETTING_BIND (TAB_WIDTH,
                         gtk_builder_get_object (self->builder, WID_TAB_WIDTH_SPIN),
                         "value",
                         G_SETTINGS_BIND_DEFAULT);

  /* update tab-mode when changed */
  g_signal_connect_swapped (gtk_builder_get_object (self->builder, WID_TAB_MODE_COMBO),
                            "changed",
                            G_CALLBACK (mousepad_prefs_dialog_tab_mode_changed),
                            self);

  /* update tab mode combo when setting changes */
  MOUSEPAD_SETTING_CONNECT_OBJECT (INSERT_SPACES,
                                   G_CALLBACK (mousepad_prefs_dialog_tab_mode_setting_changed),
                                   self,
                                   G_CONNECT_SWAPPED);

  /* update home/end when changed */
  g_signal_connect_swapped (gtk_builder_get_object (self->builder, WID_SMART_HOME_END_COMBO),
                            "changed",
                            G_CALLBACK (mousepad_prefs_dialog_home_end_changed),
                            self);

  /* update home/end combo when setting changes */
  MOUSEPAD_SETTING_CONNECT_OBJECT (SMART_HOME_END,
                                   G_CALLBACK (mousepad_prefs_dialog_home_end_setting_changed),
                                   self,
                                   G_CONNECT_SWAPPED);

  /* update toolbar style when changed */
  g_signal_connect_swapped (gtk_builder_get_object (self->builder, WID_TOOLBAR_STYLE_COMBO),
                            "changed",
                            G_CALLBACK (mousepad_prefs_dialog_toolbar_style_changed),
                            self);

  /* update toolbar style combo when the setting changes */
  MOUSEPAD_SETTING_CONNECT_OBJECT (TOOLBAR_STYLE,
                                   G_CALLBACK (mousepad_prefs_dialog_toolbar_style_setting_changed),
                                   self,
                                   G_CONNECT_SWAPPED);

  /* update toolbar icon size when changed */
  g_signal_connect_swapped (gtk_builder_get_object (self->builder, WID_TOOLBAR_ICON_SIZE_COMBO),
                            "changed",
                            G_CALLBACK (mousepad_prefs_dialog_toolbar_icon_size_changed),
                            self);

  /* update toolbar icon size combo when setting changes */
  MOUSEPAD_SETTING_CONNECT_OBJECT (TOOLBAR_ICON_SIZE,
                                   G_CALLBACK (mousepad_prefs_dialog_toolbar_icon_size_setting_changed),
                                   self,
                                   G_CONNECT_SWAPPED);
}



GtkWidget *
mousepad_prefs_dialog_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_PREFS_DIALOG, NULL);
}
