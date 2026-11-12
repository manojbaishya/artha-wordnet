/* gui.c
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
 * GUI Code
 */


#include "gui.h"
#include "status_notifier.h"
#include <stdarg.h>

/* Global variables */

static const gchar *strv_authors[] = {"Sundaram Ramaswamy <legends2k@yahoo.com>", NULL};

/* Names of relative tree tab widgets from UI file
   Note that the 'tree" prefix will be stripped and will be used within code */
static const gchar *relative_tree[] = {"treeSynonyms", "treeAntonyms", "treeDerivatives", "treePertainyms", "treeAttributes", "treeSimilar", 
"treeDomain", "treeCauses", "treeEntails", "treeHypernyms", "treeHyponyms", "treeHolonyms", "treeMeronyms"};

#define DOMAINS_COUNT (CLASS_END - CLASSIF_START + 1)
static const gchar *domain_types[] = {"Topic", "Usage", "Region", "Topic Terms", "Usage Terms", "Regional Terms"};

#define HOLO_MERO_COUNT		3
static const gchar *holo_mero_types[][3] = {{"Member Of", "Substance Of", "Part Of"}, {"Has Members", "Has Substances", "Has Parts"}};

#define FAMILIARITY_COUNT	8
static const gchar *familiarity[] = {"extremely rare","very rare","rare","uncommon","common", "familiar","very familiar","extremely familiar"};
static const gchar *freq_colors[] = {"Black", "SaddleBrown", "FireBrick", "SeaGreen", "DarkOrange", "gold", "PaleGoldenrod", "PeachPuff1"};
/* words for checking familiarity types - none, scroll (v), scroll (n), alright, sequence, set (n), set (v), give */

/* notifier_enabled is for the setting "Notify" and *notifier is for the module availability */
static GSList 			*results = NULL;
static gboolean 		last_search_successful = FALSE, advanced_mode = FALSE, auto_contract = FALSE;
static gboolean		hotkey_set = FALSE, mod_suggest = FALSE;
// options which default to true
static gboolean		notifier_enabled = TRUE, show_polysemy = TRUE, launch_minimized = TRUE, show_trayicon = TRUE;
static gboolean		last_lookup_a_notification = FALSE;
static gchar		last_search[MAX_LEMMA_LEN] = "";
#ifdef X11_AVAILABLE
static Display			*dpy = NULL;	/* grab/pump connection; also XNextEvent pump */
#endif
static guint 			hotkey_trials[] = {GDK_KEY_w, GDK_KEY_a, GDK_KEY_t, GDK_KEY_q};
ArthaAccelKey		app_hotkey = {0};
static GString			*wordnet_terms = NULL;
NotifyNotification	*mod_notifier = NULL;
static GKeyFile			*config_file = NULL;
static GMainLoop		*main_loop = NULL;
static GtkWidget		*toolbar_prev_btn = NULL, *toolbar_next_btn = NULL;
	
#ifdef G_OS_WIN32
static HWND			hMainWindow = NULL;
#endif


/* function declarations */

#ifdef X11_AVAILABLE
static int x_error_handler(Display *dpy, XErrorEvent *xevent);
#endif	// X11_AVAILABLE
static void show_window(GtkWindow *window);
static void notification_toggled(GObject *obj, gpointer user_data);
static void mode_toggled(GtkToggleButton *toggle_button, gpointer user_data);
static gchar* strip_invalid_edges(gchar *selection);
static gchar* strip_non_alpha_num(const gchar *str);
static gboolean are_same_after_strip(const gchar *strA, const gchar *strB);
#ifdef G_OS_WIN32
static inline gboolean is_alt_pressed();
static inline void print_last_error();
static gchar* win32_capture_selection();
static gboolean link_launch(GtkAboutDialog *about_dialog, gchar *uri, gpointer user_data);
#endif
static void notification_lookup(gchar *selection, GtkComboBox *combo_query);
#ifdef X11_AVAILABLE
static gpointer hotkey_thread_func(gpointer data);
static gboolean hotkey_lookup_idle(gpointer data);
#endif
static void tray_activate_toggle(gpointer user_data);
static void about_response_handle(GtkDialog *about_dialog, gint response_id, gpointer user_data);
static void about_activate(GtkButton *button, gpointer user_data);
static void quit_activate(GtkWidget *widget, gpointer user_data);
static guint8 get_frequency(guint sense_count);
static void relatives_clear_all(GtkBuilder *gui_builder);
static void antonyms_load(GSList *antonyms, GtkBuilder *gui_builder);
static guint16 append_term(gchar *target, gchar *term, gboolean is_heading);
static void build_tree(GNode *node, GtkTreeStore *tree_store, GtkTreeIter *parent_iter);
static void add_nodes(GNode *tree, GtkTreeStore *tree_store, GtkTreeIter *node_parent, GSList **dupe_finder_list);
static void trees_load(GSList *properties, GtkBuilder *gui_builder, WNIRequestFlags id);
static void holo_mero_load(GSList *properties, GtkBuilder *gui_builder, WNIRequestFlags id);
static void list_relatives_load(GSList *properties, GtkBuilder *gui_builder, WNIRequestFlags id);
static void domains_load(GSList *properties, GtkBuilder *gui_builder);
static guint8 get_attribute_pos();
static void relatives_load(GtkBuilder *gui_builder, gboolean reset_tabs);
static gchar* wildmat_to_regex(const gchar *wildmat);
static gboolean set_regex_results(const gchar *wildmat_exp, GtkBuilder *gui_builder);
static gboolean is_wildmat_expr();
static gboolean handle_wildmat_expr(GtkBuilder *gui_builder, GtkTextBuffer *buffer);
static gboolean is_entry_unique(GtkTreeModel *query_list_store, GtkTreePath *path, GtkTreeIter *iter, gchar **lemma);
static gboolean update_history(GtkComboBox *combo_query, char *lemma);
static gboolean update_history_and_save(GtkComboBox *combo_query, char *lemma);
static void show_definitions(GtkBuilder *gui_builder, GtkTextBuffer *buffer);
static void show_suggestions(GtkBuilder *gui_builder, GtkTextBuffer *buffer, GtkTextIter *cur, GSList *suggestions);
static void construct_show_notification();
static void show_searching(GtkBuilder *gui_builder);
static void query_wni(gchar *search_str,
					  WNIRequestFlags lookup_type,
					  GSList **suggestions);
static void button_search_click(GtkButton *button, gpointer user_data);
static void combo_query_changed(GtkComboBox *combo_query, gpointer user_data);
static void text_view_clicked(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer user_data);
static void lookup_selection_in_buffer(GtkTextBuffer *buffer, GtkBuilder *gui_builder);
static void expander_clicked(GtkExpander *expander, gpointer user_data);
static void query_list_updated(GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data);
static GtkTextMark *highlight_definition(guint8 id, guint16 sense, GtkTextView *text_view);
static void highlight_senses_from_synonyms(guint16 relative_index, GtkTextView *text_view);
static void highlight_senses_from_antonyms( guint8 category, guint16 relative_index, guint16 sub_level, GtkTextView *text_view);
static void highlight_senses_from_domain(guint8 category, guint16 relative_index, GtkTextView *text_view);
static void highlight_senses_from_relative_lists(WNIRequestFlags id, guint16 relative_index, GtkTextView *text_view);
static void relative_selection_changed(GtkTreeView *tree_view, gpointer user_data);
static void relative_row_activated(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data);
static void clear_history(GSimpleAction *action, GVariant *params, gpointer user_data);
static void save_history_to_file(GSimpleAction *action, GVariant *params, gpointer user_data);
static void query_combo_popup(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer user_data);
static void load_history(GtkListStore *list_store_query);
static void create_stores_renderers(GtkBuilder *gui_builder);
static void button_next_clicked(GtkButton *button, gpointer user_data);
static void button_prev_clicked(GtkButton *button, gpointer user_data);
static gboolean combo_query_scroll(GtkEventControllerScroll *controller, gdouble dx, gdouble dy, gpointer user_data);
static gboolean shortcut_hide_cb(GtkWidget *widget, GVariant *args, gpointer user_data);
static void setup_toolbar(GtkBuilder *gui_builder);
static void create_text_view_tags(GtkBuilder *gui_builder);
static void load_preference_hotkey(gboolean *first_run);
static gboolean load_preferences(GtkWindow *parent);
static gboolean update_history_in_file(const gchar *term);
static void save_preferences_to_file();
static void save_preferences();
static gboolean autocomplete_selected(GtkEntryCompletion *query_completion, GtkTreeModel *model, GtkTreeIter *iter, GtkButton *button);
static void show_loading(GtkBuilder *gui_builder);

typedef struct
{
	GtkBuilder		*gui_builder;
	GtkListStore	*completion_list;
	gchar			*index_file_contents;
	gsize		 	 index_file_length;
	gsize		 	 pos_in_file;
	gchar			*last_lemma;
}
WordnetTermsLoaderData;
static void attach_loaded_terms(WordnetTermsLoaderData *loader_data);
static gboolean terms_loader(WordnetTermsLoaderData *loader_data);
static gboolean wordnet_terms_load(WordnetTermsLoaderData *loader_data);

#ifdef X11_AVAILABLE
static void lookup_ignorable_modifiers(void);
#elif defined G_OS_WIN32

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

typedef struct
{
	guint gdk_key;
	BYTE  vk_key;
} gdk_vk;

static gboolean try_convert_to_vk(guint gdk_key, BYTE *vk);
#endif //X11_AVAILABLE
gboolean grab_ungrab_with_ignorable_modifiers(ArthaAccelKey *binding, gboolean grab);
static gboolean register_unregister_hotkey(gboolean first_run, gboolean setup_hotkey);
static void setup_settings_dialog(GtkBuilder *gui_builder);
static void set_settings_to_check_boxes(GtkBuilder *gui_builder);
static void get_settings_from_check_boxes(GtkBuilder *gui_builder,
											gboolean *new_trayicon_show,
											gboolean *new_launch_min,
											gboolean *new_polysemy_show);
static void apply_and_save_settings(GtkBuilder *gui_builder);
static void revert_settings(GtkBuilder *gui_builder, ArthaAccelKey *hotkey_backup);
static void show_settings_dialog(GtkButton *button, gpointer user_data);
static void destructor(GtkBuilder *gui_builder);
static void show_message_dlg(GtkWidget *parent_window, MessageResposeCode msg_code);
static void status_set(const gchar *text);
static void notify_startup_complete(void);
static void window_mapped(GtkWidget *widget, gpointer user_data);
static gboolean close_request_cb(GtkWindow *window, gpointer user_data);



#ifdef X11_AVAILABLE
/* BadAccess from XGrabKey (combo taken by another client) must downgrade
   to a FALSE return, never kill the app: x11_grab_error carries it. */
static volatile gint x11_grab_error = 0;
static int x_error_handler(Display *xdisplay, XErrorEvent *xevent)
{
	(void) xdisplay;

	x11_grab_error = (BadAccess == xevent->error_code) ? 1 : 2;
	if(BadAccess == xevent->error_code)
		g_warning("Hotkey combo already occupied!\n");
	else
		g_warning("X11 error %d (request %d, minor %d)\n",
			  xevent->error_code, xevent->request_code, xevent->minor_code);

	return 0;
}
#endif	// X11_AVAILABLE

/* generic non-modal message dialog: present + destroy on response.
   (GTK4 has no gtk_dialog_run; nested main loops are gone.)
   GTK >= 4.10: GtkMessageDialog is deprecated; use GtkAlertDialog. */
#if !GTK_CHECK_VERSION(4, 10, 0)
static void message_dialog_destroy(GtkWidget *dialog, gint response, gpointer user_data)
{
	(void) response;
	(void) user_data;

	gtk_window_destroy(GTK_WINDOW(dialog));
}
#endif

static void show_message_dialog(GtkWindow *parent, GtkMessageType type, const gchar *title,
				 const gchar *body_format, ...)
{
	gchar *body = NULL;
	va_list args;

	va_start(args, body_format);
	body = g_strdup_vprintf(body_format, args);
	va_end(args);

#if GTK_CHECK_VERSION(4, 10, 0)
	(void) type;
	{
		GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", title);

		gtk_alert_dialog_set_detail(dialog, body);
		gtk_alert_dialog_show(dialog, parent);
		g_object_unref(dialog);
	}
#else
	{
		GtkWidget *dialog = gtk_message_dialog_new(parent,
						       GTK_DIALOG_DESTROY_WITH_PARENT,
						       type,
						       GTK_BUTTONS_OK,
						       "%s",
						       body);
		g_object_set(dialog, "title", title, NULL);
		g_signal_connect(dialog, "response", G_CALLBACK(message_dialog_destroy), NULL);
		gtk_window_present(GTK_WINDOW(dialog));
	}
#endif

	g_free(body);
}

static GtkLabel		*status_label = NULL;

/* wrap the GTK4 startup-notification completion (NULL display safe) */
static void notify_startup_complete(void)
{
	GdkDisplay *display = gdk_display_get_default();

	if(display != NULL)
		gdk_display_notify_startup_complete(display,
				gdk_display_get_startup_notification_id(display));
}

static void status_set(const gchar *text)
{
	if(status_label == NULL)
		return;

	gtk_label_set_text(status_label, text != NULL ? text : "");
}

static void show_window(GtkWindow *window)
{
#ifdef X11_AVAILABLE
			tomboy_window_present_hardcore(window);
#else
			gtk_window_present(window);
#endif
}

/* Called by the notify toggle button in the toolbar. */
static void notification_toggled(GObject *obj, gpointer user_data)
{
	GtkBuilder *gui_builder = GTK_BUILDER(user_data);
	(void) gui_builder;

	g_object_get(obj, "active", &notifier_enabled, NULL);

	save_preferences();

	/* if no hotkey is set, then there's no point in enabling notifications;
	   intimate this to the user */
	if(!hotkey_set && notifier_enabled)
		show_message_dlg(NULL, MSG_HOTKEY_NOTSET);
}

static void mode_toggled(GtkToggleButton *toggle_button, gpointer user_data)
{
	GtkBuilder *gui_builder = GTK_BUILDER(user_data);

	advanced_mode = gtk_toggle_button_get_active(toggle_button);

	if(last_search_successful)
	{
		G_MESSAGE("Re-requesting with changed mode: %d\n", advanced_mode);

		wni_request_nyms(last_search, &results, WORDNET_INTERFACE_ALL, advanced_mode);
		relatives_clear_all(gui_builder);
		relatives_load(gui_builder, FALSE);
	}

	save_preferences();
}

static gchar* strip_invalid_edges(gchar *selection)
{
	guint i = 0;
	gboolean alphanum_encounterd = FALSE;

	if(selection)
	{
		while(selection[i] != '\0')
		{
			if(!g_ascii_isalnum(selection[i]) && selection[i] != '-' && selection[i] != '_' && 
			selection[i] != ' ' && selection[i] != '\'' && selection[i] != '.')
			{
				if(alphanum_encounterd)
				{
					selection[i] = '\0';
					break;
				}
				else
				{
					selection[i] = ' ';
				}
			}
			else
			{
				// before setting the alphanum_encountered, make sure the current char isn't other valid ones
				if(!alphanum_encounterd && selection[i] != '-' && selection[i] != '_' && selection[i] != ' ' && 
				selection[i] != '\'' && selection[i] != '.')
					alphanum_encounterd = TRUE;
			}
			i++;
		}
	}
	
	return selection;
}

static gchar* strip_non_alpha_num(const gchar *str)
{
	gchar *stripped_str = g_strstrip(g_strdup(str));
	guint16 i = 0, j = 0;
	gchar ch;
	while((ch = str[i++]))
	{
		if(g_ascii_isalnum(ch))
			stripped_str[j++] = ch;
	}
	stripped_str[j] = '\0';
	return stripped_str;
}

static gboolean are_same_after_strip(const gchar *strA, const gchar *strB)
{
	gchar *strAStripped = strip_non_alpha_num(strA);
	gchar *strBStripped = strip_non_alpha_num(strB);
	// NOT op since 0 means strings match as per strcmp
	gboolean matches = !g_strcmp0(strAStripped, strBStripped);

	g_free(strAStripped); strAStripped = NULL;
	g_free(strBStripped); strBStripped = NULL;

	return matches;
}

#ifdef G_OS_WIN32

static inline gboolean is_alt_pressed()
{
	return (0 != (0x8000 & GetAsyncKeyState(VK_MENU)));
}

static inline void print_last_error()
{
	g_warning("Error getting selection (Win32): %ld", GetLastError());
}

static gchar* win32_capture_selection()
{
	GUITHREADINFO guiThread = {sizeof(GUITHREADINFO)};
	gchar *selection = NULL;
	INPUT input[KEY_COUNT] = {{INPUT_KEYBOARD}, {INPUT_KEYBOARD}, {INPUT_KEYBOARD}, {INPUT_KEYBOARD}};
	const WORD inputKey[2] = {VK_CONTROL, VK_INSERT};
	HGLOBAL hglb = NULL;
	LPSTR charArrayFromClipboard = NULL;

	if(GetGUIThreadInfo(0, &guiThread))
	{
		if(is_alt_pressed())
		{
			input[0].ki.wVk = VK_MENU;
			input[0].ki.dwFlags = KEYEVENTF_KEYUP;
			SendInput(1, input, sizeof(INPUT));
			input[0].ki.dwFlags = 0;
			G_MESSAGE("Virtually released ALT!\n");
		}

		input[0].ki.wVk = input[2].ki.wVk = inputKey[0];
		input[1].ki.wVk = input[3].ki.wVk = inputKey[1];

		input[2].ki.dwFlags = (input[3].ki.dwFlags |= KEYEVENTF_KEYUP);
		SendInput(KEY_COUNT, input, sizeof(INPUT));

		Sleep(200);

		if(OpenClipboard(NULL))
		{
			if((hglb = GetClipboardData(CF_TEXT)))
			{
				if((charArrayFromClipboard = (LPSTR) GlobalLock(hglb)))
				{
					selection = g_strdup(charArrayFromClipboard);
					GlobalUnlock(hglb);
				}
				else
					print_last_error();
			}
			else
				print_last_error();

			CloseClipboard();
		}
		else
			print_last_error();
	}
	else
		print_last_error();

	return selection;
}

static gboolean link_launch(GtkAboutDialog *about_dialog, gchar *uri, gpointer user_data)
{
	return ((int)ShellExecute(hMainWindow, "open", uri, NULL, NULL, SW_SHOWNORMAL) > 32);
}
#endif

static void notification_lookup(gchar *selection, GtkComboBox *combo_query)
{
	gchar *stripped_lookup = g_strstrip(strip_invalid_edges(selection));
	// make sure this isn't a zero length string; valid but zero length
	if(stripped_lookup[0])
	{
		gboolean new_search = TRUE;
		if(last_search_successful)
			new_search = (g_ascii_strcasecmp(last_search, stripped_lookup) != 0);

		if(new_search)
			query_wni(stripped_lookup, WORDNET_INTERFACE_NONE, NULL);

		if(results)
		{
			construct_show_notification();

			gchar *lemma = ((WNIDefinitionItem*)((WNIOverview*)((WNINym*)results->data)->data)->definitions_list->data)->lemma;
			update_history_and_save(combo_query, lemma);

			g_stpcpy(last_search, stripped_lookup);
			last_search_successful = TRUE;
		}
		else
		{
			gboolean update_successful = notify_notification_update(mod_notifier,
																	stripped_lookup,
																	STR_STATUS_QUERY_FAILED,
																	"gtk-dialog-warning");
			if(!update_successful || (FALSE == notify_notification_show(mod_notifier, NULL)))
			{
				g_warning("Failed to display notification!\n");
			}
			last_search_successful = FALSE;
		}
	}
	last_lookup_a_notification = TRUE;
}

/* selection text has been read (or failed to be read) by the async
   clipboard read; shared by the hotkey path and used only from the
   GTK main thread */
static void hotkey_selection_ready(GtkBuilder *gui_builder, gchar *selection)
{
	GtkWindow *window = GTK_WINDOW(gtk_builder_get_object(gui_builder, WINDOW_MAIN));
	GtkComboBox *combo_query = GTK_COMBO_BOX(gtk_builder_get_object(gui_builder, COMBO_QUERY));

	if(selection)
	{
		/* setting the text is needed for both notification and normal lookup
		   since on unobscuring the window, a proper lookup needs to be done */
		GtkEntry *text_entry_query = GTK_ENTRY(gtk_combo_box_get_child(combo_query));
		gtk_editable_set_text(GTK_EDITABLE(text_entry_query), selection);
		if(gtk_widget_get_visible(GTK_WIDGET(window)) || !notifier_enabled)
		{
			GtkButton *button_search = GTK_BUTTON(gtk_builder_get_object(gui_builder, BUTTON_SEARCH));
			g_signal_emit_by_name(button_search, "clicked");
		}
		/* if notifier_enabled and notifier mod is present */
		else if(mod_notifier)
		{
			notification_lookup(selection, combo_query);
		}
	}
	else
	{
		/* no PRIMARY selection / read error: just pop up the window */
		show_window(window);
		gtk_widget_grab_focus(GTK_WIDGET(combo_query));
	}
}

static void clipboard_read_cb(GObject *source, GAsyncResult *res, gpointer user_data)
{
	GtkBuilder *gui_builder = GTK_BUILDER(user_data);
	gchar *selection = gdk_clipboard_read_text_finish(GDK_CLIPBOARD(source), res, NULL);

	hotkey_selection_ready(gui_builder, selection);
	g_free(selection);
	g_object_unref(gui_builder);
}

#ifdef X11_AVAILABLE
/* hotkey events arrive on a dedicated X11 connection in a worker thread;
   we never share the blocking XNextEvent loop with GDK's own queue */
static gpointer hotkey_thread_func(gpointer data)
{
	GtkBuilder *gui_builder = GTK_BUILDER(data);
	XEvent ev;

	while(TRUE)
	{
		XNextEvent(dpy, &ev);
		if(KeyPress == ev.type)
			g_idle_add(hotkey_lookup_idle, g_object_ref(gui_builder));
	}
	return NULL;
}

/* runs on the GTK main thread (dispatched from hotkey_thread_func) */
static gboolean hotkey_lookup_idle(gpointer data)
{
	GtkBuilder *gui_builder = GTK_BUILDER(data);
	GdkDisplay *display = gdk_display_get_default();
	GdkClipboard *clipboard = NULL;

	if(display != NULL)
		clipboard = gdk_display_get_primary_clipboard(display);

	if(clipboard != NULL)
		gdk_clipboard_read_text_async(clipboard, NULL,
					      clipboard_read_cb, gui_builder);
	else
	{
		hotkey_selection_ready(gui_builder, NULL);
		g_object_unref(gui_builder);
	}

	return G_SOURCE_REMOVE;
}
#endif

static void tray_activate_toggle(gpointer user_data)
{
	GtkBuilder *gui_builder = GTK_BUILDER(user_data);
	GtkWindow *window = GTK_WINDOW(gtk_builder_get_object(gui_builder, WINDOW_MAIN));
	GtkComboBox *combo_query = NULL;

	if(gtk_widget_get_visible(GTK_WIDGET(window)))
		gtk_widget_hide(GTK_WIDGET(window));
	else
	{
		/* close notifications, if any */
		if(mod_notifier)
			notify_notification_close(mod_notifier, NULL);
		show_window(window);
		combo_query = GTK_COMBO_BOX(gtk_builder_get_object(gui_builder, COMBO_QUERY));
		gtk_widget_grab_focus(GTK_WIDGET(combo_query));
	}
}


static void show_uri_done(GObject *source, GAsyncResult *res, gpointer user_data)
{
	GError *err = NULL;

	(void) source;
	(void) user_data;

	if(!gtk_show_uri_full_finish(NULL, res, &err))
	{
		show_message_dialog(NULL, GTK_MESSAGE_ERROR, ARTHA_NAME,
				    "Error opening %s\n\n%s",
				    STR_BUG_WEBPAGE,
				    err != NULL ? err->message : "unknown error");
		g_clear_error(&err);
	}
}

static void about_response_handle(GtkDialog *about_dialog, gint response_id, gpointer user_data)
{
	switch(response_id)
	{
		case ARTHA_RESPONSE_REPORT_BUG:
		{
#ifndef G_OS_WIN32
			gtk_show_uri_full(NULL, STR_BUG_WEBPAGE, GDK_CURRENT_TIME, NULL,
					  show_uri_done, NULL);
#else
			link_launch(GTK_ABOUT_DIALOG(about_dialog), STR_BUG_WEBPAGE, NULL);
#endif
			break;
		}
		default:
			g_warning("About Dialog: Unhandled response_id: %d!\n", response_id);
			break;
	}
	gtk_window_destroy(GTK_WINDOW(about_dialog));
}

static void about_activate(GtkButton *button, gpointer user_data)
{
	GtkWidget *about_dialog = gtk_about_dialog_new();

	g_object_set(G_OBJECT(about_dialog), "license", STR_LICENCE, "copyright", STR_COPYRIGHT, 
	"comments", STR_ABOUT, "authors", strv_authors, "version", ARTHA_VERSION, 
	"wrap-license", TRUE, 
	"website-label", STR_WEBSITE_LABEL,
	"website", ARTHA_URL, NULL);

	gtk_dialog_add_button(GTK_DIALOG(about_dialog), STR_REPORT_BUG, ARTHA_RESPONSE_REPORT_BUG);
	g_signal_connect(about_dialog, "response", G_CALLBACK(about_response_handle), NULL);

#ifdef G_OS_WIN32
	g_signal_connect(about_dialog, "activate-link", G_CALLBACK(link_launch), NULL);
#endif
	
	gtk_window_present(GTK_WINDOW(about_dialog));
}

static void quit_activate(GtkWidget *widget, gpointer user_data)
{
	G_DEBUG("Destroy called!\n");

	(void) widget;
	(void) user_data;

	if(main_loop != NULL)
		g_main_loop_quit(main_loop);
}

static guint8 get_frequency(guint sense_count)
{
	guint8 frequency = 0;

	if(sense_count == 0) frequency = 0;
	if(sense_count == 1) frequency = 1;
	if(sense_count == 2) frequency = 2;
	if(sense_count >= 3 && sense_count <= 4) frequency = 3;
	if(sense_count >= 5 && sense_count <= 8) frequency = 4;
	if(sense_count >= 9 && sense_count <= 16) frequency = 5;
	if(sense_count >= 17 && sense_count <= 32) frequency = 6;
	if(sense_count > 32 ) frequency = 7;

	return frequency;
}

static void relatives_clear_all(GtkBuilder *gui_builder)
{
	guint8 i = 0;
	GtkTreeView *temp_tree_view = NULL;
	GtkTreeStore *temp_tree_store = NULL;
	GtkWidget *relative_tab = NULL;

	for(i = 0; i < TOTAL_RELATIVES; i++)
	{
		temp_tree_view = GTK_TREE_VIEW(gtk_builder_get_object(gui_builder, relative_tree[i]));
		relative_tab = gtk_widget_get_parent(GTK_WIDGET(temp_tree_view));

		if(gtk_widget_get_visible(GTK_WIDGET(relative_tab)))
		{
			temp_tree_store = GTK_TREE_STORE(gtk_tree_view_get_model(temp_tree_view));
			if(temp_tree_store)
				gtk_tree_store_clear(temp_tree_store);
		}
		else
			gtk_widget_show(relative_tab);
	}
}

static void antonyms_load(GSList *antonyms, GtkBuilder *gui_builder)
{
	WNIAntonymItem *temp_antonym_item = NULL;
	GSList *implications = NULL;
	GtkTreeView *tree_antonyms = GTK_TREE_VIEW(gtk_builder_get_object(gui_builder, relative_tree[TREE_ANTONYMS]));
	GtkTreeStore *antonyms_tree_store = GTK_TREE_STORE(gtk_tree_view_get_model(tree_antonyms));
	GtkTreeIter iter = {0}, sub_iter = {0}, direct_iter = {0}, indirect_iter = {0};
	GtkTreePath *expand_path = NULL;
	gboolean direct_deleted = FALSE, has_indirect = FALSE;

	g_assert(antonyms_tree_store);

	if(advanced_mode)
	{
		gtk_tree_store_append(antonyms_tree_store, &direct_iter, NULL);
		gtk_tree_store_set(antonyms_tree_store, &direct_iter, 0, STR_ANTONYM_HEADER_DIRECT, 1, PANGO_WEIGHT_SEMIBOLD, 2, NULL, -1);
		gtk_tree_store_append(antonyms_tree_store, &indirect_iter, NULL);
		gtk_tree_store_set(antonyms_tree_store, &indirect_iter, 0, STR_ANTONYM_HEADER_INDIRECT, 1, PANGO_WEIGHT_SEMIBOLD, 2, NULL, -1);
	}

	while(antonyms)
	{
		temp_antonym_item = antonyms->data;
		if(temp_antonym_item)
		{
			if(DIRECT_ANT == temp_antonym_item->relation)
			{
				if(advanced_mode)
					gtk_tree_store_append(antonyms_tree_store, &iter, &direct_iter);
				else
					gtk_tree_store_append(antonyms_tree_store, &iter, NULL);
				gtk_tree_store_set(antonyms_tree_store, &iter, 0, temp_antonym_item->term, 1, PANGO_WEIGHT_NORMAL, 2, NULL, -1);
			}
			else
			{				
				iter = indirect_iter;
				has_indirect = TRUE;
			}
			implications = temp_antonym_item->implications;
			while(implications)
			{
				gtk_tree_store_append(antonyms_tree_store, &sub_iter, &iter);
				gtk_tree_store_set(antonyms_tree_store, &sub_iter, 0, ((WNIImplication*)implications->data)->term, 
				1, PANGO_WEIGHT_NORMAL, 2, (DIRECT_ANT == temp_antonym_item->relation)?NULL:temp_antonym_item->term, -1);
				implications = g_slist_next(implications);
			}
		}
		antonyms = g_slist_next(antonyms);
	}

	if(advanced_mode)
	{
		// if only Direct is present, then Indirect will be deleted, only indirect_iter becomes invalid
		// if only Indirect is present, then Direct will be deleted, and the indirect delete won't work, because the indirect_iter will get invalid
		// if both are present, none deleted, no iters become invalid
		// if both are not present, we won't be here in the first place ;)
		// notice the NOT op. for assigning direct_deleted, which is imp. coz it says true when there are no children under Direct i.e. Direct was deleted
		if((direct_deleted = !gtk_tree_model_iter_has_child(GTK_TREE_MODEL(antonyms_tree_store), &direct_iter)))
			gtk_tree_store_remove(antonyms_tree_store, &direct_iter);
		if(!gtk_tree_model_iter_has_child(GTK_TREE_MODEL(antonyms_tree_store), &indirect_iter))
			gtk_tree_store_remove(antonyms_tree_store, &indirect_iter);

		// expand one level
		expand_path = gtk_tree_path_new_from_indices(0, -1);

		// if direct list was deleted, then this is the indirect list that we are dealing, so expand, else don't
		gtk_tree_view_expand_row(tree_antonyms, expand_path, direct_deleted);
		gtk_tree_path_free(expand_path);

		// if there is a list in this path "1", then it should be indirect
		expand_path = gtk_tree_path_new_from_indices(1, -1);
		gtk_tree_view_expand_row(tree_antonyms, expand_path, TRUE);
		gtk_tree_path_free(expand_path);
		expand_path = NULL;
	}
	
	gtk_tree_view_set_headers_visible(tree_antonyms, has_indirect);
}

static guint16 append_term(gchar *target, gchar *term, gboolean is_heading)
{
	guint16 i = 0;

	while(term[i] != '\0')
	{
		*target = term[i++];
		target++;
	}
	*target = (is_heading)? ':' : ',';
	++target;
	*target = ' ';

	return (i + 2);
}

static void build_tree(GNode *node, GtkTreeStore *tree_store, GtkTreeIter *parent_iter)
{
	GNode *child = NULL;
	GSList *temp_list = NULL;
	WNIImplication *imp = NULL;
	GtkTreeIter tree_iter = {0};
	guint32 i = 0;
	gchar terms[MAX_CONCAT_STR] = "";

	if((child = g_node_first_child(node)))
	do
	{
		i = 0;
		terms[0] = '\0';

		temp_list = ((WNITreeList*)child->data)->word_list;
		while(temp_list)
		{
			imp = (WNIImplication*) temp_list->data;
			i += append_term(&terms[i], imp->term, FALSE);

			temp_list = g_slist_next(temp_list);
		}
		terms[i - 2] = '\0';

		gtk_tree_store_append(tree_store, &tree_iter, parent_iter);
		gtk_tree_store_set(tree_store, &tree_iter, 0, terms, 1, PANGO_WEIGHT_NORMAL, -1);

		build_tree(child, tree_store, &tree_iter);

	} while((child = g_node_next_sibling(child)));
}

static void add_nodes(GNode *tree, GtkTreeStore *tree_store, GtkTreeIter *node_parent, GSList **dupe_finder_list)
{
	WNIImplication *imp = NULL;
	gchar terms[MAX_CONCAT_STR] = "";
	GtkTreeIter iter = {0};
	gboolean is_dupe = FALSE;
	guint16 i = 0;
	GSList *term_list = ((WNITreeList*)tree->data)->word_list;

	if(advanced_mode)
	{
		while(term_list)
		{
			imp = (WNIImplication*) term_list->data;
			i += append_term(&terms[i], imp->term, FALSE);

			term_list = g_slist_next(term_list);
		}
		terms[i - 2] = '\0';

		gtk_tree_store_append(tree_store, &iter, node_parent);
		gtk_tree_store_set(tree_store, &iter, 0, terms, 1, PANGO_WEIGHT_NORMAL, -1);

		build_tree(tree, tree_store, &iter);
	}
	else
	{
		while(term_list)
		{
			imp = (WNIImplication*) term_list->data;

			is_dupe = FALSE;
			/* if dupe check was asked by the caller, then check else continue with append */
			if(NULL != dupe_finder_list)
			{
				if(NULL == g_slist_find_custom(*dupe_finder_list, imp->term, (GCompareFunc) &g_strcmp0))
					*dupe_finder_list = g_slist_append(*dupe_finder_list, imp->term);
				else
					is_dupe = TRUE;
			}

			if(!is_dupe)
			{
				gtk_tree_store_append(tree_store, &iter, node_parent);
				gtk_tree_store_set(tree_store, &iter, 0, imp->term, 1, PANGO_WEIGHT_NORMAL, -1);
			}

			term_list = g_slist_next(term_list);
		}
	}
}

static void trees_load(GSList *properties, GtkBuilder *gui_builder, WNIRequestFlags id)
{
	GNode *tree = NULL;
	GtkTreeView *tree_view = NULL;
	GtkTreeStore *tree_store = NULL;
	GSList *dupe_finder_list = NULL;
	guint8 i = (WORDNET_INTERFACE_HYPERNYMS == id) ? TREE_HYPERNYMS : ((WORDNET_INTERFACE_HYPONYMS == id) ? TREE_HYPONYMS : TREE_PERTAINYMS);

	tree_view = GTK_TREE_VIEW(gtk_builder_get_object(gui_builder, relative_tree[i]));
	tree_store = GTK_TREE_STORE(gtk_tree_view_get_model(tree_view));

	g_assert(tree_store);

	while(properties)
	{
		if(properties->data)
			tree = g_node_first_child((GNode*)properties->data);

		while(tree)
		{
			add_nodes(tree, tree_store, NULL, &dupe_finder_list);
			tree = g_node_next_sibling(tree);
		}

		properties = g_slist_next(properties);
	}

	g_slist_free(dupe_finder_list);
	dupe_finder_list = NULL;
}

static void holo_mero_load(GSList *properties, GtkBuilder *gui_builder, WNIRequestFlags id)
{
	GNode *tree = NULL;
	GtkTreeIter type_iter[HOLO_MERO_COUNT] = {{0}}, *relative_parent = NULL;
	GSList *dupe_finder_list[HOLO_MERO_COUNT] = {NULL};
	guint8 i = (WORDNET_INTERFACE_MERONYMS == id) ? TREE_MERONYMS : TREE_HOLONYMS;
	GtkTreePath *head_level = NULL;
	GtkTreeView *tree_view = GTK_TREE_VIEW(gtk_builder_get_object(gui_builder, relative_tree[i]));
	GtkTreeStore *tree_store = GTK_TREE_STORE(gtk_tree_view_get_model(tree_view));

	g_assert(tree_store);

	for(guint8 j = 0; j < HOLO_MERO_COUNT; ++j)
	{
		gtk_tree_store_append(tree_store, &type_iter[j], NULL);
		gtk_tree_store_set(tree_store, &type_iter[j], 0, holo_mero_types[i - TREE_HOLONYMS][j], 1, PANGO_WEIGHT_SEMIBOLD, -1);
	}

	while(properties)
	{
		if(properties->data) tree = g_node_first_child((GNode*)properties->data);

		while(tree)
		{
			i = (((WNITreeList*)tree->data)->type) - ((WORDNET_INTERFACE_HOLONYMS == id) ? ISMEMBERPTR : HASMEMBERPTR);
			relative_parent = (i < HOLO_MERO_COUNT) ? &type_iter[i] : NULL;
			add_nodes(tree, tree_store, relative_parent, &dupe_finder_list[i]);
			tree = g_node_next_sibling(tree);
		}

		properties = g_slist_next(properties);
	}

	for(i = 0; i < HOLO_MERO_COUNT; i++)
	{
		// if a node has child then expand it, else remove it
		if(gtk_tree_model_iter_has_child(GTK_TREE_MODEL(tree_store), &type_iter[i]))
		{
			head_level = gtk_tree_model_get_path(GTK_TREE_MODEL(tree_store), &type_iter[i]);
			gtk_tree_view_expand_row(tree_view, head_level, FALSE);
			gtk_tree_path_free(head_level);
			head_level = NULL;
		}
		else
		{
			gtk_tree_store_remove(tree_store, &type_iter[i]);
		}
		g_slist_free(dupe_finder_list[i]);
		dupe_finder_list[i] = NULL;
	}
}

static void list_relatives_load(GSList *properties, GtkBuilder *gui_builder, WNIRequestFlags id)
{
	guint16 i = 0;
	guint text_weight = PANGO_WEIGHT_NORMAL;
	WNIPropertyItem *temp_property = NULL;
	GtkTreeView *tree_view = NULL;
	GtkTreeStore *tree_store = NULL;
	GtkTreeIter iter = {0};

	for(i = 0; 1 != id; (id = id >> 1), i++);

	tree_view = GTK_TREE_VIEW(gtk_builder_get_object(gui_builder, relative_tree[i]));
	tree_store = GTK_TREE_STORE(gtk_tree_view_get_model(tree_view));
	g_assert(tree_store);

	while(properties)
	{
		temp_property = (WNIPropertyItem*) properties->data;
		if(temp_property)
		{
			gtk_tree_store_append(tree_store, &iter, NULL);
			text_weight = PANGO_WEIGHT_NORMAL;

			// check if synonym has more than one mapping
			// we try to get the 2nd term (I.e. index 1) instead of checking the length; parsing the whole list is insane
			if(NULL != g_slist_nth(temp_property->mapping, 1))
				text_weight = PANGO_WEIGHT_BOLD;

			gtk_tree_store_set(tree_store, &iter, 0, temp_property->term, 1, text_weight, -1);
		}
		properties = g_slist_next(properties);
	}
}

static void domains_load(GSList *properties, GtkBuilder *gui_builder)
{
	guint8 i = 0;
	WNIClassItem *class_item = NULL;
	GtkTreeView *tree_class = GTK_TREE_VIEW(gtk_builder_get_object(gui_builder, relative_tree[TREE_DOMAIN]));
	GtkTreeStore *class_tree_store = GTK_TREE_STORE(gtk_tree_view_get_model(tree_class));
	GtkTreeIter iter = {0}, class_iter[DOMAINS_COUNT] = {{0}};

	g_assert(class_tree_store);

	for(i = 0; i < DOMAINS_COUNT; i++)
	{
		gtk_tree_store_append(class_tree_store, &class_iter[i], NULL);
		gtk_tree_store_set(class_tree_store, &class_iter[i], 0, domain_types[i], 1, PANGO_WEIGHT_SEMIBOLD, -1);
	}

	while(properties)
	{
		class_item = (WNIClassItem*) properties->data;
		if(class_item)
		{
			gtk_tree_store_append(class_tree_store, &iter, &class_iter[class_item->type - CLASSIF_CATEGORY]);
			gtk_tree_store_set(class_tree_store, &iter, 0, class_item->term, 1, PANGO_WEIGHT_NORMAL, -1);
		}
		properties = g_slist_next(properties);
	}
	
	for(i = 0; i < DOMAINS_COUNT; i++)
	{
		if(!gtk_tree_model_iter_has_child(GTK_TREE_MODEL(class_tree_store), &class_iter[i]))
			gtk_tree_store_remove(class_tree_store, &class_iter[i]);
	}

	gtk_tree_view_expand_all(tree_class);
}

static guint8 get_attribute_pos()
{
	GSList *temp_results = results, *def_list = NULL;
	WNINym *temp_nym = NULL;
	WNIPropertyItem *prop_item = NULL;
	WNIPropertyMapping *prop_mapping = NULL;
	WNIDefinitionItem *def_item = NULL;

	while(temp_results)
	{
		temp_nym = (WNINym*) temp_results->data;
		if(WORDNET_INTERFACE_ATTRIBUTES == temp_nym->id)
		{
			prop_item = (WNIPropertyItem*) (((WNIProperties*) temp_nym->data)->properties_list)->data;
			prop_mapping = (WNIPropertyMapping*) prop_item->mapping->data;
			break;
		}
		temp_results = g_slist_next(temp_results);
	}

	temp_results = results;
	while(temp_results)
	{
		temp_nym = (WNINym*) temp_results->data;
		if(WORDNET_INTERFACE_OVERVIEW == temp_nym->id)
		{
			def_list = ((WNIOverview*) temp_nym->data)->definitions_list;
			while(def_list)
			{
				if(def_list)
				{
					def_item = (WNIDefinitionItem*) def_list->data;
					if(def_item->id == prop_mapping->id)
					{
						return def_item->pos;
					}
				}
				def_list = g_slist_next(def_list);
			}

			break;
		}
		
		temp_results = g_slist_next(temp_results);
	}
	
	return 0;	// control should never reach here
}

static void relatives_load(GtkBuilder *gui_builder, gboolean reset_tabs)
{
	GSList *temp_results = results;
	WNINym *temp_nym = NULL;

	guint8 i = 0;
	gboolean store_empty = TRUE, page_set = FALSE;
	GtkTreeView *temp_tree_view = NULL;
	GtkTreeStore *temp_tree_store = NULL;
	GtkWidget *relative_tab = NULL, *expander = NULL;
	GtkTreeIter temp_iter = {0};
	
	GtkLabel *tab_label = NULL;

	// instead of invalidate rect redraw, we just toggle visibility
	expander = GTK_WIDGET(gtk_builder_get_object(gui_builder, EXPANDER));
	gtk_widget_hide(expander);

	while(temp_results)
	{
		temp_nym = (WNINym*) temp_results->data;
		switch(temp_nym->id)
		{
			case WORDNET_INTERFACE_OVERVIEW:
			{
				list_relatives_load(((WNIOverview*) temp_nym->data)->synonyms_list, gui_builder, temp_nym->id);
				break;
			}
			case WORDNET_INTERFACE_ANTONYMS:
			{
				antonyms_load(((WNIProperties*) temp_nym->data)->properties_list, gui_builder);
				break;
			}
			case WORDNET_INTERFACE_ATTRIBUTES:
			case WORDNET_INTERFACE_DERIVATIONS:
			case WORDNET_INTERFACE_CAUSES:
			case WORDNET_INTERFACE_ENTAILS:
			case WORDNET_INTERFACE_SIMILAR:
			{
				list_relatives_load(((WNIProperties*) temp_nym->data)->properties_list, gui_builder, temp_nym->id);
				break;
			}
			case WORDNET_INTERFACE_PERTAINYMS:
			case WORDNET_INTERFACE_HYPERNYMS:
			case WORDNET_INTERFACE_HYPONYMS:
			{
				trees_load(((WNIProperties*) temp_nym->data)->properties_list, gui_builder, temp_nym->id);
				break;
			}
			case WORDNET_INTERFACE_HOLONYMS:
			case WORDNET_INTERFACE_MERONYMS:
			{
				holo_mero_load(((WNIProperties*) temp_nym->data)->properties_list, gui_builder, temp_nym->id);
				break;
			}
			case WORDNET_INTERFACE_CLASS:
			{
				domains_load(((WNIProperties*) temp_nym->data)->properties_list, gui_builder);
				break;
			}
			default:
				break;
		}
		temp_results = g_slist_next(temp_results);
		i++;
	}

	// set tab visibility
	for(i = 0; i < TOTAL_RELATIVES; i++)
	{
		temp_tree_view = GTK_TREE_VIEW(gtk_builder_get_object(gui_builder, relative_tree[i]));
		relative_tab = gtk_widget_get_parent(GTK_WIDGET(temp_tree_view));

		temp_tree_store = GTK_TREE_STORE(gtk_tree_view_get_model(temp_tree_view));
		if(temp_tree_store)
		{
			store_empty = !gtk_tree_model_get_iter_first(GTK_TREE_MODEL(temp_tree_store), &temp_iter);
		}
		if(store_empty || NULL == temp_tree_store)
			gtk_widget_hide(relative_tab);
		else if(reset_tabs && !page_set)
		{
			gtk_notebook_set_current_page(GTK_NOTEBOOK(gtk_builder_get_object(gui_builder, NOTEBOOK)), i);
			page_set = TRUE;
		}
		
		// see if the term has attributes in first place (check visibility for that)
		// if available, set the label based on the POS
		if(gtk_widget_get_visible(GTK_WIDGET(relative_tab)) && TREE_ATTRIBUTES == i)
		{
			tab_label = GTK_LABEL(gtk_builder_get_object(gui_builder, LABEL_ATTRIBUTES));
			if(get_attribute_pos() == ADJ)
				gtk_label_set_text(tab_label, LABEL_TEXT_ATTRIBUTE_OF);
			else
				gtk_label_set_text(tab_label, LABEL_TEXT_ATTRIBUTES);
		}
	}

	// instead of invalidate rect redraw, we just toggle visibility
	gtk_widget_show(expander);

	// if the expander was contracted automatically, then expand it back again
	if(!gtk_expander_get_expanded(GTK_EXPANDER(expander)) && auto_contract)
		gtk_expander_set_expanded(GTK_EXPANDER(expander), TRUE);
}

static gchar* wildmat_to_regex(const gchar *wildmat)
{
	guint16 i = 0;
	gchar ch = 0;
	GString *regex = g_string_new("^");

	/* Conversion Rules are simple at this point.
	   1. To get the proper term from start to end, make sure the beginning and end of the regex is ^ and $ respectively
	   2. E.g. in 'a*b', * is applied to a in regex, while in wildmat, 'a' & 'b' is sure included and * is anything b/w them
	   so in regex make it as 'a.*b', which will make * applied to . (dot means any char. except newline in regex)
	   3. A dot/period in regex acts as a Joker (?) in wildmat, so make a direct conv.
	   4. Other chars go in as such, its the same in both for stuff like [r|m|s]{m,n} or [a-m]+, etc. */

	while((ch = wildmat[i++]) != '\0')
	{
		if(ch == '*')
			regex = g_string_append(regex, ".*");
		else if(ch == '?')
			regex = g_string_append_c(regex, '.');
		else
			regex = g_string_append_c(regex, ch);
		
	}
	
	regex = g_string_append_c(regex, '$');
	regex = g_string_append_c(regex, '\0');

	return g_string_free(regex, FALSE);
}

static gboolean set_regex_results(const gchar *wildmat_exp, GtkBuilder *gui_builder)
{
	gchar *regex_pattern = NULL, *lemma = NULL;
	GRegex *regex = NULL;
	GMatchInfo *match_info = NULL;
	guint32 count = 0;
	GtkTextView *text_view = NULL;
	GtkTextBuffer *text_buffer = NULL;
	GtkTextIter cur = {0};

	gchar status_msg[MAX_STATUS_MSG] = "";
	gboolean results_set = FALSE;


	// convert the wilmat expr. to PERL regex
	if(wildmat_exp)
		regex_pattern = wildmat_to_regex(wildmat_exp);

	if(regex_pattern)
	{
		text_view = GTK_TEXT_VIEW(gtk_builder_get_object(gui_builder, TEXT_VIEW_DEFINITIONS));
		text_buffer = gtk_text_view_get_buffer(text_view);

		// clear text and get the iter
		gtk_text_buffer_set_text(text_buffer, "", -1);
		gtk_text_buffer_get_start_iter(text_buffer, &cur);

		// set heading that Regex mode is now in action
		gtk_text_buffer_insert_with_tags_by_name(text_buffer, &cur, STR_REGEX_DETECTED, -1, TAG_LEMMA, NULL);
		gtk_text_buffer_insert(text_buffer, &cur, NEW_LINE, -1);

		// compile a GRegex
		regex = g_regex_new(regex_pattern, G_REGEX_MULTILINE|G_REGEX_CASELESS|G_REGEX_UNGREEDY|G_REGEX_OPTIMIZE, G_REGEX_MATCH_NOTEMPTY, NULL);

		if(regex)
		{
			// do the actual regex matching and get the count
			g_regex_match(regex, wordnet_terms->str, G_REGEX_MATCH_NOTEMPTY, &match_info);
			count = g_match_info_get_match_count(match_info);

			// make a header for the search results to follow
			if(count > 0)
			{
				gtk_text_buffer_insert_with_tags_by_name(text_buffer, &cur, STR_SUGGEST_MATCHES, -1, TAG_MATCH, NULL);
			}

			// initialize count to 0 for the actual count to be displayed in the status bar
			count = 0;

			// insert the matches one by one
			while(g_match_info_matches(match_info))
			{

				lemma = g_match_info_fetch(match_info, 0);
				gtk_text_buffer_insert(text_buffer, &cur, NEW_LINE, -1);
				gtk_text_buffer_insert_with_tags_by_name(text_buffer, &cur, lemma, -1, TAG_SUGGESTION, NULL);
				g_free(lemma);
				lemma = NULL;
				g_match_info_next (match_info, NULL);
				count++;
			}

			// free all the initialised data
			g_match_info_free(match_info);
			match_info = NULL;
			g_regex_unref(regex);

			// set the status bar accordingly with the count
			status_set("");
			g_snprintf(status_msg, MAX_STATUS_MSG, STR_STATUS_REGEX, count, count > 0?STR_STATUS_LOOKUP_HINT:"");
			status_set(status_msg);
		}

		// if regex was invalid (it would be NULL)
		// or if the expr. fetched no results, set regex failed message
		if(NULL == regex || 0 == count)
		{
			gtk_text_buffer_insert_with_tags_by_name(text_buffer, &cur, STR_REGEX_FAILED, -1, TAG_SUGGESTION, NULL);
		}
		else
			results_set = TRUE;

		g_free(regex_pattern);
		regex_pattern = NULL;
	}
	return results_set;
}

static gboolean is_wildmat_expr(const gchar *expr)
{
	guint16 i = 0;
	gchar ch = 0;

	if(expr)
	{
		// if the passed expr. has any of the char below deem it as a wildmat expr.
		while((ch = expr[i++]) != '\0')
		{
			if(ch == '*' || ch == '?' || ch == '[' || ch == ']' || ch =='{' || ch == '}' || ch == '+')
				return TRUE;
		}
	}

	return FALSE;
}

static gboolean handle_wildmat_expr(GtkBuilder *gui_builder, GtkTextBuffer *buffer)
{
	GtkWindow *window = GTK_WINDOW(gtk_builder_get_object(gui_builder, WINDOW_MAIN));
	GtkExpander *expander = GTK_EXPANDER(gtk_builder_get_object(gui_builder, EXPANDER));

	GtkComboBox *combo_query = GTK_COMBO_BOX(gtk_builder_get_object(gui_builder, COMBO_QUERY));
	GtkEntry *query_entry = GTK_ENTRY(gtk_combo_box_get_child(combo_query));

	gchar status_msg[MAX_STATUS_MSG] = "";
	gboolean results_set = FALSE;

	// Check if the fed string is a wildmat expr.
	// If true, call the regex mod. to set the results and return
	gchar *regex_text = g_strstrip(g_strdup(gtk_editable_get_text(GTK_EDITABLE(query_entry))));
	if(is_wildmat_expr(regex_text))
	{
		// clear previously set relatives
		// clear search successful flag; without this when the prev. looked up
		// word is again dbl clicked from the results, it won't jump to the word
		relatives_clear_all(gui_builder);
		last_search_successful = FALSE;

		// contract the expander, since relatives aren't required here
		if(gtk_expander_get_expanded(expander))
		{
			gtk_expander_set_expanded(expander, FALSE);
			auto_contract = TRUE;
		}

		status_set("");

		if(wordnet_terms)
		{
			// set the status bar to inform that the search is on going
			g_snprintf(status_msg, MAX_STATUS_MSG, STR_STATUS_SEARCHING);
			status_set(status_msg);


			/* if the results were set rightly, then add it to the search history */
			if(set_regex_results(regex_text, gui_builder))
				update_history(combo_query, regex_text);
		}
		else
		{
			GtkTextIter cur = {0};
			gchar *lemma = NULL;
			// report that the search index (index.sense) is missing
			g_snprintf(status_msg, MAX_STATUS_MSG, STR_STATUS_REGEX_FILE_MISSING);
			status_set(status_msg);

			gtk_text_buffer_set_text(buffer, "", -1);
			gtk_text_buffer_get_end_iter(buffer, &cur);

			lemma = g_strdup_printf(STR_REGEX_FILE_MISSING, SetSearchdir());
			if(lemma)
			{
				gtk_text_buffer_insert_with_tags_by_name(buffer, &cur, lemma, -1, TAG_POS, NULL);

				g_free(lemma);
				lemma = NULL;
			}
		}
		results_set = TRUE;
	}

	g_free(regex_text);
	regex_text = NULL;
	
	return results_set;
}

static gboolean is_entry_unique(GtkTreeModel *query_list_store, GtkTreePath *path, GtkTreeIter *iter, gchar **lemma)
{
	gboolean duplicate = FALSE;
	gchar *str_list_item = NULL;
	gtk_tree_model_get(query_list_store, iter, 0, &str_list_item, -1);
	duplicate = (0 == g_strcmp0(*lemma, str_list_item));
	g_free(str_list_item);
	str_list_item = NULL;
	if(duplicate)
	{
		*lemma = NULL;
	}
	return duplicate;
}

static gboolean update_history(GtkComboBox *combo_query, char *lemma)
{
	GtkListStore *query_list_store = GTK_LIST_STORE(gtk_combo_box_get_model(combo_query));
	gboolean inserted = FALSE;

	if(!query_list_store)
		g_error("Unable to get query combo box's model!");

	gtk_tree_model_foreach(GTK_TREE_MODEL(query_list_store), (GtkTreeModelForeachFunc) is_entry_unique, (gpointer) &lemma);

	if(lemma)
	{
		GtkTreeIter query_list_iter = {0};
		gtk_list_store_prepend(query_list_store, &query_list_iter);
		gtk_list_store_set(query_list_store, &query_list_iter, 0, lemma, -1);
		inserted = TRUE;
	}
	return inserted;
}

static gboolean update_history_and_save(GtkComboBox *combo_query, char *lemma)
{
	gboolean saved = FALSE;
	if(update_history(combo_query, lemma))
	{
		static gboolean update_history_in_file_fail_notifed = FALSE;
		if(update_history_in_file(lemma))
		{
			saved = TRUE;
		}
		else if(!update_history_in_file_fail_notifed)
		{
			show_message_dialog(NULL, GTK_MESSAGE_ERROR, ARTHA_NAME, "%s", STR_HISTORY_FILE_UPDATE_FAILED);
			update_history_in_file_fail_notifed = TRUE;
		}
	}
	return saved;
}

static void show_definitions(GtkBuilder *gui_builder, GtkTextBuffer *buffer)
{
	g_assert(results);

	GSList *def_list = ((WNIOverview*)((WNINym*)results->data)->data)->definitions_list;
	WNIDefinitionItem *def_item = NULL;
	GSList *definitions_list = NULL, *example = NULL;
	GtkTextMark *freq_marker = NULL;
	GtkTextIter cur = {0};
	glong count = 0;
	WNIDefinition *defn = NULL;
	guint16 total_results = 0;
	guint8 total_pos = 0;

	gchar str_count[MAX_SENSE_DIGITS] = "", status_msg[MAX_STATUS_MSG] = "";

	GtkComboBox *combo_query = GTK_COMBO_BOX(gtk_builder_get_object(gui_builder, COMBO_QUERY));
	
	while(def_list)
	{
		if(def_list->data)
		{
			def_item = (WNIDefinitionItem*) def_list->data;
			definitions_list = def_item->definitions;

			gtk_text_buffer_get_end_iter(buffer, &cur);
			gtk_text_buffer_insert_with_tags_by_name(buffer, &cur, def_item->lemma, -1, TAG_LEMMA, NULL);
			gtk_text_buffer_insert(buffer, &cur, " ~ ", -1);
			gtk_text_buffer_insert_with_tags_by_name(buffer, &cur, partnames[def_item->pos], -1, TAG_POS, NULL);

			freq_marker = gtk_text_buffer_create_mark(buffer, NULL, &cur, TRUE);

			gtk_text_buffer_insert_with_tags_by_name(buffer, &cur, NEW_LINE, -1, TAG_POS, NULL);

			count = 1;
			while(definitions_list)
			{
				defn = (WNIDefinition*) definitions_list->data;
				
				total_results++;
				g_snprintf(str_count, MAX_SENSE_DIGITS, "%3d", total_results);
				gtk_text_buffer_create_mark(buffer, str_count, &cur, TRUE);

				g_snprintf(str_count, MAX_SENSE_DIGITS, "%2d. ", (int)count);
				
				gtk_text_buffer_insert_with_tags_by_name(buffer, &cur, str_count, -1, TAG_COUNTER, NULL);
				gtk_text_buffer_insert(buffer, &cur, defn->definition, -1);
			
				example = defn->examples;

				definitions_list = g_slist_next(definitions_list);
				if(example || definitions_list) gtk_text_buffer_insert(buffer, &cur, NEW_LINE, -1);

				while(example)
				{
					gtk_text_buffer_insert_with_tags_by_name(buffer, &cur, (gchar*) example->data, -1, TAG_EXAMPLE, NULL);
					example = g_slist_next(example);
					// for each example come to appear in a new line enabled this line and 
					// comment the next two buffer inserts of "; " and new line
					//if(example || definitions_list) gtk_text_buffer_insert(buffer, &cur, NEW_LINE, -1);

					if(example)
						gtk_text_buffer_insert(buffer, &cur, "; ", -1);
					else if(definitions_list)
						gtk_text_buffer_insert(buffer, &cur, NEW_LINE, -1);
				}
				count++;
			}

			if(show_polysemy)
			{
				gtk_text_buffer_get_iter_at_mark(buffer, &cur, freq_marker);

				gtk_text_buffer_insert(buffer, &cur, "    ", -1);
				gtk_text_buffer_insert_with_tags_by_name(buffer, 
									&cur, 
									familiarity[get_frequency(count - 1)], 
									-1, 
									familiarity[get_frequency(count - 1)], 
									NULL);
			}
		}
		def_list = g_slist_next(def_list);
		if(def_list)
		{
			gtk_text_buffer_get_end_iter(buffer, &cur);
			gtk_text_buffer_insert(buffer, &cur, NEW_LINE, -1);
		}

		total_pos++;
	}
	// end of setting definitions

	/* scroll to the text view top to show the first definition
	gtk_text_buffer_get_start_iter(buffer, &cur);
	gtk_text_view_scroll_to_iter(text_view, &cur, 0.0, FALSE, 0.0, 0.0); */

	// set status
	status_set("");
	g_snprintf(status_msg, MAX_STATUS_MSG, STR_STATUS_QUERY_SUCCESS, total_results, total_pos);
	status_set(status_msg);

	// manage history
	update_history_and_save(combo_query,
	((WNIDefinitionItem*)((WNIOverview*)((WNINym*)results->data)->data)->definitions_list->data)->lemma);
}

static void show_suggestions(GtkBuilder *gui_builder, GtkTextBuffer *buffer, GtkTextIter *cur, GSList *suggestions)
{
	GtkExpander *expander = GTK_EXPANDER(gtk_builder_get_object(gui_builder, EXPANDER));

	gtk_text_buffer_insert(buffer, cur, NEW_LINE, -1);
	gtk_text_buffer_insert_with_tags_by_name(buffer, cur, STR_SUGGEST_MATCHES, -1, TAG_MATCH, NULL);

	while(suggestions)
	{
		gtk_text_buffer_insert(buffer, cur, NEW_LINE, -1);
		gtk_text_buffer_insert_with_tags_by_name(buffer,
							 cur,
							 suggestions->data,
							 -1,
							 TAG_SUGGESTION,
							 NULL);
		suggestions = g_slist_next(suggestions);
	}

	// contract the expander, since relatives aren't required here
	if(gtk_expander_get_expanded(expander))
	{
		gtk_expander_set_expanded(expander, FALSE);
		auto_contract = TRUE;
	}
}

static void construct_show_notification()
{
	const gchar *lemma = ((WNIDefinitionItem*)((WNIOverview*)((WNINym*)results->data)->data)->definitions_list->data)->lemma;
	gchar *definition = 
	g_strconcat(partnames[((WNIDefinitionItem*)((WNIOverview*)((WNINym*)results->data)->data)->definitions_list->data)->pos],
				". ",
				((WNIDefinition*)((WNIDefinitionItem*)((WNIOverview*)((WNINym*)results->data)->data)->definitions_list->data)->definitions->data)->definition,
				NULL);

	// close any previous updates
	notify_notification_close(mod_notifier, NULL);

	if(definition)
	{
		gboolean update_successful = notify_notification_update(mod_notifier, lemma, definition, "gtk-dialog-info");
		if(!update_successful || (FALSE == notify_notification_show(mod_notifier, NULL)))
		{
			g_warning("Failed to display notification\n");
		}
		g_free(definition);
		definition = NULL;
	}
}

static void show_searching(GtkBuilder *gui_builder)
{
	gchar status_msg[MAX_STATUS_MSG] = "";

	(void) gui_builder;


	// set the status bar message
	status_set("");
	g_snprintf(status_msg, MAX_STATUS_MSG, STR_STATUS_SEARCHING);
	status_set(status_msg);
}

static void query_wni(gchar *search_str,
					  WNIRequestFlags lookup_type,
					  GSList **suggestions)
{
	G_MESSAGE("'%s' requested from WNI!\n", search_str);

	wni_request_nyms(search_str, &results, lookup_type, advanced_mode);

	// if the search failed and suggestions are available, check if the prime
	// suggestion is the same as the search string
	if(!results && mod_suggest)
	{
		GSList *temp_suggestions = suggestions_get(search_str);
		if(temp_suggestions && are_same_after_strip(search_str, temp_suggestions->data))
		{
			wni_request_nyms(temp_suggestions->data, &results, lookup_type, advanced_mode);
		}
		// if the caller wanted the suggestions and the suggestions lookup succeeded
		// then don't free the results, but pass it on
		else if(suggestions)
		{
			*suggestions = temp_suggestions;
		}

		// if not passing on, free
		if(!suggestions || (*suggestions != temp_suggestions))
		{
			g_slist_free_full(temp_suggestions, g_free);
		}
	}
}

static void button_search_click(GtkButton *button, gpointer user_data)
{
	GtkBuilder *gui_builder = GTK_BUILDER(user_data);
	GtkComboBox *combo_query = GTK_COMBO_BOX(gtk_builder_get_object(gui_builder, COMBO_QUERY));
	GtkEntry *combo_entry = GTK_ENTRY(gtk_combo_box_get_child(combo_query));
	GtkTextView *text_view = GTK_TEXT_VIEW(gtk_builder_get_object(gui_builder, TEXT_VIEW_DEFINITIONS));
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);

	gchar *search_str = NULL;

	// if it was a regex, then results are now furnished, so return
	if(handle_wildmat_expr(gui_builder, buffer))
		return;

	// from here on normal lookup starts
	search_str = g_strdup(gtk_editable_get_text(GTK_EDITABLE(combo_entry)));
	search_str = g_strstrip(strip_invalid_edges(search_str));

	// make sure this isn't a 0 length string
	if(search_str && search_str[0])
	{
		gboolean new_search = TRUE;
		GSList *suggestions = NULL;

		G_MESSAGE("'%s' queried!\n", search_str);

		if(last_search_successful)
			new_search = (g_ascii_strcasecmp(last_search, search_str) != 0);

		// check if this isn't the previous search query
		if(new_search)
		{
			show_searching(gui_builder);
			query_wni(search_str, WORDNET_INTERFACE_ALL, &suggestions);

			// clear prior text in definitons text view & relatives
			gtk_text_buffer_set_text(buffer, "", -1);
			relatives_clear_all(gui_builder);

			// set definitions
			if(results)
			{
				G_MESSAGE("Results successful!\n");

				show_definitions(gui_builder, buffer);
				relatives_load(gui_builder, TRUE);

				g_stpcpy(last_search, search_str);
				last_search_successful = TRUE;
			}
			else
			{
				GtkTextIter cur = {0};
				gtk_text_buffer_set_text(buffer, "", -1);
				gtk_text_buffer_get_start_iter(buffer, &cur);
				gtk_text_buffer_insert_with_tags_by_name(buffer, &cur, STR_QUERY_FAILED, -1, TAG_POS, NULL);

				if(suggestions)
					show_suggestions(gui_builder, buffer, &cur, suggestions);

				/* status bar should be updated */
				status_set("");
				status_set(STR_STATUS_QUERY_FAILED);

				last_search_successful = FALSE;
			}
			if(suggestions)
			{
				g_slist_free_full(suggestions, g_free);
				suggestions = NULL;
			}
			GtkWindow *window = GTK_WINDOW(gtk_builder_get_object(gui_builder, WINDOW_MAIN));
			show_window(window);
		}

		gtk_editable_select_region(GTK_EDITABLE(combo_entry), 0, -1);
		gtk_widget_grab_focus(GTK_WIDGET(combo_query));
	}

	if(search_str)
	{
		g_free(search_str);
		search_str = NULL;
	}
	last_lookup_a_notification = FALSE;
}

static void combo_query_changed(GtkComboBox *combo_query, gpointer user_data)
{
	GtkBuilder *gui_builder = GTK_BUILDER(user_data);
	GtkButton *button_search = GTK_BUTTON(gtk_builder_get_object(gui_builder, BUTTON_SEARCH));
	GtkTreeModel *tree_model = NULL;
	gint selected_item = gtk_combo_box_get_active(combo_query);
	gint total_items = 0;

	if(selected_item != -1)
	{
		g_signal_emit_by_name(button_search, "clicked");
		tree_model = gtk_combo_box_get_model(combo_query);
		total_items = gtk_tree_model_iter_n_children(tree_model, NULL);

		if(total_items > 1)
		{
			if(0 == selected_item)
			{
				gtk_widget_set_sensitive(toolbar_prev_btn, TRUE);
				gtk_widget_set_sensitive(toolbar_next_btn, FALSE);
			}
			else if(total_items == selected_item + 1)
			{
				gtk_widget_set_sensitive(toolbar_prev_btn, FALSE);
				gtk_widget_set_sensitive(toolbar_next_btn, TRUE);
			}
			else
			{
				gtk_widget_set_sensitive(toolbar_next_btn, TRUE);
				gtk_widget_set_sensitive(toolbar_prev_btn, TRUE);
			}
		}
	}
}

static void lookup_selection_in_buffer(GtkTextBuffer *buffer, GtkBuilder *gui_builder)
{
	GtkComboBox *combo_query = NULL;
	GtkEntry *txtQuery = NULL;
	GtkButton *button_search = NULL;
	GtkTextIter start, end;
	gchar *selection = NULL;

	if(!gtk_text_buffer_get_selection_bounds(buffer, &start, &end))
		return;

	selection = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	if(selection == NULL || *selection == '\0')
	{
		g_free(selection);
		return;
	}

	combo_query = GTK_COMBO_BOX(gtk_builder_get_object(gui_builder, COMBO_QUERY));
	txtQuery = GTK_ENTRY(gtk_combo_box_get_child(combo_query));
	button_search = GTK_BUTTON(gtk_builder_get_object(gui_builder, BUTTON_SEARCH));

	if(0 != g_strcmp0(gtk_editable_get_text(GTK_EDITABLE(txtQuery)), selection))
		gtk_editable_set_text(GTK_EDITABLE(txtQuery), selection);

	g_free(selection);
	selection = NULL;

	g_signal_emit_by_name(button_search, "clicked");
}

static void text_view_clicked(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
	GtkBuilder *gui_builder = GTK_BUILDER(user_data);
	GtkWidget *widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
	GtkTextView *defn_text_view = GTK_TEXT_VIEW(widget);
	GtkTextBuffer *defn_text_buffer = gtk_text_view_get_buffer(defn_text_view);
	GtkTextMark *insert_mark = NULL, *selection_mark = NULL;
	GtkTextIter ins_iter = {0}, sel_iter = {0};
	GdkEvent *event = NULL;
	GdkModifierType state = 0;
	gchar *trial_text = NULL;
	guint8 i = 0;
	gboolean iter_move_success = FALSE, double_click = FALSE, ctrl_held = FALSE;

	(void) x;
	(void) y;

	event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(gesture));
	if(event != NULL)
		state = gdk_event_get_modifier_state(event);

	double_click = (2 == n_press);
	ctrl_held = (0 != (state & GDK_CONTROL_MASK));

	/* a plain click keeps GTK's native selection handling;
	   double click / Ctrl+click additionally tries a WordNet lookup */
	if(!double_click && !ctrl_held)
		return;

	if(double_click && gtk_text_buffer_get_has_selection(defn_text_buffer))
	{
		/* if it was a double click, try selecting diff. combinations
		   for the sake of compound lemmas like 'work out', 'in a nutshell', etc. */
		selection_mark = gtk_text_buffer_get_selection_bound(defn_text_buffer);
		insert_mark = gtk_text_buffer_get_mark(defn_text_buffer, "insert");

		for(i = 0; ((i < 6) && (!iter_move_success)); i++)
		{
			/* on every iteration of the loop, set the iters to the actual sel. marks */
			gtk_text_buffer_get_iter_at_mark(defn_text_buffer, &ins_iter, insert_mark);
			gtk_text_buffer_get_iter_at_mark(defn_text_buffer, &sel_iter, selection_mark);

			switch(i)
			{
				case 0:
					/* try selecting two words after the actual sel. */
					if(!gtk_text_iter_forward_visible_word_ends(&sel_iter, 2))
						gtk_text_iter_forward_to_end(&sel_iter);
					iter_move_success = TRUE;
					break;
				case 1:
					/* try selecting two words before the actual sel. */
					if(gtk_text_iter_backward_visible_word_starts(&ins_iter, 2))
						iter_move_success = TRUE;
					break;
				case 2:
					/* try selecting one word before and after the actual sel. */
					if((gtk_text_iter_forward_visible_word_end(&sel_iter)) && (gtk_text_iter_backward_visible_word_start(&ins_iter)))
						iter_move_success = TRUE;
					break;
				case 3:
					/* try selecting one word next to the actual sel. */
					if(!gtk_text_iter_forward_visible_word_end(&sel_iter))
						gtk_text_iter_forward_to_end(&sel_iter);
					iter_move_success = TRUE;
					break;
				case 4:
					/* try selecting one word before the actual sel. */
					if(gtk_text_iter_backward_visible_word_start(&ins_iter))
						iter_move_success = TRUE;
					break;
				case 5:
					/* fallback to just the actual selection */
					iter_move_success = TRUE;
					break;
			}

			if(iter_move_success)
			{
				/* obtain the text for the set iters */
				trial_text = gtk_text_buffer_get_text(defn_text_buffer, &ins_iter, &sel_iter, FALSE);

				/* search the trial selection in WordNet; on a successful lookup iter_move_success is set */
				iter_move_success = wni_request_nyms(trial_text, NULL, (WNIRequestFlags) 0, FALSE);

				g_free(trial_text);
				trial_text = NULL;
			}
		}

		/* set the 'insert' and 'selection_bound' markers to the newly placed iters */
		gtk_text_buffer_move_mark(defn_text_buffer, insert_mark, &ins_iter);
		gtk_text_buffer_move_mark(defn_text_buffer, selection_mark, &sel_iter);
	}

	lookup_selection_in_buffer(defn_text_buffer, gui_builder);
}

static void expander_clicked(GtkExpander *expander, gpointer user_data)
{
	GtkBuilder *gui_builder = GTK_BUILDER(user_data);
	GtkPaned *vpaned = GTK_PANED(gtk_builder_get_object(gui_builder, VPANE));

	gtk_paned_set_position(vpaned, -1);		// unset the pane's position so that it goes to the original state

	// if the user manually expands/contracts it, reset the auto flag
	auto_contract = FALSE;
}

static void query_list_updated(GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
	GtkBuilder *gui_builder = GTK_BUILDER(user_data);
	(void) gui_builder;
	GtkTreeIter temp_iter = {0};

	if(gtk_tree_model_iter_nth_child(tree_model, &temp_iter, NULL, 1))
	{
		gtk_widget_set_sensitive(toolbar_prev_btn, TRUE);
		gtk_widget_set_sensitive(toolbar_next_btn, FALSE);
	}
}

static GtkTextMark* highlight_definition(guint8 id, guint16 sense, GtkTextView *text_view)
{
	guint16 defn_no = 0;
	GSList *wni_results = results, *definitions_list = NULL;
	GtkTextBuffer *buffer = NULL;
	GtkTextMark *mark = NULL;
	GtkTextIter start = {0}, end = {0};
	gchar str_index[MAX_SENSE_DIGITS] = "";

	while(wni_results)
	{
		if(WORDNET_INTERFACE_OVERVIEW == ((WNINym*)wni_results->data)->id)
		{
			definitions_list = ((WNIOverview*)((WNINym*)wni_results->data)->data)->definitions_list;
			
			while(definitions_list)
			{
				if(id == ((WNIDefinitionItem*)definitions_list->data)->id)
				{
					defn_no += sense + 1;
					buffer = gtk_text_view_get_buffer(text_view);

					g_snprintf(str_index, MAX_SENSE_DIGITS, "%3d", defn_no);
					mark = gtk_text_buffer_get_mark(buffer, str_index);
					if(mark)
					{
						gtk_text_buffer_get_iter_at_mark(buffer, &start, mark);
						gtk_text_buffer_get_iter_at_offset(buffer, &end, gtk_text_iter_get_offset(&start) + 3);
						gtk_text_buffer_apply_tag_by_name(buffer, TAG_HIGHLIGHT, &start, &end);
					}
					
					break;
				}
				else
					defn_no += ((WNIDefinitionItem*)definitions_list->data)->count;

				definitions_list = g_slist_next(definitions_list);
			}
			
			break;
		}
		wni_results = g_slist_next(wni_results);
	}
	
	return mark;
}

static void highlight_senses_from_synonyms(guint16 relative_index, GtkTextView *text_view)
{
	GSList *wni_results = results;
	GSList *synonyms_list = NULL;
	GSList *synonym_mapping = NULL;
	GtkTextMark *highlighted_mark = NULL, *scroll_mark = NULL;
	GtkTextBuffer *buffer = NULL;
	GtkTextIter iter = {0};
	guint16 last_offset = 0, current_offset = 0;

	while(wni_results)
	{
		if(WORDNET_INTERFACE_OVERVIEW == ((WNINym*)wni_results->data)->id)
		{
			synonyms_list = ((WNIOverview*) ((WNINym*)wni_results->data)->data)->synonyms_list;
			for(; relative_index; relative_index--, synonyms_list = g_slist_next(synonyms_list));
			synonym_mapping = ((WNIPropertyItem*) synonyms_list->data)->mapping;
			
			buffer = gtk_text_view_get_buffer(text_view);
			gtk_text_buffer_get_end_iter(buffer, &iter);
			// store the last line number
			last_offset = gtk_text_iter_get_line(&iter);

			while(synonym_mapping)
			{
				highlighted_mark = highlight_definition(((WNISynonymMapping*) synonym_mapping->data)->id, ((WNISynonymMapping*) synonym_mapping->data)->sense, text_view);
				
				gtk_text_buffer_get_iter_at_mark(buffer, &iter, highlighted_mark);

				// see if this mark is at a higher line
				current_offset = gtk_text_iter_get_line(&iter);

				if(current_offset <= last_offset)
				{
					last_offset = current_offset;
					scroll_mark = highlighted_mark;
				}

				synonym_mapping = g_slist_next(synonym_mapping);
			}

			gtk_text_view_scroll_to_mark(text_view, scroll_mark, 0.0, TRUE, 0, 0);

			break;
		}

		wni_results = g_slist_next(wni_results);
	}
}

static void highlight_senses_from_antonyms( guint8 category, guint16 relative_index, guint16 sub_level, GtkTextView *text_view)
{
	GSList *wni_results = results;
	GSList *antonyms_list = NULL;
	GSList *antonym_mapping = NULL;
	WNIAntonymItem *antonym_item = NULL;
	guint16 direct_count = 0, indirect_count = 0, count = 0;
	GtkTextBuffer *buffer = NULL;
	GtkTextMark *scroll_mark = NULL, *highlighted_mark = NULL;
	GtkTextIter iter = {0}, end = {0};

	while(wni_results)
	{
		if(WORDNET_INTERFACE_ANTONYMS == ((WNINym*)wni_results->data)->id)
		{
			antonyms_list = ((WNIProperties*) ((WNINym*) wni_results->data)->data)->properties_list;

			while(antonyms_list)
			{
				antonym_item = (WNIAntonymItem*) antonyms_list->data;

				if(antonym_item->relation <= DIRECT_ANT)
					direct_count++;
				else
				{
					count = g_slist_length(antonym_item->implications);
					indirect_count += count;
					
					if(indirect_count > relative_index)
						indirect_count = relative_index + 1;
				}

				if((category <= DIRECT_ANT && direct_count == relative_index + 1) || 
				(category == INDIRECT_ANT && indirect_count == relative_index + 1))
				{
					antonym_mapping = antonym_item->mapping;

					buffer = gtk_text_view_get_buffer(text_view);
					gtk_text_buffer_get_end_iter(buffer, &end);

					// store the last line number
					direct_count = gtk_text_iter_get_line(&end);

					while(antonym_mapping)
					{
						if(sub_level)
						{
							count += ((WNIAntonymMapping*) antonym_mapping->data)->related_word_count;
							if(sub_level > count)
							{
								antonym_mapping = g_slist_next(antonym_mapping);
								continue;
							}
						}

						highlighted_mark = highlight_definition(((WNIAntonymMapping*) antonym_mapping->data)->id, 
						((WNIAntonymMapping*) antonym_mapping->data)->sense, text_view);
						
						gtk_text_buffer_get_iter_at_mark(buffer, &iter, highlighted_mark);

						// see if this mark is at a higher line
						indirect_count = gtk_text_iter_get_line(&iter);

						if(indirect_count <= direct_count)
						{
							direct_count = indirect_count;
							scroll_mark = highlighted_mark;
						}
						
						antonym_mapping = g_slist_next(antonym_mapping);
						
						if(sub_level)
						{
							if(sub_level <= count) break;
						}
					}

					gtk_text_view_scroll_to_mark(text_view, scroll_mark, 0.0, TRUE, 0, 0);

					break;
				}

				antonyms_list = g_slist_next(antonyms_list);
			}

			break;
		}
		
		wni_results = g_slist_next(wni_results);
	}
}

static void highlight_senses_from_domain(guint8 category, guint16 relative_index, GtkTextView *text_view)
{
	GSList *wni_results = results;
	GSList *class_list = NULL;
	WNIClassItem *class_item = NULL;
	guint16 count = 0;

	while(wni_results)
	{
		if(WORDNET_INTERFACE_CLASS == ((WNINym*) wni_results->data)->id)
		{
			class_list = ((WNIProperties*) ((WNINym*) wni_results->data)->data)->properties_list;

			while(class_list)
			{
				class_item = (WNIClassItem*)class_list->data;
				if(category == class_item->type - CLASSIF_CATEGORY)
					count++;

				if(count == relative_index + 1)
				{
					gtk_text_view_scroll_to_mark(text_view, highlight_definition(class_item->id, class_item->sense, text_view), 0.0, TRUE, 0, 0);
					break;
				}

				class_list = g_slist_next(class_list);
			}

			break;
		}

		wni_results = g_slist_next(wni_results);
	}
}

static void highlight_senses_from_relative_lists(WNIRequestFlags id, guint16 relative_index, GtkTextView *text_view)
{
	GSList *wni_results = results;
	GSList *properties_list = NULL;
	GSList *property_mapping = NULL;
	WNIPropertyMapping *mapping = NULL;
	GtkTextMark *highlighted_mark = NULL, *scroll_mark = NULL;
	GtkTextBuffer *buffer = NULL;
	GtkTextIter iter = {0};
	guint16 last_offset = 0, current_offset = 0;

	while(wni_results)
	{
		if(id == ((WNINym*)wni_results->data)->id)
		{
			properties_list = ((WNIProperties*)((WNINym*)wni_results->data)->data)->properties_list;

			for(; relative_index; relative_index--, properties_list = g_slist_next(properties_list));
			property_mapping = ((WNIPropertyItem*) properties_list->data)->mapping;
			
			buffer = gtk_text_view_get_buffer(text_view);
			gtk_text_buffer_get_end_iter(buffer, &iter);
			last_offset = gtk_text_iter_get_line(&iter);
			
			while(property_mapping)
			{
				mapping = (WNIPropertyMapping*) property_mapping->data;
				highlighted_mark = highlight_definition(mapping->id, mapping->sense, text_view);
				
				gtk_text_buffer_get_iter_at_mark(buffer, &iter, highlighted_mark);

				// see if this mark is at a higher line
				current_offset = gtk_text_iter_get_line(&iter);

				if(current_offset <= last_offset)
				{
					last_offset = current_offset;
					scroll_mark = highlighted_mark;
				}

				property_mapping = g_slist_next(property_mapping);
			}
			
			gtk_text_view_scroll_to_mark(text_view, scroll_mark, 0.0, TRUE, 0, 0);

			break;
		}
		wni_results = g_slist_next(wni_results);
	}
}

static void relative_selection_changed(GtkTreeView *tree_view, gpointer user_data)
{
	GtkTreeIter iter = {0};
	GtkTreeModel *tree_store = NULL;
	gchar *term = NULL, *str_path = NULL, *str_category = NULL, *str_demark = NULL;
	gchar str_root[3] = "";
	GtkBuilder *gui_builder = GTK_BUILDER(user_data);
	GtkNotebook *notebook = GTK_NOTEBOOK(gtk_builder_get_object(gui_builder, NOTEBOOK));
	GtkTextView *text_view = GTK_TEXT_VIEW(gtk_builder_get_object(gui_builder, TEXT_VIEW_DEFINITIONS));
	GtkTextBuffer *buffer = NULL;
	GtkTextIter start = {0}, end = {0};
	guint16 sub_level = 0;

	if(gtk_tree_selection_get_selected(gtk_tree_view_get_selection(tree_view), &tree_store, &iter))
	{
		gtk_tree_model_get(tree_store, &iter, 0, &term, -1);
		str_path = gtk_tree_model_get_string_from_iter(tree_store, &iter);
		
		buffer = gtk_text_view_get_buffer(text_view);
		gtk_text_buffer_get_start_iter(buffer, &start);
		gtk_text_buffer_get_end_iter(buffer, &end);
		gtk_text_buffer_remove_tag_by_name(buffer, TAG_HIGHLIGHT, &start, &end);

		if(term && str_path)
		{
			switch(gtk_notebook_get_current_page(notebook))
			{
				case TREE_SYNONYMS:
					highlight_senses_from_synonyms((guint16) g_ascii_strtoull(str_path, NULL, 10), text_view);
					break;
				case TREE_ANTONYMS:
					// to make sure no highlighting is done for categories, check for ':' in the path
					// Categories (Direct/Indirect) won't have a parent, hence only one char. will be 
					// there in the path; if not, a word has got selected
					if(advanced_mode && (str_demark = g_strstr_len(str_path, -1, ":")) != NULL)
					{
						str_demark++;
						str_demark = g_strstr_len(str_demark, -1, ":");
						if(str_demark)
						{
							str_demark++;
							sub_level = (guint16) g_ascii_strtoull(str_demark, NULL, 10) + 1;
						}

						gtk_tree_model_get_iter_first(tree_store, &iter);
						gtk_tree_model_get(tree_store, &iter, 0, &str_category, -1);
						if(str_category)
						{
							if(0 != g_strcmp0(str_category, STR_ANTONYM_HEADER_DIRECT)) str_path[0] = '1';
							g_free(str_category);
							str_category = NULL;
						}

						strip_invalid_edges(&str_path[2]);
						/* if its a word, parent 0 means Direct and 1 Indirect */
						highlight_senses_from_antonyms((0 == str_path[0] - '0') ? DIRECT_ANT : INDIRECT_ANT, 
										(guint16) g_ascii_strtoull(&str_path[2], NULL, 10), 
										sub_level, 
										text_view);
					}
					else if(!advanced_mode)
						// if its simple mode, set category to 0
						highlight_senses_from_antonyms(0, (guint16) g_ascii_strtoull(str_path, NULL, 10), sub_level, text_view);
					break;
				case TREE_DOMAIN:
					if((str_demark = g_strstr_len(str_path, -1, ":")) != NULL)
					{
						g_snprintf(str_root, 3, "%c", str_path[0]);
						gtk_tree_model_get_iter_from_string(tree_store, &iter, str_root);

						gtk_tree_model_get(tree_store, &iter, 0, &str_category, -1);
						for(sub_level = 0; (0 != g_strcmp0(domain_types[sub_level], str_category)); sub_level++);
						g_free(str_category);
						str_category = NULL;

						str_demark++;
						highlight_senses_from_domain((guint8) sub_level, (guint16) g_ascii_strtoull(str_demark, NULL, 10), text_view);
					}
					break;
				case TREE_PERTAINYMS:
				case TREE_HYPERNYMS:
				case TREE_HYPONYMS:
				case TREE_HOLONYMS:
				case TREE_MERONYMS:
					//highlight_senses_from_relative_trees(1 << gtk_notebook_get_current_page(notebook), g_ascii_strtoull(str_path, NULL, 10), text_view);
					break;
				default:
					highlight_senses_from_relative_lists(1 << gtk_notebook_get_current_page(notebook), (guint16) g_ascii_strtoull(str_path, NULL, 10), text_view);
					break;
			}
		}

		g_free(term);
		g_free(str_path);
		term = str_path = NULL;
	}
}

static void relative_row_activated(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
	gchar *sel_term = NULL;
	GtkTreeModel *tree_store = NULL;
	GtkTreeIter iter = {0};
	GtkComboBox *combo_query = NULL;
	GtkEntry *txtQuery = NULL;
	GtkButton *button_search = NULL;
	GtkBuilder *gui_builder = GTK_BUILDER(user_data);


	if(gtk_tree_selection_get_selected(gtk_tree_view_get_selection(tree_view), &tree_store, &iter))
	{
		gtk_tree_model_get(tree_store, &iter, 0, &sel_term, -1);

		if(sel_term)
		{
			combo_query = GTK_COMBO_BOX(gtk_builder_get_object(gui_builder, COMBO_QUERY));
			txtQuery = GTK_ENTRY(gtk_combo_box_get_child(combo_query));
			button_search = GTK_BUTTON(gtk_builder_get_object(gui_builder, BUTTON_SEARCH));

			gtk_editable_set_text(GTK_EDITABLE(txtQuery), sel_term);

			g_signal_emit_by_name(button_search, "clicked");
			
			g_free(sel_term);
			sel_term = NULL;
		}
	}
}

static void clear_history(GSimpleAction *action, GVariant *params, gpointer user_data)
{
	GtkListStore *list_store_query = GTK_LIST_STORE(user_data);
	gchar *hist_file_path = NULL;
	GFile *history_file = NULL;

	(void) action;
	(void) params;

	gtk_list_store_clear(list_store_query);
	hist_file_path = g_strconcat(g_get_user_config_dir(), G_DIR_SEPARATOR_S, ARTHA_TARNAME, HISTORY_FILE_EXT, NULL);
	history_file = g_file_new_for_path(hist_file_path);
	g_free(hist_file_path);
	hist_file_path = NULL;
	if(g_file_query_exists(history_file, NULL))
	{
		g_file_delete(history_file, NULL, NULL);
	}
	g_object_unref(history_file);
	history_file = NULL;
}

/* the user picked a destination file; copy the history there and report the outcome */
static void history_save_copy(GFile *current_file)
{
	GError *err = NULL;
	GFile *history_file = NULL, *dest_file = NULL;
	gchar *hist_file_path = NULL;
	gboolean saved = FALSE;

	hist_file_path = g_strconcat(g_get_user_config_dir(), G_DIR_SEPARATOR_S, ARTHA_TARNAME, HISTORY_FILE_EXT, NULL);
	history_file = g_file_new_for_path(hist_file_path);
	dest_file = g_object_ref(current_file);

	if(g_file_query_exists(history_file, NULL))
	{
		saved = g_file_copy(history_file, dest_file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &err);
	}

	if(saved)
	{
		show_message_dialog(NULL, GTK_MESSAGE_INFO, STR_HISTORY_FILE_SAVE_SUCCESS_TITLE,
				    "%s", STR_HISTORY_FILE_SAVE_SUCCESS);
	}
	else if(err != NULL)
	{
		show_message_dialog(NULL, GTK_MESSAGE_WARNING, STR_HISTORY_FILE_SAVE_FAILURE_TITLE,
				    STR_HISTORY_FILE_SAVE_FAILED, err->message);
		g_clear_error(&err);
	}
	else
	{
		show_message_dialog(NULL, GTK_MESSAGE_WARNING, STR_HISTORY_FILE_SAVE_FAILURE_TITLE,
				    STR_HISTORY_FILE_SAVE_FAILED, STR_HISTORY_MISSING);
	}

	g_object_unref(history_file);
	g_object_unref(dest_file);
	g_free(hist_file_path);
}

/* second stage: the user picked a file; GTK >= 4.10 uses GtkFileDialog */
#if GTK_CHECK_VERSION(4, 10, 0)
static void save_history_copy_done(GObject *source, GAsyncResult *result, gpointer user_data)
{
	GFile *current_file = NULL;
	GError *err = NULL;

	(void) user_data;

	current_file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), result, &err);
	if(NULL == current_file)
	{
		/* a dismissal is not an error; stay silent */
		if(err != NULL && !g_error_matches(err, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
			show_message_dialog(NULL, GTK_MESSAGE_WARNING, STR_HISTORY_FILE_SAVE_FAILURE_TITLE,
					    "%s", err->message);
		g_clear_error(&err);
		return;
	}

	history_save_copy(current_file);
	g_object_unref(current_file);
}
#else
static void save_history_copy_done(GtkNativeDialog *dialog, gint response, gpointer user_data)
{
	GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
	GFile *current_file = NULL;

	(void) user_data;

	if(response != GTK_RESPONSE_ACCEPT)
	{
		g_object_unref(dialog);
		return;
	}

	current_file = gtk_file_chooser_get_file(chooser);
	g_object_unref(dialog);

	if(NULL == current_file)
		return;

	history_save_copy(current_file);
	g_object_unref(current_file);
}
#endif

/* save the lookup history to a file the user picks
   (GAction callback; GTK4 has no gtk_dialog_run - the flow is async)
   GTK >= 4.10: GtkFileChooserNative is deprecated; use GtkFileDialog */
static void save_history_to_file(GSimpleAction *action, GVariant *params, gpointer user_data)
{
	(void) action;
	(void) params;
	(void) user_data;

#if GTK_CHECK_VERSION(4, 10, 0)
	{
		GtkFileDialog *dialog = gtk_file_dialog_new();

		gtk_file_dialog_set_title(dialog, "Save As");
		gtk_file_dialog_save(dialog, NULL, NULL, save_history_copy_done, NULL);
		g_object_unref(dialog);
	}
#else
	{
		GtkFileChooserNative *dialog = gtk_file_chooser_native_new("Save As",
								      NULL,
								      GTK_FILE_CHOOSER_ACTION_SAVE,
								      "_Save",
								      "_Cancel");

		g_signal_connect(dialog, "response", G_CALLBACK(save_history_copy_done), NULL);
		gtk_native_dialog_show(GTK_NATIVE_DIALOG(dialog));
	}
#endif
}

static void popover_closed(GtkPopover *popover, gpointer user_data)
{
	(void) user_data;

	gtk_widget_unparent(GTK_WIDGET(popover));
}

/* attach a right-click gesture to the query entry that pops up a
   Save History / Clear History popover */
static void query_combo_popup(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
	GtkListStore *list_store_query = GTK_LIST_STORE(user_data);
	GtkWidget *entry = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
	GtkTreeIter iter = {0};
	gboolean menu_item_enabled = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list_store_query), &iter);
	GMenu *menu = g_menu_new();
	GtkWidget *popover = NULL;
	GSimpleActionGroup *action_group = g_simple_action_group_new();
	GSimpleAction *save_action = g_simple_action_new("save-history", NULL);
	GSimpleAction *clear_action = g_simple_action_new("clear-history", NULL);
	GdkRectangle pointing = { (gint) x, (gint) y, 1, 1 };

	(void) n_press;

	g_simple_action_set_enabled(save_action, menu_item_enabled);
	g_signal_connect(save_action, "activate", G_CALLBACK(save_history_to_file), NULL);
	g_simple_action_group_insert(action_group, G_ACTION(save_action));
	g_object_unref(save_action);

	g_simple_action_set_enabled(clear_action, menu_item_enabled);
	g_signal_connect(clear_action, "activate", G_CALLBACK(clear_history), list_store_query);
	g_simple_action_group_insert(action_group, G_ACTION(clear_action));
	g_object_unref(clear_action);

	g_menu_append(menu, "_Save History", "artha.save-history");
	g_menu_append(menu, "Cl_ear History", "artha.clear-history");

	popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
	gtk_widget_insert_action_group(popover, "artha", G_ACTION_GROUP(action_group));
	g_object_unref(action_group);
	g_object_unref(menu);

	gtk_popover_set_pointing_to(GTK_POPOVER(popover), &pointing);
	gtk_widget_set_parent(popover, entry);
	g_signal_connect(popover, "closed", G_CALLBACK(popover_closed), NULL);
	gtk_popover_popup(GTK_POPOVER(popover));
}

static void load_history(GtkListStore *list_store_query)
{
	gchar *hist_file_path = g_strconcat(g_get_user_config_dir(), G_DIR_SEPARATOR_S, ARTHA_TARNAME, HISTORY_FILE_EXT, NULL);
	if(!hist_file_path)
	{
		g_warning("Failed creating path to load history");
		return;
	}
	FILE *hist_file = fopen(hist_file_path, "r");
	g_free(hist_file_path);
	hist_file_path = NULL;
	if(hist_file)
	{
		gchar lookup[MAX_LEMMA_LEN] = "";
		while(fgets(lookup, MAX_LEMMA_LEN, hist_file))
		{
			size_t lookup_len = strlen(lookup);
			// some char(s) and '\n' is minimum
			if(lookup_len > 1 && lookup[lookup_len - 1] == '\n')
			{
				// remove the new line character
				lookup[lookup_len - 1] = '\0';
				GtkTreeIter iter = {0};
				gtk_list_store_prepend(list_store_query, &iter);
				gtk_list_store_set(list_store_query, &iter, 0, lookup, -1);
			}
		}
		fclose(hist_file);
	}
}

static void create_stores_renderers(GtkBuilder *gui_builder)
{
	guint8 i = 0;
	GtkComboBox *combo_query = NULL;
	GtkListStore *list_store_query = NULL;

	GtkTreeView *tree_view = NULL;
	GtkTreeStore *tree_store = NULL;
	GtkCellRenderer *tree_renderer = NULL;
	const gchar *col_name = NULL;
	GtkEntry *query_entry = NULL;

	// combo box data store
	combo_query = GTK_COMBO_BOX(gtk_builder_get_object(gui_builder, COMBO_QUERY));
	list_store_query = gtk_list_store_new(1, G_TYPE_STRING);

	load_history(list_store_query);
	// popup for saving & clearing history
	query_entry = GTK_ENTRY(gtk_combo_box_get_child(combo_query));
	GtkGesture *popup_gesture = gtk_gesture_click_new();
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(popup_gesture), GDK_BUTTON_SECONDARY);
	g_signal_connect(popup_gesture, "pressed", G_CALLBACK(query_combo_popup), list_store_query);
	gtk_widget_add_controller(GTK_WIDGET(query_entry), GTK_EVENT_CONTROLLER(popup_gesture));

	g_signal_connect(GTK_TREE_MODEL(list_store_query), "row-inserted", G_CALLBACK(query_list_updated), gui_builder);
	gtk_combo_box_set_model(combo_query, GTK_TREE_MODEL(list_store_query));
	gtk_combo_box_set_entry_text_column(combo_query, 0);
	g_object_unref(list_store_query);
	list_store_query = NULL;

	for(i = 0; i < TOTAL_RELATIVES; i++)
	{
		tree_view = GTK_TREE_VIEW(gtk_builder_get_object(gui_builder, relative_tree[i]));
		tree_renderer = gtk_cell_renderer_text_new();
		col_name = relative_tree[i];
		col_name = col_name + 4;	// skip the first 4 chars "tree" in relative_tree string array
		gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, col_name, tree_renderer, "text", 0, "weight", 1, NULL);
		
		if(i == TREE_ANTONYMS)
		{
			gtk_tree_view_insert_column_with_attributes(tree_view, -1, STR_ANTONYM_HEADER_INDIRECT_VIA, tree_renderer, "text", 2, NULL);
			tree_store = gtk_tree_store_new(3, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_STRING);
		}
		else
			tree_store = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_UINT);

		gtk_tree_view_set_model(tree_view, GTK_TREE_MODEL(tree_store));
		g_object_unref(tree_store);
		tree_store = NULL;
				
		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), FALSE);

		g_signal_connect(tree_view, "cursor-changed", G_CALLBACK(relative_selection_changed), gui_builder);
		g_signal_connect(tree_view, "row-activated", G_CALLBACK(relative_row_activated), gui_builder);
	}
}

static void button_next_clicked(GtkButton *button, gpointer user_data)
{
	GtkBuilder *gui_builder = GTK_BUILDER(user_data);
	GtkComboBox *combo_query = GTK_COMBO_BOX(gtk_builder_get_object(gui_builder, COMBO_QUERY));
	gint16 active_item = gtk_combo_box_get_active(combo_query);
	GtkTreeModel *tree_model = NULL;

	if(-1 == active_item)
	{
		tree_model = gtk_combo_box_get_model(combo_query);
		active_item = gtk_tree_model_iter_n_children(tree_model, NULL);
	}

	gtk_combo_box_set_active(combo_query, active_item - 1);
}

static void button_prev_clicked(GtkButton *button, gpointer user_data)
{
	GtkBuilder *gui_builder = GTK_BUILDER(user_data);
	GtkComboBox *combo_query = GTK_COMBO_BOX(gtk_builder_get_object(gui_builder, COMBO_QUERY));
	gint16 active_item = gtk_combo_box_get_active(combo_query);

	// in case if the search was successful and the index is 0 (current word)
	// then prev. should move it 1st element, so set active_item = 0, which + 1 will make index as 1
	// else if search had failed earlier, then prev. should take it to the 0th element
	// so leave it as -1; if it is something lesser than -1, make it -1
	if(-1 == active_item && last_search_successful)
	{
		active_item = 0;
	}
	else if(active_item < -1)
	{
		active_item = -1;
	}

	gtk_combo_box_set_active(combo_query, active_item + 1);
}

static gboolean combo_query_scroll(GtkEventControllerScroll *controller, gdouble dx, gdouble dy, gpointer user_data)
{
	GtkWidget *widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));
	GtkComboBox *combo_box = GTK_COMBO_BOX(widget);
	GtkTreeModel *model = NULL;
	gint history_count = 0, current_index = 0;
	gint delta = 0;

	(void) dx;
	(void) user_data;

	if(dy > 0)
		delta = 1;
	else if(dy < 0)
		delta = -1;
	else
		return FALSE;

	model = gtk_combo_box_get_model(combo_box);
	history_count = gtk_tree_model_iter_n_children(model, NULL);
	if(history_count == 0)
		return FALSE;

	current_index = gtk_combo_box_get_active(combo_box);
	if(-1 == current_index)
		current_index = 0;

	current_index += delta;
	if(current_index >= history_count)
		current_index = 0;
	else if(current_index < 0)
		current_index = history_count - 1;

	gtk_combo_box_set_active(combo_box, current_index);

	return TRUE;
}

static gboolean shortcut_quit_cb(GtkWidget *widget, GVariant *args, gpointer user_data)
{
	(void) widget;
	(void) args;
	(void) user_data;

	G_DEBUG("Ctrl+Q pressed!\n");

	if(main_loop != NULL)
		g_main_loop_quit(main_loop);

	return TRUE;
}

static gboolean shortcut_hide_cb(GtkWidget *widget, GVariant *args, gpointer user_data)
{
	(void) args;
	(void) user_data;

	gtk_widget_hide(widget);

	return TRUE;
}

/* Closing the window only hides it (Esc/Ctrl+W semantics): Artha keeps
   running in the tray/background instead of exiting. */
static gboolean close_request_cb(GtkWindow *window, gpointer user_data)
{
	(void) user_data;

	gtk_widget_hide(GTK_WIDGET(window));

	return TRUE;
}

/* Ctrl+Q -> quit; Ctrl+W / Escape -> hide the window (tray-only mode) */
static void setup_keyboard_shortcuts(GtkWindow *window)
{
	GtkEventController *controller = gtk_shortcut_controller_new();
	gtk_shortcut_controller_set_scope(GTK_SHORTCUT_CONTROLLER(controller), GTK_SHORTCUT_SCOPE_LOCAL);

	gtk_shortcut_controller_add_shortcut(
		GTK_SHORTCUT_CONTROLLER(controller),
		gtk_shortcut_new(gtk_shortcut_trigger_parse_string("<Control>q"),
				 gtk_callback_action_new(shortcut_quit_cb, NULL, NULL)));
	gtk_shortcut_controller_add_shortcut(
		GTK_SHORTCUT_CONTROLLER(controller),
		gtk_shortcut_new(gtk_shortcut_trigger_parse_string("<Control>w"),
				 gtk_callback_action_new(shortcut_hide_cb, NULL, NULL)));
	gtk_shortcut_controller_add_shortcut(
		GTK_SHORTCUT_CONTROLLER(controller),
		gtk_shortcut_new(gtk_shortcut_trigger_parse_string("Escape"),
				 gtk_callback_action_new(shortcut_hide_cb, NULL, NULL)));

	gtk_widget_add_controller(GTK_WIDGET(window), controller);
}


static GtkWidget *toolbar_icon_button(const gchar *icon_name, const gchar *label,
				      const gchar *tooltip, GtkToggleButton **toggle_out)
{
	GtkWidget *button = NULL;
	GtkWidget *icon = NULL;

	if(toggle_out != NULL)
		button = gtk_toggle_button_new();
	else
		button = gtk_button_new();

	icon = gtk_image_new_from_icon_name(icon_name);
	gtk_image_set_pixel_size(GTK_IMAGE(icon), 16);
	gtk_button_set_child(GTK_BUTTON(button), icon);

	gtk_widget_set_tooltip_text(button, tooltip);
	gtk_accessible_update_property(GTK_ACCESSIBLE(button),
				    GTK_ACCESSIBLE_PROPERTY_LABEL, label,
				    -1);

	if(toggle_out != NULL)
		*toggle_out = GTK_TOGGLE_BUTTON(button);

	return button;
}

static void toolbar_separator(GtkBox *toolbar)
{
	GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_widget_set_margin_start(sep, 4);
	gtk_widget_set_margin_end(sep, 4);
	gtk_box_append(toolbar, sep);
}

static void setup_toolbar(GtkBuilder *gui_builder)
{
	GtkBox *toolbar = GTK_BOX(gtk_builder_get_object(gui_builder, TOOLBAR));
	GtkWidget *button = NULL;
	GtkToggleButton *mode_toggle = NULL, *notify_toggle = NULL;

	button = toolbar_icon_button("go-previous-symbolic", STR_TOOLITEM_PREV,
				       TOOLITEM_TOOLTIP_PREV, NULL);
	gtk_widget_set_sensitive(button, FALSE);
	g_signal_connect(button, "clicked", G_CALLBACK(button_prev_clicked), gui_builder);
	gtk_box_append(toolbar, button);
	toolbar_prev_btn = button;

	button = toolbar_icon_button("go-next-symbolic", STR_TOOLITEM_NEXT,
				       TOOLITEM_TOOLTIP_NEXT, NULL);
	gtk_widget_set_sensitive(button, FALSE);
	g_signal_connect(button, "clicked", G_CALLBACK(button_next_clicked), gui_builder);
	gtk_box_append(toolbar, button);
	toolbar_next_btn = button;

	toolbar_separator(toolbar);

	button = toolbar_icon_button("view-list-symbolic", STR_TOOLITEM_MODE,
				       TOOLITEM_TOOLTIP_MODE, &mode_toggle);
	gtk_toggle_button_set_active(mode_toggle, advanced_mode);
	g_signal_connect(button, "toggled", G_CALLBACK(mode_toggled), gui_builder);
	gtk_box_append(toolbar, button);

	button = toolbar_icon_button("preferences-system-symbolic", STR_TOOLITEM_OPTIONS,
				       TOOLITEM_TOOLTIP_OPTIONS, NULL);
	g_signal_connect(button, "clicked", G_CALLBACK(show_settings_dialog), gui_builder);
	gtk_box_append(toolbar, button);

	/* if mod notify is present */
	if(mod_notifier)
	{
		button = toolbar_icon_button("preferences-system-notifications-symbolic", STR_TOOLITEM_NOTIFY,
					       TOOLITEM_TOOLTIP_NOTIFY, &notify_toggle);
		gtk_toggle_button_set_active(notify_toggle, notifier_enabled);
		g_signal_connect(button, "toggled", G_CALLBACK(notification_toggled), gui_builder);
		gtk_box_append(toolbar, button);
	}

	toolbar_separator(toolbar);

	button = toolbar_icon_button("help-about-symbolic", STR_TOOLITEM_ABOUT,
				       TOOLITEM_TOOLTIP_ABOUT, NULL);
	g_signal_connect(button, "clicked", G_CALLBACK(about_activate), NULL);
	gtk_box_append(toolbar, button);

	button = toolbar_icon_button("application-exit-symbolic", STR_TOOLITEM_QUIT,
				       TOOLITEM_TOOLTIP_QUIT, NULL);
	g_signal_connect(button, "clicked", G_CALLBACK(quit_activate), NULL);
	gtk_box_append(toolbar, button);

	/* keyboard shortcuts: Ctrl+Q quits; Ctrl+W / Escape hide the window */
	setup_keyboard_shortcuts(GTK_WINDOW(gtk_builder_get_object(gui_builder, WINDOW_MAIN)));
}


static void create_text_view_tags(GtkBuilder *gui_builder)
{
	guint8 i = 0;
	GtkTextView *text_view = NULL;
	GtkTextBuffer *buffer = NULL;

	// create text view tags
	text_view = GTK_TEXT_VIEW(gtk_builder_get_object(gui_builder, TEXT_VIEW_DEFINITIONS));
	buffer = gtk_text_view_get_buffer(text_view);

	gtk_text_buffer_create_tag(buffer, TAG_LEMMA, "weight", PANGO_WEIGHT_BOLD, NULL);
	gtk_text_buffer_create_tag(buffer, TAG_POS, "foreground", "red", "style", PANGO_STYLE_ITALIC, NULL);
	gtk_text_buffer_create_tag(buffer, TAG_COUNTER, "left_margin", 15, "weight", PANGO_WEIGHT_BOLD, "foreground", "gray", NULL);
	gtk_text_buffer_create_tag(buffer, TAG_EXAMPLE, "foreground", "blue", "font", "Serif Italic 10", "left_margin", 45, NULL);
	gtk_text_buffer_create_tag(buffer, TAG_MATCH, "foreground", "DarkGreen", "weight", PANGO_WEIGHT_SEMIBOLD, NULL);
	gtk_text_buffer_create_tag(buffer, TAG_SUGGESTION, "foreground", "blue" , "left_margin", 25, NULL);
	gtk_text_buffer_create_tag(buffer, TAG_HIGHLIGHT, "background", "black", "foreground", "white", NULL);

	for(i = 0; i < FAMILIARITY_COUNT; i++)
	{
		gtk_text_buffer_create_tag(buffer, familiarity[i], "background", freq_colors[i], 
		"foreground", (i < 4)? "white" : "black", "font", "Monospace 9", NULL);
	}
	
	/* double click / Ctrl+release triggers the WordNet lookup */
	GtkGesture *click = gtk_gesture_click_new();
	g_signal_connect(click, "released", G_CALLBACK(text_view_clicked), gui_builder);
	gtk_widget_add_controller(GTK_WIDGET(text_view), GTK_EVENT_CONTROLLER(click));
}

static void load_preference_hotkey(gboolean *first_run)
{
	GError *err = NULL;
	gchar *version = g_key_file_get_string(config_file, GROUP_SETTINGS, KEY_VERSION, NULL);

	if(version)
	{
		if(0 != g_strcmp0(version, ARTHA_VERSION))
		{
			/* version number is split here. Major, Minor, Micro */
			gchar **version_numbers = g_strsplit(version, ".", 3);
			gchar **current_version_numbers = g_strsplit(ARTHA_VERSION, ".", 3);
			
			for(guint8 i = 0; (NULL != version_numbers[i] && NULL != current_version_numbers[i]); i++)
			{
				if(g_ascii_digit_value(version_numbers[i][0]) < g_ascii_digit_value(current_version_numbers[i][0]))
				{
					*first_run = TRUE;
					break;
				}
			}

			g_strfreev(version_numbers);
			g_strfreev(current_version_numbers);
			version_numbers = current_version_numbers = NULL;
		}
		g_free(version);
		version = NULL;
	}

	/* zero can be returned in three cases:
		i.   Older version - no key set in conf (err will be set)
		ii.  New version with no key in conf file set (err will be set)
		iii. New version & entries present (err will not be set) */

	app_hotkey.accel_key = g_key_file_get_integer(config_file, GROUP_SETTINGS, KEY_ACCEL_KEY, &err);
	app_hotkey.accel_mods = g_key_file_get_integer(config_file, GROUP_SETTINGS, KEY_ACCEL_MODS, NULL);
	app_hotkey.accel_flags = g_key_file_get_integer(config_file, GROUP_SETTINGS, KEY_ACCEL_FLAGS, NULL);

	if(err)
	{
		g_error_free(err);
		err = NULL;
	}
	else if(*first_run)		/* if no error was set, then the key is present, in that case */
		*first_run = FALSE;	/* a first run is not right, it's just a version above 1.0.1 where hotkeys are already set */
}

static gboolean load_preferences(GtkWindow *parent)
{
	gboolean first_run = FALSE;

	config_file = g_key_file_new();
	if(!config_file)
	{
		g_error("Error creating GKeyFile to load/store preferences");
	}

	gchar *conf_file_path = g_strconcat(g_get_user_config_dir(), G_DIR_SEPARATOR_S, ARTHA_TARNAME, CONF_FILE_EXT, NULL);
	if(g_key_file_load_from_file(config_file, conf_file_path, G_KEY_FILE_KEEP_COMMENTS, NULL))
	{
		advanced_mode = g_key_file_get_boolean(config_file, GROUP_SETTINGS, KEY_MODE, NULL);
		notifier_enabled = g_key_file_get_boolean(config_file, GROUP_SETTINGS, KEY_NOTIFICATIONS, NULL);
		GError *err = NULL;
		show_polysemy = g_key_file_get_boolean(config_file, GROUP_SETTINGS, KEY_POLYSEMY, &err);
		if(err)
		{
			show_polysemy = TRUE;
			g_clear_error(&err);
		}
		show_trayicon = g_key_file_get_boolean(config_file, GROUP_SETTINGS, KEY_TRAYICON, &err);
		if(err)
		{
			show_trayicon = TRUE;
			g_clear_error(&err);
		}
		load_preference_hotkey(&first_run);
		launch_minimized = g_key_file_get_boolean(config_file, GROUP_SETTINGS, KEY_LAUNCH_HIDDEN, &err);
		if(err)
		{
			launch_minimized = TRUE;
			g_clear_error(&err);
		}
	}
	else
		first_run = TRUE;

	if(conf_file_path)
	{
		g_free(conf_file_path);
		conf_file_path = NULL;
	}

	/* The user might never close the app. and just shut down the system.
	   Conf file won't be written to disk in that case. Hence save the 
	   conf file when its a first run with the defaults. This is done 
	   by the main func. */
	return first_run;
}

/* this function uses FILE instead of GFile to workaround
 * a bug in GDataOutputStream, which doesn't convert '\n'
 * (C's std. new file literal)
 * to the target OS's line character; while std. FILE does */
static gboolean update_history_in_file(const gchar *term)
{
	gboolean update_succeeded = FALSE;
	gchar *hist_file_path = g_strconcat(g_get_user_config_dir(), G_DIR_SEPARATOR_S, ARTHA_TARNAME, HISTORY_FILE_EXT, NULL);
	if(!hist_file_path)
	{
		g_error("Failed creating path to save history");
	}
	FILE *hist_file = fopen(hist_file_path, "a+");
	g_free(hist_file_path);
	hist_file_path = NULL;
	if(hist_file)
	{
		update_succeeded = (fprintf(hist_file, "%s\n", term) > 0);
		fclose(hist_file);
		hist_file = NULL;
	}
	return update_succeeded;
}

static void save_preferences_to_file()
{
	gsize file_len = 0;
	gchar *conf_file_path = g_strconcat(g_get_user_config_dir(), G_DIR_SEPARATOR_S, ARTHA_TARNAME, CONF_FILE_EXT, NULL);
	gchar *file_contents = g_key_file_to_data(config_file, &file_len, NULL);
	GError *err = NULL;

	/* a fresh profile may not have the config dir yet */
	g_mkdir_with_parents(g_get_user_config_dir(), 0700);

	if(!conf_file_path || !file_contents)
	{
		g_error("Error saving preferences to file");
	}
	if(!g_file_set_contents(conf_file_path, file_contents, file_len, &err))
	{
		g_warning("Failed to save preferences to %s\nError: %s\n", conf_file_path, err->message);
		g_error_free(err);
		err = NULL;
	}

	g_free(conf_file_path);
	conf_file_path = NULL;
	g_free(file_contents);
	file_contents = NULL;
}

static void save_preferences()
{
	g_key_file_set_comment(config_file, NULL, NULL, SETTINGS_COMMENT, NULL);

	g_key_file_set_string(config_file, GROUP_SETTINGS, KEY_VERSION, ARTHA_VERSION);
	g_key_file_set_integer(config_file, GROUP_SETTINGS, KEY_ACCEL_KEY, app_hotkey.accel_key);
	g_key_file_set_integer(config_file, GROUP_SETTINGS, KEY_ACCEL_MODS, app_hotkey.accel_mods);
	g_key_file_set_integer(config_file, GROUP_SETTINGS, KEY_ACCEL_FLAGS, app_hotkey.accel_flags);
	g_key_file_set_boolean(config_file, GROUP_SETTINGS, KEY_MODE, advanced_mode);
	g_key_file_set_boolean(config_file, GROUP_SETTINGS, KEY_NOTIFICATIONS, notifier_enabled);
	g_key_file_set_boolean(config_file, GROUP_SETTINGS, KEY_POLYSEMY, show_polysemy);
	g_key_file_set_boolean(config_file, GROUP_SETTINGS, KEY_TRAYICON, show_trayicon);
	g_key_file_set_boolean(config_file, GROUP_SETTINGS, KEY_LAUNCH_HIDDEN, launch_minimized);

	save_preferences_to_file();
}

static gboolean autocomplete_selected(GtkEntryCompletion *query_completion, GtkTreeModel *model, GtkTreeIter *iter, GtkButton *button)
{
	gchar *match = NULL;
	gtk_tree_model_get(model, iter, 0, &match, -1);
	if(match)
	{
		GtkEntry *query_entry = GTK_ENTRY(gtk_entry_completion_get_entry(query_completion));
		gtk_editable_set_text(GTK_EDITABLE(query_entry), match);
		g_free(match);
		match = NULL;
		g_signal_emit_by_name(button, "clicked");
		return TRUE;
	}
	return FALSE;
}

static void show_loading(GtkBuilder *gui_builder)
{
	gchar status_msg[MAX_STATUS_MSG] = "";

	(void) gui_builder;

	// set the loading message on status bar
	status_set("");
	g_snprintf(status_msg, MAX_STATUS_MSG, STR_STATUS_INDEXING);
	status_set(status_msg);
}

static void attach_loaded_terms(WordnetTermsLoaderData *loader_data)
{
	g_free(loader_data->index_file_contents);
	loader_data->index_file_contents = NULL;
	loader_data->index_file_length = loader_data->pos_in_file = 0;
	loader_data->last_lemma = NULL;

	GtkEntryCompletion *query_entry_completion = gtk_entry_completion_new();
	gtk_entry_completion_set_model(query_entry_completion, GTK_TREE_MODEL(loader_data->completion_list));
	g_object_unref(loader_data->completion_list);
	loader_data->completion_list = NULL;
	gtk_entry_completion_set_text_column(query_entry_completion, 0);
	gtk_entry_completion_set_minimum_key_length(query_entry_completion, 3);
	GtkButton *query_button = GTK_BUTTON(gtk_builder_get_object(loader_data->gui_builder, BUTTON_SEARCH));
	g_signal_connect(query_entry_completion, "match-selected", G_CALLBACK(autocomplete_selected), query_button);

	GtkComboBox *query_combo = GTK_COMBO_BOX(gtk_builder_get_object(loader_data->gui_builder, COMBO_QUERY));
	GtkEntry *query_entry = GTK_ENTRY(gtk_combo_box_get_child(query_combo));
	gtk_entry_set_completion(query_entry, query_entry_completion);

	// clear the loading message from status bar
	status_set("");
}

static gboolean terms_loader(WordnetTermsLoaderData *loader_data)
{
	gboolean load_pending = TRUE, is_reading = TRUE;
	gchar lemma[MAX_LEMMA_LEN] = "", ch = 0;
	gsize i = 0;
	while((ch = loader_data->index_file_contents[loader_data->pos_in_file++]) != '\n')
	{
		if(is_reading)
		{
			if(ch == '%')
			{
				is_reading = FALSE;
				lemma[i++] = '\n';
				lemma[i] = '\0';
				if(g_strcmp0(lemma, loader_data->last_lemma) != 0)
				{
					g_string_append(wordnet_terms, lemma);
					loader_data->last_lemma = wordnet_terms->str + wordnet_terms->len - i;

					lemma[i - 1] = '\0';
					GtkTreeIter iter = {0};
					gtk_list_store_append(loader_data->completion_list, &iter);
					gtk_list_store_set(loader_data->completion_list, &iter, 0, lemma, -1);
				}
			}
			else
			{
				lemma[i++] = (ch == '_') ? ' ' : ch;
			}
		}
	}
	
	if(loader_data->pos_in_file == loader_data->index_file_length)
	{
		attach_loaded_terms(loader_data);
		load_pending = FALSE;
	}

	return load_pending;
}

static gboolean wordnet_terms_load(WordnetTermsLoaderData *loader_data)
{
	gboolean succeeded = FALSE;
	gchar *index_file_path = g_strdup_printf(SENSEIDXFILE, SetSearchdir());
	if(g_file_get_contents(index_file_path,
						&loader_data->index_file_contents,
						&loader_data->index_file_length,
						NULL))
	{
		show_loading(loader_data->gui_builder);

		wordnet_terms = g_string_new("");
		loader_data->last_lemma = wordnet_terms->str;
		loader_data->completion_list = gtk_list_store_new(1, G_TYPE_STRING);

		g_idle_add((GSourceFunc) terms_loader, loader_data);
		succeeded = TRUE;
	}
	g_free(index_file_path);
	index_file_path = NULL;

	return succeeded;
}

#ifdef X11_AVAILABLE
/* Resolve the physical X11 modifier mask holding a given keysym
   (caps lock, num lock, scroll lock live on wherever the user's
   server put them, not on fixed bits) */
static guint x11_modifier_mask_for_keysym(KeySym keysym)
{
	XModifierKeymap *mapping = NULL;
	KeyCode keycode = 0;
	int per_mod = 0, i, j;
	guint mask = 0;

	keycode = XKeysymToKeycode(dpy, keysym);
	if(keycode == 0 || dpy == NULL)
		return 0;

	mapping = XGetModifierMapping(dpy);
	if(mapping == NULL)
		return 0;

	per_mod = mapping->max_keypermod;
	for(i = 0; i < 8; i++)
	{
		for(j = 0; j < per_mod; j++)
		{
			/* index into the modifier map: mod_index * per_mod + key */
			KeyCode kc = mapping->modifiermap[i * per_mod + j];
			if(kc != 0 && kc == keycode)
				mask |= (1u << i);
		}
	}

	XFreeModifiermap(mapping);
	return mask;
}

static guint caps_lock_mask = 0, num_lock_mask = 0, scroll_lock_mask = 0;

static void lookup_ignorable_modifiers()
{
	caps_lock_mask   = x11_modifier_mask_for_keysym(XK_Caps_Lock);
	num_lock_mask    = x11_modifier_mask_for_keysym(XK_Num_Lock);
	scroll_lock_mask = x11_modifier_mask_for_keysym(XK_Scroll_Lock);
}

/* Map GDK modifier bits to physical X11 modifier bits. Shift/Control/Mod1
   coincide with the X11 bits; Super/Hyper/Meta resolve through the
   modifier mapping by their keysyms. */
static guint x11_accel_mods_to_xmods(guint gdk_mods)
{
	guint xmods = 0;

	if(gdk_mods & GDK_SHIFT_MASK)   xmods |= ShiftMask;
	if(gdk_mods & GDK_CONTROL_MASK) xmods |= ControlMask;
	if(gdk_mods & GDK_ALT_MASK)     xmods |= Mod1Mask;
	if(gdk_mods & GDK_SUPER_MASK)
		xmods |= x11_modifier_mask_for_keysym(XK_Super_L) | x11_modifier_mask_for_keysym(XK_Super_R);
	if(gdk_mods & GDK_HYPER_MASK)
		xmods |= x11_modifier_mask_for_keysym(XK_Hyper_L) | x11_modifier_mask_for_keysym(XK_Hyper_R);
	if(gdk_mods & GDK_META_MASK)
		xmods |= x11_modifier_mask_for_keysym(XK_Meta_L) | x11_modifier_mask_for_keysym(XK_Meta_R);

	return xmods;
}

gboolean grab_ungrab_with_ignorable_modifiers (ArthaAccelKey *binding, gboolean grab)
{
	guint i = 0, actual_mods = 0;
	Window rootwin = 0;
	gboolean any_success = FALSE;
	guint mod_masks [] =
	{
		0, /* modifier only */
		num_lock_mask,
		caps_lock_mask,
		scroll_lock_mask,
		num_lock_mask  | caps_lock_mask,
		num_lock_mask  | scroll_lock_mask,
		caps_lock_mask | scroll_lock_mask,
		num_lock_mask  | caps_lock_mask | scroll_lock_mask,
	};

	if(dpy == NULL)
		return FALSE;

	rootwin = XDefaultRootWindow(dpy);
	actual_mods = x11_accel_mods_to_xmods(binding->accel_mods);

	for(i = 0; i < G_N_ELEMENTS (mod_masks); i++)
	{
		x11_grab_error = 0;

		if(grab)
		{
			XGrabKey (dpy,
				  XKeysymToKeycode(dpy, binding->accel_key),
				  actual_mods | mod_masks [i],
				  rootwin,
				  False,
				  GrabModeAsync,
				  GrabModeAsync);
		}
		else
		{
			XUngrabKey (dpy,
				    XKeysymToKeycode(dpy, binding->accel_key),
				    actual_mods | mod_masks [i],
				    rootwin);
		}

		XSync(dpy, False);

		if(x11_grab_error == 0)
			any_success = TRUE;
	}

	return any_success;
}

#elif defined G_OS_WIN32

static gboolean try_convert_to_vk(guint gdk_key, BYTE *vk)
{
	gboolean converted = FALSE;
	static const gdk_vk keys[] = {
									{GDK_KEY_Insert, VK_INSERT},
									{GDK_KEY_Delete, VK_DELETE},
									{GDK_KEY_Select, VK_SELECT},
									{GDK_KEY_Print, VK_PRINT},
									{GDK_KEY_Execute, VK_EXECUTE},
									{GDK_KEY_Page_Up, VK_PRIOR},
									{GDK_KEY_Page_Down, VK_NEXT},
									{GDK_KEY_End, VK_END},
									{GDK_KEY_Clear, VK_CLEAR},
									{GDK_KEY_Pause, VK_PAUSE},
									{GDK_KEY_Cancel, VK_CANCEL},
									{GDK_KEY_Menu, VK_MENU},
									{GDK_KEY_3270_PrintScreen, VK_SNAPSHOT},
									{GDK_KEY_Help, VK_HELP},
									{GDK_KEY_Super_L, VK_LWIN},
									{GDK_KEY_Super_R, VK_RWIN},
									{GDK_KEY_Sleep, VK_SLEEP},
									{GDK_KEY_Back, VK_BROWSER_BACK},
									{GDK_KEY_Forward, VK_BROWSER_FORWARD},
									{GDK_KEY_Refresh, VK_BROWSER_REFRESH},
									{GDK_KEY_Stop, VK_BROWSER_STOP},
									{GDK_KEY_Search, VK_BROWSER_SEARCH},
									{GDK_KEY_Favorites, VK_BROWSER_FAVORITES},
									{GDK_KEY_HomePage, VK_BROWSER_HOME},
									{GDK_KEY_AudioMute, VK_VOLUME_MUTE},
									{GDK_KEY_AudioLowerVolume, VK_VOLUME_DOWN},
									{GDK_KEY_AudioRaiseVolume, VK_VOLUME_UP},
									{GDK_KEY_AudioNext, VK_MEDIA_NEXT_TRACK},
									{GDK_KEY_AudioPrev, VK_MEDIA_PREV_TRACK},
									{GDK_KEY_AudioStop, VK_MEDIA_STOP},
									{GDK_KEY_AudioPlay, VK_MEDIA_PLAY_PAUSE},
									{GDK_KEY_Mail, VK_LAUNCH_MAIL},
									{GDK_KEY_3270_Play, VK_PLAY}
								 };
	// function keys
	if((gdk_key >= GDK_KEY_F1) && (gdk_key <= GDK_KEY_F24))
	{
		*vk = VK_F1 + gdk_key - GDK_KEY_F1;
		converted = TRUE;
	}
	// numpad 0 to 9
	else if((gdk_key >= GDK_KEY_KP_0) && (gdk_key <= GDK_KEY_KP_9))
	{
		*vk = VK_NUMPAD0 + gdk_key - GDK_KEY_KP_0;
		converted = TRUE;
	}
	// home & arrow keys
	else if((gdk_key >= GDK_KEY_Home) && (gdk_key <= GDK_KEY_Down))
	{
		*vk = VK_HOME + gdk_key - GDK_KEY_Home;
		converted = TRUE;
	}
	// numpad multiply to divide (includes separator)
	else if((gdk_key >= GDK_KEY_KP_Multiply) && (gdk_key <= GDK_KEY_KP_Divide))
	{
		*vk = VK_MULTIPLY + gdk_key - GDK_KEY_KP_Multiply;
		converted = TRUE;
	}
	else
	{
		guint8 i = 0;
		for(; i < G_N_ELEMENTS(keys); ++i)
		{
			if(keys[i].gdk_key == gdk_key)
			{
				*vk = keys[i].vk_key;
				converted = TRUE;
				break;
			}
		}
	}
	return converted;
}

gboolean grab_ungrab_with_ignorable_modifiers (ArthaAccelKey *binding, gboolean grab)
{
	gboolean reg_success = FALSE;

	if(grab)
	{
		SHORT key = 0;
		BYTE vk_key = 0;

		if(try_convert_to_vk(binding->accel_key, &vk_key) || ((key = VkKeyScan(binding->accel_key)) != -1))
		{
			if((key != -1) && (key != 0))
			{
				vk_key = LOBYTE(key);
			}
			UINT win_mods = 0;
			win_mods |= ((binding->accel_mods & GDK_SHIFT_MASK) || (HIBYTE(key) & 1)) ? MOD_SHIFT : 0;
			win_mods |= ((binding->accel_mods & GDK_CONTROL_MASK) || (HIBYTE(key) & 2)) ? MOD_CONTROL : 0;
			win_mods |= ((binding->accel_mods & GDK_ALT_MASK) || (HIBYTE(key) & 4)) ? MOD_ALT : 0;
			win_mods |= (binding->accel_mods & GDK_SUPER_MASK) ? MOD_WIN : 0;

			reg_success = RegisterHotKey(hMainWindow, 1, win_mods, vk_key);
		}
	}
	else
	{
		reg_success = UnregisterHotKey(hMainWindow, 1);
	}

	return reg_success;
}
#endif // X11_AVAILABLE

static gboolean register_unregister_hotkey(gboolean first_run, gboolean setup_hotkey)
{
	guint i = 0;

	/* if it's first run and hotkey needs to be setup; try with the preset hotkey trials
	   else just grab or ungrab as per the last param passed */
	if(first_run && setup_hotkey)
	{
		app_hotkey.accel_mods = GDK_CONTROL_MASK | GDK_ALT_MASK;

		for(i = 0; i < G_N_ELEMENTS(hotkey_trials); ++i)
		{
			app_hotkey.accel_key = hotkey_trials[i];

			if(grab_ungrab_with_ignorable_modifiers(&app_hotkey, TRUE))
				return TRUE;
		}

		/* when control reaches here, it means hotkey couldn't be set; but since
		it's first run, save_pref will be called and whatever value is  present
		in app_hotkey (last key in hotkey_trials) will be saved; so 0 it */
		app_hotkey.accel_key = app_hotkey.accel_mods = app_hotkey.accel_flags = 0;
		
		return FALSE;
	}

	return(grab_ungrab_with_ignorable_modifiers(&app_hotkey, setup_hotkey));
}

/* valid while the options dialog is open; restore on Cancel */
static ArthaAccelKey settings_hotkey_backup;

static void options_apply_clicked(GtkButton *button, gpointer user_data)
{
	GtkBuilder *gui_builder = GTK_BUILDER(user_data);

	(void) button;

	apply_and_save_settings(gui_builder);
	/* set hotkey flag */
	hotkey_set = (gboolean) app_hotkey.accel_key;
	gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(gui_builder, DIALOG_OPTIONS)));
}

static void options_cancel_clicked(GtkButton *button, gpointer user_data)
{
	GtkBuilder *gui_builder = GTK_BUILDER(user_data);

	(void) button;

	revert_settings(gui_builder, &settings_hotkey_backup);
	hotkey_set = (gboolean) app_hotkey.accel_key;
	gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(gui_builder, DIALOG_OPTIONS)));
}

static void setup_settings_dialog(GtkBuilder *gui_builder)
{
	GtkLabel *hotkey_label = GTK_LABEL(gtk_builder_get_object(gui_builder, LABEL_HOTKEY));
	GtkBox *hbox = GTK_BOX(gtk_builder_get_object(gui_builder, HBOX_HOTKEY));
	GtkWindow *settings_dialog = GTK_WINDOW(gtk_builder_get_object(gui_builder, DIALOG_OPTIONS));
	GtkButton *apply_btn = GTK_BUTTON(gtk_builder_get_object(gui_builder, BUTTON_APPLY));
	GtkButton *cancel_btn = GTK_BUTTON(gtk_builder_get_object(gui_builder, BUTTON_CANCEL));
	GtkWidget *hotkey_accel_cell = create_hotkey_editor();

	(void) settings_dialog;

	gtk_box_append(hbox, hotkey_accel_cell);
	gtk_label_set_mnemonic_widget(hotkey_label, hotkey_accel_cell);
	g_signal_connect(apply_btn, "clicked", G_CALLBACK(options_apply_clicked), gui_builder);
	g_signal_connect(cancel_btn, "clicked", G_CALLBACK(options_cancel_clicked), gui_builder);
	set_settings_to_check_boxes(gui_builder);
}

static void set_settings_to_check_boxes(GtkBuilder *gui_builder)
{
	GtkCheckButton *polysemy_show_btn = GTK_CHECK_BUTTON(gtk_builder_get_object(gui_builder, CHKBTN_POLYSEMY));
	GtkCheckButton *status_show_btn = GTK_CHECK_BUTTON(gtk_builder_get_object(gui_builder, CHKBTN_STATUS));
	GtkCheckButton *launch_min_btn = GTK_CHECK_BUTTON(gtk_builder_get_object(gui_builder, CHKBTN_LAUNCH_MIN));
	
	// show_trayicon is the variable, while the checkbox is "hide status icon"
	// hence the NOT op.; same goes for launch_minimized too
	gtk_check_button_set_active(status_show_btn, !show_trayicon);
	gtk_check_button_set_active(launch_min_btn, !launch_minimized);
	gtk_check_button_set_active(polysemy_show_btn, show_polysemy);
}

static void get_settings_from_check_boxes(GtkBuilder *gui_builder,
											gboolean *new_trayicon_show,
											gboolean *new_launch_min,
											gboolean *new_polysemy_show)
{
	GtkCheckButton *polysemy_show_btn = GTK_CHECK_BUTTON(gtk_builder_get_object(gui_builder, CHKBTN_POLYSEMY));
	GtkCheckButton *status_show_btn = GTK_CHECK_BUTTON(gtk_builder_get_object(gui_builder, CHKBTN_STATUS));
	GtkCheckButton *launch_min_btn = GTK_CHECK_BUTTON(gtk_builder_get_object(gui_builder, CHKBTN_LAUNCH_MIN));

	// show_trayicon is the variable, while the checkbox is "hide status icon"
	// hence the NOT op.; same goes for launch_minimized
	*new_trayicon_show = !gtk_check_button_get_active(status_show_btn);
	*new_launch_min = !gtk_check_button_get_active(launch_min_btn);
	*new_polysemy_show = gtk_check_button_get_active(polysemy_show_btn);
}

static void apply_and_save_settings(GtkBuilder *gui_builder)
{
	GtkButton *search_button = GTK_BUTTON(gtk_builder_get_object(gui_builder, BUTTON_SEARCH));
	gboolean new_show_trayicon = FALSE, new_show_polysemy = FALSE;

	get_settings_from_check_boxes(gui_builder, &new_show_trayicon, &launch_minimized, &new_show_polysemy);
	if(show_polysemy != new_show_polysemy)
	{
		show_polysemy = new_show_polysemy;
		last_search_successful = FALSE;
		g_signal_emit_by_name(search_button, "clicked");
	}
	if(show_trayicon != new_show_trayicon)
	{
		show_trayicon = new_show_trayicon;
		status_notifier_set_visible(show_trayicon);
	}

	save_preferences();	
}

/* flags when changed and reverted don't affect, since only Apply code makes permanent
 * changes, while hotkey alone is an exception to this; when changed, it captures the
 * hotkey changed to; hence when reverted, this alone should be undone and the old
 * hotkey should be set again
 */
static void revert_settings(GtkBuilder *gui_builder, ArthaAccelKey *hotkey_backup)
{
	/* if prev. hotkey and app_hotkey are not the same and 'Apply' wasn't clicked
	   then revert hotkey before saving the preferences */
	if(hotkey_backup->accel_key != app_hotkey.accel_key || 
	   hotkey_backup->accel_mods != app_hotkey.accel_mods || 
	   hotkey_backup->accel_flags != app_hotkey.accel_flags)
	{
		grab_ungrab_with_ignorable_modifiers(&app_hotkey, FALSE);
		if(grab_ungrab_with_ignorable_modifiers(hotkey_backup, TRUE))
			app_hotkey = *hotkey_backup;
		else
		{
			show_message_dlg(NULL, MSG_HOTKEY_FAILED);
			memset(&app_hotkey, 0, sizeof(ArthaAccelKey));
		}
	}
}

static void show_settings_dialog(GtkButton *button, gpointer user_data)
{
	GtkBuilder *gui_builder = GTK_BUILDER(user_data);
	GtkWindow *settings_dialog = GTK_WINDOW(gtk_builder_get_object(gui_builder, DIALOG_OPTIONS));

	(void) button;

	/* remember the current hotkey; Cancel restores it */
	settings_hotkey_backup = app_hotkey;

	/* update the old value on the controls; in case the user changed the form
	 * value and presses cancel, the next launch of the Options dialog shouldn't
	 * show the 'Cancel'ed form states */
	set_settings_to_check_boxes(gui_builder);

	gtk_window_present(settings_dialog);
}

static void destructor(GtkBuilder *gui_builder)
{
	(void) gui_builder;

	if(config_file)
	{
		g_key_file_free(config_file);
		config_file = NULL;
	}

	if(wordnet_terms)
	{
	        g_string_free(wordnet_terms, TRUE);
	        wordnet_terms = NULL;
	}

	/* be responsible, give back the OS what you got from it :) */
	if(results)
	{
		wni_free(&results);
		results = NULL;
	}

	/* if hotkey is registered, unregister it */
	if(hotkey_set)
	{
		register_unregister_hotkey(FALSE, FALSE);

		hotkey_set = FALSE;
	}

	if(mod_suggest)
	{
		suggestions_uninit();
		mod_suggest = FALSE;
	}

	if(mod_notifier)
	{
		mod_notify_uninit();
	}
}

static void show_message_dlg(GtkWidget *parent_window, MessageResposeCode msg_code)
{
	GtkMessageType msg_type = GTK_MESSAGE_INFO;
	gchar *str_body = NULL;
	gchar *str_temp1 = NULL, *str_temp2 = NULL;

	if(MSG_DB_LOAD_ERROR == msg_code)
	{
		gchar *searchdir = SetSearchdir();
		str_body = str_temp1 = g_strdup_printf(STR_MSG_WN_ERROR, searchdir != NULL ? searchdir : "");
		msg_type = GTK_MESSAGE_ERROR;
	}
	else if(MSG_HOTKEY_SUCCEEDED_FIRST_RUN == msg_code)
	{
		str_temp1 = gtk_accelerator_get_label(app_hotkey.accel_key, app_hotkey.accel_mods);
		str_temp2 = g_strdup_printf(STR_MSG_WELCOME_ARBITRARY_SUCCEEDED, str_temp1);
		g_free(str_temp1); str_temp1 = NULL;

		str_body = str_temp1 = g_strdup_printf("%s\n\n%s %s", 
					STR_MSG_WELCOME_HOTKEY_HEADER, 
					str_temp2, 
					STR_MSG_WELCOME_HOTKEY_FOOTER);
	}
	else if(MSG_HOTKEY_FAILED_FIRST_RUN == msg_code)
	{
		str_body = str_temp1 = g_strdup_printf("%s\n%s", 
					STR_MSG_WELCOME_HOTKEY_HEADER,  
					STR_MSG_WELCOME_HOTKEY_FOOTER);
	}
	else if(MSG_HOTKEY_FAILED == msg_code)
	{
		str_temp1 = gtk_accelerator_get_label(app_hotkey.accel_key, app_hotkey.accel_mods);
		str_temp2 = g_strdup_printf(STR_MSG_WELCOME_HOTKEY_FAILED, str_temp1);
		g_free(str_temp1); str_temp1 = NULL;

		str_body = str_temp1 = g_strdup_printf("%s %s", str_temp2, STR_MSG_WELCOME_HOTKEY_FOOTER);
		msg_type = GTK_MESSAGE_WARNING;
	}
	else if(MSG_HOTKEY_NOTSET == msg_code)
	{
		str_body = STR_MSG_HOTKEY_NOTSET;
		msg_type = GTK_MESSAGE_WARNING;
	}

#if GTK_CHECK_VERSION(4, 10, 0)
	{
		GtkAlertDialog *alert = gtk_alert_dialog_new("%s", ARTHA_NAME);

		(void) msg_type;
		gtk_alert_dialog_set_detail(alert, str_body);
		gtk_alert_dialog_show(alert, parent_window ? GTK_WINDOW(parent_window) : NULL);
		g_object_unref(alert);
	}
#else
	{
		GtkWidget *msg_dialog = gtk_message_dialog_new(parent_window ? GTK_WINDOW(parent_window) : NULL,
							       GTK_DIALOG_DESTROY_WITH_PARENT,
							       msg_type,
							       GTK_BUTTONS_OK,
							       "%s",
							       str_body);
		g_object_set(msg_dialog, "title", ARTHA_NAME, NULL);
		g_signal_connect(msg_dialog, "response", G_CALLBACK(message_dialog_destroy), NULL);
		gtk_window_present(GTK_WINDOW(msg_dialog));
	}
#endif

	if(str_temp1)
	{
		g_free(str_temp1);
		str_temp1 = NULL;
	}

	if(str_temp2)
	{
		g_free(str_temp2);
		str_temp2 = NULL;
	}
}

/* when the window is (re-)shown, make sure a proper lookup is made
   if the last lookup was a notification i.e. a light lookup */
static void window_mapped(GtkWidget *widget, gpointer user_data)
{
	(void) widget;

	if(last_lookup_a_notification)
	{
		last_search_successful = FALSE;
		g_signal_emit_by_name(user_data, "clicked");
	}
}

int main(int argc, char *argv[])
{
	GtkBuilder *gui_builder = NULL;
	GtkWidget *window = NULL, *button_search = NULL, *combo_query = NULL, *combo_entry = NULL, *settings_dialog = NULL;
	(void) argc;
	(void) argv;
	GtkExpander *expander = NULL;
	GError *err = NULL;
	gboolean first_run = FALSE, hotkey_reg_failed = FALSE;
	gchar *ui_file_path = NULL, *icon_file_path = NULL;

#ifdef G_OS_WIN32
	/* logic for single instance apps. on Win32 */
	HANDLE single_instance_mutex = CreateMutex(NULL, FALSE, TEXT(ARTHA_NAME));

	if(ERROR_ALREADY_EXISTS == GetLastError() || !single_instance_mutex)
	{
		hMainWindow = FindWindow(NULL, TEXT(STR_APP_TITLE));
		if(hMainWindow)
			PostMessage(hMainWindow, WM_ARTHA_RELAUNCH, 0, 0);
		return 0;
	}
#endif

#ifdef X11_AVAILABLE
	/* Xlib thread safety before ANY display is opened: the hotkey pump
	   thread shares our dedicated connection (never GDK's) */
	XInitThreads();
#endif

	g_set_application_name(ARTHA_NAME);

	if(gtk_init_check())
	{
#ifdef DBUS_AVAILABLE
		/* if we're not the first instance of artha, then quit */
		if(FALSE == instance_handler_am_i_unique())
		{
			/* signal the primary instance of this invocation */
			instance_handler_send_signal();

			/* notify the desktop env. that the start up is complete */
			notify_startup_complete();

			return 0;
		}
#endif
		gui_builder = gtk_builder_new();
		if(gui_builder)
		{
			ui_file_path = g_build_filename(APP_DIR, UI_FILE, NULL);
			if(gtk_builder_add_from_file(gui_builder, ui_file_path, &err))
			{
				/* if WordNet's database files are not opened, open it */
				if((OpenDB != 1) && (wninit() != 0))
						show_message_dlg(NULL, MSG_DB_LOAD_ERROR);

				status_label = GTK_LABEL(gtk_builder_get_object(gui_builder, "statusLabel"));

				window = GTK_WIDGET(gtk_builder_get_object(gui_builder, WINDOW_MAIN));
				if(window)
				{
#ifdef DBUS_AVAILABLE
					/* if the control's here, then it's sure that we're the unique instance;
					register for dbus signals from possible duplicate instances */
					if(!instance_handler_register_signal(GTK_WINDOW(window)))
					{
						g_error("Unable to register for duplicate instance signals!\n");
					}
#endif
					/* try to load preferences and see if this is the first run */
					first_run = load_preferences(GTK_WINDOW(window));

#ifdef X11_AVAILABLE

					/* A dedicated X11 connection for the hotkey. Never share it
					   with GDK (XNextEvent here would steal GTK's events).
					   On Wayland or when X is otherwise unavailable, the
					   global hotkey is disabled - the app works regardless. */
					{
						GdkDisplay *gd = gdk_display_get_default();

						if(gd != NULL && GDK_IS_X11_DISPLAY(gd))
							dpy = XOpenDisplay(NULL);
					}

					if(NULL == dpy)
					{
						g_warning("Can't open an X11 display; the global hotkey is disabled!\n");
					}
					else
					{
						G_DEBUG("XDisplay Opened!\n");

						XSynchronize(dpy, True);	/* without this, error callback is delayed */
						XSetErrorHandler(x_error_handler);

						lookup_ignorable_modifiers();
					}

#elif defined G_OS_WIN32

					/* GTK4 has no window filters; the Win32 hotkey receiver
					   (WndProc hook) is not yet ported. Global hotkeys are
					   unavailable in this build; Artha works regardless. */

#endif // X11_AVAILABLE

					if(app_hotkey.accel_key || first_run)
					{
						if(register_unregister_hotkey(first_run, TRUE))
						{
							hotkey_set = TRUE;
#ifdef X11_AVAILABLE
							/* start the blocking XNextEvent pump on our dedicated connection */
							if(NULL != dpy)
								g_thread_new("artha-hotkey-pump", hotkey_thread_func, gui_builder);
#endif

							if(first_run)
							{
								show_message_dlg(window, MSG_HOTKEY_SUCCEEDED_FIRST_RUN);
							}
						}
						else
						{
							if(first_run)
								show_message_dlg(window, MSG_HOTKEY_FAILED_FIRST_RUN);
							else
							{
								hotkey_reg_failed = TRUE;

								show_message_dlg(window, MSG_HOTKEY_FAILED);
								/* since the previously set hotkey is now not registerable, memzero
								   (i.e. disable) app_hotkey and save prefs */
								app_hotkey.accel_key = app_hotkey.accel_mods = (GdkModifierType) (app_hotkey.accel_flags = 0);
							}
						}
					}

					/* save preferences here - after all setting based loads are done */
					save_preferences();

					setup_settings_dialog(gui_builder);
					settings_dialog = GTK_WIDGET(gtk_builder_get_object(gui_builder, DIALOG_OPTIONS));

					icon_file_path = g_build_filename(ICON_DIR, ICON_FILE, NULL);
					if(!g_file_test(icon_file_path, G_FILE_TEST_IS_REGULAR))
					{
						g_warning("Error loading icon file!\n%s not found!\n", icon_file_path);
						g_free(icon_file_path);
						icon_file_path = NULL;
					}

					mod_notify_init();

					setup_toolbar(gui_builder);

					/* Register the StatusNotifierItem with the session bus's
					   org.kde.StatusNotifierWatcher, if one exists. Without a
					   watcher (stock GNOME) there is simply no tray icon;
					   Artha works regardless. */
					status_notifier_setup(icon_file_path, tray_activate_toggle, gui_builder);
					status_notifier_set_visible(show_trayicon);

					g_free(icon_file_path);
					icon_file_path = NULL;


					button_search = GTK_WIDGET(gtk_builder_get_object(gui_builder, BUTTON_SEARCH));
					g_signal_connect(button_search, "clicked", G_CALLBACK(button_search_click), gui_builder);

					combo_query = GTK_WIDGET(gtk_builder_get_object(gui_builder, COMBO_QUERY));
					g_signal_connect(combo_query, "changed", G_CALLBACK(combo_query_changed), gui_builder);

					/* get the GtkEntry in GtkComboBox and set activates-default to TRUE; so that
					   it pass the ENTER key signal to query button */
					combo_entry = GTK_WIDGET(gtk_combo_box_get_child(GTK_COMBO_BOX(combo_query)));
					g_object_set(combo_entry, "activates-default", TRUE, NULL);

					create_text_view_tags(gui_builder);

					/* do main window specific connects: closing the window hides it
					   (tray-only mode), it does not destroy the app */
					g_signal_connect(window, "close-request", G_CALLBACK(close_request_cb), NULL);
					g_signal_connect(window, "map", G_CALLBACK(window_mapped), button_search);

					{
						GtkEventController *scroll = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL |
											       GTK_EVENT_CONTROLLER_SCROLL_DISCRETE);
						g_signal_connect(scroll, "scroll", G_CALLBACK(combo_query_scroll), button_search);
						gtk_widget_add_controller(combo_query, scroll);
					}

					expander = GTK_EXPANDER(gtk_builder_get_object(gui_builder, EXPANDER));
					g_signal_connect(expander, "activate", G_CALLBACK(expander_clicked), gui_builder);

					gtk_widget_grab_focus(GTK_WIDGET(combo_query));

					G_DEBUG("GUI loaded successfully!\n");

					create_stores_renderers(gui_builder);

					mod_suggest = suggestions_init();

					// show the window if it's a first run or a hotkey couldn't be set
					// if the window is not shown, set notify the startup is complete
					if(first_run || hotkey_reg_failed || !launch_minimized)
						gtk_widget_set_visible(window, TRUE);
					else
						notify_startup_complete();

					// index all wordnet terms from the index.sense onto memory
					WordnetTermsLoaderData loader_data = { gui_builder };
					wordnet_terms_load(&loader_data);

					main_loop = g_main_loop_new(NULL, FALSE);
					g_main_loop_run(main_loop);
					g_main_loop_unref(main_loop);
					main_loop = NULL;

					status_notifier_cleanup();
					destructor(gui_builder);

					/* GtkBuilder drops references to any held, except toplevel widgets */
					gtk_window_destroy(GTK_WINDOW(settings_dialog));
					gtk_window_destroy(GTK_WINDOW(window));
				}
				else
				{
					g_error("Error loading GUI!\n Corrupted data in UI file!\n");
				}
			}
			else
			{
				/* shouldn't call g_error directly, since it's a terminating call;
				   so print the error, free the resources and then do it */
				g_print("%s: ", err->message);
				g_error_free(err);
				err = NULL;
				g_error("Error loading GUI from %s!\n", ui_file_path);
			}

			g_free(ui_file_path);
			ui_file_path = NULL;
		}
		else
		{
			g_error("Error creating GtkBuilder!\n");
		}
	}
	else
	{
		g_error("Error initializing GUI!\n");
	}

#ifdef G_OS_WIN32
	CloseHandle(single_instance_mutex);
#endif

	return 0;
}

