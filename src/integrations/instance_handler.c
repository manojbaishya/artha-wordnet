/* instance_handler.c
 * Artha - Free cross-platform open thesaurus
 * Copyright (c) 2009 Sundaram Ramaswamy, legends2k@yahoo.com
 *
 * Artha is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Artha is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Artha; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


/*
 * Code to handle multiple instances using GDBus (org.artha.unique)
 */


#ifdef DBUS_AVAILABLE

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "integrations/tomboyutil.h"

#define STR_UNIQUE_BUS_NAME		"org.artha.unique"
#define STR_DUPLICATE_OBJECT_PATH	"/org/artha/duplicate"
#define STR_DUPLICATE_INTERFACE_NAME	"org.artha.duplicate"
#define STR_SIGNAL_NAME			"duplicate_instance_invoked"

/* org.freedesktop.DBus RequestName reply codes */
#define DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER	1
#define DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER	4
#define DBUS_NAME_FLAG_DO_NOT_QUEUE		4

static GDBusConnection *bus = NULL;
static guint		  signal_sub_id = 0;

static void signal_receiver(GDBusConnection *connection,
			    const gchar *sender_name,
			    const gchar *object_path,
			    const gchar *interface_name,
			    const gchar *signal_name,
			    GVariant *parameters,
			    gpointer user_data)
{
	(void) connection;
	(void) sender_name;
	(void) object_path;
	(void) interface_name;
	(void) signal_name;
	(void) parameters;

	/* present the unique instance window to the user
	when a duplicate instance was invoked */
	tomboy_window_present_hardcore(GTK_WINDOW(user_data));
}

void instance_handler_send_signal(void)
{
	GError *err = NULL;

	if(bus == NULL)
		return;

	g_dbus_connection_emit_signal(bus,
				      "org.artha.unique",
				      STR_DUPLICATE_OBJECT_PATH,
				      STR_DUPLICATE_INTERFACE_NAME,
				      STR_SIGNAL_NAME,
				      NULL,
				      &err);
	if(err != NULL)
	{
		g_warning("D-Bus error emitting %s: %s", STR_SIGNAL_NAME, err->message);
		g_clear_error(&err);
	}
}

gboolean instance_handler_am_i_unique(void)
{
	GError *err = NULL;
	GVariant *reply = NULL;
	guint32	   result = 0;

	bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
	if(bus == NULL)
	{
		/* degrade to plain multi-instance, never abort the app */
		g_warning("D-Bus session bus unavailable (%s); running without single-instance support",
			  err != NULL ? err->message : "unknown error");
		g_clear_error(&err);
		return TRUE;
	}

	reply = g_dbus_connection_call_sync(bus,
					     "org.freedesktop.DBus",
					     "/org/freedesktop/DBus",
					     "org.freedesktop.DBus",
					     "RequestName",
					     g_variant_new("(su)", STR_UNIQUE_BUS_NAME,
							    DBUS_NAME_FLAG_DO_NOT_QUEUE),
					     G_VARIANT_TYPE("(u)"),
					     G_DBUS_CALL_FLAGS_NONE,
					     -1,
					     NULL,
					     &err);
	if(reply == NULL)
	{
		g_warning("D-Bus RequestName failed (%s); running without single-instance support",
			  err != NULL ? err->message : "unknown error");
		g_clear_error(&err);
		g_object_unref(bus);
		bus = NULL;
		return TRUE;
	}

	g_variant_get(reply, "(u)", &result);
	g_variant_unref(reply);

	/* PRIMARY_OWNER 1 or ALREADY_OWNER 4 mean we hold the name;
	   anything else (name taken, IN_QUEUE impossible with DO_NOT_QUEUE)
	   means a duplicate instance is being invoked. */
	return (result == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER ||
		result == DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER);
}

gboolean instance_handler_register_signal(GtkWindow *window)
{
	g_assert(bus != NULL);

	signal_sub_id = g_dbus_connection_signal_subscribe(bus,
						       "org.artha.unique",
						       STR_DUPLICATE_INTERFACE_NAME,
						       STR_SIGNAL_NAME,
						       STR_DUPLICATE_OBJECT_PATH,
						       NULL,
						       G_DBUS_SIGNAL_FLAGS_NONE,
						       signal_receiver,
						       window,
						       NULL);
	return (signal_sub_id != 0);
}

#endif		/* DBUS_AVAILABLE */
