/* status_notifier.h
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
 * StatusNotifierItem (SNI) implementation over raw GDBus.
 *
 * GTK4 has no GtkStatusIcon; modern tray hosts (KStatusNotifierWatcher,
 * e.g. KDE Plasma, Ayatana/StatusNotifier-compatible watchers) expose
 * org.kde.StatusNotifierWatcher on the session bus. When no watcher is
 * present (stock GNOME), the tray is silently absent - the application
 * works regardless.
 */

#ifndef STATUS_NOTIFIER_H
#define STATUS_NOTIFIER_H

#include <glib.h>

/* Callback invoked when the user activates the tray item (left click). */
typedef void (*StatusNotifierActivateCallback)(gpointer user_data);

/*
 * Register an SNI on the session bus and register it with the
 * StatusNotifierWatcher, if any.
 *
 * icon_file: path to a PNG shown in the tray (may be NULL; then the
 *      themable "IconName" of "artha" is relied on alone).
 * activate_cb / user_data: invoked on left click of the tray item.
 *
 * Returns TRUE if the item was registered with a watcher; FALSE when
 * no watcher exists (stock GNOME) or an error occurred. No tray is
 * ever a hard failure.
 */
gboolean status_notifier_setup(const gchar* icon_file, StatusNotifierActivateCallback activate_cb,
                               gpointer user_data);

/* Show/hide the item by toggling the SNI "Status" property. */
void status_notifier_set_visible(gboolean visible);

/* Unregister the item from the bus. Safe to call when never set up. */
void status_notifier_cleanup(void);

#endif /* STATUS_NOTIFIER_H */
