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

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-application.h>
#include <mousepad/mousepad-window.h>



static void mousepad_application_class_init       (MousepadApplicationClass  *klass);
static void mousepad_application_init             (MousepadApplication       *application);
static void mousepad_application_window_destroyed (GtkWidget                 *window,
                                                   MousepadApplication       *application);
static void mousepad_application_finalize         (GObject                   *object);



struct _MousepadApplicationClass
{
  GObjectClass __parent__;
};

struct _MousepadApplication
{
  GObject  __parent__;

  /* internal list of all the opened windows */
  GList   *windows;
};



static GObjectClass *mousepad_application_parent_class;



/**
 * mousepad_application_get:
 *
 * Returns the global shared #MousepadApplication instance.
 * This method takes a reference on the global instance
 * for the caller, so you must call g_object_unref()
 * on it when done.
 *
 * Return value: the shared #MousepadApplication instance.
 **/
MousepadApplication*
mousepad_application_get (void)
{
  static MousepadApplication *application = NULL;

  if (G_UNLIKELY (application == NULL))
    {
      application = g_object_new (MOUSEPAD_TYPE_APPLICATION, NULL);
    }
  else
    {
      g_object_ref (G_OBJECT (application));
    }

  return application;
}



GType
mousepad_application_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_type_register_static_simple (G_TYPE_OBJECT,
                                            I_("MousepadApplication"),
                                            sizeof (MousepadApplicationClass),
                                            (GClassInitFunc) mousepad_application_class_init,
                                            sizeof (MousepadApplication),
                                            (GInstanceInitFunc) mousepad_application_init,
                                            0);
    }

  return type;
}



static void
mousepad_application_class_init (MousepadApplicationClass *klass)
{
  GObjectClass *gobject_class;

  mousepad_application_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_application_finalize;
}



static void
mousepad_application_init (MousepadApplication *application)
{
  gchar *path;

  /* check if we have a saved accel map */
  path = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, PACKAGE_NAME "/accels.scm");
  if (G_LIKELY (path != NULL))
    {
      /* load the accel map */
      gtk_accel_map_load (path);
      g_free (path);
    }
}



static void
mousepad_application_finalize (GObject *object)
{
  MousepadApplication *application = MOUSEPAD_APPLICATION (object);
  GList               *li;
  gchar               *path;

  /* save the current accel map */
  path = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, PACKAGE_NAME "/accels.scm", TRUE);
  if (G_LIKELY (path != NULL))
    {
      /* save the accel map */
      gtk_accel_map_save (path);
      g_free (path);
    }

  /* destroy the windows if they are still opened */
  for (li = application->windows; li != NULL; li = li->next)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (li->data), G_CALLBACK (mousepad_application_window_destroyed), application);
      gtk_widget_destroy (GTK_WIDGET (li->data));
    }

  g_list_free (application->windows);

  (*G_OBJECT_CLASS (mousepad_application_parent_class)->finalize) (object);
}



/**
 * mousepad_application_get_windows:
 * @application: A #MousepadApplication.
 *
 * Returns a list of #MousepadWindows currently registered by the
 * #MousepadApplication. The returned list is owned by the caller and
 * must be freed using g_list_free().
 *
 * Return value: the list of regular #MousepadWindows in @application.
 **/
GList*
mousepad_application_get_windows (MousepadApplication *application)
{
  GList *windows = NULL;
  GList *li;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_APPLICATION (application), NULL);

  for (li = application->windows; li != NULL; li = li->next)
    if (G_LIKELY (MOUSEPAD_IS_WINDOW (li->data)))
      windows = g_list_prepend (windows, li->data);

  return windows;
}



/**
 * mousepad_application_has_windows:
 * @application : a #MousepadApplication.
 *
 * Returns %TRUE if @application controls atleast one window.
 *
 * Return value: %TRUE if @application controls atleast one window.
 **/
gboolean
mousepad_application_has_windows (MousepadApplication *application)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_APPLICATION (application), FALSE);

  return (application->windows != NULL);
}



/**
 * mousepad_application_open_window:
 * @application       : A #MousepadApplication.
 * @screen            : The #GdkScreen on which to open the window or %NULL
 *                      to open on the default screen.
 * @working_directory : The default working directory for this window.
 * @filenames         : A list of filenames we try to open in tabs. The file names
 *                      can either be absolute paths, supported URIs or relative file
 *                      names to @working_directory or %NULL for an untitled document.
 *
 * Opens a new Mousepad window and tries to open all the file names in tabs. If
 * @filename is %NULL an empty Untiled documenten will be opened in the window.
 **/
void
mousepad_application_open_window (MousepadApplication  *application,
                                  GdkScreen            *screen,
                                  const gchar          *working_directory,
                                  gchar               **filenames)
{
  GtkWidget *window;
  gboolean   succeed;

  _mousepad_return_if_fail (MOUSEPAD_IS_APPLICATION (application));
  _mousepad_return_if_fail (screen == NULL || GDK_IS_SCREEN (screen));

  /* create a new window */
  window = mousepad_window_new ();

  /* get the screen */
  if (G_UNLIKELY (screen == NULL))
    screen = gdk_screen_get_default ();

  /* move to the correct screen */
  gtk_window_set_screen (GTK_WINDOW (window), screen);

  /* open the filenames or an empty tab */
  if (filenames != NULL && *filenames != NULL)
    {
      /* try to open the files */
      succeed = mousepad_window_open_files (MOUSEPAD_WINDOW (window), working_directory, filenames);

      /* if we failed, open an empty tab */
      if (G_UNLIKELY (succeed == FALSE))
        mousepad_window_open_tab (MOUSEPAD_WINDOW (window), NULL);
    }
  else
    {
      mousepad_window_open_tab (MOUSEPAD_WINDOW (window), NULL);
    }

  /* connect to the "destroy" signal */
  g_signal_connect (G_OBJECT (window), "destroy",
                    G_CALLBACK (mousepad_application_window_destroyed), application);

  /* add the window to our internal list */
  application->windows = g_list_prepend (application->windows, window);

  gtk_widget_show (window);
}



/**
 * mousepad_application_window_destroyed:
 * @window      : The window that has been destroyed.
 * @application : A #MousepadApplication.
 *
 * This function removes @window from the registed windows in @application.
 * When there are no more windows left in @application, the application is
 * terminated.
 **/
static void
mousepad_application_window_destroyed (GtkWidget           *window,
                                       MousepadApplication *application)
{
  _mousepad_return_if_fail (GTK_IS_WINDOW (window));
  _mousepad_return_if_fail (MOUSEPAD_IS_APPLICATION (application));
  _mousepad_return_if_fail (g_list_find (application->windows, window) != NULL);

  /* remove the window from the list */
  application->windows = g_list_remove (application->windows, window);

  /* quit if there are no windows opened */
  if (application->windows == NULL)
    gtk_main_quit ();
}
