/* $Id$ */
/*
 * Copyright (c) 2004-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2007      Nick Schermer <nick@xfce.org>
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

#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>

#include <mousepad/mousepad-dbus.h>
#include <mousepad/mousepad-private.h>



static void      mousepad_dbus_service_class_init     (MousepadDBusServiceClass  *klass);
static void      mousepad_dbus_service_init           (MousepadDBusService       *dbus_service);
static void      mousepad_dbus_service_finalize       (GObject                   *object);
static gboolean  mousepad_dbus_service_launch_files   (MousepadDBusService       *dbus_service,
                                                       const gchar               *working_directory,
                                                       gchar                    **filenames,
                                                       GError                   **error);



struct _MousepadDBusServiceClass
{
  GObjectClass __parent__;
};

struct _MousepadDBusService
{
  GObject __parent__;

  DBusGConnection *connection;
};



static GObjectClass *mousepad_dbus_service_parent_class;



GType
mousepad_dbus_service_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_type_register_static_simple (G_TYPE_OBJECT,
                                            I_("MousepadDBusService"),
                                            sizeof (MousepadDBusServiceClass),
                                            (GClassInitFunc) mousepad_dbus_service_class_init,
                                            sizeof (MousepadDBusService),
                                            (GInstanceInitFunc) mousepad_dbus_service_init,
                                            0);
    }

  return type;
}



static void
mousepad_dbus_service_class_init (MousepadDBusServiceClass *klass)
{
  extern const DBusGObjectInfo  dbus_glib_mousepad_dbus_service_object_info;
  GObjectClass                 *gobject_class;

  /* determine the parent type class */
  mousepad_dbus_service_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_dbus_service_finalize;

  /* install the D-BUS info for our class */
  dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (klass), &dbus_glib_mousepad_dbus_service_object_info);
}



static void
mousepad_dbus_service_init (MousepadDBusService *dbus_service)
{
  GError *error = NULL;

  /* try to connect to the session bus */
  dbus_service->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

  if (G_LIKELY (dbus_service->connection != NULL))
    {
      /* register the /org/xfce/TextEditor object for Mousepad */
      dbus_g_connection_register_g_object (dbus_service->connection, "/org/xfce/Mousepad", G_OBJECT (dbus_service));

      /* request the org.xfce.Mousepad name for Mousepad */
      dbus_bus_request_name (dbus_g_connection_get_connection (dbus_service->connection), "org.xfce.Mousepad", DBUS_NAME_FLAG_REPLACE_EXISTING, NULL);
    }
  else
    {
#ifdef NDEBUG
      /* hide this warning when the user is root and debug is disabled */
      if (geteuid () != 0)
#endif
        {
          /* notify the user that D-BUS service won't be available */
          g_message ("Failed to connect to the D-BUS session bus: %s\n", error->message);
        }

      g_error_free (error);
    }
}



static void
mousepad_dbus_service_finalize (GObject *object)
{
  MousepadDBusService *dbus_service = MOUSEPAD_DBUS_SERVICE (object);

  /* release the D-BUS connection object */
  if (G_LIKELY (dbus_service->connection != NULL))
    dbus_g_connection_unref (dbus_service->connection);

  (*G_OBJECT_CLASS (mousepad_dbus_service_parent_class)->finalize) (object);
}



static gboolean
mousepad_dbus_service_launch_files (MousepadDBusService  *dbus_service,
                                    const gchar          *working_directory,
                                    gchar               **filenames,
                                    GError              **error)
{
  MousepadApplication *application;
   GdkScreen          *screen;

  _mousepad_return_val_if_fail (g_path_is_absolute (working_directory), FALSE);
  _mousepad_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  screen = gdk_screen_get_default ();

  application = mousepad_application_get ();
  mousepad_application_open_window (application, screen, working_directory, filenames);
  g_object_unref (G_OBJECT (application));

  return TRUE;
}



gboolean
mousepad_dbus_client_launch_files (gchar       **filenames,
                                   const gchar  *working_directory,
                                   GError      **error)
{
  DBusConnection *connection;
  DBusMessage    *message;
  DBusMessage    *result;
  DBusError       derror;

  _mousepad_return_val_if_fail (g_path_is_absolute (working_directory), FALSE);
  _mousepad_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  dbus_error_init (&derror);

  /* try to connect to the session bus */
  connection = dbus_bus_get (DBUS_BUS_SESSION, &derror);
  if (G_UNLIKELY (connection == NULL))
    {
      dbus_set_g_error (error, &derror);
      dbus_error_free (&derror);
      return FALSE;
    }

  /* generate the message */
  message = dbus_message_new_method_call ("org.xfce.Mousepad", "/org/xfce/Mousepad",
                                          "org.xfce.Mousepad", "LaunchFiles");
  dbus_message_set_auto_start (message, FALSE);
  dbus_message_append_args (message,
                            DBUS_TYPE_STRING, &working_directory,
                            DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &filenames, filenames ? g_strv_length (filenames) : 0,
                            DBUS_TYPE_INVALID);

  /* send the message */
  result = dbus_connection_send_with_reply_and_block (connection, message, -1, &derror);

  /* release ref */
  dbus_message_unref (message);

  /* check if no reply was received */
  if (result == NULL)
    {
#ifndef NDEBUG
      /* set the error when debug is enabled */
      dbus_set_g_error (error, &derror);
#endif
      dbus_error_free (&derror);
      return FALSE;
    }

  /* but maybe we received an error */
  if (dbus_message_get_type (result) == DBUS_MESSAGE_TYPE_ERROR)
    {
      dbus_set_error_from_message (&derror, result);
      dbus_set_g_error (error, &derror);
      dbus_message_unref (result);
      dbus_error_free (&derror);
      return FALSE;
    }

  /* it seems everything worked */
  dbus_message_unref (result);

  return TRUE;
}



#include <mousepad/mousepad-dbus-infos.h>

