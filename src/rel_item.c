/* rel_item.c
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

#include "rel_item.h"

#include <string.h>

struct _RelItem
{
	GObject parent_instance;

	gchar      *term;
	gchar      *via;
	PangoWeight weight;
	gint        variant;   /* 0 = plain row, 1 = direct antonym item, 2 = indirect antonym item */
	gboolean    is_group;
	GListStore *children;  /* non-NULL only for group rows */
};

G_DEFINE_TYPE(RelItem, rel_item, G_TYPE_OBJECT)

enum
{
	PROP_0,
	PROP_TERM,
	PROP_VIA,
	PROP_WEIGHT,
	PROP_VARIANT,
	PROP_IS_GROUP
};

static void rel_item_finalize(GObject *object)
{
	RelItem *item = REL_ITEM(object);

	g_clear_pointer(&item->term, g_free);
	g_clear_pointer(&item->via, g_free);
	g_clear_object(&item->children);

	G_OBJECT_CLASS(rel_item_parent_class)->finalize(object);
}

static void rel_item_set_property(GObject *object, guint prop_id,
				     const GValue *value, GParamSpec *pspec)
{
	RelItem *item = REL_ITEM(object);

	switch(prop_id)
	{
		case PROP_TERM:
			g_free(item->term);
			item->term = g_value_dup_string(value);
			if(item->term == NULL)
				item->term = g_strdup("");
			break;
		case PROP_VIA:
			g_free(item->via);
			item->via = g_value_dup_string(value);
			break;
		case PROP_WEIGHT:
			item->weight = (PangoWeight) g_value_get_int(value);
			break;
		case PROP_VARIANT:
			item->variant = g_value_get_int(value);
			break;
		case PROP_IS_GROUP:
			item->is_group = g_value_get_boolean(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static void rel_item_get_property(GObject *object, guint prop_id,
				     GValue *value, GParamSpec *pspec)
{
	RelItem *item = REL_ITEM(object);

	switch(prop_id)
	{
		case PROP_TERM:
			g_value_set_string(value, item->term);
			break;
		case PROP_VIA:
			g_value_set_string(value, item->via);
			break;
		case PROP_WEIGHT:
			g_value_set_int(value, item->weight);
			break;
		case PROP_VARIANT:
			g_value_set_int(value, item->variant);
			break;
		case PROP_IS_GROUP:
			g_value_set_boolean(value, item->is_group);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static void rel_item_class_init(RelItemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->finalize = rel_item_finalize;
	object_class->set_property = rel_item_set_property;
	object_class->get_property = rel_item_get_property;

	g_object_class_install_property(object_class, PROP_TERM,
			g_param_spec_string("term", "Term", "The row's text",
					    "", G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property(object_class, PROP_VIA,
			g_param_spec_string("via", "Via", "Source term for indirect antonym implications",
					    NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property(object_class, PROP_WEIGHT,
			g_param_spec_int("weight", "Weight", "PangoWeight of the row's text",
				       0, G_MAXINT, PANGO_WEIGHT_NORMAL,
				       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property(object_class, PROP_VARIANT,
			g_param_spec_int("variant", "Variant", "Row role: 0 plain, 1 direct antonym, 2 indirect antonym",
				       0, G_MAXINT, 0,
				       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property(object_class, PROP_IS_GROUP,
			g_param_spec_boolean("is-group", "Is group", "Whether the row is a collapsible group header",
					     FALSE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void rel_item_init(RelItem *item)
{
	item->weight = PANGO_WEIGHT_NORMAL;
}

RelItem *rel_item_new(const gchar *term, PangoWeight weight, const gchar *via, gboolean is_group, gint variant)
{
	return g_object_new(REL_TYPE_ITEM,
			    "term", term != NULL ? term : "",
			    "weight", (gint) weight,
			    "via", via,
			    "is-group", is_group,
			    "variant", variant,
			    NULL);
}

gboolean rel_item_get_is_group(RelItem *item)
{
	return item->is_group;
}

const gchar *rel_item_get_term(RelItem *item)
{
	return item->term;
}

const gchar *rel_item_get_via(RelItem *item)
{
	return item->via;
}

PangoWeight rel_item_get_weight(RelItem *item)
{
	return item->weight;
}

gint rel_item_get_variant(RelItem *item)
{
	return item->variant;
}

GListStore *rel_item_ensure_children(RelItem *item)
{
	if(item->children == NULL)
		item->children = g_list_store_new(REL_TYPE_ITEM);
	return item->children;
}

GListStore *rel_item_get_children(RelItem *item)
{
	return item->children;
}

/* ------------------------------------------------------------------ */

static GListModel *rel_item_create_children(gpointer item, gpointer user_data)
{
	(void) user_data;

	RelItem *rel_item = REL_ITEM(item);
	GListStore *children = rel_item_get_children(rel_item);

	if(children != NULL)
		return G_LIST_MODEL(g_object_ref(children));
	return NULL;
}

static void rel_via_attributes(GtkLabel *via)
{
	PangoAttrList *attrs = pango_attr_list_new();

	/* dim gray for the "via <term>" suffix shown on indirect antonym rows */
	pango_attr_list_insert(attrs, pango_attr_foreground_new(0x6d6d, 0x6d6d, 0x6d6d));
	gtk_label_set_attributes(via, attrs);
	pango_attr_list_unref(attrs);
}

static void factory_setup(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	GtkWidget *expander = gtk_tree_expander_new();
	GtkWidget *term_label = gtk_label_new(NULL);
	GtkWidget *via_label = gtk_label_new(NULL);

	(void) factory;
	(void) user_data;

	gtk_widget_set_hexpand(term_label, TRUE);
	gtk_widget_set_halign(term_label, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(term_label), 0.0f);
	gtk_label_set_ellipsize(GTK_LABEL(term_label), PANGO_ELLIPSIZE_NONE);

	gtk_widget_set_halign(via_label, GTK_ALIGN_END);
	rel_via_attributes(GTK_LABEL(via_label));
	gtk_widget_set_visible(via_label, FALSE);

	gtk_tree_expander_set_child(GTK_TREE_EXPANDER(expander), term_label);
	gtk_widget_set_hexpand(expander, TRUE);
	gtk_widget_set_halign(expander, GTK_ALIGN_START);

	gtk_box_append(GTK_BOX(box), expander);
	gtk_box_append(GTK_BOX(box), via_label);

	gtk_list_item_set_child(list_item, box);

	g_object_set_data(G_OBJECT(list_item), "artha-via", via_label);
	g_object_set_data(G_OBJECT(list_item), "artha-term", term_label);
	g_object_set_data(G_OBJECT(list_item), "artha-expander", expander);
}

static void factory_teardown(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
	(void) factory;
	(void) user_data;

	g_object_set_data(G_OBJECT(list_item), "artha-via", NULL);
	g_object_set_data(G_OBJECT(list_item), "artha-term", NULL);
	g_object_set_data(G_OBJECT(list_item), "artha-expander", NULL);
	gtk_list_item_set_child(list_item, NULL);
}

static void factory_unbind(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
	GtkTreeExpander *expander = GTK_TREE_EXPANDER(g_object_get_data(G_OBJECT(list_item), "artha-expander"));

	(void) factory;
	(void) user_data;

	if(expander != NULL)
		gtk_tree_expander_set_list_row(expander, NULL);
}

static void factory_bind(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
	GtkTreeListRow *row = GTK_TREE_LIST_ROW(gtk_list_item_get_item(list_item));
	RelItem *item = REL_ITEM(gtk_tree_list_row_get_item(row));
	GtkTreeExpander *expander = GTK_TREE_EXPANDER(g_object_get_data(G_OBJECT(list_item), "artha-expander"));
	GtkLabel *term_label = GTK_LABEL(g_object_get_data(G_OBJECT(list_item), "artha-term"));
	GtkLabel *via_label = GTK_LABEL(g_object_get_data(G_OBJECT(list_item), "artha-via"));
	const gchar *via = NULL;

	(void) factory;
	(void) user_data;

	gtk_tree_expander_set_list_row(expander, row);
	gtk_label_set_text(term_label, rel_item_get_term(item));

	{
		PangoAttrList *attrs = pango_attr_list_new();
		pango_attr_list_insert(attrs, pango_attr_weight_new(rel_item_get_weight(item)));
		gtk_label_set_attributes(term_label, attrs);
		pango_attr_list_unref(attrs);
	}

	via = rel_item_get_via(item);
	if(via != NULL && *via != '\0')
	{
		gtk_label_set_text(via_label, via);
		gtk_widget_set_visible(GTK_WIDGET(via_label), TRUE);
	}
	else
	{
		gtk_label_set_text(via_label, "");
		gtk_widget_set_visible(GTK_WIDGET(via_label), FALSE);
	}

	g_object_unref(item);
}

static GtkListItemFactory *rel_view_get_factory(void)
{
	static GtkListItemFactory *factory = NULL;

	if(factory == NULL)
	{
		GtkSignalListItemFactory *signal_factory = GTK_SIGNAL_LIST_ITEM_FACTORY(gtk_signal_list_item_factory_new());

		g_signal_connect(signal_factory, "setup", G_CALLBACK(factory_setup), NULL);
		g_signal_connect(signal_factory, "bind", G_CALLBACK(factory_bind), NULL);
		g_signal_connect(signal_factory, "unbind", G_CALLBACK(factory_unbind), NULL);
		g_signal_connect(signal_factory, "teardown", G_CALLBACK(factory_teardown), NULL);

		factory = GTK_LIST_ITEM_FACTORY(signal_factory);
	}

	return factory;
}

GListStore *rel_view_root_store(GtkWidget *view)
{
	GtkSelectionModel *selection = gtk_list_view_get_model(GTK_LIST_VIEW(view));
	GtkTreeListModel *tree_model = GTK_TREE_LIST_MODEL(gtk_single_selection_get_model(GTK_SINGLE_SELECTION(selection)));
	GListModel *root = gtk_tree_list_model_get_model(tree_model);

	return G_LIST_STORE(root);
}

void rel_view_build(GtkListView *view)
{
	GListStore *root = g_list_store_new(REL_TYPE_ITEM);
	/* the constructors take ownership (transfer full) of the models they
	   wrap — passing our references in, no further unrefs of root/tree_model */
	GtkTreeListModel *tree_model = gtk_tree_list_model_new(G_LIST_MODEL(root), FALSE, FALSE,
								   rel_item_create_children, NULL, NULL);
	GtkSingleSelection *selection = gtk_single_selection_new(G_LIST_MODEL(tree_model));

	gtk_single_selection_set_autoselect(selection, TRUE);
	gtk_single_selection_set_can_unselect(selection, FALSE);
	g_object_set_data(G_OBJECT(selection), "artha-view", view);

	gtk_list_view_set_model(view, GTK_SELECTION_MODEL(selection)); /* the view holds its own ref */
	gtk_list_view_set_factory(view, rel_view_get_factory());
	g_object_unref(selection); /* drop our ref; the view's ref keeps it alive */

	gtk_widget_set_hexpand(GTK_WIDGET(view), TRUE);
	gtk_widget_set_vexpand(GTK_WIDGET(view), TRUE);
}

void rel_view_remove_all(GtkWidget *view)
{
	GListStore *root = rel_view_root_store(view);

	if(root != NULL)
		g_list_store_remove_all(root);
}

guint rel_view_get_n_items(GtkWidget *view)
{
	GListStore *root = rel_view_root_store(view);

	if(root == NULL)
		return 0;
	return g_list_model_get_n_items(G_LIST_MODEL(root));
}

void rel_view_expand_roots(GtkWidget *view)
{
	GtkSelectionModel *selection = gtk_list_view_get_model(GTK_LIST_VIEW(view));
	GtkTreeListModel *tree_model = GTK_TREE_LIST_MODEL(gtk_single_selection_get_model(GTK_SINGLE_SELECTION(selection)));
	GListModel *root = gtk_tree_list_model_get_model(tree_model);
	guint root_count = 0;

	if(root == NULL)
		return;

	root_count = g_list_model_get_n_items(root);
	for(guint i = 0; i < root_count; i++)
	{
		GtkTreeListRow *row = gtk_tree_list_model_get_child_row(tree_model, i);

		if(row != NULL)
		{
			if(gtk_tree_list_row_is_expandable(row))
				gtk_tree_list_row_set_expanded(row, TRUE);
			g_object_unref(row);
		}
	}
}

gboolean rel_view_get_selection(GtkWidget *view, RelViewSelection *out)
{
	GtkSelectionModel *selection = gtk_list_view_get_model(GTK_LIST_VIEW(view));
	GtkTreeListModel *tree_model = GTK_TREE_LIST_MODEL(gtk_single_selection_get_model(GTK_SINGLE_SELECTION(selection)));
	guint selected = 0;
	GtkTreeListRow *row = NULL;
	RelItem *item = NULL;

	if(out == NULL)
		return FALSE;

	memset(out, 0, sizeof(RelViewSelection));

	selected = gtk_single_selection_get_selected(GTK_SINGLE_SELECTION(selection));
	if(selected == GTK_INVALID_LIST_POSITION)
		return FALSE;

	row = gtk_tree_list_model_get_row(tree_model, selected);
	if(row == NULL)
		return FALSE;

	item = REL_ITEM(gtk_tree_list_row_get_item(row));

	out->position = gtk_tree_list_row_get_position(row);
	out->depth = gtk_tree_list_row_get_depth(row);
	out->is_group = rel_item_get_is_group(item);
	g_strlcpy(out->term, rel_item_get_term(item), REL_TERM_MAX);
	out->variant = rel_item_get_variant(item);

	{
		GtkTreeListRow *parent = gtk_tree_list_row_get_parent(row);

		out->has_parent = (parent != NULL);
		out->parent_term[0] = '\0';
		if(parent != NULL)
		{
			RelItem *parent_item = REL_ITEM(gtk_tree_list_row_get_item(parent));

			out->parent_position = gtk_tree_list_row_get_position(parent);
			g_strlcpy(out->parent_term, rel_item_get_term(parent_item), REL_TERM_MAX);
			g_object_unref(parent_item);
			g_object_unref(parent);
		}
	}

	g_object_unref(item);
	g_object_unref(row);
	return TRUE;
}
