/*
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


 /** \file
 * Free text search (front component)
 */
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>

#include "gtk/compat.h"
#include "gtk/search.h"
#include "gtk/scaffolding.h"
#include "gtk/window.h"
#include "utils/config.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "desktop/browser_private.h"
#include "desktop/gui.h"
#include "desktop/search.h"
#include "desktop/searchweb.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

static void nsgtk_search_set_status(bool found, void *p);
static void nsgtk_search_set_hourglass(bool active, void *p);
static void nsgtk_search_add_recent(const char *string, void *p);

static struct gui_search_callbacks nsgtk_search_callbacks = {
	nsgtk_search_set_forward_state,
	nsgtk_search_set_back_state,
	nsgtk_search_set_status,
	nsgtk_search_set_hourglass,
	nsgtk_search_add_recent
};

/** connected to the search forward button */

gboolean nsgtk_search_forward_button_clicked(GtkWidget *widget, gpointer data)
{
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;
	struct browser_window *bw = nsgtk_get_browser_window(
			nsgtk_scaffolding_top_level(g));

	assert(bw);

	search_flags_t flags = SEARCH_FLAG_FORWARDS |
			(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			nsgtk_scaffolding_search(g)->caseSens)) ?
			SEARCH_FLAG_CASE_SENSITIVE : 0) | 
			(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			nsgtk_scaffolding_search(g)->checkAll)) ?
			SEARCH_FLAG_SHOWALL : 0);

	browser_window_search(bw, &nsgtk_search_callbacks, (void *)bw, flags,
			gtk_entry_get_text(nsgtk_scaffolding_search(g)->entry));
	return TRUE;
}

/** connected to the search back button */

gboolean nsgtk_search_back_button_clicked(GtkWidget *widget, gpointer data)
{
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;
	struct browser_window *bw = nsgtk_get_browser_window(
			nsgtk_scaffolding_top_level(g));

	assert(bw);

	search_flags_t flags = 0 |(gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(
			nsgtk_scaffolding_search(g)->caseSens)) ?
			SEARCH_FLAG_CASE_SENSITIVE : 0) | 
			(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			nsgtk_scaffolding_search(g)->checkAll)) ?
			SEARCH_FLAG_SHOWALL : 0);

	browser_window_search(bw, &nsgtk_search_callbacks, (void *)bw, flags,
			gtk_entry_get_text(nsgtk_scaffolding_search(g)->entry));
	return TRUE;
}

/** connected to the search close button */

gboolean nsgtk_search_close_button_clicked(GtkWidget *widget, gpointer data)
{
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;
	nsgtk_scaffolding_toggle_search_bar_visibility(g);
	return TRUE;	
}

/** connected to the search entry [typing] */

gboolean nsgtk_search_entry_changed(GtkWidget *widget, gpointer data)
{
	nsgtk_scaffolding *g = (nsgtk_scaffolding *)data;
	struct browser_window *bw = nsgtk_get_browser_window(
			nsgtk_scaffolding_top_level(g));

	assert(bw != NULL);

	nsgtk_search_set_forward_state(true, (void *)bw);
	nsgtk_search_set_back_state(true, (void *)bw);

	search_flags_t flags = SEARCH_FLAG_FORWARDS |
			(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			nsgtk_scaffolding_search(g)->caseSens)) ?
			SEARCH_FLAG_CASE_SENSITIVE : 0) | 
			(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			nsgtk_scaffolding_search(g)->checkAll)) ?
			SEARCH_FLAG_SHOWALL : 0);

	browser_window_search(bw, &nsgtk_search_callbacks, (void *)bw, flags,
			gtk_entry_get_text(nsgtk_scaffolding_search(g)->entry));
	return TRUE;
}

/** connected to the search entry [return key] */

gboolean nsgtk_search_entry_activate(GtkWidget *widget, gpointer data)
{
	nsgtk_scaffolding *g = (nsgtk_scaffolding *)data;
	struct browser_window *bw = nsgtk_get_browser_window(
			nsgtk_scaffolding_top_level(g));

	assert(bw);

	search_flags_t flags = SEARCH_FLAG_FORWARDS |
			(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			nsgtk_scaffolding_search(g)->caseSens)) ?
			SEARCH_FLAG_CASE_SENSITIVE : 0) | 
			(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			nsgtk_scaffolding_search(g)->checkAll)) ?
			SEARCH_FLAG_SHOWALL : 0);

	browser_window_search(bw, &nsgtk_search_callbacks, (void *)bw, flags,
			gtk_entry_get_text(nsgtk_scaffolding_search(g)->entry));
	return FALSE;
}

/** allows escape key to close search bar too */

gboolean nsgtk_search_entry_key(GtkWidget *widget, GdkEventKey *event, 
		gpointer data)
{
	if (event->keyval == GDK_KEY(Escape)) {
		struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;
		nsgtk_scaffolding_toggle_search_bar_visibility(g);
	}
	return FALSE;
}

/** connected to the websearch entry [return key] */

gboolean nsgtk_websearch_activate(GtkWidget *widget, gpointer data)
{
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;
	temp_open_background = 0;
	search_web_new_window(nsgtk_get_browser_window(
			nsgtk_scaffolding_top_level(g)),
			(char *)gtk_entry_get_text(GTK_ENTRY(
			nsgtk_scaffolding_websearch(g))));
	temp_open_background = -1;
	return TRUE;
}

/**
 * allows a click in the websearch entry field to clear the name of the
 * provider
 */

gboolean nsgtk_websearch_clear(GtkWidget *widget, GdkEventFocus *f, 
		gpointer data)
{
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;
	gtk_editable_select_region(GTK_EDITABLE(
			nsgtk_scaffolding_websearch(g)), 0, -1);
	gtk_widget_grab_focus(GTK_WIDGET(nsgtk_scaffolding_websearch(g)));
	return TRUE;
}

/**
* Change the displayed search status.
* \param found  search pattern matched in text
* \param p the pointer sent to search_verify_new() / search_create_context()
*/

void nsgtk_search_set_status(bool found, void *p)
{
}

/**
* display hourglass while searching
* \param active start/stop indicator
* \param p the pointer sent to search_verify_new() / search_create_context()
*/

void nsgtk_search_set_hourglass(bool active, void *p)
{
}

/**
* add search string to recent searches list
* front is at liberty how to implement the bare notification
* should normally store a strdup() of the string;
* core gives no guarantee of the integrity of the const char *
* \param string search pattern
* \param p the pointer sent to search_verify_new() / search_create_context()
*/

void nsgtk_search_add_recent(const char *string, void *p)
{
}

/**
* activate search forwards button in gui
* \param active activate/inactivate
* \param p the pointer sent to search_verify_new() / search_create_context()
*/

void nsgtk_search_set_forward_state(bool active, void *p)
{
	struct browser_window *bw = (struct browser_window *)p;
	if ((bw != NULL) && (bw->window != NULL)) {
		struct gtk_scaffolding *g = nsgtk_get_scaffold(bw->window);
		gtk_widget_set_sensitive(GTK_WIDGET(nsgtk_scaffolding_search(
				g)->buttons[1]), active);
	}
}

/**
* activate search back button in gui
* \param active activate/inactivate
* \param p the pointer sent to search_verify_new() / search_create_context()
*/

void nsgtk_search_set_back_state(bool active, void *p)
{
	struct browser_window *bw = (struct browser_window *)p;
	if ((bw != NULL) && (bw->window != NULL)) {
		struct gtk_scaffolding *g = nsgtk_get_scaffold(bw->window);
		gtk_widget_set_sensitive(GTK_WIDGET(nsgtk_scaffolding_search(
				g)->buttons[0]), active);
	}
}
