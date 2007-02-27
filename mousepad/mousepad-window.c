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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <mousepad/mousepad-application.h>
#include <mousepad/mousepad-preferences.h>
#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-file.h>
#include <mousepad/mousepad-screen.h>
#include <mousepad/mousepad-view.h>
#include <mousepad/mousepad-tab-label.h>
#include <mousepad/mousepad-window.h>
#include <mousepad/mousepad-window-ui.h>



static void              mousepad_window_class_init                   (MousepadWindowClass    *klass);
static void              mousepad_window_init                         (MousepadWindow         *window);
static void              mousepad_window_dispose                      (GObject                *object);
static void              mousepad_window_finalize                     (GObject                *object);
static gboolean          mousepad_window_configure_event              (GtkWidget              *widget,
                                                                       GdkEventConfigure      *event);
static gboolean          mousepad_window_save_geometry_timer          (gpointer                user_data);
static void              mousepad_window_save_geometry_timer_destroy  (gpointer                user_data);
static MousepadScreen   *mousepad_window_get_active                   (MousepadWindow         *window);
static gboolean          mousepad_window_save_as                      (MousepadWindow         *window,
                                                                       MousepadScreen         *screen);
static void              mousepad_window_add                          (MousepadWindow         *window,
                                                                       MousepadScreen         *screen);
static gboolean          mousepad_window_close_screen                 (MousepadWindow         *window,
                                                                       MousepadScreen         *screen);
static void              mousepad_window_create_recent_menu           (MousepadWindow         *window);
static void              mousepad_window_add_recent                   (const gchar            *filename);
static void              mousepad_window_set_title                    (MousepadWindow         *window,
                                                                       MousepadScreen         *screen);
static void              mousepad_window_notify_title                 (MousepadScreen         *screen,
                                                                       GParamSpec             *pspec,
                                                                       MousepadWindow         *window);
static void              mousepad_window_page_notified                (GtkNotebook            *notebook,
                                                                       GParamSpec             *pspec,
                                                                       MousepadWindow         *window);
static void              mousepad_window_tab_removed                  (GtkNotebook            *notebook,
                                                                       MousepadScreen         *screen,
                                                                       MousepadWindow         *window);
static void              mousepad_window_error_dialog                 (GtkWindow              *window,
                                                                       GError                 *error,
                                                                       const gchar            *message);
static void              mousepad_window_update_actions               (MousepadWindow         *window);
static void              mousepad_window_rebuild_gomenu               (MousepadWindow         *window);
static void              mousepad_window_selection_changed            (MousepadScreen         *screen,
                                                                       gboolean                selected,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_open_new_window       (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_open_file             (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_open_recent           (GtkRecentChooser       *chooser,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_save_file             (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_save_file_as          (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_close_tab             (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_button_close_tab             (MousepadScreen         *screen);
static gboolean          mousepad_window_delete_event                 (MousepadWindow         *window,
                                                                       GdkEvent               *event);
static void              mousepad_window_action_close                 (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_close_all_windows     (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_open_new_tab          (GtkAction              *action,
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
static void              mousepad_window_action_jump_to               (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_select_font           (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_prev_tab              (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_next_tab              (GtkAction              *action,
                                                                       MousepadWindow         *window);
static void              mousepad_window_action_about                 (GtkAction              *action,
                                                                       MousepadWindow         *window);




struct _MousepadWindowClass
{
  GtkWindowClass __parent__;
};

struct _MousepadWindow
{
  GtkWindow            __parent__;

  MousepadPreferences *preferences;

  GtkActionGroup      *action_group;
  GtkUIManager        *ui_manager;

  GtkWidget           *notebook;

  /* support to remember window geometry */
  gint                 save_geometry_timer_id;
};



static const GtkActionEntry action_entries[] =
{
  { "file-menu", NULL, N_ ("_File"), NULL, NULL, NULL, },
  { "new-tab", "tab-new", N_ ("New _Tab"), "<control>T", N_ ("Open a new Mousepad tab"), G_CALLBACK (mousepad_window_action_open_new_tab), },
  { "new-window", "window-new", N_ ("_New Window"), "<control>N", N_ ("Open a new Mousepad window"), G_CALLBACK (mousepad_window_action_open_new_window), },
  { "open-file", GTK_STOCK_OPEN, N_ ("_Open File"), NULL, N_ ("Open a new document"), G_CALLBACK (mousepad_window_action_open_file), },
  { "recent-menu", NULL, N_ ("Open _Recent"), NULL, NULL, NULL, },
  { "save-file", GTK_STOCK_SAVE, N_ ("_Save"), NULL, N_ ("Save current document"), G_CALLBACK (mousepad_window_action_save_file), },
  { "save-file-as", GTK_STOCK_SAVE_AS, N_ ("Save _As"), NULL, N_ ("Save current document as another file"), G_CALLBACK (mousepad_window_action_save_file_as), },
  { "close-tab", GTK_STOCK_CLOSE, N_ ("C_lose Tab"), "<control>W", N_ ("Close the current Mousepad tab"), G_CALLBACK (mousepad_window_action_close_tab), },
  { "close-window", GTK_STOCK_QUIT, N_ ("_Close Window"), "<control>Q", N_ ("Close the Mousepad window"), G_CALLBACK (mousepad_window_action_close), },
  { "close-all-windows", NULL, N_ ("Close _All Windows"), "<control><shift>W", N_ ("Close all Mousepad windows"), G_CALLBACK (mousepad_window_action_close_all_windows), },

  { "edit-menu", NULL, N_ ("_Edit"), NULL, NULL, NULL, },
  { "undo", GTK_STOCK_UNDO, N_ ("_Undo"), NULL, NULL, NULL, },
  { "redo", GTK_STOCK_REDO, N_ ("_Redo"), NULL, NULL, NULL, },
  { "cut", GTK_STOCK_CUT, N_ ("Cu_t"), NULL, NULL, G_CALLBACK (mousepad_window_action_cut), },
  { "copy", GTK_STOCK_COPY, N_ ("_Copy"), NULL, NULL, G_CALLBACK (mousepad_window_action_copy), },
  { "paste", GTK_STOCK_PASTE, N_ ("_Paste"), NULL, NULL, G_CALLBACK (mousepad_window_action_paste), },
  { "delete", GTK_STOCK_DELETE, N_ ("_Delete"), NULL, NULL, G_CALLBACK (mousepad_window_action_delete), },
  { "select-all", GTK_STOCK_SELECT_ALL, N_ ("Select _All"), NULL, NULL, G_CALLBACK (mousepad_window_action_select_all), },

  { "search-menu", NULL, N_ ("_Search"), NULL, NULL, NULL, },
  { "find", GTK_STOCK_FIND, N_ ("_Find"), NULL, NULL, NULL, },
  { "find-next", NULL, N_ ("Find _Next"), NULL, NULL, NULL, },
  { "find-previous", NULL, N_ ("Find _Previous"), NULL, NULL, NULL, },
  { "replace", GTK_STOCK_FIND_AND_REPLACE, N_ ("_Replace"), NULL, NULL, NULL, },
  { "jump-to", GTK_STOCK_JUMP_TO, N_ ("_Jump To"), NULL, NULL, G_CALLBACK (mousepad_window_action_jump_to), },

  { "view-menu", NULL, N_ ("_View"), NULL, NULL, NULL, },
  { "font", GTK_STOCK_SELECT_FONT, N_ ("_Font"), NULL, N_ ("Choose Mousepad font"), G_CALLBACK (mousepad_window_action_select_font), },

  { "go-menu", NULL, N_ ("_Go"), NULL, },
  { "back", GTK_STOCK_GO_BACK, N_ ("Back"), NULL, N_ ("Select the previous tab"), G_CALLBACK (mousepad_window_action_prev_tab), },
  { "forward", GTK_STOCK_GO_FORWARD, N_ ("Forward"), NULL, N_ ("Select the next tab"), G_CALLBACK (mousepad_window_action_next_tab), },

  { "help-menu", NULL, N_ ("_Help"), NULL, },
  { "about", GTK_STOCK_ABOUT, N_ ("_About"), NULL,N_  ("Display information about Mousepad"), G_CALLBACK (mousepad_window_action_about), },
};

static const GtkToggleActionEntry toggle_action_entries[] =
{
  { "word-wrap", NULL, N_ ("_Word Wrap"), NULL, NULL, NULL, FALSE, },
  { "line-numbers", NULL, N_ ("_Line Numbers"), NULL, NULL, NULL, FALSE, },
  { "auto-indent", NULL, N_ ("_Auto Indent"), NULL, NULL, NULL, FALSE, },
};



static GObjectClass *mousepad_window_parent_class;



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
  GtkWidget     *vbox;
  GtkWidget     *label;
  GtkWidget     *separator;
  GtkWidget     *ebox;
  GtkAction     *action;
  gint           width, height;

  window->preferences = mousepad_preferences_get ();

  /* signal for monitoring */
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
  window->action_group = gtk_action_group_new ("MousepadActionGroup");
  gtk_action_group_set_translation_domain (window->action_group, GETTEXT_PACKAGE);
  gtk_action_group_add_actions (window->action_group, action_entries, G_N_ELEMENTS (action_entries), GTK_WIDGET (window));
  gtk_action_group_add_toggle_actions (window->action_group, toggle_action_entries, G_N_ELEMENTS (toggle_action_entries), GTK_WIDGET (window));

  /* create mutual bindings for the toggle buttons */
  action = gtk_action_group_get_action (window->action_group, "line-numbers");
  exo_mutual_binding_new (G_OBJECT (window->preferences), "line-numbers", G_OBJECT (action), "active");
  action = gtk_action_group_get_action (window->action_group, "auto-indent");
  exo_mutual_binding_new (G_OBJECT (window->preferences), "auto-indent", G_OBJECT (action), "active");
  action = gtk_action_group_get_action (window->action_group, "word-wrap");
  exo_mutual_binding_new (G_OBJECT (window->preferences), "word-wrap", G_OBJECT (action), "active");

  window->ui_manager = gtk_ui_manager_new ();
  gtk_ui_manager_insert_action_group (window->ui_manager, window->action_group, 0);
  gtk_ui_manager_add_ui_from_string (window->ui_manager, mousepad_window_ui, mousepad_window_ui_length, NULL);

  /* append the recent-menu items */
  mousepad_window_create_recent_menu (window);

  accel_group = gtk_ui_manager_get_accel_group (window->ui_manager);
  gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show (vbox);

  menubar = gtk_ui_manager_get_widget (window->ui_manager, "/main-menu");
  gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, FALSE, 0);
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
      gtk_box_pack_start (GTK_BOX (vbox), ebox, FALSE, FALSE, 0);
      gtk_widget_show (ebox);

      /* add the label with the root warning */
      label = gtk_label_new (_("Warning, you are using the root account, you may harm your system."));
      gtk_misc_set_padding (GTK_MISC (label), 6, 3);
      gtk_container_add (GTK_CONTAINER (ebox), label);
      gtk_widget_show (label);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, FALSE, 0);
      gtk_widget_show (separator);
    }

  window->notebook = g_object_new (GTK_TYPE_NOTEBOOK,
                                   "homogeneous", FALSE,
                                   "scrollable", TRUE,
                                   "show-border", FALSE,
                                   "show-tabs", FALSE,
                                   "tab-hborder", 0,
                                   "tab-vborder", 0,
                                   NULL);

  g_signal_connect (G_OBJECT (window->notebook), "notify::page",
                    G_CALLBACK (mousepad_window_page_notified), window);
  g_signal_connect (G_OBJECT (window->notebook), "remove",
                    G_CALLBACK (mousepad_window_tab_removed), window);

  gtk_box_pack_start (GTK_BOX (vbox), window->notebook, TRUE, TRUE, 0);
  gtk_widget_show (window->notebook);
}



static void
mousepad_window_dispose (GObject *object)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (object);

  /* destroy the save geometry timer source */
  if (G_UNLIKELY (window->save_geometry_timer_id > 0))
    g_source_remove (window->save_geometry_timer_id);

  (*G_OBJECT_CLASS (mousepad_window_parent_class)->dispose) (object);
}



static void
mousepad_window_finalize (GObject *object)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (object);

  g_object_unref (G_OBJECT (window->ui_manager));
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

  /* let Gtk+ handle the configure event */
  return (*GTK_WIDGET_CLASS (mousepad_window_parent_class)->configure_event) (widget, event);
}



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



static MousepadScreen *
mousepad_window_get_active (MousepadWindow *window)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (window->notebook);
  gint         page_num;

  page_num = gtk_notebook_get_current_page (notebook);

  if (G_LIKELY (page_num >= 0))
    return MOUSEPAD_SCREEN (gtk_notebook_get_nth_page (notebook, page_num));
  else
    return NULL;
}



gboolean
mousepad_window_open_tab (MousepadWindow *window,
                          const gchar    *filename)
{
  GtkWidget   *screen;
  GError      *error = NULL;
  gboolean     succeed = TRUE;
  gint         npages = 0, i;
  const gchar *opened_filename;

  /* get the number of page in the notebook */
  if (filename)
    npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook)) - 1;

  /* walk though the tabs */
  for (i = 0; i < npages; i++)
    {
      screen = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i);

      if (G_LIKELY (screen != NULL))
        {
          /* get the filename */
          opened_filename = mousepad_screen_get_filename (MOUSEPAD_SCREEN (screen));

          /* see if the file is already opened */
          if (exo_str_is_equal (filename, opened_filename))
            {
              /* switch to the tab */
              gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), i);

              /* and we're done */
              return TRUE;
            }
        }
    }

  /* new screen */
  screen = mousepad_screen_new ();

  /* load a file, if there is any */
  if (filename)
    succeed = mousepad_screen_open_file (MOUSEPAD_SCREEN (screen), filename, &error);

  if (G_LIKELY (succeed))
    mousepad_window_add (window, MOUSEPAD_SCREEN (screen));
  else
    {
      /* something went wrong, remove the screen */
      gtk_widget_destroy (GTK_WIDGET (screen));

      /* show the warning */
      mousepad_window_error_dialog (GTK_WINDOW (window), error, _("Failed to open file"));
      g_error_free (error);
    }

  return succeed;
}



void
mousepad_window_open_files (MousepadWindow  *window,
                            const gchar     *working_directory,
                            gchar          **filenames)
{
  gint   n;
  gchar *filename = NULL;

  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  _mousepad_return_if_fail (working_directory != NULL);
  _mousepad_return_if_fail (filenames != NULL);
  _mousepad_return_if_fail (*filenames != NULL);

  /* walk through all the filenames */
  for (n = 0; filenames[n] != NULL; ++n)
    {
      /* check if the filename looks like an uri */
      if (g_strstr_len (filenames[n], 5, "file:"))
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
}



static GtkFileChooserConfirmation
mousepad_window_save_as_callback (GtkFileChooser *dialog)
{
  gchar                      *filename;
  GError                     *error = NULL;
  GtkFileChooserConfirmation  result;

  /* get the filename */
  filename = gtk_file_chooser_get_filename (dialog);

  if (mousepad_file_is_writable (filename, &error))
    {
      /* show the normal confirmation dialog */
      result = GTK_FILE_CHOOSER_CONFIRMATION_CONFIRM;
    }
  else
    {
      /* tell the user he cannot write to this file */
      mousepad_window_error_dialog (GTK_WINDOW (dialog), error, _("Permission denied"));
      g_error_free (error);

      /* the user doesn't have permission to write to the file */
      result = GTK_FILE_CHOOSER_CONFIRMATION_SELECT_AGAIN;
    }

  /* cleanup */
  g_free (filename);

  return result;
}



static gboolean
mousepad_window_save_as (MousepadWindow *window,
                         MousepadScreen *screen)
{
  GtkWidget   *dialog;
  gboolean     succeed = FALSE;
  gchar       *filename;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);
  _mousepad_return_val_if_fail (MOUSEPAD_IS_SCREEN (screen), FALSE);

  dialog = gtk_file_chooser_dialog_new (_("Save As"),
                                        GTK_WINDOW (window),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_SAVE, GTK_RESPONSE_OK,
                                        NULL);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), TRUE);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  /* we check if the user is allowed to write to the file */
  g_signal_connect (G_OBJECT (dialog), "confirm-overwrite",
                                G_CALLBACK (mousepad_window_save_as_callback), NULL);

  /* set the current file, or the temp name */
  if (mousepad_screen_get_filename (screen) != NULL)
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), mousepad_screen_get_filename (screen));
  else
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), mousepad_screen_get_title (screen, FALSE));

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
      /* get the filename the user has selected */
      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

      if (G_LIKELY (filename != NULL))
        {
          /* set the filename */
          mousepad_screen_set_filename (screen, filename);

          /* cleanup */
          g_free (filename);

          /* it worked */
          succeed = TRUE;
        }
    }

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  return succeed;
}



static void
mousepad_window_modified_changed (MousepadScreen *screen,
                                  MousepadWindow *window)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  _mousepad_return_if_fail (MOUSEPAD_IS_SCREEN (screen));

  mousepad_window_set_title (window, screen);
}



static void
mousepad_window_add (MousepadWindow *window,
                     MousepadScreen *screen)
{
  GtkWidget      *label;
  gboolean        always_show_tabs;
  gint            page, npages;
  MousepadScreen *active;

  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  _mousepad_return_if_fail (MOUSEPAD_IS_SCREEN (screen));

  /* connect some bindings to the screen */
  exo_binding_new (G_OBJECT (window->preferences), "line-numbers", G_OBJECT (screen), "line-numbers");
  exo_binding_new (G_OBJECT (window->preferences), "word-wrap", G_OBJECT (screen), "word-wrap");
  exo_binding_new (G_OBJECT (window->preferences), "auto-indent", G_OBJECT (screen), "auto-indent");
  exo_binding_new (G_OBJECT (window->preferences), "font-name", G_OBJECT (screen), "font-name");

  label = mousepad_tab_label_new ();
  gtk_widget_show (label);

  /* add the window to the screen */
  g_object_set_data (G_OBJECT (screen), I_("mousepad-window"), window);

  /* connect some bindings and signals to the tab label */
  exo_binding_new (G_OBJECT (screen), "title", G_OBJECT (label), "title");
  exo_binding_new (G_OBJECT (screen), "filename", G_OBJECT (label), "tooltip");
  g_signal_connect_swapped (G_OBJECT (label), "close-tab", G_CALLBACK (mousepad_window_button_close_tab), screen);
  g_signal_connect (G_OBJECT (screen), "notify::title", G_CALLBACK (mousepad_window_notify_title), window);

  g_signal_connect (G_OBJECT (screen), "selection-changed", G_CALLBACK (mousepad_window_selection_changed), window);
  g_signal_connect (G_OBJECT (screen), "modified-changed", G_CALLBACK (mousepad_window_modified_changed), window);

  /* append the page to the window */
  page = gtk_notebook_append_page (GTK_NOTEBOOK (window->notebook),
                                   GTK_WIDGET (screen), label);

  /* check if we should always display tabs */
  g_object_get (G_OBJECT (window->preferences), "misc-always-show-tabs", &always_show_tabs, NULL);

  /* change the visibility of the tabs accordingly */
  npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (window->notebook), always_show_tabs || (npages > 1));

  /* don't focus the notebook */
  GTK_WIDGET_UNSET_FLAGS (window->notebook, GTK_CAN_FOCUS);

  /* rebuild the go menu */
  mousepad_window_rebuild_gomenu (window);

  /* show the screen */
  gtk_widget_show (GTK_WIDGET (screen));

  /* get the active tab */
  active = mousepad_window_get_active (window);

  /* switch to the new tab */
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), page);

  /* destroy the (old) active tab if it's empty
   * and the new tab is not */
  if (active != NULL &&
      mousepad_screen_get_modified (active) == FALSE &&
      mousepad_screen_get_filename (active) == NULL &&
      mousepad_screen_get_filename (screen) != NULL)
    gtk_widget_destroy (GTK_WIDGET (active));
}



static gboolean
mousepad_window_close_screen (MousepadWindow *window,
                              MousepadScreen *screen)
{
  GtkWidget *dialog;
  gint       response;
  gboolean   succeed = FALSE;
  GError    *error = NULL;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);
  _mousepad_return_val_if_fail (MOUSEPAD_IS_SCREEN (screen), FALSE);

  /* check if the document needs to be saved */
  if (mousepad_screen_get_modified (screen))
    {
      /* create the question dialog */
      dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_QUESTION,
                                       GTK_BUTTONS_NONE,
                                       _("Do you want to save changes to this\ndocument before closing?"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                _("If you don't save, your changes will be lost."));
      gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                              _("_Don't Save"), GTK_RESPONSE_HELP,
                              GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                              GTK_STOCK_SAVE, GTK_RESPONSE_OK,
                              NULL);
      gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

      /* run the dialog */
      response = gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);

      switch (response)
        {
          case GTK_RESPONSE_HELP:
            /* don't save, only destroy the screen */
            succeed = TRUE;
            break;
          case GTK_RESPONSE_CANCEL:
            /* we do nothing */
            break;
          case GTK_RESPONSE_OK:
            /* save the screen */
            if (mousepad_screen_get_filename (screen) == NULL)
              {
                /* file has no name yet, save as */
                if (mousepad_window_save_as (window, screen))
                  succeed = mousepad_screen_save_file (screen, &error);
              }
            else
              {
                succeed = mousepad_screen_save_file (screen, &error);
              }
            break;
        }
    }
  else
    {
      /* no changes in the screen, safe to destroy it */
      succeed = TRUE;
    }

  /* destroy the screen */
  if (succeed)
    {
      gtk_widget_destroy (GTK_WIDGET (screen));
    }
  else if (error != NULL)
    {
      /* something went wrong with saving the file */
      mousepad_window_error_dialog (GTK_WINDOW (window), error, _("Failed to save file"));
      g_error_free (error);
    }

  return succeed;
}



static void
mousepad_window_create_recent_menu (MousepadWindow *window)
{
  GtkRecentFilter *filter;
  GtkWidget       *menu;
  GtkWidget       *item;
  gint             recent_menu_limit;

  _mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* get the recent menu item */
  item = gtk_ui_manager_get_widget (window->ui_manager, "/main-menu/file-menu/recent-menu");

  /* create a new recent chooser */
  menu = gtk_recent_chooser_menu_new ();

  /* show only Mousepad items */
  filter = gtk_recent_filter_new ();
  gtk_recent_filter_add_application (filter, "Mousepad Text Editor");
  gtk_recent_chooser_add_filter (GTK_RECENT_CHOOSER (menu), filter);

  /* get the recent menu limit number */
  g_object_get (G_OBJECT (window->preferences), "misc-recent-menu-limit", &recent_menu_limit, NULL);

  /* set recent menu properties */
  gtk_recent_chooser_set_local_only (GTK_RECENT_CHOOSER (menu), TRUE);
  gtk_recent_chooser_set_limit (GTK_RECENT_CHOOSER (menu), recent_menu_limit);
  gtk_recent_chooser_set_show_tips (GTK_RECENT_CHOOSER (menu), TRUE);

  g_signal_connect (G_OBJECT (menu), "item-activated",
                    G_CALLBACK(mousepad_window_action_open_recent), window);

  /* append the recent menu */
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
}



static void
mousepad_window_add_recent (const gchar *filename)
{
  GtkRecentData     info;
  gchar            *uri;
  GtkRecentManager *manager = gtk_recent_manager_get_default ();

  _mousepad_return_if_fail (filename != NULL);

  /* create the recent data */
  info.display_name = NULL;
  info.description  = NULL;
  info.mime_type    = "text/plain";
  info.app_name     = "Mousepad Text Editor";
  info.app_exec     = "Mousepad %F";
  info.groups       = NULL;
  info.is_private   = FALSE;

  /* create an uri from the filename */
  uri = g_filename_to_uri (filename, NULL, NULL);

  /* add it */
  if (G_LIKELY (uri != NULL))
    {
      gtk_recent_manager_add_full (manager, uri, &info);
      g_free (uri);
    }
}



static void
mousepad_window_set_title (MousepadWindow *window,
                           MousepadScreen *screen)
{
  gchar       *string;
  const gchar *title;
  gboolean     show_full_path;
  GtkTextView *textview;

  /* get the active textview */
  textview = mousepad_screen_get_text_view (screen);

  /* whether to show the full path in the window title */
  g_object_get (G_OBJECT (window->preferences), "misc-show-full-path-in-title", &show_full_path, NULL);

  /* the window title */
  title = mousepad_screen_get_title (screen, show_full_path);

  if (!gtk_text_view_get_editable (textview))
    {
      string = g_strdup_printf ("%s [%s] - %s",
                                title, _("Read Only"), PACKAGE_NAME);
    }
  else
    {
      string = g_strdup_printf ("%s%s - %s",
                                mousepad_screen_get_modified (screen) ? "*" : "",
                                title, PACKAGE_NAME);
    }

  /* get the window title */
  gtk_window_set_title (GTK_WINDOW (window), string);

  /* cleanup */
  g_free (string);
}



static void
mousepad_window_notify_title (MousepadScreen *screen,
                              GParamSpec     *pspec,
                              MousepadWindow *window)
{
  mousepad_window_set_title (window, screen);
}



static void
mousepad_window_page_notified (GtkNotebook    *notebook,
                               GParamSpec     *pspec,
                               MousepadWindow *window)
{
  MousepadScreen *screen;

  screen = mousepad_window_get_active (window);
  if (G_LIKELY (screen != NULL))
    {
      mousepad_window_set_title (window, screen);

      mousepad_window_update_actions (window);
    }
}



static void
mousepad_window_tab_removed (GtkNotebook    *notebook,
                             MousepadScreen *screen,
                             MousepadWindow *window)
{
  gboolean always_show_tabs;
  gint     npages;

  npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

  if (G_UNLIKELY (npages == 0))
    {
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
      mousepad_window_rebuild_gomenu (window);

      /* update all screen sensitive actions */
      mousepad_window_update_actions (window);
    }
}



static void
mousepad_window_error_dialog (GtkWindow   *window,
                              GError      *error,
                              const gchar *message)
{
  GtkWidget   *dialog;

  _mousepad_return_if_fail (GTK_IS_WINDOW (window));

  /* create the warning dialog */
  dialog = gtk_message_dialog_new (window,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   message);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);

  /* set the message from GError */
  if (G_LIKELY (error && error->message))
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              error->message);

  /* run the dialog */
  gtk_dialog_run (GTK_DIALOG (dialog));

  /* destroy it */
  gtk_widget_destroy (dialog);

}



static void
mousepad_window_selection_changed (MousepadScreen *screen,
                                   gboolean        selected,
                                   MousepadWindow *window)
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
mousepad_window_update_actions (MousepadWindow *window)
{
  GtkAction        *action;
  GtkNotebook      *notebook = GTK_NOTEBOOK (window->notebook);
  MousepadScreen   *screen;
  GtkTextView      *textview;
  GtkTextBuffer    *buffer;
  GtkWidget        *goitem;
  gboolean          cycle_tabs;
  gint              n_pages;
  gint              page_num;
  gboolean          selected;

  /* determine the number of pages */
  n_pages = gtk_notebook_get_n_pages (notebook);

  /* set sensitivity for the "Close Tab" action */
  action = gtk_action_group_get_action (window->action_group, "close-tab");
  gtk_action_set_sensitive (action, (n_pages > 1));

  /* update the actions for the current terminal screen */
  screen = mousepad_window_get_active (window);
  if (G_LIKELY (screen != NULL))
    {
      page_num = gtk_notebook_page_num (notebook, GTK_WIDGET (screen));

      textview = mousepad_screen_get_text_view (screen);
      buffer = mousepad_screen_get_text_buffer (screen);

      g_object_get (G_OBJECT (window->preferences), "misc-cycle-tabs", &cycle_tabs, NULL);

      action = gtk_action_group_get_action (window->action_group, "back");
      gtk_action_set_sensitive (action, (cycle_tabs && n_pages > 1) || (page_num > 0));

      action = gtk_action_group_get_action (window->action_group, "forward");
      gtk_action_set_sensitive (action, (cycle_tabs && n_pages > 1 ) || (page_num < n_pages - 1));

      action = gtk_action_group_get_action (window->action_group, "save-file");
      gtk_action_set_sensitive (action, gtk_text_view_get_editable (textview));

      g_object_get (G_OBJECT (buffer), "has-selection", &selected, NULL);
      mousepad_window_selection_changed (screen, selected, window);

      /* update the "Go" menu */
      goitem = g_object_get_data (G_OBJECT (screen), "go-menu-item");
      if (G_LIKELY (goitem != NULL))
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (goitem), TRUE);
    }
}


static void
mousepad_window_action_goto (GtkRadioAction *action,
                             GtkNotebook    *notebook)
{
  gint page = gtk_radio_action_get_current_value (action);

  gtk_notebook_set_current_page (notebook, page);
}



static void
mousepad_window_rebuild_gomenu (MousepadWindow *window)
{
  GtkWidget      *screen;
  gint            npages;
  gint            n;
  gchar           name[15];
  gchar           accelerator[7];
  GtkRadioAction *action;
  GSList         *group = NULL;
  const gchar    *go_menu_path;
  static guint    merge_id = 0;

  /* remove the old merge */
  if (merge_id > 1)
    gtk_ui_manager_remove_ui (window->ui_manager, merge_id);

  /* create a new merge id */
  merge_id = gtk_ui_manager_new_merge_id (window->ui_manager);

  /* the path in the ui where the items will be added */
  go_menu_path = "/main-menu/go-menu/placeholder-go-items";

  /* walk through the notebook pages */
  npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));
  for (n = 0; n < npages; ++n)
    {
      screen = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), n);

      /* create a new action name */
      g_snprintf (name, sizeof (name), "document-%d", n);

      /* create the radio action */
      action = gtk_radio_action_new (name, NULL, NULL, NULL, n);
      exo_binding_new (G_OBJECT (screen), "title", G_OBJECT (action), "label");
      gtk_radio_action_set_group (action, group);
      group = gtk_radio_action_get_group (action);
      g_signal_connect (G_OBJECT (action), "activate",
                        G_CALLBACK (mousepad_window_action_goto), window->notebook);

      /* connect the action to the screen to we can easily active it when the user toggled the tabs */
      g_object_set_data (G_OBJECT (screen), I_("go-menu-item"), action);

      if (G_LIKELY (n < 9))
        {
          /* create an accelerator and add it to the menu */
          g_snprintf (accelerator, sizeof (accelerator), "<Alt>%d", n+1);
          gtk_action_group_add_action_with_accel (window->action_group, GTK_ACTION (action), accelerator);
        }
      else
        /* add a menu item without accelerator */
        gtk_action_group_add_action (window->action_group, GTK_ACTION (action));

      /* release the action */
      g_object_unref (G_OBJECT (action));

      /* add the action to the go menu */
      gtk_ui_manager_add_ui (window->ui_manager, merge_id,
                             go_menu_path, name, name,
                             GTK_UI_MANAGER_MENUITEM, FALSE);
    }

  /* make sure the ui is up2date to avoid flickering */
  gtk_ui_manager_ensure_update (window->ui_manager);
}


/**
 * Menu Actions
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

      /* open the selected files */
      for (li = filenames; li != NULL; li = li->next)
        {
          filename = li->data;

          /* open the file in a new tab */
          mousepad_window_open_tab (window, filename);

          /* add to the recent files */
          mousepad_window_add_recent (filename);

          /* cleanup */
          g_free (filename);
        }

      /* free the list */
      g_slist_free(filenames);
    }

  /* destroy dialog */
  gtk_widget_destroy (chooser);
}



static void
mousepad_window_action_open_recent (GtkRecentChooser *chooser,
                                    MousepadWindow   *window)
{
  gchar            *uri;
  gchar            *filename;
  gboolean          succeed;
  GtkRecentManager *manager;

  /* get the file uri */
  uri = gtk_recent_chooser_get_current_uri (chooser);

  if (G_LIKELY(uri != NULL))
    {
      /* get the filename from the uri */
      filename = g_filename_from_uri (uri, NULL, NULL);

      if (G_LIKELY (filename != NULL))
        {
          /* open a new tab */
          succeed = mousepad_window_open_tab (window, filename);

          /* remove the file from the recent menu if Mousepad failed to open it */
          if (G_UNLIKELY (succeed == FALSE))
            {
              manager = gtk_recent_manager_get_default ();
              gtk_recent_manager_remove_item (manager, uri, NULL);
            }

          /* cleanup */
          g_free (filename);
        }

      /* cleanup */
      g_free (uri);
    }
}



static void
mousepad_window_action_save_file (GtkAction      *action,
                                  MousepadWindow *window)
{
  MousepadScreen *screen;
  GError         *error = NULL;

  screen = mousepad_window_get_active (window);

  if (G_LIKELY (screen != NULL))
    {
      if (mousepad_screen_get_filename (screen) != NULL)
        mousepad_screen_save_file (screen, &error);
      else
        {
          if (mousepad_window_save_as (window, screen))
            mousepad_screen_save_file (screen, &error);
        }

      /* check for errors */
      if (G_UNLIKELY (error != NULL))
        {
          mousepad_window_error_dialog (GTK_WINDOW (window), error, _("Failed to save file"));
          g_error_free (error);
        }
    }
}



static void
mousepad_window_action_save_file_as (GtkAction      *action,
                                     MousepadWindow *window)
{
  MousepadScreen *screen;
  GError         *error = NULL;

  screen = mousepad_window_get_active (window);

  if (G_LIKELY (screen != NULL))
    {
      /* popup the save as dialog, if that worked, save the file, if
       * that worked too, add the (new) file to the recent menu */
      if (mousepad_window_save_as (window, screen))
        if (mousepad_screen_save_file (screen, &error))
          mousepad_window_add_recent (mousepad_screen_get_filename (screen));

      /* check for errors */
      if (G_UNLIKELY (error != NULL))
        {
          mousepad_window_error_dialog (GTK_WINDOW (window), error,  _("Failed to save file as"));
          g_error_free (error);
        }
      else
        {
          /* make sure save-file is sensitive */
          action = gtk_action_group_get_action (window->action_group, "save-file");
          gtk_action_set_sensitive (action, TRUE);
        }
    }
}



static void
mousepad_window_action_close_tab (GtkAction      *action,
                                  MousepadWindow *window)
{
  MousepadScreen *screen;

  screen = mousepad_window_get_active (window);

  if (G_LIKELY (screen != NULL))
    mousepad_window_close_screen (window, screen);
}



static void
mousepad_window_button_close_tab (MousepadScreen *screen)
{
  MousepadWindow *window;

  _mousepad_return_if_fail (MOUSEPAD_IS_SCREEN (screen));

  /* get the mousepad window of this screen */
  window = g_object_get_data (G_OBJECT (screen), I_("mousepad-window"));

  /* close the screen */
  if (G_LIKELY (window != NULL))
    mousepad_window_close_screen (window, screen);
}



static gboolean
mousepad_window_delete_event (MousepadWindow *window,
                              GdkEvent       *event)
{
  /* try to close the windows */
  mousepad_window_action_close (NULL, window);

  /* we will close the window when it's time */
  return TRUE;
}



static void
mousepad_window_action_close (GtkAction      *action,
                              MousepadWindow *window)
{
  gint       npages, i;
  GtkWidget *screen;

  /* get the number of page in the notebook */
  npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook)) - 1;

  /* close all the tabs, one by one. the window will close
   * when all the tabs are closed */
  for (i = npages; i >= 0; --i)
    {
      screen = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i);

      if (G_LIKELY (screen != NULL))
        {
          /* switch to the tab we're going to close */
          gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), i);

          /* ask user what to do, break when he/she hits the cancel button */
          if (!mousepad_window_close_screen (window, MOUSEPAD_SCREEN (screen)))
            break;
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
mousepad_window_action_cut (GtkAction      *action,
                            MousepadWindow *window)
{
  MousepadScreen *screen;
  GtkTextBuffer  *buffer;
  GtkClipboard   *clipboard;
  GtkTextView    *textview;

  /* get the active screen */
  screen = mousepad_window_get_active (window);

  if (G_LIKELY (screen != NULL))
    {
      /* get the clipboard, textview and the buffer */
      buffer = mousepad_screen_get_text_buffer (screen);
      textview = mousepad_screen_get_text_view (screen);
      clipboard = gtk_widget_get_clipboard (GTK_WIDGET (textview), GDK_SELECTION_CLIPBOARD);

      /* cut the text */
      gtk_text_buffer_cut_clipboard (buffer, clipboard,
                                     gtk_text_view_get_editable (textview));

      /* scroll to visible area */
      gtk_text_view_scroll_to_mark (textview,
                                    gtk_text_buffer_get_insert (buffer),
                                    MOUSEPAD_SCROLL_MARGIN,
                                    FALSE, 0.0, 0.0);
    }
}



static void
mousepad_window_action_copy (GtkAction      *action,
                             MousepadWindow *window)
{
  MousepadScreen *screen;
  GtkTextBuffer  *buffer;
  GtkClipboard   *clipboard;

  screen = mousepad_window_get_active (window);

  if (G_LIKELY (screen != NULL))
    {
      /* get the buffer, textview and the clipboard */
      buffer = mousepad_screen_get_text_buffer (screen);
      clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window), GDK_SELECTION_CLIPBOARD);

      /* copy the selected text */
      gtk_text_buffer_copy_clipboard (buffer, clipboard);
    }
}



static void
mousepad_window_action_paste (GtkAction      *action,
                              MousepadWindow *window)
{
  MousepadScreen *screen;
  GtkTextBuffer  *buffer;
  GtkClipboard   *clipboard;
  GtkTextView    *textview;

  screen = mousepad_window_get_active (window);

  if (G_LIKELY (screen != NULL))
    {
      /* get the buffer and the clipboard */
      buffer = mousepad_screen_get_text_buffer (screen);
      textview = mousepad_screen_get_text_view (screen);
      clipboard = gtk_widget_get_clipboard (GTK_WIDGET (textview), GDK_SELECTION_CLIPBOARD);

      /* paste the clipboard content */
      gtk_text_buffer_paste_clipboard (buffer, clipboard, NULL,
                                       gtk_text_view_get_editable (textview));

      /* scroll to visible area */
      gtk_text_view_scroll_to_mark (textview,
                                    gtk_text_buffer_get_insert (buffer),
                                    MOUSEPAD_SCROLL_MARGIN,
                                    FALSE, 0.0, 0.0);
    }
}



static void
mousepad_window_action_delete (GtkAction      *action,
                               MousepadWindow *window)
{
  MousepadScreen *screen;
  GtkTextBuffer  *buffer;
  GtkTextView    *textview;

  screen = mousepad_window_get_active (window);

  if (G_LIKELY (screen != NULL))
    {
      /* get the buffer */
      buffer = mousepad_screen_get_text_buffer (screen);
      textview = mousepad_screen_get_text_view (screen);

      /* delete the selected text */
      gtk_text_buffer_delete_selection (buffer, TRUE,
                                        gtk_text_view_get_editable (textview));

      /* scroll to visible area */
      gtk_text_view_scroll_to_mark (textview,
                                    gtk_text_buffer_get_insert (buffer),
                                    MOUSEPAD_SCROLL_MARGIN,
                                    FALSE, 0.0, 0.0);
    }
}



static void
mousepad_window_action_select_all (GtkAction      *action,
                                   MousepadWindow *window)
{

  MousepadScreen *screen;
  GtkTextBuffer  *buffer;
  GtkTextIter     start, end;

  screen = mousepad_window_get_active (window);

  if (G_LIKELY (screen != NULL))
    {
      /* get the buffer */
      buffer = mousepad_screen_get_text_buffer (screen);

      /* get the start and end iter */
      gtk_text_buffer_get_bounds (buffer, &start, &end);

      /* select everything between those iters */
      gtk_text_buffer_select_range (buffer, &start, &end);
    }
}



static void
mousepad_window_action_jump_to (GtkAction      *action,
                                MousepadWindow *window)
{
  GtkWidget      *dialog;
  GtkWidget      *hbox, *label, *button;
  GtkTextBuffer  *buffer;
  MousepadScreen *screen;
  gint            num, num_max;
  GtkTextIter     iter;
  GtkAdjustment   *adjustment;


  screen = mousepad_window_get_active (window);

  if (G_LIKELY (screen != NULL))
    {
      buffer = mousepad_screen_get_text_buffer (screen);

      /* get the current line number */
      gtk_text_buffer_get_iter_at_mark (buffer, &iter, gtk_text_buffer_get_insert (buffer));
      num = gtk_text_iter_get_line (&iter) + 1;

      /* get the last line number */
      gtk_text_buffer_get_end_iter (buffer, &iter);
      num_max = gtk_text_iter_get_line (&iter) + 1;

      /* build the dialog */
      dialog = gtk_dialog_new_with_buttons (_("Jump To"),
                                            GTK_WINDOW (window),
                                            GTK_DIALOG_MODAL
                                            | GTK_DIALOG_DESTROY_WITH_PARENT
                                            | GTK_DIALOG_NO_SEPARATOR,
                                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                            GTK_STOCK_JUMP_TO, GTK_RESPONSE_OK,
                                            NULL);
      gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
      gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

      hbox = gtk_hbox_new (FALSE, 8);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, FALSE, FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
      gtk_widget_show (hbox);

      label = gtk_label_new_with_mnemonic (_("_Line number:"));
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
      gtk_widget_show (label);

      /* the spin button adjustment */
      adjustment = (GtkAdjustment *) gtk_adjustment_new (num, 1, num_max, 1, 10, 0);

      /* the spin button */
      button = gtk_spin_button_new (adjustment, 1, 0);
      gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (button), TRUE);
      gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (button), TRUE);
      gtk_entry_set_width_chars (GTK_ENTRY (button), 8);
      gtk_entry_set_activates_default (GTK_ENTRY (button), TRUE);
      gtk_label_set_mnemonic_widget (GTK_LABEL(label), button);
      gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
      gtk_widget_show (button);

      if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
        {
          /* jump to the line */
          num = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (button)) - 1;
          gtk_text_buffer_get_iter_at_line (buffer, &iter, num);
          gtk_text_buffer_place_cursor (buffer, &iter);

          /* scroll to visible area */
          gtk_text_view_scroll_to_mark (mousepad_screen_get_text_view (screen),
                                        gtk_text_buffer_get_insert (buffer),
                                        MOUSEPAD_SCROLL_MARGIN,
                                        FALSE, 0.0, 0.0);
        }

      /* destroy the dialog */
      gtk_widget_destroy (dialog);
    }
}



static void
mousepad_window_action_select_font (GtkAction      *action,
                                    MousepadWindow *window)
{
  GtkWidget *dialog;
  gchar     *font_name;

  dialog = gtk_font_selection_dialog_new (_("Choose Mousepad Font"));
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));

  /* set the current font */
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
      g_free (font_name);
    }

  /* destroy dialog */
  gtk_widget_destroy (dialog);
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
mousepad_window_action_about (GtkAction      *action,
                              MousepadWindow *window)
{
  static const gchar *authors[] =
  {
    "Erik Harrison <erikharrison@xfce.org>",
    "Nick Schermer <nick@xfce.org>",
    "Benedikt Meurer <benny@xfce.org>",
    NULL,
  };

  /* show the dialog */
  gtk_about_dialog_set_email_hook (exo_url_about_dialog_hook, NULL, NULL);
  gtk_about_dialog_set_url_hook (exo_url_about_dialog_hook, NULL, NULL);
  gtk_show_about_dialog (GTK_WINDOW (window),
                         "authors", authors,
                         "comments", _("Mousepad is a fast text editor for the Xfce Desktop Environment."),
                         "copyright", _("Copyright \302\251 2004-2007 Xfce Development Team"),
                         "destroy-with-parent", TRUE,
                         "license", XFCE_LICENSE_GPL,
                         "logo-icon-name", PACKAGE_NAME,
                         "name", PACKAGE_NAME,
                         "version", PACKAGE_VERSION,
                         "translator-credits", _("translator-credits"),
                         "website", "http://www.xfce.org/projects/mousepad",
                         NULL);
}

