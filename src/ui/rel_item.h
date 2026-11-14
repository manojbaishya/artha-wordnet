/* rel_item.h
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
 * GTK 4.10 replaced GtkTreeView/GtkTreeStore with GListStore + GtkListView.
 * RelItem is the row object for the relatives panes: a plain text row
 * (term + weight + optional "via" source term for indirect antonyms)
 * that may carry a child GListStore, making it a group header
 * (Direct/Indirect Antonyms, domain categories, holo/meronym types).
 * Group rows are rendered with a GtkTreeExpander so the user can
 * collapse/expand them; leaf rows just show the text.
 */

#ifndef __REL_ITEM_H__
#define __REL_ITEM_H__

#include <gtk/gtk.h>
#include <pango/pango.h>

#define REL_TYPE_ITEM (rel_item_get_type())
G_DECLARE_FINAL_TYPE(RelItem, rel_item, REL, ITEM, GObject)

/* variant: 0 = plain row, 1 = direct antonym item, 2 = indirect antonym item */
RelItem* rel_item_new(const gchar* term, PangoWeight weight, const gchar* via, gboolean is_group,
                      gint variant);

gboolean rel_item_get_is_group(RelItem* item);
const gchar* rel_item_get_term(RelItem* item);
const gchar* rel_item_get_via(RelItem* item);
PangoWeight rel_item_get_weight(RelItem* item);
gint rel_item_get_variant(RelItem* item);

/* returns the item's children store, creating it on first use;
   NULL for leaf items */
GListStore* rel_item_ensure_children(RelItem* item);
GListStore* rel_item_get_children(RelItem* item);

/* wires a bare GtkListView taken from the glade file: attaches the
   GListStore<RelItem> > GtkTreeListModel > GtkSingleSelection model stack
   and the shared factory. Call once per pane at startup. The selection
   model carries "artha-view" and "artha-tab" (int) data */
void rel_view_build(GtkListView* view);

/* the pane's root GListStore (transfer none); NULL if the pane is unwired */
GListStore* rel_view_root_store(GtkWidget* view);

void rel_view_remove_all(GtkWidget* view);
guint rel_view_get_n_items(GtkWidget* view);

/* expand every expandable top-level row (i.e. the group headers);
   used for antonyms, domains and holo/meronyms. Hypernym-style nested
   trees are left collapsed, as before */
void rel_view_expand_roots(GtkWidget* view);

/* selection snapshot used by the sense-highlight logic:
   - position         index within the row's sibling list
   - parent_position  index of the parent row within ITS parent's list
                      (only meaningful when has_parent is set)
   - depth            0 for top-level rows, 1 for rows under a group, …
   - term             the selected row's term text
   - parent_term      the parent row's term (e.g. the domain category name);
                      empty for top-level rows
   - variant          row role: 0 plain/group, 1 direct antonym item,
                      2 indirect antonym item, 3 implication of a direct
                      antonym, 4 implication of an indirect antonym */
#define REL_TERM_MAX 512

typedef struct {
    guint position;
    guint parent_position;
    guint depth;
    gboolean has_parent;
    gchar term[REL_TERM_MAX];
    gchar parent_term[REL_TERM_MAX];
    gboolean is_group;
    gint variant;
} RelViewSelection;

gboolean rel_view_get_selection(GtkWidget* view, RelViewSelection* out);

#endif /* __REL_ITEM_H__ */
