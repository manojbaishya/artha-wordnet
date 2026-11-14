/* tomboyutil.c
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
 * gtk_window_present function doesn't always present the window to the
 * user; hence libtomboy's tomboy_window_present_hardcore function is
 * used with minor modifications.
 *
 * Credit for this code goes fully to Alex Graveley, alex@beatniksoftware.com
 *
 * GTK4/X11: raw Xlib client messages instead of GDK properties, and
 * no _NET_WM_USER_TIME override: the window manager tracks focus time
 * on its own in modern X11/EWMH implementations.
 */

#ifdef X11_AVAILABLE

#include <gdk/x11/gdkx.h>
#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string.h>

#ifndef TRACE
#define TRACE(x)                                                                                   \
    do {                                                                                           \
    } while (0);
#endif

/* _NET_WM_USER_TIME is intentionally not handled here anymore. */

static void tomboy_window_move_to_current_workspace(GtkWindow* window) {
    GdkSurface* surface = gtk_native_get_surface(GTK_NATIVE(window));
    GdkDisplay* gdk_dpy;
    Display* display;
    Window xroot, xwin;
    Atom net_current_desktop, net_wm_desktop, out_type;
    int out_format;
    unsigned long out_len, out_bytes_after;
    unsigned long* out_val;
    unsigned long workspace;
    XEvent xev;

    gdk_dpy = gdk_display_get_default();
    if (gdk_dpy == NULL || !GDK_IS_X11_DISPLAY(gdk_dpy))
        return;

    display = gdk_x11_display_get_xdisplay(gdk_dpy);
    xroot = DefaultRootWindow(display);
    xwin = surface != NULL ? gdk_x11_surface_get_xid(surface) : 0;
    if (xwin == 0)
        return;

    net_current_desktop = XInternAtom(display, "_NET_CURRENT_DESKTOP", False);
    net_wm_desktop = XInternAtom(display, "_NET_WM_DESKTOP", False);

    if (XGetWindowProperty(display, xroot, net_current_desktop, 0, 1, False, XA_CARDINAL, &out_type,
                           &out_format, &out_len, &out_bytes_after,
                           (unsigned char**)&out_val) != Success ||
        out_val == NULL)
        return;

    workspace = *out_val;
    XFree(out_val);

    TRACE(g_print("Setting _NET_WM_DESKTOP to: %lu\n", workspace));

    memset(&xev, 0, sizeof(xev));
    xev.xclient.type = ClientMessage;
    xev.xclient.serial = 0;
    xev.xclient.send_event = True;
    xev.xclient.display = display;
    xev.xclient.window = xwin;
    xev.xclient.message_type = net_wm_desktop;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = workspace;
    xev.xclient.data.l[1] = 0;
    xev.xclient.data.l[2] = 0;

    XSendEvent(display, xroot, False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    XFlush(display);
}

void tomboy_window_present_hardcore(GtkWindow* window) {
    if (!gtk_widget_get_realized(GTK_WIDGET(window)))
        gtk_widget_realize(GTK_WIDGET(window));
    else if (gtk_widget_get_visible(GTK_WIDGET(window)))
        tomboy_window_move_to_current_workspace(window);

    gtk_window_present(window);
}

#endif /* X11_AVAILABLE */
