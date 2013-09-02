/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
 * Copyright 2010 Stephen Fryatt <stevef@netsurf-browser.org>
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
 * Cookies (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "content/urldb.h"
#include "desktop/cookies_old.h"
#include "desktop/tree.h"
#include "riscos/cookies.h"
#include "riscos/dialog.h"
#include "riscos/menus.h"
#include "utils/nsoption.h"
#include "riscos/toolbar.h"
#include "riscos/treeview.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "utils/messages.h"
#include "utils/log.h"
#include "utils/url.h"
#include "utils/utils.h"

static void ro_gui_cookies_toolbar_update_buttons(void);
static void ro_gui_cookies_toolbar_save_buttons(char *config);
static bool ro_gui_cookies_menu_prepare(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_pointer *pointer);
static void ro_gui_cookies_menu_warning(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action);
static bool ro_gui_cookies_menu_select(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action);
static void ro_gui_cookies_toolbar_click(button_bar_action action);

struct ro_treeview_callbacks ro_cookies_treeview_callbacks = {
	ro_gui_cookies_toolbar_click,
	ro_gui_cookies_toolbar_update_buttons,
	ro_gui_cookies_toolbar_save_buttons
};

/* The RISC OS cookie window, toolbar and treeview data. */

static struct ro_cookies_window {
	wimp_w		window;
	struct toolbar	*toolbar;
	ro_treeview	*tv;
	wimp_menu	*menu;
} cookies_window;

/**
 * Pre-Initialise the cookies tree.  This is called for things that
 * need to be done at the gui_init() stage, such as loading templates.
 */

void ro_gui_cookies_preinitialise(void)
{
	/* Create our window. */

	cookies_window.window = ro_gui_dialog_create("tree");
	ro_gui_set_window_title(cookies_window.window,
			messages_get("Cookies"));
}

/**
 * Initialise cookies tree, at the gui_init2() stage.
 */

void ro_gui_cookies_postinitialise(void)
{
	/* Create our toolbar. */

	cookies_window.toolbar = ro_toolbar_create(NULL, cookies_window.window,
			THEME_STYLE_COOKIES_TOOLBAR, TOOLBAR_FLAGS_NONE,
			ro_treeview_get_toolbar_callbacks(), NULL,
			"HelpCookiesToolbar");
	if (cookies_window.toolbar != NULL) {
		ro_toolbar_add_buttons(cookies_window.toolbar,
				cookies_toolbar_buttons,
				       nsoption_charp(toolbar_cookies));
		ro_toolbar_rebuild(cookies_window.toolbar);
	}

	/* Create the treeview with the window and toolbar. */

	cookies_window.tv = ro_treeview_create(cookies_window.window,
			cookies_window.toolbar, &ro_cookies_treeview_callbacks,
			TREE_COOKIES);
	if (cookies_window.tv == NULL) {
		LOG(("Failed to allocate treeview"));
		return;
	}

	ro_toolbar_update_client_data(cookies_window.toolbar,
			cookies_window.tv);

	/* Initialise the cookies into the tree. */

	cookies_initialise(ro_treeview_get_tree(cookies_window.tv),
			   tree_directory_icon_name,
			   tree_content_icon_name);


	/* Build the cookies window menu. */

	static const struct ns_menu cookies_definition = {
		"Cookies", {
			{ "Cookies", NO_ACTION, 0 },
			{ "Cookies.Expand", TREE_EXPAND_ALL, 0 },
			{ "Cookies.Expand.All", TREE_EXPAND_ALL, 0 },
			{ "Cookies.Expand.Folders", TREE_EXPAND_FOLDERS, 0 },
			{ "Cookies.Expand.Links", TREE_EXPAND_LINKS, 0 },
			{ "Cookies.Collapse", TREE_COLLAPSE_ALL, 0 },
			{ "Cookies.Collapse.All", TREE_COLLAPSE_ALL, 0 },
			{ "Cookies.Collapse.Folders", TREE_COLLAPSE_FOLDERS, 0 },
			{ "Cookies.Collapse.Links", TREE_COLLAPSE_LINKS, 0 },
			{ "Cookies.Toolbars", NO_ACTION, 0 },
			{ "_Cookies.Toolbars.ToolButtons", TOOLBAR_BUTTONS, 0 },
			{ "Cookies.Toolbars.EditToolbar",TOOLBAR_EDIT, 0 },
			{ "Selection", TREE_SELECTION, 0 },
			{ "Selection.Delete", TREE_SELECTION_DELETE, 0 },
			{ "SelectAll", TREE_SELECT_ALL, 0 },
			{ "Clear", TREE_CLEAR_SELECTION, 0 },
			{NULL, 0, 0}
		}
	};
	cookies_window.menu = ro_gui_menu_define_menu(&cookies_definition);

	ro_gui_wimp_event_register_menu(cookies_window.window,
			cookies_window.menu, false, false);
	ro_gui_wimp_event_register_menu_prepare(cookies_window.window,
			ro_gui_cookies_menu_prepare);
	ro_gui_wimp_event_register_menu_selection(cookies_window.window,
			ro_gui_cookies_menu_select);
	ro_gui_wimp_event_register_menu_warning(cookies_window.window,
			ro_gui_cookies_menu_warning);
}

/**
 * Open the cookies window.
 *
 */

void ro_gui_cookies_open(void)
{
	tree_set_redraw(ro_treeview_get_tree(cookies_window.tv), true);

	ro_gui_cookies_toolbar_update_buttons();

	if (!ro_gui_dialog_open_top(cookies_window.window,
			cookies_window.toolbar, 600, 800)) {
		ro_treeview_set_origin(cookies_window.tv, 0,
				-(ro_toolbar_height(cookies_window.toolbar)));
	}
}


/**
 * Handle toolbar button clicks.
 *
 * \param  action		The action to handle
 */

void ro_gui_cookies_toolbar_click(button_bar_action action)
{
	switch (action) {
	case TOOLBAR_BUTTON_DELETE:
		cookies_delete_selected();
		break;

	case TOOLBAR_BUTTON_EXPAND:
		cookies_expand_cookies();
		break;

	case TOOLBAR_BUTTON_COLLAPSE:
		cookies_collapse_cookies();
		break;

	case TOOLBAR_BUTTON_OPEN:
		cookies_expand_domains();
		break;

	case TOOLBAR_BUTTON_CLOSE:
		cookies_collapse_domains();
		break;

	default:
		break;
	}
}


/**
 * Update the button state in the cookies toolbar.
 */

void ro_gui_cookies_toolbar_update_buttons(void)
{
	ro_toolbar_set_button_shaded_state(cookies_window.toolbar,
			TOOLBAR_BUTTON_DELETE,
			!ro_treeview_has_selection(cookies_window.tv));
}


/**
 * Save a new button arrangement in the cookies toolbar.
 *
 * \param *config		The new button configuration string.
 */

void ro_gui_cookies_toolbar_save_buttons(char *config)
{
	nsoption_set_charp(toolbar_cookies, config);
	ro_gui_save_options();
}


/**
 * Prepare the cookies menu for opening
 *
 * \param  w			The window owning the menu.
 * \param  i			The icon owning the menu.
 * \param  *menu		The menu about to be opened.
 * \param  *pointer		Pointer to the relevant wimp event block, or
 *				NULL for an Adjust click.
 * \return			true if the event was handled; else false.
 */

bool ro_gui_cookies_menu_prepare(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_pointer *pointer)
{
	bool selection;

	if (menu != cookies_window.menu)
		return false;

	selection = ro_treeview_has_selection(cookies_window.tv);

	ro_gui_menu_set_entry_shaded(cookies_window.menu,
			TREE_SELECTION, !selection);
	ro_gui_menu_set_entry_shaded(cookies_window.menu,
			TREE_CLEAR_SELECTION, !selection);

	ro_gui_menu_set_entry_shaded(menu, TOOLBAR_BUTTONS,
			ro_toolbar_menu_option_shade(cookies_window.toolbar));
	ro_gui_menu_set_entry_ticked(menu, TOOLBAR_BUTTONS,
			ro_toolbar_menu_buttons_tick(cookies_window.toolbar));

	ro_gui_menu_set_entry_shaded(menu, TOOLBAR_EDIT,
			ro_toolbar_menu_edit_shade(cookies_window.toolbar));
	ro_gui_menu_set_entry_ticked(menu, TOOLBAR_EDIT,
			ro_toolbar_menu_edit_tick(cookies_window.toolbar));

	return true;
}

/**
 * Handle submenu warnings for the cookies menu
 *
 * \param  w			The window owning the menu.
 * \param  i			The icon owning the menu.
 * \param  *menu		The menu to which the warning applies.
 * \param  *selection		The wimp menu selection data.
 * \param  action		The selected menu action.
 */

void ro_gui_cookies_menu_warning(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action)
{
	/* Do nothing */
}

/**
 * Handle selections from the cookies menu
 *
 * \param  w			The window owning the menu.
 * \param  i			The icon owning the menu.
 * \param  *menu		The menu from which the selection was made.
 * \param  *selection		The wimp menu selection data.
 * \param  action		The selected menu action.
 * \return			true if action accepted; else false.
 */

bool ro_gui_cookies_menu_select(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action)
{
	switch (action) {
	case TREE_EXPAND_ALL:
		cookies_expand_all();
		return true;
	case TREE_EXPAND_FOLDERS:
		cookies_expand_domains();
		return true;
	case TREE_EXPAND_LINKS:
		cookies_expand_cookies();
		return true;
	case TREE_COLLAPSE_ALL:
		cookies_collapse_all();
		return true;
	case TREE_COLLAPSE_FOLDERS:
		cookies_collapse_domains();
		return true;
	case TREE_COLLAPSE_LINKS:
		cookies_collapse_cookies();
		return true;
	case TREE_SELECTION_DELETE:
		cookies_delete_selected();
		return true;
	case TREE_SELECT_ALL:
		cookies_select_all();
		return true;
	case TREE_CLEAR_SELECTION:
		cookies_clear_selection();
		return true;
	case TOOLBAR_BUTTONS:
		ro_toolbar_set_display_buttons(cookies_window.toolbar,
				!ro_toolbar_get_display_buttons(
					cookies_window.toolbar));
		return true;
	case TOOLBAR_EDIT:
		ro_toolbar_toggle_edit(cookies_window.toolbar);
		return true;
	default:
		return false;
	}

	return false;
}

/**
 * Check if a particular window handle is the cookies window
 *
 * \param window  the window in question
 * \return  true if this window is the cookies
 */

bool ro_gui_cookies_check_window(wimp_w window)
{
	if (cookies_window.window == window)
		return true;
	else
		return false;
}

/**
 * Check if a particular menu handle is the cookies menu
 *
 * \param  *menu		The menu in question.
 * \return			true if this menu is the cookies menu
 */

bool ro_gui_cookies_check_menu(wimp_menu *menu)
{
	if (cookies_window.menu == menu)
		return true;
	else
		return false;
}

