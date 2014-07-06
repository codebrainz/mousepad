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

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-settings.h>
#include <mousepad/mousepad-application.h>
#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-replace-dialog.h>
#include <mousepad/mousepad-window.h>



static void        mousepad_application_finalize                  (GObject                   *object);
static void        mousepad_application_window_destroyed          (GtkWidget                 *window,
                                                                   MousepadApplication        *application);
static GtkWidget  *mousepad_application_create_window             (MousepadApplication        *application);
static void        mousepad_application_new_window_with_document  (MousepadWindow             *existing,
                                                                   MousepadDocument           *document,
                                                                   gint                        x,
                                                                   gint                        y,
                                                                   MousepadApplication        *application);
static void        mousepad_application_new_window                (MousepadWindow             *existing,
                                                                   MousepadApplication        *application);



struct _MousepadApplicationClass
{
  GObjectClass __parent__;
};

struct _MousepadApplication
{
  GObject  __parent__;

  /* internal list of all the opened windows */
  GSList  *windows;
};



G_DEFINE_TYPE (MousepadApplication, mousepad_application, G_TYPE_OBJECT);



static void
mousepad_application_class_init (MousepadApplicationClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_application_finalize;
}



static void
mousepad_application_init (MousepadApplication *application)
{
  gchar *filename;

  /* check if we have a saved accel map */
  filename = mousepad_util_get_save_location (MOUSEPAD_ACCELS_RELPATH, FALSE);
  if (G_LIKELY (filename != NULL))
    {
      /* load the accel map */
      gtk_accel_map_load (filename);

      /* cleanup */
      g_free (filename);
    }

    /* Initialize the MousepadSettings singleton if not already initialized */
    MOUSEPAD_GSETTINGS_ONCE_INIT ();
}



static void
mousepad_application_finalize (GObject *object)
{
  MousepadApplication *application = MOUSEPAD_APPLICATION (object);
  GSList              *li;
  gchar               *filename;

  /* flush the history items of the replace dialog
   * this is a bit of an ugly place, but cleaning on a window close
   * isn't a good option eighter */
  mousepad_replace_dialog_history_clean ();

  /* save the current accel map */
  filename = mousepad_util_get_save_location (MOUSEPAD_ACCELS_RELPATH, TRUE);
  if (G_LIKELY (filename != NULL))
    {
      /* save the accel map */
      gtk_accel_map_save (filename);

      /* cleanup */
      g_free (filename);
    }

  /* destroy the windows if they are still opened */
  for (li = application->windows; li != NULL; li = li->next)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (li->data), G_CALLBACK (mousepad_application_window_destroyed), application);
      gtk_widget_destroy (GTK_WIDGET (li->data));
    }

  /* cleanup the list of windows */
  g_slist_free (application->windows);

  /* Flush GSettings to disk */
  MOUSEPAD_GSETTINGS_SYNC ();

  (*G_OBJECT_CLASS (mousepad_application_parent_class)->finalize) (object);
}



MousepadApplication*
mousepad_application_get (void)
{
  static MousepadApplication *application = NULL;

  if (G_UNLIKELY (application == NULL))
    {
      application = g_object_new (MOUSEPAD_TYPE_APPLICATION, NULL);
      g_object_add_weak_pointer (G_OBJECT (application), (gpointer) &application);
    }
  else
    {
      g_object_ref (G_OBJECT (application));
    }

  return application;
}



gboolean
mousepad_application_has_windows (MousepadApplication *application)
{
  mousepad_return_val_if_fail (MOUSEPAD_IS_APPLICATION (application), FALSE);

  return (application->windows != NULL);
}



static void
mousepad_application_window_destroyed (GtkWidget           *window,
                                       MousepadApplication *application)
{
  mousepad_return_if_fail (GTK_IS_WINDOW (window));
  mousepad_return_if_fail (MOUSEPAD_IS_APPLICATION (application));
  mousepad_return_if_fail (g_slist_find (application->windows, window) != NULL);

  /* remove the window from the list */
  application->windows = g_slist_remove (application->windows, window);

  /* quit if there are no windows opened */
  if (application->windows == NULL)
    gtk_main_quit ();
}



void
mousepad_application_take_window (MousepadApplication *application,
                                  GtkWindow           *window)
{
  mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  mousepad_return_if_fail (MOUSEPAD_IS_APPLICATION (application));
  mousepad_return_if_fail (g_slist_find (application->windows, window) == NULL);

  /* connect to the "destroy" signal */
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (mousepad_application_window_destroyed), application);

  /* add the window to our internal list */
  application->windows = g_slist_prepend (application->windows, window);
}



static GtkWidget *
mousepad_application_create_window (MousepadApplication *application)
{
  GtkWidget *window;

  /* create a new window */
  window = mousepad_window_new ();

  /* hook up the new window */
  mousepad_application_take_window (application, GTK_WINDOW (window));

  /* connect signals */
  g_signal_connect (G_OBJECT (window), "new-window-with-document", G_CALLBACK (mousepad_application_new_window_with_document), application);
  g_signal_connect (G_OBJECT (window), "new-window", G_CALLBACK (mousepad_application_new_window), application);

  return window;
}



static void
mousepad_application_new_window_with_document (MousepadWindow      *existing,
                                               MousepadDocument    *document,
                                               gint                 x,
                                               gint                 y,
                                               MousepadApplication *application)
{
  GtkWidget *window;
  GdkScreen *screen;

  mousepad_return_if_fail (MOUSEPAD_IS_WINDOW (existing));
  mousepad_return_if_fail (document == NULL || MOUSEPAD_IS_DOCUMENT (document));
  mousepad_return_if_fail (MOUSEPAD_IS_APPLICATION (application));

  /* create a new window (signals added and already hooked up) */
  window = mousepad_application_create_window (application);

  /* place the new window on the same screen as the existing window */
  screen = gtk_window_get_screen (GTK_WINDOW (existing));
  if (G_LIKELY (screen != NULL))
    gtk_window_set_screen (GTK_WINDOW (window), screen);

  /* move the window on valid cooridinates */
  if (x > -1 && y > -1)
    gtk_window_move (GTK_WINDOW (window), x, y);

  /* create an empty document if no document was send */
  if (document == NULL)
    document = mousepad_document_new ();

  /* add the document to the new window */
  mousepad_window_add (MOUSEPAD_WINDOW (window), document);

  /* show the window */
  gtk_widget_show (window);
}



static void
mousepad_application_new_window (MousepadWindow      *existing,
                                 MousepadApplication *application)
{
  /* trigger new document function */
  mousepad_application_new_window_with_document (existing, NULL, -1, -1, application);
}



void
mousepad_application_new_window_with_files (MousepadApplication  *application,
                                            GdkScreen            *screen,
                                            const gchar          *working_directory,
                                            gchar               **filenames)
{
  GtkWidget        *window;
  gboolean          succeed = FALSE;
  MousepadDocument *document;

  mousepad_return_if_fail (MOUSEPAD_IS_APPLICATION (application));
  mousepad_return_if_fail (screen == NULL || GDK_IS_SCREEN (screen));

  /* create a new window (signals added and already hooked up) */
  window = mousepad_application_create_window (application);

  /* place the window on the right screen */
  gtk_window_set_screen (GTK_WINDOW (window), screen ? screen : gdk_screen_get_default ());

  /* try to open the files */
  if (working_directory && filenames && g_strv_length (filenames))
    succeed = mousepad_window_open_files (MOUSEPAD_WINDOW (window), working_directory, filenames);

  /* open an empty document */
  if (succeed == FALSE)
    {
      /* create a new document */
      document = mousepad_document_new ();

      /* add the document to the new window */
      mousepad_window_add (MOUSEPAD_WINDOW (window), document);
    }

  /* show the window */
  gtk_widget_show (window);
}
