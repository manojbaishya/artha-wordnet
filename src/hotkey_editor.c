/* hotkey_editor.c
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
 * This code sets up the hotkey editor button used in the hotkey changer
 * dialog. The button displays the current global hotkey (via
 * gtk_accelerator_get_label()); while it is in capture mode it records
 * the next key combination pressed.
 *
 * This used to be a one-row GtkTreeView with a GtkCellRendererAccel,
 * neither of which exists in GTK 4.
 */


#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include "gui.h"

#define STR_INVALID_HOTKEY "The shortcut \"%s\" cannot be used because \
it will become impossible to type using this key.\nPlease try with a \
combination key such as Control, Alt or Shift at the same time."

#define STR_DUPLICATE_HOTKEY "This key combination (%s) is perhaps already \
taken up by another application. Please choose a different one!"

#define STR_INVALID_HOTKEY_TITLE	"Invalid Hotkey"

#define STR_CAPTURE_PROMPT		"Press Shortcut…"
#define STR_NO_HOTKEY			"None"

/* these keys should never be part of the hotkey combo */
static const guint forbidden_keyvals[] =
{
	/* Navigation keys */
	GDK_KEY_Home,
	GDK_KEY_Left,
	GDK_KEY_Up,
	GDK_KEY_Right,
	GDK_KEY_Down,
	GDK_KEY_Page_Up,
	GDK_KEY_Page_Down,
	GDK_KEY_End,
	GDK_KEY_Tab,

	/* Return */
	GDK_KEY_KP_Enter,
	GDK_KEY_Return,

	GDK_KEY_space,
	GDK_KEY_Mode_switch,
	GDK_KEY_Delete,
	GDK_KEY_Print,
	GDK_KEY_Insert
};

/* modifier keyvals that can never complete a hotkey combo on their own;
   while capturing we swallow them and keep waiting for a real key */
static gboolean keyval_is_modifier(guint keyval)
{
	switch(keyval)
	{
		case GDK_KEY_Shift_L:
		case GDK_KEY_Shift_R:
		case GDK_KEY_Control_L:
		case GDK_KEY_Control_R:
		case GDK_KEY_Meta_L:
		case GDK_KEY_Meta_R:
		case GDK_KEY_Alt_L:
		case GDK_KEY_Alt_R:
		case GDK_KEY_Super_L:
		case GDK_KEY_Super_R:
		case GDK_KEY_Hyper_L:
		case GDK_KEY_Hyper_R:
		case GDK_KEY_ISO_Level3_Shift:
		case GDK_KEY_ISO_Level5_Shift:
		/* GDK_KEY_ISO_Group_Shift is an alias of GDK_KEY_Mode_switch (same value) */
		case GDK_KEY_Mode_switch:
		case GDK_KEY_Caps_Lock:
		case GDK_KEY_Num_Lock:
		case GDK_KEY_Scroll_Lock:
		case GDK_KEY_Multi_key:
		case GDK_KEY_ISO_Enter:
			return TRUE;
		default:
			return FALSE;
	}
}

gboolean grab_ungrab_with_ignorable_modifiers(ArthaAccelKey *binding, gboolean grab);
extern ArthaAccelKey app_hotkey;

/* present a modal-ish warning dialog tied to the hotkey editor window;
   GTK4 has no gtk_dialog_run; GTK >= 4.10 uses GtkAlertDialog
   (GtkMessageDialog is deprecated) */
#if !GTK_CHECK_VERSION(4, 10, 0)
static void hk_message_dialog_destroy(GtkWidget *dialog, gint response, gpointer user_data)
{
	(void) response;
	(void) user_data;

	gtk_window_destroy(GTK_WINDOW(dialog));
}
#endif

static void hk_show_message(GtkWidget *view, const gchar *title, const gchar *body_format,
			     const gchar *arg)
{
	GtkRoot *root = gtk_widget_get_root(GTK_WIDGET(view));
	GtkWindow *parent = GTK_IS_WINDOW(root) ? GTK_WINDOW(root) : NULL;
	gchar *body = g_strdup_printf(body_format, arg);

#if GTK_CHECK_VERSION(4, 10, 0)
	{
		GtkAlertDialog *alert = gtk_alert_dialog_new("%s", title);

		gtk_alert_dialog_set_detail(alert, body);
		gtk_alert_dialog_show(alert, parent);
		g_object_unref(alert);
	}
#else
	{
		GtkWidget *msg_dialog = gtk_message_dialog_new(parent,
						    GTK_DIALOG_DESTROY_WITH_PARENT,
						    GTK_MESSAGE_WARNING,
						    GTK_BUTTONS_OK,
						    "%s",
						    body);
		g_object_set(msg_dialog, "title", title, NULL);
		g_signal_connect(msg_dialog, "response", G_CALLBACK(hk_message_dialog_destroy), NULL);
		gtk_window_present(GTK_WINDOW(msg_dialog));
	}
#endif
	g_free(body);
}

static gboolean keyval_is_forbidden (guint keyval)
{
	guint i = 0;

	for (i = 0; i < G_N_ELEMENTS(forbidden_keyvals); i++)
	{
		if (keyval == forbidden_keyvals[i])
			return TRUE;
	}

	return FALSE;
}

/* the button doubles as the state: capture mode is on while it shows the
   prompt; capturing is a plain boolean so we don't keep extra objects alive */
static gboolean capturing = FALSE;

static void hotkey_editor_refresh_label(GtkWidget *button)
{
	gchar *accel_label = NULL;

	if(app_hotkey.accel_key)
		accel_label = gtk_accelerator_get_label (app_hotkey.accel_key, app_hotkey.accel_mods);

	gtk_button_set_label (GTK_BUTTON(button), accel_label ? accel_label : STR_NO_HOTKEY);
	g_free (accel_label);
}

static void hotkey_editor_leave_capture(GtkWidget *button)
{
	capturing = FALSE;
	hotkey_editor_refresh_label(button);
}

/* attach the capture key controller to the root window. GTK key events go to
   the widget holding the keyboard focus, which is not necessarily the button;
   a CAPTURE-phase controller on the toplevel sees every key the user presses
   regardless of focus. Done lazily on first capture because the button is
   not inside a window yet when create_hotkey_editor() returns.
   Ownership: gtk_widget_add_controller() does NOT take over our reference —
   releasing it would leave the window holding a dangling controller (assert
   storm on every crossing/key event). steal_data() moves our reference out
   of the button marker without firing the destroy notify; the dialog then
   parks it in the root and the window's lifetime keeps it valid. */
static void hotkey_editor_attach_capture_controller(GtkWidget *button)
{
	GtkWidget *root = GTK_WIDGET(gtk_widget_get_root(button));
	GtkEventController *controller = GTK_EVENT_CONTROLLER(
		g_object_steal_data(G_OBJECT(button), "artha-capture-controller"));

	if(controller == NULL)
		return;

	if(root == NULL ||
	   g_object_get_data(G_OBJECT(root), "artha-capture-controller-added") != NULL)
	{
		g_object_unref(controller);
		return;
	}

	gtk_widget_add_controller(root, controller);
	g_object_set_data(G_OBJECT(root), "artha-capture-controller-added", GINT_TO_POINTER(1));
	/* root now owns the controller; the window lives for the app's lifetime */
	g_object_set_data_full(G_OBJECT(root), "artha-capture-controller-keep",
			       controller, g_object_unref);
}

static void hotkey_editor_button_clicked(GtkButton *button, gpointer user_data)
{
	(void) user_data;

	if(capturing)
	{
		/* click while capturing cancels; the hotkey stays unchanged */
		hotkey_editor_leave_capture(GTK_WIDGET(button));
		return;
	}
	capturing = TRUE;
	gtk_button_set_label (button, STR_CAPTURE_PROMPT);
	hotkey_editor_attach_capture_controller(GTK_WIDGET(button));
	gtk_widget_grab_focus (GTK_WIDGET(button));
}

static void hotkey_editor_focus_leave(GtkEventControllerFocus *controller, gpointer user_data)
{
	GtkWidget *button = GTK_WIDGET(user_data);

	(void) controller;

	/* focus moved elsewhere while capturing: cancel, like the old cell renderer did */
	if(capturing)
		hotkey_editor_leave_capture(button);
}

static gboolean hotkey_editor_key_pressed(GtkEventControllerKey *controller, guint keyval,
					     guint keycode, GdkModifierType state, gpointer user_data)
{
	GtkWidget *button = GTK_WIDGET(user_data);
	ArthaAccelKey temp_key = {0};
	gchar *temp_str = NULL;
	gboolean reg_succeeded = FALSE;

	(void) controller;
	if(!capturing)
		return FALSE;

	/* CapsLock isn't supported as a keybinding modifier, so keep it from confusing us */
	state &= ~GDK_LOCK_MASK;

	if(keyval == GDK_KEY_Escape)
	{
		/* cancel capture; the hotkey stays unchanged */
		hotkey_editor_leave_capture(button);
		return TRUE;
	}

	if(keyval == GDK_KEY_BackSpace || keyval == GDK_KEY_Delete)
	{
		/* clear the hotkey */
		if(app_hotkey.accel_key)
			grab_ungrab_with_ignorable_modifiers(&app_hotkey, FALSE);
		app_hotkey.accel_key = app_hotkey.accel_mods = app_hotkey.accel_flags = 0;
		hotkey_editor_leave_capture(button);
		return TRUE;
	}

	/* pure modifier press: keep capturing and wait for the real key */
	if(keyval_is_modifier(keyval))
		return TRUE;

	temp_key.accel_key = keyval;
	temp_key.accel_flags = keycode;
	temp_key.accel_mods = state;

	/* sanity check and check to see if the same key combo was pressed again */
	if(app_hotkey.accel_key == temp_key.accel_key &&
	   app_hotkey.accel_mods == temp_key.accel_mods &&
	   app_hotkey.accel_flags == temp_key.accel_flags)
	{
		hotkey_editor_leave_capture(button);
		return TRUE;
	}

	/* Check for unmodified keys */
	if(temp_key.accel_mods == 0 && temp_key.accel_key != 0)
	{
		if ((temp_key.accel_key >= GDK_KEY_a && temp_key.accel_key <= GDK_KEY_z)
		   || (temp_key.accel_key >= GDK_KEY_A && temp_key.accel_key <= GDK_KEY_Z)
		   || (temp_key.accel_key >= GDK_KEY_0 && temp_key.accel_key <= GDK_KEY_9)
		   || (temp_key.accel_key >= GDK_KEY_kana_fullstop && temp_key.accel_key <= GDK_KEY_semivoicedsound)
		   || (temp_key.accel_key >= GDK_KEY_Arabic_comma && temp_key.accel_key <= GDK_KEY_Arabic_sukun)
		   || (temp_key.accel_key >= GDK_KEY_Serbian_dje && temp_key.accel_key <= GDK_KEY_Cyrillic_HARDSIGN)
		   || (temp_key.accel_key >= GDK_KEY_Greek_ALPHAaccent && temp_key.accel_key <= GDK_KEY_Greek_omega)
		   || (temp_key.accel_key >= GDK_KEY_hebrew_doublelowline && temp_key.accel_key <= GDK_KEY_hebrew_taf)
		   || (temp_key.accel_key >= GDK_KEY_Thai_kokai && temp_key.accel_key <= GDK_KEY_Thai_lekkao)
		   || (temp_key.accel_key >= GDK_KEY_Hangul && temp_key.accel_key <= GDK_KEY_Hangul_Special)
		   || (temp_key.accel_key >= GDK_KEY_Hangul_Kiyeog && temp_key.accel_key <= GDK_KEY_Hangul_J_YeorinHieuh)
		   || keyval_is_forbidden (temp_key.accel_key))
		{
			temp_str = gtk_accelerator_get_label (keyval, state);

			hotkey_editor_leave_capture(button);
			hk_show_message(button, STR_INVALID_HOTKEY_TITLE, STR_INVALID_HOTKEY, temp_str);
			g_free (temp_str); temp_str = NULL;
			return TRUE;
		}
	}

#ifdef G_OS_WIN32
	/* Vista onwards calling RegisterHotKey registers more than one hotkey
	with the same ID, avoid this by unregistering before registering
	a new one */
	if(!app_hotkey.accel_key || grab_ungrab_with_ignorable_modifiers(&app_hotkey, FALSE))
	{
		reg_succeeded = grab_ungrab_with_ignorable_modifiers(&temp_key, TRUE);
		/* if the new hotkey couldn't be set, then reset the old hotkey */
		if(!reg_succeeded)
		{
			grab_ungrab_with_ignorable_modifiers(&app_hotkey, TRUE);
		}
#else
	/* try registering the new key combo */
	if((reg_succeeded = grab_ungrab_with_ignorable_modifiers(&temp_key, TRUE)))
	{
		/* unregister the previous hotkey */
		grab_ungrab_with_ignorable_modifiers(&app_hotkey, FALSE);
#endif
	}

	if(reg_succeeded)
	{
		/* set the value so that it gets reflected in the button label */
		app_hotkey = temp_key;
	}
	else
	{
		temp_str = gtk_accelerator_get_label (keyval, state);

		hotkey_editor_leave_capture(button);
		hk_show_message(button, STR_INVALID_HOTKEY_TITLE, STR_DUPLICATE_HOTKEY, temp_str);

		g_free (temp_str); temp_str = NULL;

		return TRUE;
	}

	hotkey_editor_leave_capture(button);
	return TRUE;
}

GtkWidget* create_hotkey_editor(void)
{
	GtkWidget *hotkey_button = NULL;
	GtkEventController *key_controller = NULL;
	GtkEventController *focus_controller = NULL;

	/* a flat button displaying the current accelerator; activate it to capture
	   the next key combo, Escape to cancel, BackSpace/Delete to clear */
	hotkey_button = gtk_button_new_with_label(STR_NO_HOTKEY);
	gtk_widget_set_hexpand(hotkey_button, TRUE);
	gtk_widget_set_halign(hotkey_button, GTK_ALIGN_FILL);
	g_object_set(hotkey_button, "visible", TRUE, NULL);

	key_controller = gtk_event_controller_key_new();
	/* CAPTURE phase + attached to the root window (see button_clicked), so the
	   next key combo is recorded even if focus is not on the button */
	gtk_event_controller_set_propagation_phase(key_controller, GTK_PHASE_CAPTURE);
	g_signal_connect(key_controller, "key-pressed", G_CALLBACK(hotkey_editor_key_pressed), hotkey_button);
	g_object_set_data_full(G_OBJECT(hotkey_button), "artha-capture-controller",
			       key_controller, g_object_unref);

	focus_controller = gtk_event_controller_focus_new();
	g_signal_connect(focus_controller, "leave", G_CALLBACK(hotkey_editor_focus_leave), hotkey_button);
	gtk_widget_add_controller(hotkey_button, focus_controller);

	g_signal_connect(hotkey_button, "clicked", G_CALLBACK(hotkey_editor_button_clicked), NULL);

	hotkey_editor_refresh_label(hotkey_button);

	return hotkey_button;
}
