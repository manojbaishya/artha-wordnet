/* mod_notify.c
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
 * Dynamic linking of libnotify's functions for passive desktop
 * notifications.
 */

#include "addons/mod_notify.h"
#include "core/wni.h"
#include <gmodule.h>
#include <stdlib.h>

#ifdef G_OS_WIN32
#define NOTIFY_FILE "libnotify-1.dll"
#else
#define NOTIFY_FILE "libnotify.so.4"
#endif

GModule* mod_notify = NULL;
gboolean notify_inited = FALSE;
extern NotifyNotification* mod_notifier;

// function pointers matching functions in mod_notify.h
gboolean (*fn_notify_init)(const char* app_name);
void (*fn_notify_uninit)(void);
NotifyNotification* (*fn_notify_notification_new)(const char* summary, const char* body,
                                                  const char* icon);
gboolean (*fn_notify_notification_update)(NotifyNotification* notification, const char* summary,
                                          const char* body, const char* icon);
gboolean (*fn_notify_notification_show)(NotifyNotification* notification, GError** error);
gboolean (*fn_notify_notification_close)(NotifyNotification* notification, GError** error);

// Pass-through functions.  Don’t check pointer validity as it’s the caller’s responsibility
gboolean notify_init(const char* app_name) {
    return fn_notify_init(app_name);
}
void notify_uninit(void) {
    fn_notify_uninit();
}

NotifyNotification* notify_notification_new(const char* summary, const char* body,
                                            const char* icon) {
    return fn_notify_notification_new(summary, body, icon);
}

gboolean notify_notification_update(NotifyNotification* notification, const char* summary,
                                    const char* body, const char* icon) {
    return fn_notify_notification_update(notification, summary, body, icon);
}

gboolean notify_notification_show(NotifyNotification* notification, GError** error) {
    return fn_notify_notification_show(notification, error);
}

gboolean notify_notification_close(NotifyNotification* notification, GError** error) {
    return fn_notify_notification_close(notification, error);
}

static GModule* lookup_dynamic_library() {
    GModule* mod = g_module_open(NOTIFY_FILE, G_MODULE_BIND_LAZY);
    // NOTE: Looking up a single version should do; refer
    // https://stackoverflow.com/q/12485029/183120
#ifndef G_OS_WIN32
    // for non-Win32 machines try to lookup older versions of libnotify
    if (!mod) {
        gchar notify_file_str[] = NOTIFY_FILE;
        const size_t version_index = G_N_ELEMENTS(NOTIFY_FILE) - 2;
        for (gchar i = '3'; ((!mod) && (i > '0')); --i) {
            notify_file_str[version_index] = i;
            mod = g_module_open(notify_file_str, G_MODULE_BIND_LAZY);
        }
    }
#endif
    return mod;
}

gboolean mod_notify_init() {
    gboolean retval = FALSE;

    if (g_module_supported() && (mod_notify = lookup_dynamic_library())) {
        g_module_symbol(mod_notify, G_STRINGIFY(notify_init), (gpointer*)&fn_notify_init);
        g_module_symbol(mod_notify, G_STRINGIFY(notify_uninit), (gpointer*)&fn_notify_uninit);
        g_module_symbol(mod_notify, G_STRINGIFY(notify_notification_new),
                        (gpointer*)&fn_notify_notification_new);
        g_module_symbol(mod_notify, G_STRINGIFY(notify_notification_update),
                        (gpointer*)&fn_notify_notification_update);
        g_module_symbol(mod_notify, G_STRINGIFY(notify_notification_show),
                        (gpointer*)&fn_notify_notification_show);
        g_module_symbol(mod_notify, G_STRINGIFY(notify_notification_close),
                        (gpointer*)&fn_notify_notification_close);

        if (NULL != fn_notify_init && NULL != fn_notify_uninit &&
            NULL != fn_notify_notification_new && NULL != fn_notify_notification_update &&
            NULL != fn_notify_notification_show && NULL != fn_notify_notification_close) {
            if (notify_init(ARTHA_NAME)) {
                notify_inited = TRUE;

                /* initialize summary as Artha (Package Name)
                   this will, however, be modified to the looked up word before display */
                mod_notifier = notify_notification_new(ARTHA_NAME, NULL, "gtk-dialog-info");
                if (mod_notifier) {
                    G_MESSAGE("Notification module successfully loaded");
                    retval = TRUE;
                }
            }
        }

        if (!mod_notifier) {
            if (notify_inited) {
                notify_uninit();
                notify_inited = FALSE;
            }
            g_module_close(mod_notify);
            mod_notify = NULL;
        }
    }

    G_MESSAGE("Failed to load notifications module");
    return retval;
}

gboolean mod_notify_uninit() {
    if (mod_notifier) {
        /* close notifications, if open */
        notify_notification_close(mod_notifier, NULL);

        /*
           reason it's not required in Win32 is this
           module is custom written and doesn't use
           the GObject framework, while in *nix
           notify_notification_new internally does a
           g_object_new and returns GObject* as
           NotifyNotification*
        */
#ifdef G_OS_WIN32
        free(mod_notifier);
        mod_notifier = NULL;
#else
        g_object_unref(G_OBJECT(mod_notifier));
#endif
        mod_notifier = NULL;
    }

    if (notify_inited) {
        notify_uninit();
        notify_inited = FALSE;
    }

    gboolean mod_uninit = TRUE;
    if (mod_notify) {
        mod_uninit = g_module_close(mod_notify);
        mod_notify = NULL;
    }

    return mod_uninit;
}
