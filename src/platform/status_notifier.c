/* status_notifier.c
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
 * Minimal org.kde.StatusNotifierItem implementation over raw GDBus.
 *
 * No right-click context menu is offered (the legacy GtkStatusIcon
 * popup with _Hide/_Quit was dropped): GTK4 has no portable menu API
 * for tray items and stock GNOME has no StatusNotifierWatcher at all.
 * Activation (left click) is the only supported interaction.
 */

#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>

#include "platform/status_notifier.h"

#define SNI_BUS_NAME		"org.artha.StatusNotifierItem"
#define SNI_OBJECT_PATH		"/StatusNotifierItem"
#define SNI_INTERFACE_NAME	"org.kde.StatusNotifierItem"
#define WATCHER_BUS_NAME	"org.kde.StatusNotifierWatcher"
#define WATCHER_OBJECT_PATH	"/StatusNotifierWatcher"
#define WATCHER_INTERFACE_NAME	"org.kde.StatusNotifierWatcher"

#define SNI_ICON_NAME		"artha"
#define SNI_TITLE		"Artha"
#define SNI_ID			"artha"
#define SNI_CATEGORY		"ApplicationStatus"

static const gchar introspect_xml[] =
	"<node>"
	"  <interface name=\"org.kde.StatusNotifierItem\">"
	"    <property name=\"Category\" type=\"s\" access=\"read\"/>"
	"    <property name=\"Id\" type=\"s\" access=\"read\"/>"
	"    <property name=\"Title\" type=\"s\" access=\"read\"/>"
	"    <property name=\"Status\" type=\"s\" access=\"read\"/>"
	"    <property name=\"WindowId\" type=\"u\" access=\"read\"/>"
	"    <property name=\"IconName\" type=\"s\" access=\"read\"/>"
	"    <property name=\"IconPixmap\" type=\"a(iiay)\" access=\"read\"/>"
	"    <property name=\"OverlayIconName\" type=\"s\" access=\"read\"/>"
	"    <property name=\"ItemIsMenu\" type=\"b\" access=\"read\"/>"
	"    <method name=\"Activate\"><arg name=\"x\" type=\"i\" direction=\"in\"/><arg name=\"y\" type=\"i\" direction=\"in\"/></method>"
	"    <method name=\"SecondaryActivate\"><arg name=\"x\" type=\"i\" direction=\"in\"/><arg name=\"y\" type=\"i\" direction=\"in\"/></method>"
	"    <method name=\"Scroll\"><arg name=\"delta\" type=\"i\" direction=\"in\"/><arg name=\"orientation\" type=\"s\" direction=\"in\"/></method>"
	"    <method name=\"ProvideXdgActivationToken\"><arg name=\"token\" type=\"s\" direction=\"in\"/></method>"
	"    <method name=\"ContextMenu\"></method>"
	"    <signal name=\"NewIcon\"/>"
	"    <signal name=\"NewToolTip\"/>"
	"    <signal name=\"NewStatus\"><arg name=\"status\" type=\"s\"/></signal>"
	"  </interface>"
	"</node>";

static GDBusConnection	*sni_bus = NULL;
static guint		 sni_registration_id = 0;
static GVariant		*sni_pixmap = NULL;	/* cached a(iiay) */
static gchar		*sni_status = NULL;	/* Status property */
static StatusNotifierActivateCallback sni_activate_cb = NULL;
static gpointer		 sni_activate_data = NULL;

/* Build a normalized ARGB32-big-endian pixmap variant from a PNG file.
   Returns NULL when the file cannot be decoded. */
static GVariant *load_pixmap(const gchar *icon_file)
{
	GdkPixbuf *pixbuf = NULL, *converted = NULL;
	gint width = 0, height = 0, channels = 0, rowstride = 0;
	const guchar *pixels = NULL;
	guchar *argb = NULL;
	gint x, y;
	gsize offset = 0;
	GVariantBuilder builder;
	GVariant *fixed_array, *pixmap;

	pixbuf = gdk_pixbuf_new_from_file(icon_file, NULL);
	if(pixbuf == NULL)
		return NULL;

	if(!gdk_pixbuf_get_has_alpha(pixbuf))
	{
		converted = gdk_pixbuf_add_alpha(pixbuf, FALSE, 0, 0, 0);
		g_object_unref(pixbuf);
		pixbuf = converted;
		converted = NULL;
	}

	width    = gdk_pixbuf_get_width(pixbuf);
	height   = gdk_pixbuf_get_height(pixbuf);
	channels = gdk_pixbuf_get_n_channels(pixbuf);
	rowstride = gdk_pixbuf_get_rowstride(pixbuf);
	pixels   = gdk_pixbuf_get_pixels(pixbuf);

	argb = g_malloc0((gsize) width * height * 4);
	for(y = 0; y < height; y++)
	{
		const guchar *row = pixels + (gsize) y * rowstride;
		for(x = 0; x < width; x++)
		{
			const guchar *px = row + (gsize) x * channels;

			/* SNI wants big-endian A, R, G, B per pixel */
			argb[offset++] = px[3];	/* A */
			argb[offset++] = px[0];	/* R */
			argb[offset++] = px[1];	/* G */
			argb[offset++] = px[2];	/* B */
		}
	}

	g_variant_builder_init(&builder, G_VARIANT_TYPE("a(iiay)"));
	fixed_array = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, argb,
						    (gsize) width * height * 4, 1);
	g_variant_builder_add(&builder, "(ii@ay)", width, height, fixed_array);
	pixmap = g_variant_builder_end(&builder);

	g_free(argb);
	g_object_unref(pixbuf);
	return g_variant_ref_sink(pixmap);
}

static void sni_method_call(GDBusConnection *connection,
			     const gchar *sender,
			     const gchar *object_path,
			     const gchar *interface_name,
			     const gchar *method_name,
			     GVariant *parameters,
			     GDBusMethodInvocation *invocation,
			     gpointer user_data)
{
	(void) connection;
	(void) sender;
	(void) object_path;
	(void) interface_name;
	(void) parameters;
	(void) user_data;

	if(g_str_equal(method_name, "Activate") ||
	   g_str_equal(method_name, "SecondaryActivate"))
	{
		if(sni_activate_cb != NULL)
			sni_activate_cb(sni_activate_data);
		g_dbus_method_invocation_return_value(invocation, NULL);
		return;
	}

	/* Scroll / ProvideXdgActivationToken / ContextMenu:
	   accepted and ignored by design. */
	g_dbus_method_invocation_return_value(invocation, NULL);
}

static GVariant *sni_get_property(GDBusConnection *connection,
				      const gchar *sender,
				      const gchar *object_path,
				      const gchar *interface_name,
				      const gchar *property_name,
				      GError **error,
				      gpointer user_data)
{
	(void) connection;
	(void) sender;
	(void) object_path;
	(void) interface_name;
	(void) error;
	(void) user_data;

	if(g_str_equal(property_name, "Category"))
		return g_variant_new_string(SNI_CATEGORY);
	if(g_str_equal(property_name, "Id"))
		return g_variant_new_string(SNI_ID);
	if(g_str_equal(property_name, "Title"))
		return g_variant_new_string(SNI_TITLE);
	if(g_str_equal(property_name, "Status"))
		return g_variant_new_string(sni_status != NULL
					     ? sni_status : "Active");
	if(g_str_equal(property_name, "WindowId"))
		return g_variant_new_uint32(0);
	if(g_str_equal(property_name, "IconName"))
		return g_variant_new_string(SNI_ICON_NAME);
	if(g_str_equal(property_name, "IconPixmap"))
	{
		GVariantBuilder empty;

		if(sni_pixmap != NULL)
			return g_variant_ref(sni_pixmap);

		g_variant_builder_init(&empty, G_VARIANT_TYPE("a(iiay)"));
		return g_variant_builder_end(&empty);
	}
	if(g_str_equal(property_name, "OverlayIconName"))
		return g_variant_new_string("");
	if(g_str_equal(property_name, "ItemIsMenu"))
		return g_variant_new_boolean(FALSE);

	return NULL;
}

static const GDBusInterfaceVTable sni_vtable = {
	sni_method_call,
	sni_get_property,
	NULL	/* set_property - all properties are read-only */
};

static void emit_status_changed(const gchar *status)
{
	GVariant *properties = NULL;
	GVariantBuilder builder;

	if(sni_bus == NULL)
		return;

	g_dbus_connection_emit_signal(sni_bus,
				       NULL,
				       SNI_OBJECT_PATH,
				       SNI_INTERFACE_NAME,
				       "NewStatus",
				       g_variant_new("(s)", status),
				       NULL);

	g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
	g_variant_builder_add(&builder, "{sv}",
			       "Status",
			       g_variant_new_string(status));
	properties = g_variant_builder_end(&builder);
	g_dbus_connection_emit_signal(sni_bus,
				       NULL,
				       SNI_OBJECT_PATH,
				       "org.freedesktop.DBus.Properties",
				       "PropertiesChanged",
				       g_variant_new("(s@a{sv}@as)",
						     SNI_INTERFACE_NAME,
						     properties,
						     g_variant_new_strv(NULL, 0)),
				       NULL);
}

gboolean status_notifier_setup(const gchar *icon_file,
				    StatusNotifierActivateCallback activate_cb,
				gpointer user_data)
{
	GDBusNodeInfo *introspection = NULL;
	GError *err = NULL;
	GVariant *reply = NULL;
	gboolean registered = FALSE;
	const gchar *unique_name = NULL;

	sni_activate_cb = activate_cb;
	sni_activate_data = user_data;

	if(sni_bus != NULL)
		return TRUE;

	sni_bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
	if(sni_bus == NULL)
	{
		g_warning("StatusNotifierItem: session bus unavailable (%s); no tray icon",
			  err != NULL ? err->message : "unknown error");
		g_clear_error(&err);
		return FALSE;
	}

	if(icon_file != NULL)
		sni_pixmap = load_pixmap(icon_file);

	sni_status = g_strdup("Active");
	introspection = g_dbus_node_info_new_for_xml(introspect_xml, &err);
	if(introspection == NULL)
	{
		g_warning("StatusNotifierItem: introspection parse failed: %s",
			  err != NULL ? err->message : "unknown error");
		g_clear_error(&err);
		g_object_unref(sni_bus);
		sni_bus = NULL;
		return FALSE;
	}

	sni_registration_id = g_dbus_connection_register_object(
			sni_bus,
			SNI_OBJECT_PATH,
			g_dbus_node_info_lookup_interface(introspection,
							     SNI_INTERFACE_NAME),
			&sni_vtable,
			NULL,
			NULL,
			&err);
	g_dbus_node_info_unref(introspection);

	if(sni_registration_id == 0)
	{
		g_warning("StatusNotifierItem: object registration failed: %s",
			  err != NULL ? err->message : "unknown error");
		g_clear_error(&err);
		g_object_unref(sni_bus);
		sni_bus = NULL;
		return FALSE;
	}

	unique_name = g_dbus_connection_get_unique_name(sni_bus);
	reply = g_dbus_connection_call_sync(sni_bus,
					     WATCHER_BUS_NAME,
					     WATCHER_OBJECT_PATH,
					     WATCHER_INTERFACE_NAME,
					     "RegisterStatusNotifierItem",
					     g_variant_new("(s)", unique_name),
					     G_VARIANT_TYPE("()"),
					     G_DBUS_CALL_FLAGS_NONE,
					     3000,	/* ms - a slow watcher must not hang artha */
					     NULL,
					     &err);

	if(reply != NULL)
	{
		g_variant_unref(reply);
		registered = TRUE;
	}
	else
	{
		/* No watcher running (stock GNOME): the application works,
		   the tray icon is simply absent. */
		g_warning("StatusNotifierWatcher not available (%s); no tray icon",
			  err != NULL ? err->message : "unknown error");
		g_clear_error(&err);
		g_dbus_connection_unregister_object(sni_bus, sni_registration_id);
		sni_registration_id = 0;
		g_object_unref(sni_bus);
		sni_bus = NULL;
		g_clear_pointer(&sni_status, g_free);
		if(sni_pixmap != NULL)
			g_variant_unref(sni_pixmap);
		sni_pixmap = NULL;
		return FALSE;
	}

	return registered;
}

void status_notifier_set_visible(gboolean visible)
{
	const gchar *status = visible ? "Active" : "Passive";

	if(g_strcmp0(sni_status, status) == 0)
		return;

	g_free(sni_status);
	sni_status = g_strdup(status);
	emit_status_changed(status);
}

void status_notifier_cleanup(void)
{
	if(sni_bus != NULL)
	{
		if(sni_registration_id != 0)
			g_dbus_connection_unregister_object(sni_bus,
							    sni_registration_id);
		g_object_unref(sni_bus);
	}

	sni_bus = NULL;
	sni_registration_id = 0;
	g_clear_pointer(&sni_status, g_free);
	if(sni_pixmap != NULL)
		g_variant_unref(sni_pixmap);
	sni_pixmap = NULL;
	sni_activate_cb = NULL;
	sni_activate_data = NULL;
}
