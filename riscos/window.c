/*
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
 * Copyright 2005 Richard Wilson <info@tinct.net>
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
 * Copyright 2010, 2011 Stephen Fryatt <stevef@netsurf-browser.org>
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
 * Browser window handling (implementation).
 */

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include "oslib/colourtrans.h"
#include "oslib/osbyte.h"
#include "oslib/osfile.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "utils/config.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "css/css.h"
#include "desktop/browser.h"
#include "desktop/cookies.h"
#include "desktop/frames.h"
#include "desktop/history_core.h"
#include "desktop/hotlist.h"
#include "desktop/mouse.h"
#include "desktop/plotters.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"
#include "desktop/thumbnail.h"
#include "desktop/tree.h"
#include "desktop/gui.h"
#include "render/box.h"
#include "render/form.h"
#include "riscos/bitmap.h"
#include "riscos/buffer.h"
#include "riscos/cookies.h"
#include "riscos/dialog.h"
#include "riscos/global_history.h"
#include "riscos/gui.h"
#include "riscos/gui/status_bar.h"
#include "riscos/help.h"
#include "riscos/hotlist.h"
#include "riscos/menus.h"
#include "riscos/options.h"
#include "riscos/oslib_pre7.h"
#include "riscos/save.h"
#include "riscos/content-handlers/sprite.h"
#include "riscos/toolbar.h"
#include "riscos/thumbnail.h"
#include "riscos/url_complete.h"
#include "riscos/url_suggest.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/wimputils.h"
#include "riscos/window.h"
#include "utils/log.h"
#include "utils/talloc.h"
#include "utils/url.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "utils/messages.h"


static void gui_window_set_extent(struct gui_window *g, int width, int height);

static void ro_gui_window_redraw(wimp_draw *redraw);
static void ro_gui_window_open(wimp_open *open);
static void ro_gui_window_close(wimp_w w);
static bool ro_gui_window_click(wimp_pointer *mouse);
static bool ro_gui_window_keypress(wimp_key *key);
static bool ro_gui_window_toolbar_keypress(void *data, wimp_key *key);
static bool ro_gui_window_handle_local_keypress(struct gui_window *g,
		wimp_key *key, bool is_toolbar);
static bool ro_gui_window_menu_prepare(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_pointer *pointer);
static void ro_gui_window_menu_warning(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action);
static bool ro_gui_window_menu_select(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action);
static void ro_gui_window_menu_close(wimp_w w, wimp_i i, wimp_menu *menu);

static void ro_gui_window_toolbar_click(void *data,
		toolbar_action_type action_type, union toolbar_action action);

static bool ro_gui_window_content_export_types(hlcache_handle *h,
		bool *export_draw, bool *export_sprite);
static bool ro_gui_window_up_available(struct browser_window *bw);
static void ro_gui_window_prepare_pageinfo(struct gui_window *g);
static void ro_gui_window_prepare_objectinfo(hlcache_handle *object,
		const char *href);

static void ro_gui_window_launch_url(struct gui_window *g, const char *url);
static bool ro_gui_window_navigate_up(struct gui_window *g, const char *url);
static void ro_gui_window_action_home(struct gui_window *g);
static void ro_gui_window_action_new_window(struct gui_window *g);
static void ro_gui_window_action_local_history(struct gui_window *g);
static void ro_gui_window_action_navigate_back_new(struct gui_window *g);
static void ro_gui_window_action_navigate_forward_new(struct gui_window *g);
static void ro_gui_window_action_save(struct gui_window *g,
		gui_save_type save_type);
static void ro_gui_window_action_search(struct gui_window *g);
static void ro_gui_window_action_zoom(struct gui_window *g);
static void ro_gui_window_action_add_bookmark(struct gui_window *g);
static void ro_gui_window_action_print(struct gui_window *g);
static void ro_gui_window_action_page_info(struct gui_window *g);

static void ro_gui_window_remove_update_boxes(struct gui_window *g);
static void ro_gui_window_update_toolbar_buttons(struct gui_window *g);
static void ro_gui_window_update_toolbar(void *data);
static void ro_gui_window_save_toolbar_buttons(void *data, char *config);
static void ro_gui_window_update_theme(void *data, bool ok);

static bool ro_gui_window_import_text(struct gui_window *g,
		const char *filename, bool toolbar);
static void ro_gui_window_clone_options(struct browser_window *new_bw,
		struct browser_window *old_bw);

static bool ro_gui_window_prepare_form_select_menu(struct browser_window *bw,
		struct form_control *control);
static void ro_gui_window_process_form_select_menu(struct gui_window *g,
		wimp_selection *selection);

#ifndef wimp_KEY_END
#define wimp_KEY_END wimp_KEY_COPY
#endif

#ifndef wimp_WINDOW_GIVE_SHADED_ICON_INFO
	/* RISC OS 5+. Requires OSLib trunk. */
#define wimp_WINDOW_GIVE_SHADED_ICON_INFO ((wimp_extra_window_flags) 0x10u)
#endif

#define SCROLL_VISIBLE_PADDING 32

/** Remembers which iconised sprite numbers are in use */
static bool iconise_used[64];
static int iconise_next = 0;

/** Whether a pressed mouse button has become a drag */
static bool mouse_drag_select;
static bool mouse_drag_adjust;

/** List of all browser windows. */
static struct gui_window	*window_list = 0;
/** GUI window which is being redrawn. Valid only during redraw. */
struct gui_window		*ro_gui_current_redraw_gui;
/** Form control which gui_form_select_menu is for. */
static struct form_control	*gui_form_select_control;
/** The browser window menu handle. */
static wimp_menu		*ro_gui_browser_window_menu = NULL;
/** Menu of options for form select controls. */
static wimp_menu		*gui_form_select_menu = NULL;
/** Object under menu, or 0 if no object. */
static hlcache_handle		*current_menu_object = 0;
/** URL of link under menu, or 0 if no link. */
static const char		*current_menu_url = 0;

static float scale_snap_to[] = {0.10, 0.125, 0.25, 0.333, 0.5, 0.75,
				1.0,
				1.5, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0, 16.0};
#define SCALE_SNAP_TO_SIZE (sizeof scale_snap_to) / (sizeof(float))

/** An entry in ro_gui_pointer_table. */
struct ro_gui_pointer_entry {
	bool wimp_area;  /** The pointer is in the Wimp's sprite area. */
	char sprite_name[16];
	int xactive;
	int yactive;
};

/** Map from gui_pointer_shape to pointer sprite data. Must be ordered as
 * enum gui_pointer_shape. */
struct ro_gui_pointer_entry ro_gui_pointer_table[] = {
	{ true, "ptr_default", 0, 0 },
	{ false, "ptr_point", 6, 0 },
	{ false, "ptr_caret", 4, 9 },
	{ false, "ptr_menu", 6, 4 },
	{ false, "ptr_ud", 6, 7 },
	{ false, "ptr_ud", 6, 7 },
	{ false, "ptr_lr", 7, 6 },
	{ false, "ptr_lr", 7, 6 },
	{ false, "ptr_ld", 7, 7 },
	{ false, "ptr_ld", 7, 7 },
	{ false, "ptr_rd", 7, 7 },
	{ false, "ptr_rd", 6, 7 },
	{ false, "ptr_cross", 7, 7 },
	{ false, "ptr_move", 8, 0 },
	{ false, "ptr_wait", 7, 10 },
	{ false, "ptr_help", 0, 0 },
	{ false, "ptr_nodrop", 0, 0 },
	{ false, "ptr_nt_allwd", 10, 10 },
	{ false, "ptr_progress", 0, 0 },
};

struct update_box {
	int x0;
	int y0;
	int x1;
	int y1;
	bool use_buffer;
	struct gui_window *g;
	struct update_box *next;
};

struct update_box *pending_updates;
#define MARGIN 4

static const struct toolbar_callbacks ro_gui_window_toolbar_callbacks = {
	ro_gui_window_update_theme,
	ro_gui_window_update_toolbar,
	(void (*)(void *)) ro_gui_window_update_toolbar_buttons,
	ro_gui_window_toolbar_click,
	ro_gui_window_toolbar_keypress,
	ro_gui_window_save_toolbar_buttons
};


/**
 * Initialise the browser window module and its menus.
 */

void ro_gui_window_initialise(void)
{
	/* Build the browser window menu. */

	static const struct ns_menu browser_definition = {
		"NetSurf", {
			{ "Page", BROWSER_PAGE, 0 },
			{ "Page.PageInfo",BROWSER_PAGE_INFO, &dialog_pageinfo },
			{ "Page.Save", BROWSER_SAVE, &dialog_saveas },
			{ "Page.SaveComp", BROWSER_SAVE_COMPLETE, &dialog_saveas },
			{ "Page.Export", NO_ACTION, 0 },
#ifdef WITH_DRAW_EXPORT
			{ "Page.Export.Draw", BROWSER_EXPORT_DRAW, &dialog_saveas },
#endif
#ifdef WITH_PDF_EXPORT
			{ "Page.Export.PDF", BROWSER_EXPORT_PDF, &dialog_saveas },
#endif
			{ "Page.Export.Text", BROWSER_EXPORT_TEXT, &dialog_saveas },
			{ "Page.SaveURL", NO_ACTION, 0 },
			{ "Page.SaveURL.URI", BROWSER_SAVE_URL_URI, &dialog_saveas },
			{ "Page.SaveURL.URL", BROWSER_SAVE_URL_URL, &dialog_saveas },
			{ "Page.SaveURL.LinkText", BROWSER_SAVE_URL_TEXT, &dialog_saveas },
			{ "_Page.Print", BROWSER_PRINT, &dialog_print },
			{ "Page.NewWindow", BROWSER_NEW_WINDOW, 0 },
			{ "Page.FindText", BROWSER_FIND_TEXT, &dialog_search },
			{ "Page.ViewSrc", BROWSER_VIEW_SOURCE, 0 },
			{ "Object", BROWSER_OBJECT, 0 },
			{ "Object.Object", BROWSER_OBJECT_OBJECT, 0 },
			{ "Object.Object.ObjInfo", BROWSER_OBJECT_INFO, &dialog_objinfo },
			{ "Object.Object.ObjSave", BROWSER_OBJECT_SAVE, &dialog_saveas },
			{ "Object.Object.Export", BROWSER_OBJECT_EXPORT, 0 },
			{ "Object.Object.Export.Sprite", BROWSER_OBJECT_EXPORT_SPRITE, &dialog_saveas },
#ifdef WITH_DRAW_EXPORT
			{ "Object.Object.Export.ObjDraw", BROWSER_OBJECT_EXPORT_DRAW, &dialog_saveas },
#endif
			{ "Object.Object.SaveURL", NO_ACTION, 0 },
			{ "Object.Object.SaveURL.URI", BROWSER_OBJECT_SAVE_URL_URI, &dialog_saveas },
			{ "Object.Object.SaveURL.URL", BROWSER_OBJECT_SAVE_URL_URL, &dialog_saveas },
			{ "Object.Object.SaveURL.LinkText", BROWSER_OBJECT_SAVE_URL_TEXT, &dialog_saveas },
			{ "Object.Object.ObjPrint", BROWSER_OBJECT_PRINT, 0 },
			{ "Object.Object.ObjReload", BROWSER_OBJECT_RELOAD, 0 },
			{ "Object.Link", BROWSER_OBJECT_LINK, 0 },
			{ "Object.Link.LinkSave", BROWSER_LINK_SAVE, 0 },
			{ "Object.Link.LinkSave.URI", BROWSER_LINK_SAVE_URI, &dialog_saveas },
			{ "Object.Link.LinkSave.URL", BROWSER_LINK_SAVE_URL, &dialog_saveas },
			{ "Object.Link.LinkSave.LinkText", BROWSER_LINK_SAVE_TEXT, &dialog_saveas },
			{ "_Object.Link.LinkDload", BROWSER_LINK_DOWNLOAD, 0 },
			{ "Object.Link.LinkNew", BROWSER_LINK_NEW_WINDOW, 0 },
			{ "Selection", BROWSER_SELECTION, 0 },
			{ "_Selection.SelSave", BROWSER_SELECTION_SAVE, &dialog_saveas },
			{ "Selection.Copy", BROWSER_SELECTION_COPY, 0 },
			{ "Selection.Cut", BROWSER_SELECTION_CUT, 0 },
			{ "_Selection.Paste", BROWSER_SELECTION_PASTE, 0 },
			{ "Selection.Clear", BROWSER_SELECTION_CLEAR, 0 },
			{ "Selection.SelectAll", BROWSER_SELECTION_ALL, 0 },
			{ "Navigate", NO_ACTION, 0 },
			{ "Navigate.Home", BROWSER_NAVIGATE_HOME, 0 },
			{ "Navigate.Back", BROWSER_NAVIGATE_BACK, 0 },
			{ "Navigate.Forward", BROWSER_NAVIGATE_FORWARD, 0 },
			{ "_Navigate.UpLevel", BROWSER_NAVIGATE_UP, 0 },
			{ "Navigate.Reload", BROWSER_NAVIGATE_RELOAD_ALL, 0 },
			{ "Navigate.Stop", BROWSER_NAVIGATE_STOP, 0 },
			{ "View", NO_ACTION, 0 },
			{ "View.ScaleView", BROWSER_SCALE_VIEW, &dialog_zoom },
			{ "View.Images", NO_ACTION, 0 },
			{ "View.Images.ForeImg", BROWSER_IMAGES_FOREGROUND, 0 },
			{ "View.Images.BackImg", BROWSER_IMAGES_BACKGROUND, 0 },
			{ "View.Toolbars", NO_ACTION, 0 },
			{ "View.Toolbars.ToolButtons", TOOLBAR_BUTTONS, 0 },
			{ "View.Toolbars.ToolAddress", TOOLBAR_ADDRESS_BAR, 0 },
			{ "_View.Toolbars.ToolThrob", TOOLBAR_THROBBER, 0 },
			{ "View.Toolbars.EditToolbar", TOOLBAR_EDIT, 0 },
			{ "_View.Render", NO_ACTION, 0 },
			{ "View.Render.RenderAnims", BROWSER_BUFFER_ANIMS, 0 },
			{ "View.Render.RenderAll", BROWSER_BUFFER_ALL, 0 },
			{ "_View.OptDefault", BROWSER_SAVE_VIEW, 0 },
			{ "View.Window", NO_ACTION, 0 },
			{ "View.Window.WindowSave", BROWSER_WINDOW_DEFAULT, 0 },
			{ "View.Window.WindowStagr", BROWSER_WINDOW_STAGGER, 0 },
			{ "_View.Window.WindowSize", BROWSER_WINDOW_COPY, 0 },
			{ "View.Window.WindowReset", BROWSER_WINDOW_RESET, 0 },
			{ "Utilities", NO_ACTION, 0 },
			{ "Utilities.Hotlist", HOTLIST_SHOW, 0 },
			{ "Utilities.Hotlist.HotlistAdd", HOTLIST_ADD_URL, 0 },
			{ "Utilities.Hotlist.HotlistShow", HOTLIST_SHOW, 0 },
			{ "Utilities.History", HISTORY_SHOW_GLOBAL, 0 },
			{ "Utilities.History.HistLocal", HISTORY_SHOW_LOCAL, 0 },
			{ "Utilities.History.HistGlobal", HISTORY_SHOW_GLOBAL, 0 },
			{ "Utilities.Cookies", COOKIES_SHOW, 0 },
			{ "Utilities.Cookies.ShowCookies", COOKIES_SHOW, 0 },
			{ "Utilities.Cookies.DeleteCookies", COOKIES_DELETE, 0 },
			{ "Help", HELP_OPEN_CONTENTS, 0 },
			{ "Help.HelpContent", HELP_OPEN_CONTENTS, 0 },
			{ "Help.HelpGuide", HELP_OPEN_GUIDE, 0 },
			{ "_Help.HelpInfo", HELP_OPEN_INFORMATION, 0 },
			{ "Help.HelpCredits", HELP_OPEN_CREDITS, 0 },
			{ "_Help.HelpLicence", HELP_OPEN_LICENCE, 0 },
			{ "Help.HelpInter", HELP_LAUNCH_INTERACTIVE, 0 },
			{NULL, 0, 0}
		}
	};
	ro_gui_browser_window_menu =
			ro_gui_menu_define_menu(&browser_definition);

}


/*
 * Interface With Core
 */

/**
 * Create and open a new browser window.
 *
 * \param  bw	  browser_window structure to update
 * \param  clone  the browser window to clone options from, or NULL for default
 * \return  gui_window, or 0 on error and error reported
 */

struct gui_window *gui_create_browser_window(struct browser_window *bw,
		struct browser_window *clone, bool new_tab)
{
	int screen_width, screen_height, win_width, win_height, scroll_width;
	static int window_count = 2;
	wimp_window window;
	wimp_window_state state;
	os_error *error;
	bool open_centred = true;
	struct gui_window *g;
	struct browser_window *top;

	g = malloc(sizeof *g);
	if (!g) {
		warn_user("NoMemory", 0);
		return 0;
	}
	g->bw = bw;
	g->toolbar = 0;
	g->status_bar = 0;
	g->old_width = 0;
	g->old_height = 0;
	g->update_extent = true;
	strcpy(g->title, "NetSurf");
	g->iconise_icon = -1;

	/* Set the window position */
	if (clone && clone->window && option_window_size_clone) {
		for (top = clone; top->parent; top = top->parent);
		state.w = top->window->window;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
		window.visible.x0 = state.visible.x0;
		window.visible.x1 = state.visible.x1;
		window.visible.y0 = state.visible.y0 - 48;
		window.visible.y1 = state.visible.y1 - 48;
		open_centred = false;
	} else {
		ro_gui_screen_size(&screen_width, &screen_height);

		/* Check if we have a preferred position */
		if ((option_window_screen_width != 0) &&
				(option_window_screen_height != 0)) {
			win_width = (option_window_width * screen_width) /
					option_window_screen_width;
			win_height = (option_window_height * screen_height) /
					option_window_screen_height;
			window.visible.x0 = (option_window_x * screen_width) /
					option_window_screen_width;
			window.visible.y0 = (option_window_y * screen_height) /
					option_window_screen_height;
			if (option_window_stagger) {
				window.visible.y0 += 96 -
						(48 * (window_count % 5));
			}
			open_centred = false;
			if (win_width < 100)
				win_width = 100;
			if (win_height < 100)
				win_height = 100;
		} else {

		       /* Base how we define the window height/width
			  on the compile time options set */
			win_width = screen_width * 3 / 4;
			if (1600 < win_width)
				win_width = 1600;
			win_height = win_width * 3 / 4;

			window.visible.x0 = (screen_width - win_width) / 2;
			window.visible.y0 = ((screen_height - win_height) / 2) +
					96 - (48 * (window_count % 5));
		}
		window.visible.x1 = window.visible.x0 + win_width;
		window.visible.y1 = window.visible.y0 + win_height;
	}

	/* General flags for a non-movable, non-resizable, no-title bar window */
	window.xscroll = 0;
	window.yscroll = 0;
	window.next = wimp_TOP;
	window.flags =	wimp_WINDOW_MOVEABLE |
			wimp_WINDOW_NEW_FORMAT |
			wimp_WINDOW_VSCROLL |
			wimp_WINDOW_HSCROLL |
			wimp_WINDOW_IGNORE_XEXTENT |
			wimp_WINDOW_IGNORE_YEXTENT |
			wimp_WINDOW_SCROLL_REPEAT;
	window.title_fg = wimp_COLOUR_BLACK;
	window.title_bg = wimp_COLOUR_LIGHT_GREY;
	window.work_fg = wimp_COLOUR_LIGHT_GREY;
	window.work_bg = wimp_COLOUR_TRANSPARENT;
	window.scroll_outer = wimp_COLOUR_DARK_GREY;
	window.scroll_inner = wimp_COLOUR_MID_LIGHT_GREY;
	window.highlight_bg = wimp_COLOUR_CREAM;
	window.extra_flags = wimp_WINDOW_USE_EXTENDED_SCROLL_REQUEST |
			wimp_WINDOW_GIVE_SHADED_ICON_INFO;
	window.extent.x0 = 0;
	window.extent.y0 = -(window.visible.y1 - window.visible.y0);
	window.extent.x1 = window.visible.x1 - window.visible.x0;
	window.extent.y1 = 0;
	window.title_flags = wimp_ICON_TEXT |
			wimp_ICON_INDIRECTED |
			wimp_ICON_HCENTRED;
	window.work_flags = wimp_BUTTON_CLICK_DRAG <<
			wimp_ICON_BUTTON_TYPE_SHIFT;
	window.sprite_area = wimpspriteop_AREA;
	window.xmin = 1;
	window.ymin = 1;
	window.title_data.indirected_text.text = g->title;
	window.title_data.indirected_text.validation = (char *) -1;
	window.title_data.indirected_text.size = 255;
	window.icon_count = 0;

	/* Add in flags */
	window.flags |=	wimp_WINDOW_SIZE_ICON |
			wimp_WINDOW_BACK_ICON |
			wimp_WINDOW_CLOSE_ICON |
			wimp_WINDOW_TITLE_ICON |
			wimp_WINDOW_TOGGLE_ICON;

	if (open_centred) {
		scroll_width = ro_get_vscroll_width(NULL);
		window.visible.x0 -= scroll_width;
	}

	error = xwimp_create_window(&window, &g->window);
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		free(g);
		return 0;
	}

	/* Link into window list */
	g->prev = 0;
	g->next = window_list;
	if (window_list)
		window_list->prev = g;
	window_list = g;
	window_count++;

	/* Add in a toolbar and status bar */
	g->status_bar = ro_gui_status_bar_create(g->window,
			option_toolbar_status_width);
	g->toolbar = ro_toolbar_create(NULL, g->window,
			THEME_STYLE_BROWSER_TOOLBAR, TOOLBAR_FLAGS_NONE,
			&ro_gui_window_toolbar_callbacks, g,
			"HelpToolbar");
	if (g->toolbar != NULL) {
		ro_toolbar_add_buttons(g->toolbar,
				brower_toolbar_buttons,
				option_toolbar_browser);
		ro_toolbar_add_url(g->toolbar);
		ro_toolbar_add_throbber(g->toolbar);
		ro_toolbar_rebuild(g->toolbar);
	}

	/* Register event handlers.  Do this quickly, as some of the things
	 * that follow will indirectly look up our user data: this MUST
	 * be set first!
	 */
	ro_gui_wimp_event_set_user_data(g->window, g);
	ro_gui_wimp_event_register_open_window(g->window, ro_gui_window_open);
	ro_gui_wimp_event_register_close_window(g->window, ro_gui_window_close);
	ro_gui_wimp_event_register_redraw_window(g->window, ro_gui_window_redraw);
	ro_gui_wimp_event_register_keypress(g->window, ro_gui_window_keypress);
	ro_gui_wimp_event_register_mouse_click(g->window, ro_gui_window_click);
	ro_gui_wimp_event_register_menu(g->window, ro_gui_browser_window_menu,
			true, false);
	ro_gui_wimp_event_register_menu_prepare(g->window,
			ro_gui_window_menu_prepare);
	ro_gui_wimp_event_register_menu_selection(g->window,
			ro_gui_window_menu_select);
	ro_gui_wimp_event_register_menu_warning(g->window,
			ro_gui_window_menu_warning);
	ro_gui_wimp_event_register_menu_close(g->window,
			ro_gui_window_menu_close);

	/* Set the window options */
	bw->window = g;
	ro_gui_window_clone_options(bw, clone);
	ro_gui_window_update_toolbar_buttons(g);

	/* Open the window at the top of the stack */
	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return g;
	}

	state.next = wimp_TOP;

	ro_gui_window_open(PTR_WIMP_OPEN(&state));

	/* Claim the caret */
	if (ro_toolbar_take_caret(g->toolbar))
		ro_gui_url_complete_start(g->toolbar);
	else
		gui_window_place_caret(g, -100, -100, 0);

	return g;
}


/**
 * Close a browser window and free any related resources.
 *
 * \param  g  gui_window to destroy
 */

void gui_window_destroy(struct gui_window *g)
{
	os_error *error;
	wimp_w w;

	assert(g);

	/* stop any tracking */
	if (gui_track_gui_window == g) {
		gui_track_gui_window = NULL;
		gui_current_drag_type = GUI_DRAG_NONE;
	}

	/* remove from list */
	if (g->prev)
		g->prev->next = g->next;
	else
		window_list = g->next;
	if (g->next)
		g->next->prev = g->prev;

	/* destroy toolbar */
	if (g->toolbar)
		ro_toolbar_destroy(g->toolbar);
	if (g->status_bar)
		ro_gui_status_bar_destroy(g->status_bar);

	w = g->window;
	ro_gui_url_complete_close();
	ro_gui_dialog_close_persistent(w);
	if (current_menu_window == w)
		ro_gui_menu_closed();
	ro_gui_window_remove_update_boxes(g);

	/* delete window */
	error = xwimp_delete_window(w);
	if (error) {
		LOG(("xwimp_delete_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	ro_gui_wimp_event_finalise(w);

	free(g);
}


/**
 * Set the title of a browser window.
 *
 * \param  g	  gui_window to update
 * \param  title  new window title, copied
 */

void gui_window_set_title(struct gui_window *g, const char *title)
{
	int scale_disp;

	assert(g);
	assert(title);

	if (g->bw->scale != 1.0) {
		scale_disp = g->bw->scale * 100;
		if (ABS((float)scale_disp - g->bw->scale * 100) >= 0.05)
			snprintf(g->title, sizeof g->title, "%s (%.1f%%)",
					title, g->bw->scale * 100);
		else
			snprintf(g->title, sizeof g->title, "%s (%i%%)",
					title, scale_disp);
	} else {
		strncpy(g->title, title, sizeof g->title);
	}

	ro_gui_set_window_title(g->window, g->title);
}


/**
 * Force a redraw of the entire contents of a browser window.
 *
 * \param  g   gui_window to redraw
 */
void gui_window_redraw_window(struct gui_window *g)
{
	wimp_window_info info;
	os_error *error;

	assert(g);
	info.w = g->window;
	error = xwimp_get_window_info_header_only(&info);
	if (error) {
		LOG(("xwimp_get_window_info_header_only: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	error = xwimp_force_redraw(g->window, info.extent.x0, info.extent.y0,
			info.extent.x1, info.extent.y1);
	if (error) {
		LOG(("xwimp_force_redraw: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Redraw an area of a window.
 *
 * \param  g   gui_window
 * \param  data  content_msg_data union with filled in redraw data
 */

void gui_window_update_box(struct gui_window *g, const struct rect *rect)
{
	bool use_buffer;
	int x0, y0, x1, y1;
	struct update_box *cur;

	x0 = floorf(rect->x0 * 2 * g->bw->scale);
	y0 = -ceilf(rect->y1 * 2 * g->bw->scale);
	x1 = ceilf(rect->x1 * 2 * g->bw->scale) + 1;
	y1 = -floorf(rect->y0 * 2 * g->bw->scale) + 1;
	use_buffer =
		(g->option.buffer_everything || g->option.buffer_animations);

	/* try to optimise buffered redraws */
	if (use_buffer) {
		for (cur = pending_updates; cur != NULL; cur = cur->next) {
			if ((cur->g != g) || (!cur->use_buffer))
				continue;
			if ((((cur->x0 - x1) < MARGIN) || ((cur->x1 - x0) < MARGIN)) &&
					(((cur->y0 - y1) < MARGIN) || ((cur->y1 - y0) < MARGIN))) {
				cur->x0 = min(cur->x0, x0);
				cur->y0 = min(cur->y0, y0);
				cur->x1 = max(cur->x1, x1);
				cur->y1 = max(cur->y1, y1);
				return;
			}

		}
	}
	cur = malloc(sizeof(struct update_box));
	if (!cur) {
		LOG(("No memory for malloc."));
		warn_user("NoMemory", 0);
		return;
	}
	cur->x0 = x0;
	cur->y0 = y0;
	cur->x1 = x1;
	cur->y1 = y1;
	cur->next = pending_updates;
	pending_updates = cur;
	cur->g = g;
	cur->use_buffer = use_buffer;
}


/**
 * Get the scroll position of a browser window.
 *
 * \param  g   gui_window
 * \param  sx  receives x ordinate of point at top-left of window
 * \param  sy  receives y ordinate of point at top-left of window
 * \return true iff successful
 */

bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
	wimp_window_state state;
	os_error *error;
	int toolbar_height = 0;

	assert(g);

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	if (g->toolbar)
		toolbar_height = ro_toolbar_full_height(g->toolbar);
	*sx = state.xscroll / (2 * g->bw->scale);
	*sy = -(state.yscroll - toolbar_height) / (2 * g->bw->scale);
	return true;
}


/**
 * Set the scroll position of a browser window.
 *
 * \param  g   gui_window to scroll
 * \param  sx  point to place at top-left of window
 * \param  sy  point to place at top-left of window
 */

void gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
	wimp_window_state state;
	os_error *error;

	assert(g);

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	state.xscroll = sx * 2 * g->bw->scale;
	state.yscroll = -sy * 2 * g->bw->scale;
	if (g->toolbar)
		state.yscroll += ro_toolbar_full_height(g->toolbar);
	ro_gui_window_open(PTR_WIMP_OPEN(&state));
}


/**
 * Scrolls the specified area of a browser window into view.
 *
 * \param  g   gui_window to scroll
 * \param  x0  left point to ensure visible
 * \param  y0  bottom point to ensure visible
 * \param  x1  right point to ensure visible
 * \param  y1  top point to ensure visible
 */
void gui_window_scroll_visible(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	wimp_window_state state;
	os_error *error;
	int cx0, cy0, width, height;
	int padding_available;
	int toolbar_height = 0;
	int correction;

	assert(g);

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	if (g->toolbar)
		toolbar_height = ro_toolbar_full_height(g->toolbar);

	x0 = x0 * 2 * g->bw->scale;
	y0 = y0 * 2 * g->bw->scale;
	x1 = x1 * 2 * g->bw->scale;
	y1 = y1 * 2 * g->bw->scale;

	cx0 = state.xscroll;
	cy0 = -state.yscroll + toolbar_height;
	width = state.visible.x1 - state.visible.x0;
	height = state.visible.y1 - state.visible.y0 - toolbar_height;

	/* make sure we're visible */
	correction = (x1 - cx0 - width);
	if (correction > 0)
		cx0 += correction;
	correction = (y1 - cy0 - height);
	if (correction > 0)
		cy0 += correction;
	if (x0 < cx0)
		cx0 = x0;
	if (y0 < cy0)
		cy0 = y0;

	/* try to give a SCROLL_VISIBLE_PADDING border of space around us */
	padding_available = (width - x1 + x0) / 2;
	if (padding_available > 0) {
		if (padding_available > SCROLL_VISIBLE_PADDING)
			padding_available = SCROLL_VISIBLE_PADDING;
		correction = (cx0 + width - x1);
		if (correction < padding_available)
			cx0 += padding_available;
		correction = (x0 - cx0);
		if (correction < padding_available)
			cx0 -= padding_available;
	}
	padding_available = (height - y1 + y0) / 2;
	if (padding_available > 0) {
		if (padding_available > SCROLL_VISIBLE_PADDING)
			padding_available = SCROLL_VISIBLE_PADDING;
		correction = (cy0 + height - y1);
		if (correction < padding_available)
			cy0 += padding_available;
		correction = (y0 - cy0);
		if (correction < padding_available)
			cy0 -= padding_available;
	}

	state.xscroll = cx0;
	state.yscroll = -cy0 + toolbar_height;
	ro_gui_window_open(PTR_WIMP_OPEN(&state));
}


/**
 * Find the current dimensions of a browser window's content area.
 *
 * \param g	 gui_window to measure
 * \param width	 receives width of window
 * \param height receives height of window
 * \param scaled whether to return scaled values
 */

void gui_window_get_dimensions(struct gui_window *g, int *width, int *height, bool scaled)
{
  	/* use the cached window sizes */
	*width = g->old_width / 2;
	*height = g->old_height / 2;
	if (scaled) {
		*width /= g->bw->scale;
		*height /= g->bw->scale;
	}
}


/**
 * Update the extent of the inside of a browser window to that of the
 * current content.
 *
 * \param  g		gui_window to update the extent of
 */

void gui_window_update_extent(struct gui_window *g)
{
	os_error		*error;
	wimp_window_info	info;
	wimp_window_state	state;
	bool			update;
	unsigned int		flags;
	int			scroll = 0;

	assert(g);

	info.w = g->window;
	error = xwimp_get_window_info_header_only(&info);
	if (error) {
		LOG(("xwimp_get_window_info_header_only: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* scroll on toolbar height change */
	if (g->toolbar) {
		scroll = ro_toolbar_height(g->toolbar) - info.extent.y1;
		info.yscroll += scroll;
	}

	/* only allow a further reformat if we've gained/lost scrollbars */
	flags = info.flags & (wimp_WINDOW_HSCROLL | wimp_WINDOW_VSCROLL);
	update = g->bw->reformat_pending;
	g->update_extent = true;
	ro_gui_window_open(PTR_WIMP_OPEN(&info));

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	if (flags == (state.flags & (wimp_WINDOW_HSCROLL | wimp_WINDOW_VSCROLL)))
		g->bw->reformat_pending = update;
}


/**
 * Set the status bar of a browser window.
 *
 * \param  g	 gui_window to update
 * \param  text  new status text
 */

void gui_window_set_status(struct gui_window *g, const char *text)
{
	if (g->status_bar)
		ro_gui_status_bar_set_text(g->status_bar, text);
}


/**
 * Change mouse pointer shape
 */

void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
	static gui_pointer_shape curr_pointer = GUI_POINTER_DEFAULT;
	struct ro_gui_pointer_entry *entry;
	os_error *error;

	if (shape == curr_pointer)
		return;

	assert(shape < sizeof ro_gui_pointer_table /
			sizeof ro_gui_pointer_table[0]);

	entry = &ro_gui_pointer_table[shape];

	if (entry->wimp_area) {
		/* pointer in the Wimp's sprite area */
		error = xwimpspriteop_set_pointer_shape(entry->sprite_name,
				1, entry->xactive, entry->yactive, 0, 0);
		if (error) {
			LOG(("xwimpspriteop_set_pointer_shape: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	} else {
		/* pointer in our own sprite area */
		error = xosspriteop_set_pointer_shape(osspriteop_USER_AREA,
				gui_sprites,
				(osspriteop_id) entry->sprite_name,
				1, entry->xactive, entry->yactive, 0, 0);
		if (error) {
			LOG(("xosspriteop_set_pointer_shape: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}

	curr_pointer = shape;
}


/**
 * Remove the mouse pointer from the screen
 */

void gui_window_hide_pointer(struct gui_window *g)
{
	os_error *error;

	error = xwimpspriteop_set_pointer_shape(NULL, 0x30, 0, 0, 0, 0);
	if (error) {
		LOG(("xwimpspriteop_set_pointer_shape: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Set the contents of a window's address bar.
 *
 * \param  g	gui_window to update
 * \param  url  new url for address bar
 */

void gui_window_set_url(struct gui_window *g, const char *url)
{
	if (!g->toolbar)
		return;

	ro_toolbar_set_url(g->toolbar, url, true, false);
	ro_gui_url_complete_start(g->toolbar);
}


/**
 * Update the interface to reflect start of page loading.
 *
 * \param  g  window with start of load
 */

void gui_window_start_throbber(struct gui_window *g)
{
	ro_gui_window_update_toolbar_buttons(g);
	ro_gui_menu_refresh(ro_gui_browser_window_menu);
	if (g->toolbar != NULL)
		ro_toolbar_start_throbbing(g->toolbar);
}



/**
 * Update the interface to reflect page loading stopped.
 *
 * \param  g  window with start of load
 */

void gui_window_stop_throbber(struct gui_window *g)
{
	ro_gui_window_update_toolbar_buttons(g);
	ro_gui_menu_refresh(ro_gui_browser_window_menu);
	if (g->toolbar != NULL)
		ro_toolbar_stop_throbbing(g->toolbar);
}

/**
 * set favicon
 */

void gui_window_set_icon(struct gui_window *g, hlcache_handle *icon)
{
	if (g == NULL || g->toolbar == NULL)
		return;

	ro_toolbar_set_site_favicon(g->toolbar, icon);
}

/**
* set gui display of a retrieved favicon representing the search provider
* \param ico may be NULL for local calls; then access current cache from
* search_web_ico()
*/
void gui_window_set_search_ico(hlcache_handle *ico)
{
}

/**
 * Place the caret in a browser window.
 *
 * \param  g	   window with caret
 * \param  x	   coordinates of caret
 * \param  y	   coordinates of caret
 * \param  height  height of caret
 */

void gui_window_place_caret(struct gui_window *g, int x, int y, int height)
{
	os_error *error;

	error = xwimp_set_caret_position(g->window, -1,
			x * 2, -(y + height) * 2, height * 2, -1);
	if (error) {
		LOG(("xwimp_set_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Remove the caret, if present.
 *
 * \param  g	   window with caret
 */

void gui_window_remove_caret(struct gui_window *g)
{
	wimp_caret caret;
	os_error *error;

	error = xwimp_get_caret_position(&caret);
	if (error) {
		LOG(("xwimp_get_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	if (caret.w != g->window)
		/* we don't have the caret: do nothing */
		return;

	/* hide caret, but keep input focus */
	gui_window_place_caret(g, -100, -100, 0);
}


/**
 * Called when the gui_window has new content.
 *
 * \param  g  the gui_window that has new content
 */

void gui_window_new_content(struct gui_window *g)
{
	ro_gui_menu_refresh(ro_gui_browser_window_menu);
	ro_gui_window_update_toolbar_buttons(g);
	ro_gui_dialog_close_persistent(g->window);
	ro_toolbar_set_content_favicon(g->toolbar, g->bw->current_content);
}


/**
 * Starts drag scrolling of a browser window
 *
 * \param gw  gui window
 */

bool gui_window_scroll_start(struct gui_window *g)
{
	wimp_window_info_base info;
	wimp_pointer pointer;
	os_error *error;
	wimp_drag drag;
	int height;
	int width;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	info.w = g->window;
	error = xwimp_get_window_info_header_only((wimp_window_info*)&info);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	width  = info.extent.x1 - info.extent.x0;
	height = info.extent.y1 - info.extent.y0;

	drag.type = wimp_DRAG_USER_POINT;
	drag.bbox.x1 = pointer.pos.x + info.xscroll;
	drag.bbox.y0 = pointer.pos.y + info.yscroll;
	drag.bbox.x0 = drag.bbox.x1 - (width  - (info.visible.x1 - info.visible.x0));
	drag.bbox.y1 = drag.bbox.y0 + (height - (info.visible.y1 - info.visible.y0));

	if (g->toolbar) {
		int tbar_height = ro_toolbar_full_height(g->toolbar);
		drag.bbox.y0 -= tbar_height;
		drag.bbox.y1 -= tbar_height;
	}

	error = xwimp_drag_box(&drag);
	if (error) {
		LOG(("xwimp_drag_box: 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	gui_track_gui_window = g;
	gui_current_drag_type = GUI_DRAG_SCROLL;
	return true;
}


/**
 * Platform-dependent part of starting a box scrolling operation,
 * for frames and textareas.
 *
 * \param  x0  minimum x ordinate of box relative to mouse pointer
 * \param  y0  minimum y ordinate
 * \param  x1  maximum x ordinate
 * \param  y1  maximum y ordinate
 * \return true iff succesful
 */

bool gui_window_box_scroll_start(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	wimp_pointer pointer;
	os_error *error;
	wimp_drag drag;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	drag.type = wimp_DRAG_USER_POINT;
	drag.bbox.x0 = pointer.pos.x + (int)(x0 * 2 * g->bw->scale);
	drag.bbox.y0 = pointer.pos.y + (int)(y0 * 2 * g->bw->scale);
	drag.bbox.x1 = pointer.pos.x + (int)(x1 * 2 * g->bw->scale);
	drag.bbox.y1 = pointer.pos.y + (int)(y1 * 2 * g->bw->scale);

	error = xwimp_drag_box(&drag);
	if (error) {
		LOG(("xwimp_drag_box: 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	gui_current_drag_type = GUI_DRAG_SCROLL;
	return true;
}


/**
 * Save the specified content as a link.
 *
 * \param  g  gui_window containing the content
 * \param  c  the content to save
 */
void gui_window_save_link(struct gui_window *g, const char *url,
		const char *title)
{
	ro_gui_save_prepare(GUI_SAVE_LINK_URL, NULL, NULL, url, title);
	ro_gui_dialog_open_persistent(g->window, dialog_saveas, true);
}


/**
 * Updates a windows extent.
 *
 * \param  g  the gui_window to update
 * \param  width  the minimum width, or -1 to use window width
 * \param  height  the minimum height, or -1 to use window height
 */

void gui_window_set_extent(struct gui_window *g, int width, int height)
{
  	int screen_width;
	int toolbar_height = 0;
	hlcache_handle *h;
	wimp_window_state state;
	os_error *error;

	h = g->bw->current_content;
	if (g->toolbar)
		toolbar_height = ro_toolbar_full_height(g->toolbar);

	/* get the current state */
	if ((height == -1) || (width == -1)) {
		state.w = g->window;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
		if (width == -1)
			width = state.visible.x1 - state.visible.x0;
		if (height == -1) {
			height = state.visible.y1 - state.visible.y0;
			height -= toolbar_height;
		}
	}

	/* the top-level framed window is a total pain. to get it to maximise
	 * to the top of the screen we need to fake it having a suitably large
	 * extent */
	if (g->bw->children) {
		ro_gui_screen_size(&screen_width, &height);
		if (g->toolbar)
			height -= ro_toolbar_full_height(g->toolbar);
		height -= ro_get_hscroll_height(g->window);
		height -= ro_get_title_height(g->window);
	}
	if (h) {
		width = max(width, content_get_width(h) * 2 * g->bw->scale);
		height = max(height, content_get_height(h) * 2 * g->bw->scale);
	}
	os_box extent = { 0, -height, width, toolbar_height };
	error = xwimp_set_extent(g->window, &extent);
	if (error) {
		LOG(("xwimp_set_extent: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
}


/**
 * Display a menu of options for a form select control.
 *
 * \param  bw	    browser window containing form control
 * \param  control  form control of type GADGET_SELECT
 */

void gui_create_form_select_menu(struct browser_window *bw,
		struct form_control *control)
{
	os_error	*error;
	wimp_pointer	pointer;

	/* The first time the menu is opened, control bypasses the normal
	 * Menu Prepare event and so we prepare here.  On any re-opens,
	 * ro_gui_window_prepare_form_select_menu() is called from the
	 * normal wimp event.
	 */

	if (!ro_gui_window_prepare_form_select_menu(bw, control))
		return;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		ro_gui_menu_closed();
		return;
	}

	gui_form_select_control = control;
	ro_gui_menu_create(gui_form_select_menu,
			pointer.pos.x, pointer.pos.y, bw->window->window);
}


/*
 * RISC OS Wimp Event Handlers
 */


/**
 * Handle a Redraw_Window_Request for a browser window.
 */

void ro_gui_window_redraw(wimp_draw *redraw)
{
	osbool more;
	struct gui_window *g = (struct gui_window *)ro_gui_wimp_event_get_user_data(redraw->w);
	os_error *error;
	struct redraw_context ctx = {
		.interactive = true,
		.plot = &ro_plotters
	};

	/* We can't render locked contents.  If the browser window is not
	 * ready for redraw, do nothing.  Else, in the case of buffered
	 * rendering we'll show random data. */
	if (!browser_window_redraw_ready(g->bw))
		return;

	ro_gui_current_redraw_gui = g;

	error = xwimp_redraw_window(redraw, &more);
	if (error) {
		LOG(("xwimp_redraw_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	while (more) {
		struct rect clip;

		/* OS's redraw request coordinates are in screen coordinates,
		 * with an origin at the bottom left of the screen.
		 * Find the coordinate of the top left of the document in terms
		 * of OS screen coordinates.
		 * NOTE: OS units are 2 per px. */
		ro_plot_origin_x = redraw->box.x0 - redraw->xscroll;
		ro_plot_origin_y = redraw->box.y1 - redraw->yscroll;

		/* Convert OS redraw rectangle request coordinates into NetSurf
		 * coordinates. NetSurf coordinates have origin at top left of
		 * document and units are in px. */
		clip.x0 = (redraw->clip.x0 - ro_plot_origin_x) / 2; /* left   */
		clip.y0 = (ro_plot_origin_y - redraw->clip.y1) / 2; /* top    */
		clip.x1 = (redraw->clip.x1 - ro_plot_origin_x) / 2; /* right  */
		clip.y1 = (ro_plot_origin_y - redraw->clip.y0) / 2; /* bottom */

		if (ro_gui_current_redraw_gui->option.buffer_everything)
			ro_gui_buffer_open(redraw);

		browser_window_redraw(g->bw, 0, 0, &clip, &ctx);

		if (ro_gui_current_redraw_gui->option.buffer_everything)
			ro_gui_buffer_close();

		/* Check to see if there are more rectangles to draw and
		 * get next one */
		error = xwimp_get_rectangle(redraw, &more);
		/* RISC OS 3.7 returns an error here if enough buffer was
		   claimed to cause a new dynamic area to be created. It
		   doesn't actually stop anything working, so we mask it out
		   for now until a better fix is found. This appears to be a
		   bug in RISC OS. */
		if (error && !(ro_gui_current_redraw_gui->
				option.buffer_everything &&
				error->errnum == error_WIMP_GET_RECT)) {
			LOG(("xwimp_get_rectangle: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			ro_gui_current_redraw_gui = NULL;
			return;
		}
	}
	ro_gui_current_redraw_gui = NULL;
}


/**
 * Open a window using the given wimp_open, handling toolbars and resizing.
 */

void ro_gui_window_open(wimp_open *open)
{
	struct gui_window *g = (struct gui_window *)ro_gui_wimp_event_get_user_data(open->w);
	int width = open->visible.x1 - open->visible.x0;
	int height = open->visible.y1 - open->visible.y0;
	int size, fheight, fwidth, toolbar_height = 0;
	bool no_vscroll, no_hscroll;
	float new_scale = 0;
	hlcache_handle *h;
	wimp_window_state state;
	os_error *error;
	wimp_w parent;
	bits linkage;

	if (open->next == wimp_TOP && g->iconise_icon >= 0) {
		/* window is no longer iconised, release its sprite number */
		iconise_used[g->iconise_icon] = false;
		g->iconise_icon = -1;
	}

	h = g->bw->current_content;

	/* get the current flags/nesting state */
	state.w = g->window;
	error = xwimp_get_window_state_and_nesting(&state, &parent, &linkage);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* account for toolbar height, if present */
	if (g->toolbar)
		toolbar_height = ro_toolbar_full_height(g->toolbar);
	height -= toolbar_height;

	/* work with the state from now on so we can modify flags */
	state.visible = open->visible;
	state.xscroll = open->xscroll;
	state.yscroll = open->yscroll;
	state.next = open->next;

	/* handle 'auto' scroll bars' and non-fitting scrollbar removal */
	if ((g->bw->scrolling == SCROLLING_AUTO) ||
			(g->bw->scrolling == SCROLLING_YES)) {
		/* windows lose scrollbars when containing a frameset */
		no_hscroll = false;
		no_vscroll = g->bw->children;

		/* hscroll */
		size = ro_get_hscroll_height(NULL);
		if (g->bw->border)
			size -= 2;
		fheight = height;
		if (state.flags & wimp_WINDOW_HSCROLL)
			fheight += size;
		if (!no_hscroll) {
			if (!(state.flags & wimp_WINDOW_HSCROLL)) {
				height -= size;
				state.visible.y0 += size;
				if (h) {
					g->bw->reformat_pending = true;
					browser_reformat_pending = true;
				}
			}
			state.flags |= wimp_WINDOW_HSCROLL;
		} else {
			if (state.flags & wimp_WINDOW_HSCROLL) {
				height += size;
				state.visible.y0 -= size;
				if (h) {
					g->bw->reformat_pending = true;
					browser_reformat_pending = true;
				}
			}
			state.flags &= ~wimp_WINDOW_HSCROLL;
		}

		/* vscroll */
		size = ro_get_vscroll_width(NULL);
		if (g->bw->border)
			size -= 2;
		fwidth = width;
		if (state.flags & wimp_WINDOW_VSCROLL)
			fwidth += size;
		if (!no_vscroll) {
			if (!(state.flags & wimp_WINDOW_VSCROLL)) {
				width -= size;
				state.visible.x1 -= size;
				if (h) {
					g->bw->reformat_pending = true;
					browser_reformat_pending = true;
				}
			}
			state.flags |= wimp_WINDOW_VSCROLL;
		} else {
			if (state.flags & wimp_WINDOW_VSCROLL) {
				width += size;
				state.visible.x1 += size;
				if (h) {
					g->bw->reformat_pending = true;
					browser_reformat_pending = true;
				}
			}
			state.flags &= ~wimp_WINDOW_VSCROLL;
		}
	}

	/* reformat or change extent if necessary */
	if ((h) && (g->old_width != width || g->old_height != height)) {
	  	/* Ctrl-resize of a top-level window scales the content size */
		if ((g->old_width > 0) && (g->old_width != width) &&
				(ro_gui_ctrl_pressed()))
			new_scale = (g->bw->scale * width) / g->old_width;
		g->bw->reformat_pending = true;
		browser_reformat_pending = true;
	}
	if (g->update_extent || g->old_width != width ||
			g->old_height != height) {
		g->old_width = width;
		g->old_height = height;
		g->update_extent = false;
		gui_window_set_extent(g, width, height);
	}

	/* first resize stops any flickering by making the URL window on top */
	ro_gui_url_complete_resize(g->toolbar, PTR_WIMP_OPEN(&state));

	error = xwimp_open_window_nested_with_flags(&state, parent, linkage);
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* update the toolbar */
	if (g->status_bar)
		ro_gui_status_bar_resize(g->status_bar);
	if (g->toolbar) {
		ro_toolbar_process(g->toolbar, -1, false);
		/* second resize updates to the new URL bar width */
		ro_gui_url_complete_resize(g->toolbar, open);
	}

	/* set the new scale from a ctrl-resize. this must be done at the end as
	 * it may cause a frameset recalculation based on the new window size.
	 */
	if (new_scale > 0)
		browser_window_set_scale(g->bw, new_scale, true);
}


/**
 * Handle wimp closing event
 */

void ro_gui_window_close(wimp_w w)
{
	struct gui_window *g = (struct gui_window *)ro_gui_wimp_event_get_user_data(w);
	wimp_pointer pointer;
	os_error *error;
	char *temp_name, *r;
	char *filename;
	hlcache_handle *h = NULL;
	bool destroy;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	if (g->bw)
		h = g->bw->current_content;
	if (pointer.buttons & wimp_CLICK_ADJUST) {
		destroy = !ro_gui_shift_pressed();
		filename = (h && content_get_url(h)) ?
				url_to_path(nsurl_access(content_get_url(h))) :
				NULL;
		if (filename) {
			temp_name = malloc(strlen(filename) + 32);
			if (temp_name) {
				sprintf(temp_name, "Filer_OpenDir %s",
						filename);
				r = temp_name + strlen(temp_name);
				while (r > temp_name) {
					if (*r == '.') {
						*r = '\0';
						break;
					}
					r--;
				}
				error = xos_cli(temp_name);
				if (error) {
					LOG(("xos_cli: 0x%x: %s",
							error->errnum,
							error->errmess));
					warn_user("MiscError", error->errmess);
					return;
				}
				free(temp_name);
			}
			free(filename);
		} else {
			/* this is pointless if we are about to close the
			 * window */
			if (!destroy && g->bw != NULL &&
					g->bw->current_content != NULL)
				ro_gui_window_navigate_up(g->bw->window,
						nsurl_access(content_get_url(
						g->bw->current_content)));
		}
	}
	else
		destroy = true;

	if (destroy)
		browser_window_destroy(g->bw);
}


/**
 * Handle Mouse_Click events in a browser window.  This should never see
 * Menu clicks, as these will be routed to the menu handlers.
 *
 * \param *pointer		details of mouse click
 * \return			true if click handled, false otherwise
 */

bool ro_gui_window_click(wimp_pointer *pointer)
{
	struct gui_window *g;
	os_coord pos;

	/* We should never see Menu clicks. */

	if (pointer->buttons == wimp_CLICK_MENU)
		return false;

	g = (struct gui_window *) ro_gui_wimp_event_get_user_data(pointer->w);

	/* try to close url-completion */
	ro_gui_url_complete_close();

	/* set input focus */
	if (pointer->buttons == wimp_CLICK_SELECT ||
			pointer->buttons == wimp_CLICK_ADJUST)
		gui_window_place_caret(g, -100, -100, 0);

	if (ro_gui_window_to_window_pos(g, pointer->pos.x, pointer->pos.y, &pos))
		browser_window_mouse_click(g->bw,
				ro_gui_mouse_click_state(pointer->buttons,
				wimp_BUTTON_CLICK_DRAG),
				pos.x, pos.y);

	return true;
}


/**
 * Process Key_Pressed events in a browser window.
 *
 * \param *key			The wimp keypress block for the event.
 * \return			true if the event was handled, else false.
 */

bool ro_gui_window_keypress(wimp_key *key)
{
	struct gui_window	*g;
	hlcache_handle		*h;
	os_error		*error;
	wimp_pointer		pointer;
	uint32_t		c = (uint32_t) key->c;

	g = (struct gui_window *) ro_gui_wimp_event_get_user_data(key->w);
	if (g == NULL)
		return false;

	h = g->bw->current_content;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s\n",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	/* First send the key to the browser window, eg. form fields. */

	if ((unsigned)c < 0x20 || (0x7f <= c && c <= 0x9f) ||
			(c & IS_WIMP_KEY)) {
	/* Munge control keys into unused control chars */
	/* We can't map onto 1->26 (reserved for ctrl+<qwerty>
	   That leaves 27->31 and 128->159 */
		switch (c & ~IS_WIMP_KEY) {
		case wimp_KEY_TAB: c = 9; break;
		case wimp_KEY_SHIFT | wimp_KEY_TAB: c = 11; break;

		/* cursor movement keys */
		case wimp_KEY_HOME:
		case wimp_KEY_CONTROL | wimp_KEY_LEFT:
			c = KEY_LINE_START;
			break;
		case wimp_KEY_END:
			if (os_version >= RISCOS5)
				c = KEY_LINE_END;
			else
				c = KEY_DELETE_RIGHT;
			break;
		case wimp_KEY_CONTROL | wimp_KEY_RIGHT: c = KEY_LINE_END;   break;
		case wimp_KEY_CONTROL | wimp_KEY_UP:	c = KEY_TEXT_START; break;
		case wimp_KEY_CONTROL | wimp_KEY_DOWN:  c = KEY_TEXT_END;   break;
		case wimp_KEY_SHIFT | wimp_KEY_LEFT:	c = KEY_WORD_LEFT ; break;
		case wimp_KEY_SHIFT | wimp_KEY_RIGHT:	c = KEY_WORD_RIGHT; break;
		case wimp_KEY_SHIFT | wimp_KEY_UP:	c = KEY_PAGE_UP;    break;
		case wimp_KEY_SHIFT | wimp_KEY_DOWN:	c = KEY_PAGE_DOWN;  break;
		case wimp_KEY_LEFT:  c = KEY_LEFT; break;
		case wimp_KEY_RIGHT: c = KEY_RIGHT; break;
		case wimp_KEY_UP:    c = KEY_UP; break;
		case wimp_KEY_DOWN:  c = KEY_DOWN; break;

		/* editing */
		case wimp_KEY_CONTROL | wimp_KEY_END:
			c = KEY_DELETE_LINE_END;
			break;
		case wimp_KEY_DELETE:
			if (ro_gui_ctrl_pressed())
				c = KEY_DELETE_LINE_START;
			else if (os_version < RISCOS5)
				c = KEY_DELETE_LEFT;
			break;

		default:
			break;
		}
	}

	if (!(c & IS_WIMP_KEY)) {
		if (browser_window_key_press(g->bw, c))
			return true;
	}

	return ro_gui_window_handle_local_keypress(g, key, false);
}


/**
 * Callback handler for keypresses within browser window toolbars.
 *
 * \param *data			Client data, pointing to the GUI Window.
 * \param *key			The keypress data.
 * \return			true if the keypress was handled; else false.
 */

bool ro_gui_window_toolbar_keypress(void *data, wimp_key *key)
{
	struct gui_window *g = (struct gui_window *) data;

	if (g != NULL)
		return ro_gui_window_handle_local_keypress(g, key, true);

	return false;
}


/**
 * Handle keypresses within the RISC OS GUI: this is to be called after the
 * core has been given a chance to act, or on keypresses in the toolbar where
 * the core doesn't get involved.
 *
 * \param *g			The gui window to which the keypress applies.
 * \param *key			The keypress data.
 * \param is_toolbar		true if the keypress is from a toolbar;
 *				else false.
 * \return			true if the keypress was claimed; else false.
 */

bool ro_gui_window_handle_local_keypress(struct gui_window *g, wimp_key *key,
		bool is_toolbar)
{
	hlcache_handle		*h;
	wimp_window_state	state;
	int			y;
	const char		*toolbar_url;
	os_error		*error;
	float			scale;
	uint32_t		c = (uint32_t) key->c;

	if (g == NULL)
		return false;

	h = g->bw->current_content;

	switch (c) {
	case IS_WIMP_KEY + wimp_KEY_F1:	/* Help. */
		ro_gui_open_help_page("documentation/index");
		return true;

	case IS_WIMP_KEY + wimp_KEY_CONTROL + wimp_KEY_F1:
		ro_gui_window_action_page_info(g);
		return true;

	case IS_WIMP_KEY + wimp_KEY_F2:
		if (g->toolbar == NULL)
			return false;
		ro_gui_url_complete_close();
		ro_toolbar_set_url(g->toolbar, "www.", true, true);
		ro_gui_url_complete_start(g->toolbar);
		ro_gui_url_complete_keypress(g->toolbar, wimp_KEY_DOWN);
		return true;

	case IS_WIMP_KEY + wimp_KEY_CONTROL + wimp_KEY_F2:
		/* Close window. */
		ro_gui_url_complete_close();
		browser_window_destroy(g->bw);
		return true;

	case 19:		/* Ctrl + S */
	case IS_WIMP_KEY + wimp_KEY_F3:
		ro_gui_window_action_save(g, GUI_SAVE_SOURCE);
		return true;

	case IS_WIMP_KEY + wimp_KEY_CONTROL + wimp_KEY_F3:
		ro_gui_window_action_save(g, GUI_SAVE_TEXT);
		return true;

	case IS_WIMP_KEY + wimp_KEY_SHIFT + wimp_KEY_F3:
		ro_gui_window_action_save(g, GUI_SAVE_COMPLETE);
		return true;

	case IS_WIMP_KEY + wimp_KEY_CONTROL + wimp_KEY_SHIFT + wimp_KEY_F3:
		ro_gui_window_action_save(g, GUI_SAVE_DRAW);
		return true;

	case 6:			/* Ctrl + F */
	case IS_WIMP_KEY + wimp_KEY_F4:	/* Search */
		ro_gui_window_action_search(g);
		return true;

	case IS_WIMP_KEY + wimp_KEY_F5:	/* Reload */
		if (g->bw != NULL)
			browser_window_reload(g->bw, false);
		return true;

	case 18:		/* Ctrl+R (Full reload) */
	case IS_WIMP_KEY + wimp_KEY_CONTROL + wimp_KEY_F5:
		if (g->bw != NULL)
			browser_window_reload(g->bw, true);
		return true;

	case IS_WIMP_KEY + wimp_KEY_F6:	/* Hotlist */
		ro_gui_hotlist_open();
		return true;

	case IS_WIMP_KEY + wimp_KEY_F7:	/* Show local history */
		ro_gui_window_action_local_history(g);
		return true;

	case IS_WIMP_KEY + wimp_KEY_CONTROL + wimp_KEY_F7:
		/* Show global history */
		ro_gui_global_history_open();
		return true;

	case IS_WIMP_KEY + wimp_KEY_F8:	/* View source */
		ro_gui_view_source(h);
		return true;

	case IS_WIMP_KEY + wimp_KEY_F9:
		/* Dump content for debugging. */
		ro_gui_dump_content(h);
		return true;

	case IS_WIMP_KEY + wimp_KEY_CONTROL + wimp_KEY_F9:
		urldb_dump();
		return true;

	case IS_WIMP_KEY + wimp_KEY_CONTROL + wimp_KEY_SHIFT + wimp_KEY_F9:
		talloc_report_full(0, stderr);
		return true;

	case IS_WIMP_KEY + wimp_KEY_F11:	/* Zoom */
		ro_gui_window_action_zoom(g);
		return true;

	case IS_WIMP_KEY + wimp_KEY_SHIFT + wimp_KEY_F11:
		/* Toggle display of box outlines. */
		html_redraw_debug = !html_redraw_debug;
		gui_window_redraw_window(g);
		return true;

	case wimp_KEY_RETURN:
		if (is_toolbar) {
			toolbar_url = ro_toolbar_get_url(g->toolbar);
			if (toolbar_url != NULL)
				ro_gui_window_launch_url(g, toolbar_url);
		}
		return true;

	case wimp_KEY_ESCAPE:
		if (ro_gui_url_complete_close()) {
			ro_gui_url_complete_start(g->toolbar);
			return true;
		}

		if (g->bw != NULL)
			browser_window_stop(g->bw);
		return true;

	case 14:	/* CTRL+N */
		ro_gui_window_action_new_window(g);
		return true;

	case 17:       /* CTRL+Q (Zoom out) */
	case 23:       /* CTRL+W (Zoom in) */
		if (!h)
			break;
		scale = g->bw->scale;
		if (ro_gui_shift_pressed() && c == 17)
			scale = g->bw->scale - 0.1;
		else if (ro_gui_shift_pressed() && c == 23)
			scale = g->bw->scale + 0.1;
		else if (c == 17) {
			for (int i = SCALE_SNAP_TO_SIZE - 1; i >= 0; i--)
				if (scale_snap_to[i] < g->bw->scale) {
					scale = scale_snap_to[i];
					break;
				}
		} else {
			for (unsigned int i = 0; i < SCALE_SNAP_TO_SIZE; i++)
				if (scale_snap_to[i] > g->bw->scale) {
					scale = scale_snap_to[i];
					break;
				}
		}
		if (scale < scale_snap_to[0])
			scale = scale_snap_to[0];
		if (scale > scale_snap_to[SCALE_SNAP_TO_SIZE - 1])
			scale = scale_snap_to[SCALE_SNAP_TO_SIZE - 1];
		if (g->bw->scale != scale) {
			browser_window_set_scale(g->bw, scale, true);
		}
		return true;

	case IS_WIMP_KEY + wimp_KEY_PRINT:
		ro_gui_window_action_print(g);
		return true;

	case IS_WIMP_KEY | wimp_KEY_LEFT:
	case IS_WIMP_KEY | wimp_KEY_RIGHT:
	case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_LEFT:
	case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_RIGHT:
		if (is_toolbar)
			return false;
		break;
	case IS_WIMP_KEY + wimp_KEY_UP:
	case IS_WIMP_KEY + wimp_KEY_DOWN:
	case IS_WIMP_KEY + wimp_KEY_PAGE_UP:
	case IS_WIMP_KEY + wimp_KEY_PAGE_DOWN:
	case wimp_KEY_HOME:
	case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_UP:
	case IS_WIMP_KEY + wimp_KEY_END:
		break;
	default:
		return false; /* This catches any keys we don't want to claim */
	}

	/* Any keys that exit from the above switch() via break should be
	 * processed as scroll actions in the browser window. */

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}

	y = state.visible.y1 - state.visible.y0 - 32;
	if (g->toolbar)
		y -= ro_toolbar_full_height(g->toolbar);

	switch (c) {
		case IS_WIMP_KEY | wimp_KEY_LEFT:
			state.xscroll -= 32;
			break;
		case IS_WIMP_KEY | wimp_KEY_RIGHT:
			state.xscroll += 32;
			break;
		case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_LEFT:
			state.xscroll = -0x10000000;
			break;
		case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_RIGHT:
			state.xscroll = 0x10000000;
			break;
		case IS_WIMP_KEY | wimp_KEY_UP:
			state.yscroll += 32;
			break;
		case IS_WIMP_KEY | wimp_KEY_DOWN:
			state.yscroll -= 32;
			break;
		case IS_WIMP_KEY | wimp_KEY_PAGE_UP:
			state.yscroll += y;
			break;
		case IS_WIMP_KEY | wimp_KEY_PAGE_DOWN:
			state.yscroll -= y;
			break;
		case wimp_KEY_HOME:
		case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_UP:
			state.yscroll = 0x10000000;
			break;
		case IS_WIMP_KEY | wimp_KEY_END:
		case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_DOWN:
			state.yscroll = -0x10000000;
			break;
	}

	error = xwimp_open_window(PTR_WIMP_OPEN(&state));
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
	}

	return true;
}


/**
 * Prepare the browser window menu for (re-)opening
 *
 * \param  w			The window owning the menu.
 * \param  i			The icon owning the menu.
 * \param  *menu		The menu about to be opened.
 * \param  *pointer		Pointer to the relevant wimp event block, or
 *				NULL for an Adjust click.
 * \return			true if the event was handled; else false.
 */

bool ro_gui_window_menu_prepare(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_pointer *pointer)
{
	struct gui_window	*g;
	struct browser_window	*bw;
	struct toolbar		*toolbar;
	struct contextual_content cont;
	hlcache_handle		*h = NULL;
	bool			export_sprite, export_draw;
	os_coord		pos;

	g = (struct gui_window *) ro_gui_wimp_event_get_user_data(w);
	toolbar = g->toolbar;
	bw = g->bw;

	/* If this is the form select menu, handle it now and then exit.
	 * Otherwise, carry on to the main browser window menu.
	 */

	if (menu == gui_form_select_menu) {
		return ro_gui_window_prepare_form_select_menu(g->bw,
				gui_form_select_control);
	}

	if (menu != ro_gui_browser_window_menu)
		return false;

	/* If this is a new opening for the browser window menu (ie. not for a
	 * toolbar menu), get details of the object under the pointer.
	 */

	if (pointer != NULL && g->window == w) {
		ro_gui_url_complete_close();

		current_menu_object = NULL;
		current_menu_url = NULL;

		if (ro_gui_window_to_window_pos(g, pointer->pos.x,
				pointer->pos.y, &pos)) {
			browser_window_get_contextual_content(bw,
					pos.x, pos.y, &cont);
			h = cont.main;

			current_menu_object = cont.object;
			current_menu_url = cont.link_url;
		}
	}

	/* Shade menu entries according to the state of the window and object
	 * under the pointer.
	 */

	/* Toolbar (Sub)Menu */

	ro_gui_menu_set_entry_shaded(menu, TOOLBAR_BUTTONS,
			ro_toolbar_menu_option_shade(toolbar));
	ro_gui_menu_set_entry_ticked(menu, TOOLBAR_BUTTONS,
			ro_toolbar_menu_buttons_tick(toolbar));

	ro_gui_menu_set_entry_shaded(menu, TOOLBAR_ADDRESS_BAR,
			ro_toolbar_menu_edit_shade(toolbar));
	ro_gui_menu_set_entry_ticked(menu, TOOLBAR_ADDRESS_BAR,
			ro_toolbar_menu_url_tick(toolbar));

	ro_gui_menu_set_entry_shaded(menu, TOOLBAR_THROBBER,
			ro_toolbar_menu_edit_shade(toolbar));
	ro_gui_menu_set_entry_ticked(menu, TOOLBAR_THROBBER,
			ro_toolbar_menu_throbber_tick(toolbar));

	ro_gui_menu_set_entry_shaded(menu, TOOLBAR_EDIT,
			ro_toolbar_menu_edit_shade(toolbar));
	ro_gui_menu_set_entry_ticked(menu, TOOLBAR_EDIT,
			ro_toolbar_menu_edit_tick(toolbar));

	/* Page Submenu */

	ro_gui_menu_set_entry_shaded(menu, BROWSER_PAGE, h == NULL ||
			(content_get_type(h) != CONTENT_HTML &&
			content_get_type(h) != CONTENT_TEXTPLAIN));

	ro_gui_menu_set_entry_shaded(menu, BROWSER_PAGE_INFO, h == NULL);

	ro_gui_menu_set_entry_shaded(menu, BROWSER_PRINT, h == NULL);

	ro_gui_menu_set_entry_shaded(menu, BROWSER_NEW_WINDOW, h == NULL);

	ro_gui_menu_set_entry_shaded(menu, BROWSER_FIND_TEXT,
			h == NULL || (content_get_type(h) != CONTENT_HTML &&
				content_get_type(h) != CONTENT_TEXTPLAIN));


	ro_gui_menu_set_entry_shaded(menu, BROWSER_VIEW_SOURCE, h == NULL);

	ro_gui_menu_set_entry_shaded(menu, BROWSER_SAVE_URL_URI, h == NULL);
	ro_gui_menu_set_entry_shaded(menu, BROWSER_SAVE_URL_URL, h == NULL);
	ro_gui_menu_set_entry_shaded(menu, BROWSER_SAVE_URL_TEXT, h == NULL);

	ro_gui_menu_set_entry_shaded(menu, BROWSER_SAVE, h == NULL);
	ro_gui_menu_set_entry_shaded(menu, BROWSER_SAVE_COMPLETE, h == NULL);
	ro_gui_menu_set_entry_shaded(menu, BROWSER_EXPORT_DRAW, h == NULL);
	ro_gui_menu_set_entry_shaded(menu, BROWSER_EXPORT_PDF, h == NULL);
	ro_gui_menu_set_entry_shaded(menu, BROWSER_EXPORT_TEXT, h == NULL);

	ro_gui_menu_set_entry_shaded(menu, BROWSER_LINK_SAVE_URI,
			!current_menu_url);
	ro_gui_menu_set_entry_shaded(menu, BROWSER_LINK_SAVE_URL,
			!current_menu_url);
	ro_gui_menu_set_entry_shaded(menu, BROWSER_LINK_SAVE_TEXT,
			!current_menu_url);



	/* Object Submenu */

	ro_gui_menu_set_entry_shaded(menu, BROWSER_OBJECT,
			current_menu_object == NULL &&
			current_menu_url == NULL);

	ro_gui_menu_set_entry_shaded(menu, BROWSER_OBJECT_LINK,
			current_menu_url == NULL);

	ro_gui_menu_set_entry_shaded(menu, BROWSER_OBJECT_INFO,
			current_menu_object == NULL);
	ro_gui_menu_set_entry_shaded(menu, BROWSER_OBJECT_RELOAD,
			current_menu_object == NULL);
	ro_gui_menu_set_entry_shaded(menu, BROWSER_OBJECT_OBJECT,
			current_menu_object == NULL);

	ro_gui_menu_set_entry_shaded(menu, BROWSER_OBJECT_PRINT, true);
			/* Not yet implemented */

	ro_gui_menu_set_entry_shaded(menu, BROWSER_OBJECT_SAVE,
			current_menu_object == NULL);

	ro_gui_menu_set_entry_shaded(menu, BROWSER_OBJECT_SAVE_URL_URI,
			current_menu_object == NULL);
	ro_gui_menu_set_entry_shaded(menu, BROWSER_OBJECT_SAVE_URL_URL,
			current_menu_object == NULL);
	ro_gui_menu_set_entry_shaded(menu, BROWSER_OBJECT_SAVE_URL_TEXT,
			current_menu_object == NULL);

	if (current_menu_object != NULL)
		ro_gui_window_content_export_types(current_menu_object,
				&export_draw, &export_sprite);
	else
		ro_gui_window_content_export_types(h,
				&export_draw, &export_sprite);

	ro_gui_menu_set_entry_shaded(menu, BROWSER_OBJECT_EXPORT,
			(h == NULL && current_menu_object == NULL)
				|| !(export_sprite || export_draw));

	ro_gui_menu_set_entry_shaded(menu, BROWSER_OBJECT_EXPORT_SPRITE,
			(h == NULL && current_menu_object == NULL)
				|| !export_sprite);

	ro_gui_menu_set_entry_shaded(menu, BROWSER_OBJECT_EXPORT_DRAW,
			(h == NULL && current_menu_object == NULL)
				|| !export_draw);


	/* Selection Submenu */

	ro_gui_menu_set_entry_shaded(menu, BROWSER_SELECTION,
			h == NULL || (content_get_type(h) != CONTENT_HTML &&
				content_get_type(h) != CONTENT_TEXTPLAIN));
			/* make menu available if there's anything that /could/
			 * be selected */

	ro_gui_menu_set_entry_shaded(menu, BROWSER_SELECTION_SAVE,
			!browser_window_has_selection(bw));

	ro_gui_menu_set_entry_shaded(menu, BROWSER_SELECTION_COPY,
			!browser_window_has_selection(bw));

	ro_gui_menu_set_entry_shaded(menu, BROWSER_SELECTION_CUT,
			!browser_window_has_selection(bw) ||
			selection_read_only(browser_window_get_selection(bw)));

	ro_gui_menu_set_entry_shaded(menu, BROWSER_SELECTION_PASTE,
			h == NULL || bw->paste_callback == NULL);

	ro_gui_menu_set_entry_shaded(menu, BROWSER_SELECTION_CLEAR,
			!browser_window_has_selection(bw));


	/* Navigate Submenu */

	ro_gui_menu_set_entry_shaded(menu, BROWSER_NAVIGATE_BACK,
			!browser_window_back_available(bw));

	ro_gui_menu_set_entry_shaded(menu, BROWSER_NAVIGATE_FORWARD,
			!browser_window_forward_available(bw));

	ro_gui_menu_set_entry_shaded(menu, BROWSER_NAVIGATE_RELOAD_ALL,
			!browser_window_reload_available(bw));

	ro_gui_menu_set_entry_shaded(menu, BROWSER_NAVIGATE_STOP,
			!browser_window_stop_available(bw));

	ro_gui_menu_set_entry_shaded(menu, BROWSER_NAVIGATE_UP,
			!ro_gui_window_up_available(bw));



	/* View Submenu */

	ro_gui_menu_set_entry_shaded(menu, BROWSER_IMAGES_FOREGROUND, true);
	ro_gui_menu_set_entry_ticked(menu, BROWSER_IMAGES_FOREGROUND, true);
			/* Not yet implemented. */

	ro_gui_menu_set_entry_ticked(menu, BROWSER_IMAGES_BACKGROUND,
			g != NULL && g->option.background_images);

	ro_gui_menu_set_entry_shaded(menu, BROWSER_BUFFER_ANIMS,
			g == NULL || g->option.buffer_everything);
	ro_gui_menu_set_entry_ticked(menu, BROWSER_BUFFER_ANIMS, g != NULL &&
			(g->option.buffer_animations ||
			g->option.buffer_everything));

	ro_gui_menu_set_entry_shaded(menu, BROWSER_BUFFER_ALL, g == NULL);
	ro_gui_menu_set_entry_ticked(menu, BROWSER_BUFFER_ALL,
			g != NULL && g->option.buffer_everything);

	ro_gui_menu_set_entry_shaded(menu, BROWSER_SCALE_VIEW, h == NULL);

	ro_gui_menu_set_entry_shaded(menu, BROWSER_WINDOW_STAGGER,
			option_window_screen_width == 0);
	ro_gui_menu_set_entry_ticked(menu, BROWSER_WINDOW_STAGGER,
			((option_window_screen_width == 0) ||
			option_window_stagger));

	ro_gui_menu_set_entry_ticked(menu, BROWSER_WINDOW_COPY,
			option_window_size_clone);

	ro_gui_menu_set_entry_shaded(menu, BROWSER_WINDOW_RESET,
			option_window_screen_width == 0);


	/* Utilities Submenu */

	ro_gui_menu_set_entry_shaded(menu, HOTLIST_ADD_URL, h == NULL);

	ro_gui_menu_set_entry_shaded(menu, HISTORY_SHOW_LOCAL,
			(bw == NULL || (bw->history == NULL) ||
			!(h != NULL || history_back_available(bw->history) ||
			history_forward_available(bw->history))));


	/* Help Submenu */

	ro_gui_menu_set_entry_ticked(menu, HELP_LAUNCH_INTERACTIVE,
			ro_gui_interactive_help_available() &&
			option_interactive_help);

	return true;
}


/**
 * Handle submenu warnings for a browser window menu
 *
 * \param  w			The window owning the menu.
 * \param  i			The icon owning the menu.
 * \param  *menu		The menu to which the warning applies.
 * \param  *selection		The wimp menu selection data.
 * \param  action		The selected menu action.
 */

void ro_gui_window_menu_warning(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action)
{
	struct gui_window	*g;
	struct browser_window	*bw;
	hlcache_handle		*h;
	struct toolbar		*toolbar;
	bool			export;

	if (menu != ro_gui_browser_window_menu)
		return;

	g = (struct gui_window *) ro_gui_wimp_event_get_user_data(w);
	toolbar = g->toolbar;
	bw = g->bw;
	h = bw->current_content;

	switch (action) {
	case BROWSER_PAGE_INFO:
		if (h != NULL)
			ro_gui_window_prepare_pageinfo(g);
		break;

	case BROWSER_FIND_TEXT:
		if (h != NULL && (content_get_type(h) == CONTENT_HTML ||
				content_get_type(h) == CONTENT_TEXTPLAIN))
			ro_gui_search_prepare(g->bw);
		break;

	case BROWSER_SCALE_VIEW:
		if (h != NULL)
			ro_gui_dialog_prepare_zoom(g);
		break;

	case BROWSER_PRINT:
		if (h != NULL)
			ro_gui_print_prepare(g);
		break;

	case BROWSER_OBJECT_INFO:
		if (current_menu_object != NULL)
			ro_gui_window_prepare_objectinfo(current_menu_object,
					current_menu_url);
		break;

	case BROWSER_OBJECT_SAVE:
		if (current_menu_object != NULL)
			ro_gui_save_prepare(GUI_SAVE_OBJECT_ORIG,
					current_menu_object, NULL, NULL, NULL);
		break;

	case BROWSER_SELECTION_SAVE:
		if (browser_window_has_selection(bw))
			ro_gui_save_prepare(GUI_SAVE_TEXT_SELECTION, NULL,
					browser_window_get_selection(bw),
					NULL, NULL);
		break;

	case BROWSER_SAVE_URL_URI:
		if (h != NULL)
			ro_gui_save_prepare(GUI_SAVE_LINK_URI, NULL, NULL,
					nsurl_access(content_get_url(h)),
					content_get_title(h));
		break;

	case BROWSER_SAVE_URL_URL:
		if (h != NULL)
			ro_gui_save_prepare(GUI_SAVE_LINK_URL, NULL, NULL,
					nsurl_access(content_get_url(h)),
					content_get_title(h));
		break;

	case BROWSER_SAVE_URL_TEXT:
		if (h != NULL)
			ro_gui_save_prepare(GUI_SAVE_LINK_TEXT, NULL, NULL,
					nsurl_access(content_get_url(h)),
					content_get_title(h));
		break;

	case BROWSER_OBJECT_SAVE_URL_URI:
		if (current_menu_object != NULL)
			ro_gui_save_prepare(GUI_SAVE_LINK_URI, NULL, NULL,
					nsurl_access(content_get_url(
							current_menu_object)),
					content_get_title(current_menu_object));
		break;

	case BROWSER_OBJECT_SAVE_URL_URL:
		if (current_menu_object != NULL)
			ro_gui_save_prepare(GUI_SAVE_LINK_URL, NULL, NULL,
					nsurl_access(content_get_url(
							current_menu_object)),
					content_get_title(current_menu_object));
		break;

	case BROWSER_OBJECT_SAVE_URL_TEXT:
		if (current_menu_object != NULL)
			ro_gui_save_prepare(GUI_SAVE_LINK_TEXT, NULL, NULL,
					nsurl_access(content_get_url(
							current_menu_object)),
					content_get_title(current_menu_object));
		break;

	case BROWSER_SAVE:
		if (h != NULL)
			ro_gui_save_prepare(GUI_SAVE_SOURCE, h, NULL, NULL, NULL);
		break;

	case BROWSER_SAVE_COMPLETE:
		if (h != NULL)
			ro_gui_save_prepare(GUI_SAVE_COMPLETE, h, NULL, NULL, NULL);
		break;

	case BROWSER_EXPORT_DRAW:
		if (h != NULL)
			ro_gui_save_prepare(GUI_SAVE_DRAW, h, NULL, NULL, NULL);
		break;

	case BROWSER_EXPORT_PDF:
		if (h != NULL)
			ro_gui_save_prepare(GUI_SAVE_PDF, h, NULL, NULL, NULL);
		break;

	case BROWSER_EXPORT_TEXT:
		if (h != NULL)
			ro_gui_save_prepare(GUI_SAVE_TEXT, h, NULL, NULL, NULL);
		break;

	case BROWSER_LINK_SAVE_URI:
		if (current_menu_url != NULL)
			ro_gui_save_prepare(GUI_SAVE_LINK_URI, NULL, NULL,
					current_menu_url, NULL);
		break;

	case BROWSER_LINK_SAVE_URL:
		if (current_menu_url != NULL)
			ro_gui_save_prepare(GUI_SAVE_LINK_URL, NULL, NULL,
					current_menu_url, NULL);
		break;

	case BROWSER_LINK_SAVE_TEXT:
		if (current_menu_url != NULL)
			ro_gui_save_prepare(GUI_SAVE_LINK_TEXT, NULL, NULL,
					current_menu_url, NULL);
		break;

	case BROWSER_OBJECT_EXPORT_SPRITE:
		if (current_menu_object != NULL) {
			ro_gui_window_content_export_types(current_menu_object,
					NULL, &export);

			if (export)
				ro_gui_save_prepare(GUI_SAVE_OBJECT_NATIVE,
						current_menu_object,
						NULL, NULL, NULL);
		} else if (h != NULL) {
			ro_gui_window_content_export_types(h, NULL, &export);

			if (export)
				ro_gui_save_prepare(GUI_SAVE_OBJECT_NATIVE,
						h, NULL, NULL, NULL);
		}
		break;

	case BROWSER_OBJECT_EXPORT_DRAW:
		if (current_menu_object != NULL) {
			ro_gui_window_content_export_types(current_menu_object,
					&export, NULL);

			if (export)
				ro_gui_save_prepare(GUI_SAVE_OBJECT_NATIVE,
						current_menu_object,
						NULL, NULL, NULL);
		} else if (h != NULL) {
			ro_gui_window_content_export_types(h, &export, NULL);

			if (export)
				ro_gui_save_prepare(GUI_SAVE_OBJECT_NATIVE,
						h, NULL, NULL, NULL);
		}
		break;

	default:
		break;
	}
}


/**
 * Handle selections from a browser window menu
 *
 * \param  w			The window owning the menu.
 * \param  i			The icon owning the menu.
 * \param  *menu		The menu from which the selection was made.
 * \param  *selection		The wimp menu selection data.
 * \param  action		The selected menu action.
 * \return			true if action accepted; else false.
 */

bool ro_gui_window_menu_select(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action)
{
	struct gui_window	*g;
	struct browser_window	*bw;
	hlcache_handle		*h;
	struct toolbar		*toolbar;
	wimp_window_state	state;
	os_error		*error;

	g = (struct gui_window *) ro_gui_wimp_event_get_user_data(w);
	toolbar = g->toolbar;
	bw = g->bw;
	h = bw->current_content;

	/* If this is a form menu from the core, handle it now and then exit.
	 * Otherwise, carry on to the main browser window menu.
	 */

	if (menu == gui_form_select_menu && w == g->window) {
		ro_gui_window_process_form_select_menu(g, selection);

		return true;
	}

	/* We're now safe to assume that this is either the browser or
	 * toolbar window menu.
	 */

	switch (action) {

	/* help actions */
	case HELP_OPEN_CONTENTS:
		ro_gui_open_help_page("documentation/index");
		break;
	case HELP_OPEN_GUIDE:
		ro_gui_open_help_page("documentation/guide");
		break;
	case HELP_OPEN_INFORMATION:
		ro_gui_open_help_page("documentation/info");
		break;
	case HELP_OPEN_CREDITS:
		browser_window_create("about:credits", NULL, 0, true, false);
		break;
	case HELP_OPEN_LICENCE:
		browser_window_create("about:licence", NULL, 0, true, false);
		break;
	case HELP_LAUNCH_INTERACTIVE:
		if (!ro_gui_interactive_help_available()) {
			ro_gui_interactive_help_start();
			option_interactive_help = true;
		} else {
			option_interactive_help = !option_interactive_help;
		}
		break;

	/* history actions */
	case HISTORY_SHOW_LOCAL:
		ro_gui_window_action_local_history(g);
		break;
	case HISTORY_SHOW_GLOBAL:
		ro_gui_global_history_open();
		break;

	/* hotlist actions */
	case HOTLIST_ADD_URL:
		ro_gui_window_action_add_bookmark(g);
		break;
	case HOTLIST_SHOW:
		ro_gui_hotlist_open();
		break;

	/* cookies actions */
	case COOKIES_SHOW:
		ro_gui_cookies_open();
		break;

	case COOKIES_DELETE:
		cookies_delete_all();
		break;

	/* page actions */
	case BROWSER_PAGE_INFO:
		ro_gui_window_action_page_info(g);
		break;
	case BROWSER_PRINT:
		ro_gui_window_action_print(g);
		break;
	case BROWSER_NEW_WINDOW:
		ro_gui_window_action_new_window(g);
		break;
	case BROWSER_VIEW_SOURCE:
		if (h != NULL)
			ro_gui_view_source(h);
		break;

	/* object actions */
	case BROWSER_OBJECT_INFO:
		if (current_menu_object != NULL) {
			ro_gui_window_prepare_objectinfo(current_menu_object,
					current_menu_url);
			ro_gui_dialog_open_persistent(g->window,
					dialog_objinfo, false);
		}
		break;
	case BROWSER_OBJECT_RELOAD:
		if (current_menu_object != NULL) {
			content_invalidate_reuse_data(current_menu_object);
			browser_window_reload(bw, false);
		}
		break;

		/* link actions */
	case BROWSER_LINK_SAVE_URI:
		if (current_menu_url != NULL) {
			ro_gui_save_prepare(GUI_SAVE_LINK_URI, NULL, NULL,
					current_menu_url, NULL);
			ro_gui_dialog_open_persistent(g->window, dialog_saveas,
					false);
		}
		break;
	case BROWSER_LINK_SAVE_URL:
		if (current_menu_url != NULL) {
			ro_gui_save_prepare(GUI_SAVE_LINK_URL, NULL, NULL,
					current_menu_url, NULL);
			ro_gui_dialog_open_persistent(g->window, dialog_saveas,
					false);
		}
		break;
	case BROWSER_LINK_SAVE_TEXT:
		if (current_menu_url != NULL) {
			ro_gui_save_prepare(GUI_SAVE_LINK_TEXT, NULL, NULL,
					current_menu_url, NULL);
			ro_gui_dialog_open_persistent(g->window, dialog_saveas,
					false);
		}
		break;
	case BROWSER_LINK_DOWNLOAD:
		if (current_menu_url != NULL)
			browser_window_download(bw, current_menu_url,
					nsurl_access(content_get_url(h)));
		break;
	case BROWSER_LINK_NEW_WINDOW:
		if (current_menu_url != NULL)
			browser_window_create(current_menu_url, bw,
					nsurl_access(content_get_url(h)),
					true, false);
		break;

		/* save actions */
	case BROWSER_OBJECT_SAVE:
		if (current_menu_object != NULL) {
			ro_gui_save_prepare(GUI_SAVE_OBJECT_ORIG,
					current_menu_object, NULL, NULL, NULL);
			ro_gui_dialog_open_persistent(g->window, dialog_saveas,
					false);
		}
		break;
	case BROWSER_OBJECT_EXPORT_SPRITE:
		if (current_menu_object != NULL) {
			ro_gui_save_prepare(GUI_SAVE_OBJECT_NATIVE,
					current_menu_object, NULL, NULL, NULL);
			ro_gui_dialog_open_persistent(g->window, dialog_saveas,
					false);
		}
		break;
	case BROWSER_OBJECT_EXPORT_DRAW:
		if (current_menu_object != NULL) {
			ro_gui_save_prepare(GUI_SAVE_OBJECT_NATIVE,
					current_menu_object, NULL, NULL, NULL);
			ro_gui_dialog_open_persistent(g->window, dialog_saveas,
					false);
		}
		break;
	case BROWSER_SAVE:
		ro_gui_window_action_save(g, GUI_SAVE_SOURCE);
		break;
	case BROWSER_SAVE_COMPLETE:
		ro_gui_window_action_save(g, GUI_SAVE_COMPLETE);
		break;
	case BROWSER_EXPORT_DRAW:
		ro_gui_window_action_save(g, GUI_SAVE_DRAW);
		break;
	case BROWSER_EXPORT_PDF:
		ro_gui_window_action_save(g, GUI_SAVE_PDF);
		break;
	case BROWSER_EXPORT_TEXT:
		ro_gui_window_action_save(g, GUI_SAVE_TEXT);
		break;
	case BROWSER_SAVE_URL_URI:
		ro_gui_window_action_save(g, GUI_SAVE_LINK_URI);
		break;
	case BROWSER_SAVE_URL_URL:
		ro_gui_window_action_save(g, GUI_SAVE_LINK_URL);
		break;
	case BROWSER_SAVE_URL_TEXT:
		ro_gui_window_action_save(g, GUI_SAVE_LINK_TEXT);
		break;

	/* selection actions */
	case BROWSER_SELECTION_SAVE:
		if (h != NULL) {
			ro_gui_save_prepare(GUI_SAVE_TEXT_SELECTION, NULL,
					browser_window_get_selection(bw),
					NULL, NULL);
			ro_gui_dialog_open_persistent(g->window, dialog_saveas,
					false);
		}
		break;
	case BROWSER_SELECTION_COPY:
		browser_window_key_press(bw, KEY_COPY_SELECTION);
		break;
	case BROWSER_SELECTION_CUT:
		browser_window_key_press(bw, KEY_CUT_SELECTION);
		break;
	case BROWSER_SELECTION_PASTE:
		browser_window_key_press(bw, KEY_PASTE);
		break;
	case BROWSER_SELECTION_ALL:
		browser_window_key_press(bw, KEY_SELECT_ALL);
		break;
	case BROWSER_SELECTION_CLEAR:
		browser_window_key_press(bw, KEY_CLEAR_SELECTION);
		break;

	/* navigation actions */
	case BROWSER_NAVIGATE_HOME:
		ro_gui_window_action_home(g);
		break;
	case BROWSER_NAVIGATE_BACK:
		if (bw != NULL && bw->history != NULL)
			history_back(bw, bw->history);
		break;
	case BROWSER_NAVIGATE_FORWARD:
		if (bw != NULL && bw->history != NULL)
			history_forward(bw, bw->history);
		break;
	case BROWSER_NAVIGATE_UP:
		if (bw != NULL && h != NULL)
			ro_gui_window_navigate_up(bw->window,
					nsurl_access(content_get_url(h)));
		break;
	case BROWSER_NAVIGATE_RELOAD_ALL:
		if (bw != NULL)
			browser_window_reload(bw, true);
		break;
	case BROWSER_NAVIGATE_STOP:
		if (bw != NULL)
			browser_window_stop(bw);
		break;

	/* browser window/display actions */
	case BROWSER_SCALE_VIEW:
		ro_gui_window_action_zoom(g);
		break;
	case BROWSER_FIND_TEXT:
		ro_gui_window_action_search(g);
		break;
	case BROWSER_IMAGES_BACKGROUND:
		if (g != NULL) {
			g->option.background_images =
					!g->option.background_images;
			gui_window_redraw_window(g);
		}
		break;
	case BROWSER_BUFFER_ANIMS:
		if (g != NULL)
			g->option.buffer_animations =
					!g->option.buffer_animations;
		break;
	case BROWSER_BUFFER_ALL:
		if (g != NULL)
		g->option.buffer_everything =
				!g->option.buffer_everything;
		break;
	case BROWSER_SAVE_VIEW:
		if (bw != NULL) {
			ro_gui_window_default_options(bw);
			ro_gui_save_options();
		}
		break;
	case BROWSER_WINDOW_DEFAULT:
		if (g != NULL) {
			ro_gui_screen_size(&option_window_screen_width,
					&option_window_screen_height);
			state.w = w;
			error = xwimp_get_window_state(&state);
			if (error) {
				LOG(("xwimp_get_window_state: 0x%x: %s",
						error->errnum,
						error->errmess));
				warn_user("WimpError", error->errmess);
			}
			option_window_x = state.visible.x0;
			option_window_y = state.visible.y0;
			option_window_width =
					state.visible.x1 - state.visible.x0;
			option_window_height =
					state.visible.y1 - state.visible.y0;
			ro_gui_save_options();
		}
		break;
	case BROWSER_WINDOW_STAGGER:
		option_window_stagger = !option_window_stagger;
		ro_gui_save_options();
		break;
	case BROWSER_WINDOW_COPY:
		option_window_size_clone = !option_window_size_clone;
		ro_gui_save_options();
		break;
	case BROWSER_WINDOW_RESET:
		option_window_screen_width = 0;
		option_window_screen_height = 0;
		ro_gui_save_options();
		break;

	/* toolbar actions */
	case TOOLBAR_BUTTONS:
		assert(toolbar);
		ro_toolbar_set_display_buttons(toolbar,
				!ro_toolbar_get_display_buttons(toolbar));
		break;
	case TOOLBAR_ADDRESS_BAR:
		assert(toolbar);
		ro_toolbar_set_display_url(toolbar,
				!ro_toolbar_get_display_url(toolbar));
		if (ro_toolbar_get_display_url(toolbar))
			ro_toolbar_take_caret(toolbar);
		break;
	case TOOLBAR_THROBBER:
		assert(toolbar);
		ro_toolbar_set_display_throbber(toolbar,
				!ro_toolbar_get_display_throbber(toolbar));
		break;
	case TOOLBAR_EDIT:
		assert(toolbar);
		ro_toolbar_toggle_edit(toolbar);
		break;

	default:
		return false;
	}

	return true;
}


/**
 * Handle the closure of a browser window menu
 *
 * \param  w			The window owning the menu.
 * \param  i			The icon owning the menu.
 * \param  *menu		The menu that is being closed.
 */

void ro_gui_window_menu_close(wimp_w w, wimp_i i, wimp_menu *menu)
{
	if (menu == ro_gui_browser_window_menu) {
		current_menu_object = NULL;
		current_menu_url = NULL;
	} else if (menu == gui_form_select_menu) {
		gui_form_select_control = NULL;
	}
}


/**
 * Process Scroll_Request events.
 *
 * \TODO -- This handles Scroll Events for all windows, not just browser
 *          windows.  Either it needs to move somewhere else, or the
 *          code needs to be split and Scroll Events dispatched via the
 *          Wimp_Event module.  Currently it doesn't properly handle
 *          events affecting treeview windows, anyway.
 */

void ro_gui_scroll_request(wimp_scroll *scroll)
{
	struct gui_window	*g = ro_gui_window_lookup(scroll->w);
	struct toolbar		*toolbar;

	if (g && g->bw->current_content && ro_gui_shift_pressed()) {
		/* extended scroll request with shift held down; change zoom */
		float scale, inc;

		if (scroll->ymin & 3)
			inc = 0.02;  /* RO5 sends the msg 5 times;
				      * don't ask me why */
		else
			inc = (1 << (ABS(scroll->ymin)>>2)) / 20.0F;

		if (scroll->ymin > 0) {
			scale = g->bw->scale + inc;
			if (scale > scale_snap_to[SCALE_SNAP_TO_SIZE - 1])
				scale = scale_snap_to[SCALE_SNAP_TO_SIZE - 1];
		} else {
			scale = g->bw->scale - inc;
			if (scale < scale_snap_to[0])
				scale = scale_snap_to[0];
		}
		if (g->bw->scale != scale)
			browser_window_set_scale(g->bw, scale, true);
	} else {
		int x = scroll->visible.x1 - scroll->visible.x0 - 32;
		int y = scroll->visible.y1 - scroll->visible.y0 - 32;
		os_error *error;

		/* This has to handle all windows, not just browser ones. */
		toolbar = ro_toolbar_parent_window_lookup(scroll->w);
		assert(g == NULL || g->toolbar == NULL ||
				g->toolbar == toolbar);
		if (toolbar != NULL)
			y -= ro_toolbar_full_height(toolbar);

		switch (scroll->xmin) {
			case wimp_SCROLL_PAGE_LEFT:
				scroll->xscroll -= x;
				break;
			case wimp_SCROLL_COLUMN_LEFT:
				scroll->xscroll -= 32;
				break;
			case wimp_SCROLL_COLUMN_RIGHT:
				scroll->xscroll += 32;
				break;
			case wimp_SCROLL_PAGE_RIGHT:
				scroll->xscroll += x;
				break;
			default:
				scroll->xscroll += (x * (scroll->xmin>>2)) >> 2;
				break;
		}

		switch (scroll->ymin) {
			case wimp_SCROLL_PAGE_UP:
				scroll->yscroll += y;
				break;
			case wimp_SCROLL_LINE_UP:
				scroll->yscroll += 32;
				break;
			case wimp_SCROLL_LINE_DOWN:
				scroll->yscroll -= 32;
				break;
			case wimp_SCROLL_PAGE_DOWN:
				scroll->yscroll -= y;
				break;
			default:
				scroll->yscroll += (y * (scroll->ymin>>2)) >> 2;
				break;
		}

		error = xwimp_open_window((wimp_open *) scroll);
		if (error) {
			LOG(("xwimp_open_window: 0x%x: %s",
					error->errnum, error->errmess));
		}
	}
}


/**
 * Handle Message_DataLoad (file dragged in) for a window.
 *
 * \param  g	    window
 * \param  message  Message_DataLoad block
 * \return  true if the load was processed
 *
 * If the file was dragged into a form file input, it is used as the value.
 */

bool ro_gui_window_dataload(struct gui_window *g, wimp_message *message)
{
	struct box *box;
	struct box *file_box = 0;
	struct box *text_box = 0;
	struct browser_window *bw = g->bw;
	hlcache_handle *h;
	int box_x, box_y;
	os_error *error;
	os_coord pos;

	h = bw->current_content;

	/* HTML content only. */
	if (!bw->current_content || content_get_type(h) != CONTENT_HTML)
		return false;

	/* Ignore directories etc. */
	if (0x1000 <= message->data.data_xfer.file_type)
		return false;

	if (!ro_gui_window_to_window_pos(g, message->data.data_xfer.pos.x,
			message->data.data_xfer.pos.y, &pos))
		return false;

	box = html_get_box_tree(h);

	/* Consider the margins of the html page now */
	box_x = box->margin[LEFT];
	box_y = box->margin[TOP];

	while ((box = box_at_point(box, pos.x, pos.y, &box_x, &box_y, &h))) {
		if (box->style && css_computed_visibility(box->style) ==
				CSS_VISIBILITY_HIDDEN)
			continue;

		if (box->gadget) {
			switch (box->gadget->type) {
				case GADGET_FILE:
					file_box = box;
				break;

				case GADGET_TEXTBOX:
				case GADGET_TEXTAREA:
				case GADGET_PASSWORD:
					text_box = box;
					break;

				default:	/* appease compiler */
					break;
			}
		}
	}

	if (!file_box && !text_box)
		return false;

	if (file_box) {
		utf8_convert_ret ret;
		char *utf8_fn;

		ret = utf8_from_local_encoding(
				message->data.data_xfer.file_name, 0,
				&utf8_fn);
		if (ret != UTF8_CONVERT_OK) {
			/* A bad encoding should never happen */
			assert(ret != UTF8_CONVERT_BADENC);
			LOG(("utf8_from_local_encoding failed"));
			/* Load was for us - just no memory */
			return true;
		}

		/* Found: update form input. */
		free(file_box->gadget->value);
		file_box->gadget->value = utf8_fn;

		/* Redraw box. */
		box_coords(file_box, &pos.x, &pos.y);

		error = xwimp_force_redraw(bw->window->window,
				pos.x * 2, -(pos.y + file_box->height) * 2,
				(pos.x + file_box->width) * 2, -pos.y * 2);
		if (error) {
			LOG(("xwimp_force_redraw: 0x%x: %s",
			     error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	} else {

		const char *filename = message->data.data_xfer.file_name;

		browser_window_mouse_click(g->bw, BROWSER_MOUSE_PRESS_1, pos.x, pos.y);

		if (!ro_gui_window_import_text(g, filename, false))
			return true;  /* it was for us, it just didn't work! */
	}

	/* send DataLoadAck */
	message->action = message_DATA_LOAD_ACK;
	message->your_ref = message->my_ref;
	error = xwimp_send_message(wimp_USER_MESSAGE, message, message->sender);
	if (error) {
		LOG(("xwimp_send_message: 0x%x: %s\n",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	return true;
}


/**
 * Handle pointer movements in a browser window.
 *
 * \param  g	    browser window that the pointer is in
 * \param  pointer  new mouse position
 */

void ro_gui_window_mouse_at(struct gui_window *g, wimp_pointer *pointer)
{
	os_coord pos;

	if (ro_gui_window_to_window_pos(g, pointer->pos.x, pointer->pos.y, &pos))
		browser_window_mouse_track(g->bw,
				ro_gui_mouse_drag_state(pointer->buttons,
						wimp_BUTTON_CLICK_DRAG),
				pos.x, pos.y);
}


/**
 * Window is being iconised. Create a suitable thumbnail sprite
 * (which, sadly, must be in the Wimp sprite pool), and return
 * the sprite name and truncated title to the iconiser
 *
 * \param  g   the gui window being iconised
 * \param  wi  the WindowInfo message from the iconiser
 */

void ro_gui_window_iconise(struct gui_window *g,
		wimp_full_message_window_info *wi)
{
	/* sadly there is no 'legal' way to get the sprite into
	 * the Wimp sprite pool other than via a filing system */
	const char *temp_fname = "Pipe:$._tmpfile";
	struct browser_window *bw = g->bw;
	osspriteop_header *overlay = NULL;
	osspriteop_header *sprite_header;
	struct bitmap *bitmap;
	osspriteop_area *area;
	int width = 34, height = 34;
	hlcache_handle *h;
	os_error *error;
	int len, id;

	assert(bw);

	h = bw->current_content;
	if (!h) return;

	/* if an overlay sprite is defined, locate it and gets its dimensions
	 * so that we can produce a thumbnail with the same dimensions */
	if (!ro_gui_wimp_get_sprite("ic_netsfxx", &overlay)) {
		error = xosspriteop_read_sprite_info(osspriteop_PTR,
				(osspriteop_area *)0x100,
				(osspriteop_id)overlay, &width, &height, NULL,
				NULL);
		if (error) {
			LOG(("xosspriteop_read_sprite_info: 0x%x: %s",
				error->errnum, error->errmess));
			warn_user("MiscError", error->errmess);
			overlay = NULL;
		}
		else if (sprite_bpp(overlay) != 8) {
			LOG(("overlay sprite is not 8bpp"));
			overlay = NULL;
		}
	}

	/* create the thumbnail sprite */
	bitmap = bitmap_create(width, height, BITMAP_NEW | BITMAP_OPAQUE |
			BITMAP_CLEAR_MEMORY);
	if (!bitmap) {
		LOG(("Thumbnail initialisation failed."));
		return;
	}
	thumbnail_create(h, bitmap, NULL);
	if (overlay)
		bitmap_overlay_sprite(bitmap, overlay);
	area = thumbnail_convert_8bpp(bitmap);
	bitmap_destroy(bitmap);
	if (!area) {
		LOG(("Thumbnail conversion failed."));
		return;
	}

	/* choose a suitable sprite name */
	id = 0;
	while (iconise_used[id])
		if ((unsigned)++id >= NOF_ELEMENTS(iconise_used)) {
			id = iconise_next;
			if ((unsigned)++iconise_next >=
					NOF_ELEMENTS(iconise_used))
				iconise_next = 0;
			break;
		}

	sprite_header = (osspriteop_header *)(area + 1);
	len = sprintf(sprite_header->name, "ic_netsf%.2d", id);

	error = xosspriteop_save_sprite_file(osspriteop_USER_AREA,
			area, temp_fname);
	if (error) {
		LOG(("xosspriteop_save_sprite_file: 0x%x:%s",
			error->errnum, error->errmess));
		warn_user("MiscError", error->errmess);
		free(area);
		return;
	}

	error = xwimpspriteop_merge_sprite_file(temp_fname);
	if (error) {
		LOG(("xwimpspriteop_merge_sprite_file: 0x%x:%s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		remove(temp_fname);
		free(area);
		return;
	}

	memcpy(wi->sprite_name, sprite_header->name + 3, len - 2); /* inc NUL */
	strncpy(wi->title, g->title, sizeof(wi->title));
	wi->title[sizeof(wi->title) - 1] = '\0';

	if (wimptextop_string_width(wi->title, 0) > 182) {
		/* work around bug in Pinboard where it will fail to display
		 * the icon if the text is very wide */
		if (strlen(wi->title) > 10)
			wi->title[10] = '\0';	/* pinboard does this anyway */
		while (wimptextop_string_width(wi->title, 0) > 182)
			wi->title[strlen(wi->title) - 1] = '\0';
	}

	wi->size = sizeof(wimp_full_message_window_info);
	wi->your_ref = wi->my_ref;
	error = xwimp_send_message(wimp_USER_MESSAGE, (wimp_message*)wi,
			wi->sender);
	if (error) {
		LOG(("xwimp_send_message: 0x%x:%s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	else {
		g->iconise_icon = id;
		iconise_used[id] = true;
	}

	free(area);
}


/**
 * Completes scrolling of a browser window
 *
 * \param g  gui window
 */

void ro_gui_window_scroll_end(struct gui_window *g, wimp_dragged *drag)
{
	wimp_pointer pointer;
	os_error *error;
	os_coord pos;

	gui_current_drag_type = GUI_DRAG_NONE;
	if (!g)
		return;

	error = xwimp_drag_box((wimp_drag*)-1);
	if (error) {
		LOG(("xwimp_drag_box: 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	error = xwimpspriteop_set_pointer_shape("ptr_default", 0x31, 0, 0, 0, 0);
	if (error) {
		LOG(("xwimpspriteop_set_pointer_shape: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	if (ro_gui_window_to_window_pos(g, drag->final.x0, drag->final.y0, &pos))
		browser_window_mouse_track(g->bw, 0, pos.x, pos.y);
}


/**
 * Completes resizing of a browser frame
 *
 * \param g  gui window
 */

void ro_gui_window_frame_resize_end(struct gui_window *g, wimp_dragged *drag)
{
	/* our clean-up is the same as for page scrolling */
	ro_gui_window_scroll_end(g, drag);
}


/**
 * Process Mouse_Click events in a toolbar's button bar.  This does not handle
 * other clicks in a toolbar: these are handled by the toolbar module itself.
 *
 * \param  *data		The GUI window associated with the click.
 * \param  action_type		The action type to be handled.
 * \param  action		The action to process.
 */

void ro_gui_window_toolbar_click(void *data,
		toolbar_action_type action_type, union toolbar_action action)
{
	struct gui_window	*g = data;
	struct browser_window	*new_bw;
	gui_save_type		save_type;

	if (g == NULL)
		return;


	if (action_type == TOOLBAR_ACTION_URL &&
			action.url == TOOLBAR_URL_DRAG_URL) {
		if (g->bw->current_content) {
			hlcache_handle *h = g->bw->current_content;

			if (ro_gui_shift_pressed())
				save_type = GUI_SAVE_LINK_URL;
			else
				save_type = GUI_SAVE_LINK_TEXT;

			ro_gui_drag_save_link(save_type,
					nsurl_access(content_get_url(h)),
					content_get_title(h), g);
		}

		return;
	}


	/* By now, the only valid action left is a button click.  If it isn't
	 * one of those, give up.
	 */

	if (action_type != TOOLBAR_ACTION_BUTTON)
		return;

	switch (action.button) {
	case TOOLBAR_BUTTON_BACK:
		if (g->bw != NULL && g->bw->history != NULL)
				history_back(g->bw, g->bw->history);
		break;

	case TOOLBAR_BUTTON_BACK_NEW:
		ro_gui_window_action_navigate_back_new(g);
		break;

	case TOOLBAR_BUTTON_FORWARD:
		if (g->bw != NULL && g->bw->history != NULL)
				history_forward(g->bw, g->bw->history);
		break;

	case TOOLBAR_BUTTON_FORWARD_NEW:
		ro_gui_window_action_navigate_forward_new(g);
		break;

	case TOOLBAR_BUTTON_STOP:
		if (g->bw != NULL)
			browser_window_stop(g->bw);
		break;

	case TOOLBAR_BUTTON_RELOAD:
		if (g->bw != NULL)
			browser_window_reload(g->bw, false);
		break;

	case TOOLBAR_BUTTON_RELOAD_ALL:
		if (g->bw != NULL)
			browser_window_reload(g->bw, true);
		break;

	case TOOLBAR_BUTTON_HISTORY_LOCAL:
		ro_gui_window_action_local_history(g);
		break;

	case TOOLBAR_BUTTON_HISTORY_GLOBAL:
		ro_gui_global_history_open();
		break;

	case TOOLBAR_BUTTON_HOME:
		ro_gui_window_action_home(g);
		break;

	case TOOLBAR_BUTTON_SEARCH:
		ro_gui_window_action_search(g);
		break;

	case TOOLBAR_BUTTON_SCALE:
		ro_gui_window_action_zoom(g);
		break;

	case TOOLBAR_BUTTON_BOOKMARK_OPEN:
		ro_gui_hotlist_open();
		break;

	case TOOLBAR_BUTTON_BOOKMARK_ADD:
		ro_gui_window_action_add_bookmark(g);
		break;

	case TOOLBAR_BUTTON_SAVE_SOURCE:
		ro_gui_window_action_save(g, GUI_SAVE_SOURCE);
		break;

	case TOOLBAR_BUTTON_SAVE_COMPLETE:
		ro_gui_window_action_save(g, GUI_SAVE_COMPLETE);
		break;

	case TOOLBAR_BUTTON_PRINT:
		ro_gui_window_action_print(g);
		break;

	case TOOLBAR_BUTTON_UP:
		if (g->bw != NULL && g->bw->current_content != NULL)
			ro_gui_window_navigate_up(g->bw->window,
					nsurl_access(content_get_url(
					g->bw->current_content)));
		break;

	case TOOLBAR_BUTTON_UP_NEW:
		if (g->bw && g->bw->current_content) {
			hlcache_handle *h = g->bw->current_content;
			new_bw = browser_window_create(NULL,
						g->bw, NULL, false,
						false);
			/* do it without loading the content
			 * into the new window */
			ro_gui_window_navigate_up(new_bw->window,
					nsurl_access(content_get_url(h)));
		}
		break;

	default:
		break;
	}

	ro_gui_window_update_toolbar_buttons(g);
}


/**
 * Handle Message_DataLoad (file dragged in) for a toolbar
 *
 * \TODO -- This belongs in the toolbar module, and should be moved there
 *          once the module is able to usefully handle its own events.
 *
 * \param  g	    window
 * \param  message  Message_DataLoad block
 * \return true if the load was processed
 */

bool ro_gui_toolbar_dataload(struct gui_window *g, wimp_message *message)
{
	if (message->data.data_xfer.file_type == osfile_TYPE_TEXT &&
		ro_gui_window_import_text(g, message->data.data_xfer.file_name, true)) {

		os_error *error;

		/* send DataLoadAck */
		message->action = message_DATA_LOAD_ACK;
		message->your_ref = message->my_ref;
		error = xwimp_send_message(wimp_USER_MESSAGE, message, message->sender);
		if (error) {
			LOG(("xwimp_send_message: 0x%x: %s\n",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
		return true;
	}
	return false;
}


/*
 * Helper code for the Wimp Event Handlers.
 */

/**
 * Check if a particular menu handle is a browser window menu
 *
 * \param  *menu		The menu in question.
 * \return			true if this menu is a browser window menu
 */

bool ro_gui_window_check_menu(wimp_menu *menu)
{
	return (ro_gui_browser_window_menu == menu) ? true : false;
}


/**
 * Return boolean flags to show what RISC OS types we can sensibly convert
 * the given object into.
 *
 * \TODO -- This should probably be somewhere else but in window.c, and
 *          should probably even be done in content_().
 *
 * \param *h			The object to test.
 * \param *export_draw		true on exit if a drawfile would be possible.
 * \param *export_sprite	true on exit if a sprite would be possible.
 * \return			true if valid data is returned; else false.
 */

bool ro_gui_window_content_export_types(hlcache_handle *h,
		bool *export_draw, bool *export_sprite)
{
	bool	found_type = false;

	if (export_draw != NULL)
		*export_draw = false;
	if (export_sprite != NULL)
		*export_sprite = false;

	if (h != NULL && content_get_type(h) == CONTENT_IMAGE) {
		switch (ro_content_native_type(h)) {
		case osfile_TYPE_SPRITE:
			/* bitmap types (Sprite export possible) */
			found_type = true;
			if (export_sprite != NULL)
				*export_sprite = true;
			break;
		case osfile_TYPE_DRAW:
			/* vector types (Draw export possible) */
			found_type = true;
			if (export_draw != NULL)
				*export_draw = true;
			break;
		default:
			break;
		}
	}

	return found_type;
}


/**
 * Return true if a browser window can navigate upwards.
 *
 * \TODO -- This should probably be somewhere else but in window.c.
 *
 * \param *bw		the browser window to test.
 * \return		true if navigation up is possible; else false.
 */

bool ro_gui_window_up_available(struct browser_window *bw)
{
	bool		result = false, compare;
	char		*parent;
	url_func_result	res;

	if (bw != NULL && bw->current_content != NULL) {
		res = url_parent(nsurl_access(content_get_url(
				bw->current_content)), &parent);

		if (res == URL_FUNC_OK) {
			res = url_compare(nsurl_access(content_get_url(
					bw->current_content)),
					parent, false, &compare);
			if (res == URL_FUNC_OK)
				result = !compare;
			free(parent);
		} else {
			result = false;
		}
	}

	return result;
}


/**
 * Prepare the page info window for use.
 *
 * \param *g		The GUI window block to use.
 */

void ro_gui_window_prepare_pageinfo(struct gui_window *g)
{
	hlcache_handle *h = g->bw->current_content;
	char icon_buf[20] = "file_xxx";
	char enc_buf[40];
	char enc_token[10] = "Encoding0";
	const char *icon = icon_buf;
	const char *title, *url;
	lwc_string *mime;
	const char *enc = "-";

	assert(h);

	title = content_get_title(h);
	if (title == NULL)
		title = "-";
	url = nsurl_access(content_get_url(h));
	if (url == NULL)
		url = "-";
	mime = content_get_mime_type(h);

	sprintf(icon_buf, "file_%x", ro_content_filetype(h));
	if (!ro_gui_wimp_sprite_exists(icon_buf))
		sprintf(icon_buf, "file_xxx");

	if (content_get_type(h) == CONTENT_HTML) {
		if (html_get_encoding(h)) {
			enc_token[8] = '0' + html_get_encoding_source(h);
			snprintf(enc_buf, sizeof enc_buf, "%s (%s)",
					html_get_encoding(h),
					messages_get(enc_token));
			enc = enc_buf;
		} else {
			enc = messages_get("EncodingUnk");
		}
	}

	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_ICON,
			icon, true);
	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_TITLE,
			title, true);
	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_URL,
			url, true);
	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_ENC,
			enc, true);
	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_TYPE,
			lwc_string_data(mime), true);

	lwc_string_unref(mime);
}


/**
 * Prepare the object info window for use
 *
 * \param *object	the object for which information is to be displayed
 * \param *href		corresponding href, if any
 */

void ro_gui_window_prepare_objectinfo(hlcache_handle *object, const char *href)
{
	char icon_buf[20] = "file_xxx";
	const char *url;
	lwc_string *mime;
	const char *target = "-";

	sprintf(icon_buf, "file_%.3x",
			ro_content_filetype(object));
	if (!ro_gui_wimp_sprite_exists(icon_buf))
		sprintf(icon_buf, "file_xxx");

	url = nsurl_access(content_get_url(object));
	if (url == NULL)
		url = "-";
	mime = content_get_mime_type(object);

	if (href)
		target = href;

	ro_gui_set_icon_string(dialog_objinfo, ICON_OBJINFO_ICON,
			icon_buf, true);
	ro_gui_set_icon_string(dialog_objinfo, ICON_OBJINFO_URL,
			url, true);
	ro_gui_set_icon_string(dialog_objinfo, ICON_OBJINFO_TARGET,
			target, true);
	ro_gui_set_icon_string(dialog_objinfo, ICON_OBJINFO_TYPE,
			lwc_string_data(mime), true);

	lwc_string_unref(mime);
}


/*
 * User Actions in the browser window
 */

/**
 * Launch a new url in the given window.
 *
 * \param  g	gui_window to update
 * \param  url  url to be launched
 */

void ro_gui_window_launch_url(struct gui_window *g, const char *url)
{
	char *url2;

	ro_gui_url_complete_close();

	url2 = strdup(url);
	if (url2 != NULL) {
		gui_window_set_url(g, url2);
		browser_window_go(g->bw, url2, 0, true);
		free(url2);
	}
}


/**
 * Navigate up one level
 *
 * \param  g	the gui_window to open the parent link in
 * \param  url  the URL to open the parent of
 */
bool ro_gui_window_navigate_up(struct gui_window *g, const char *url) {
	char *parent;
	url_func_result res;
	bool compare;

	if (!g || (!g->bw))
		return false;

	res = url_parent(url, &parent);
	if (res == URL_FUNC_OK) {
		res = url_compare(url, parent, false, &compare);
		if ((res == URL_FUNC_OK) && !compare)
			browser_window_go(g->bw, parent, 0, true);
		free(parent);
	}
	return true;
}


/**
 * Perform a Navigate Home action on a browser window.
 *
 * \param *g			The browser window to act on.
 */

void ro_gui_window_action_home(struct gui_window *g)
{
	char			url[80];

	if (g == NULL || g->bw == NULL)
		return;

	if ((option_homepage_url) && (option_homepage_url[0])) {
		browser_window_go(g->bw, option_homepage_url, 0, true);
	} else {
		snprintf(url, sizeof url,
				"file:///<NetSurf$Dir>/Docs/welcome/index_%s",
				option_language);
		browser_window_go(g->bw, url, 0, true);
	}
}


/**
 * Navigate back from a browser window into a new window.
 *
 * \param *g			The browser window to act on.
 */

void ro_gui_window_action_navigate_back_new(struct gui_window *g)
{
	struct browser_window	*new_bw;

	if (g == NULL || g->bw == NULL)
		return;

	new_bw = browser_window_create(NULL, g->bw, NULL, false, false);

	if (new_bw != NULL && new_bw->history != NULL)
		history_back(new_bw, new_bw->history);
}


/**
 * Navigate forward from a browser window into a new window.
 *
 * \param *g			The browser window to act on.
 */

void ro_gui_window_action_navigate_forward_new(struct gui_window *g)
{
	struct browser_window	*new_bw;

	if (g == NULL || g->bw == NULL)
		return;

	new_bw = browser_window_create(NULL, g->bw, NULL, false, false);

	if (new_bw != NULL && new_bw->history != NULL)
		history_forward(new_bw, new_bw->history);
}


/**
 * Open a new browser window.
 *
 * \param *g			The browser window to act on.
 */

void ro_gui_window_action_new_window(struct gui_window *g)
{
	if (g == NULL || g->bw == NULL || g->bw->current_content == NULL)
		return;

	browser_window_create(nsurl_access(
			content_get_url(g->bw->current_content)), g->bw,
			0, false, false);
}


/**
 * Open a local history pane for a browser window.
 *
 * \param *g			The browser window to act on.
 */

void ro_gui_window_action_local_history(struct gui_window *g)
{
	if (g != NULL && g->bw != NULL && g->bw->history != NULL)
		ro_gui_history_open(g->bw, g->bw->history, true);
}


/**
 * Open a save dialogue for a browser window contents.
 *
 * \param *g			The browser window to act on.
 * \param save_type		The type of save to open.
 */

void ro_gui_window_action_save(struct gui_window *g, gui_save_type save_type)
{
	if (g == NULL || g->bw == NULL || g->bw->current_content == NULL)
		return;

	ro_gui_save_prepare(save_type, g->bw->current_content,
			NULL, NULL, NULL);
	ro_gui_dialog_open_persistent(g->window, dialog_saveas, true);
}


/**
 * Open a text search dialogue for a browser window.
 *
 * \param *g			The browser window to act on.
 */

void ro_gui_window_action_search(struct gui_window *g)
{
	if (g == NULL || g->bw == NULL || g->bw->current_content == NULL ||
			(content_get_type(g->bw->current_content) !=
				CONTENT_HTML &&
			content_get_type(g->bw->current_content) !=
				CONTENT_TEXTPLAIN))
		return;

	ro_gui_search_prepare(g->bw);
	ro_gui_dialog_open_persistent(g->window, dialog_search, true);
}


/**
 * Open a zoom dialogue for a browser window.
 *
 * \param *g			The browser window to act on.
 */

void ro_gui_window_action_zoom(struct gui_window *g)
{
	if (g == NULL)
		return;

	ro_gui_dialog_prepare_zoom(g);
	ro_gui_dialog_open_persistent(g->window, dialog_zoom, true);
}


/**
 * Add a hotlist entry for a browser window.
 *
 * \param *g			The browser window to act on.
 */

void ro_gui_window_action_add_bookmark(struct gui_window *g)
{
	if (g == NULL || g->bw == NULL || g->bw->current_content == NULL ||
			content_get_url(g->bw->current_content) == NULL)
		return;

	ro_gui_hotlist_add_page(nsurl_access(content_get_url(g->bw->current_content)));
}


/**
 * Open a print dialogue for a browser window.
 *
 * \param *g			The browser window to act on.
 */

void ro_gui_window_action_print(struct gui_window *g)
{
	if (g == NULL)
		return;

	ro_gui_print_prepare(g);
	ro_gui_dialog_open_persistent(g->window, dialog_print, true);
}


/**
 * Open a page info box for a browser window.
 *
 * \param *g			The browser window to act on.
 */

void ro_gui_window_action_page_info(struct gui_window *g)
{
	if (g == NULL || g->bw == NULL || g->bw->current_content == NULL)
		return;

	ro_gui_window_prepare_pageinfo(g);
	ro_gui_dialog_open_persistent(g->window, dialog_pageinfo, false);
}


/*
 * Window and Toolbar Redraw and Update
 */

/**
 * Redraws the content for all windows.
 */

void ro_gui_window_redraw_all(void)
{
	struct gui_window *g;
	for (g = window_list; g; g = g->next)
		gui_window_redraw_window(g);
}


/**
 * Remove all pending update boxes for a window
 *
 * \param  g   gui_window
 */
void ro_gui_window_remove_update_boxes(struct gui_window *g)
{
	struct update_box *cur;

	for (cur = pending_updates; cur != NULL; cur = cur->next) {
		if (cur->g == g)
			cur->g = NULL;
	}
}


/**
 * Redraw any pending update boxes.
 */
void ro_gui_window_update_boxes(void)
{
	osbool more;
	bool use_buffer;
	wimp_draw update;
	struct rect clip;
	os_error *error;
	struct update_box *cur;
	struct gui_window *g;
	struct redraw_context ctx = {
		.interactive = true,
		.plot = &ro_plotters
	};

	for (cur = pending_updates; cur != NULL; cur = cur->next) {
		g = cur->g;
		if (!g)
			continue;

		use_buffer = cur->use_buffer;

		update.w = g->window;
		update.box.x0 = cur->x0;
		update.box.y0 = cur->y0;
		update.box.x1 = cur->x1;
		update.box.y1 = cur->y1;

		error = xwimp_update_window(&update, &more);
		if (error) {
			LOG(("xwimp_update_window: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			continue;
		}

		/* Set the current redraw gui_window to get options from */
		ro_gui_current_redraw_gui = g;

		ro_plot_origin_x = update.box.x0 - update.xscroll;
		ro_plot_origin_y = update.box.y1 - update.yscroll;

		while (more) {
			clip.x0 = (update.clip.x0 - ro_plot_origin_x) / 2;
			clip.y0 = (ro_plot_origin_y - update.clip.y1) / 2;
			clip.x1 = (update.clip.x1 - ro_plot_origin_x) / 2;
			clip.y1 = (ro_plot_origin_y - update.clip.y0) / 2;

			if (use_buffer)
				ro_gui_buffer_open(&update);

			browser_window_redraw(g->bw, 0, 0, &clip, &ctx);

			if (use_buffer)
				ro_gui_buffer_close();

			error = xwimp_get_rectangle(&update, &more);
			/* RISC OS 3.7 returns an error here if enough buffer
			 * was claimed to cause a new dynamic area to be
			 * created. It doesn't actually stop anything working,
			 * so we mask it out for now until a better fix is
			 * found. This appears to be a bug in RISC OS. */
			if (error && !(use_buffer &&
					error->errnum == error_WIMP_GET_RECT)) {
				LOG(("xwimp_get_rectangle: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
				ro_gui_current_redraw_gui = NULL;
				continue;
			}
		}

		/* Reset the current redraw gui_window to prevent
		 * thumbnails from retaining options */
		ro_gui_current_redraw_gui = NULL;
	}
	while (pending_updates) {
		cur = pending_updates;
		pending_updates = pending_updates->next;
		free(cur);
	}
}


/**
 * Process pending reformats
 */

void ro_gui_window_process_reformats(void)
{
	struct gui_window *g;

	browser_reformat_pending = false;
	for (g = window_list; g; g = g->next) {
		if (!g->bw->reformat_pending)
			continue;
		g->bw->reformat_pending = false;
		browser_window_reformat(g->bw, false,
				g->old_width / 2,
				g->old_height / 2);
	}
}


/**
 * Destroy all browser windows.
 */

void ro_gui_window_quit(void)
{
	struct gui_window *cur;

	while (window_list) {
		cur = window_list;
		window_list = window_list->next;

		browser_window_destroy(cur->bw);
	}
}


/**
 * Animate the "throbbers" of all browser windows.
 */

void ro_gui_throb(void)
{
	struct gui_window	*g;
	struct browser_window	*top;

	for (g = window_list; g; g = g->next) {
		if (!g->bw->throbbing)
			continue;
		for (top = g->bw; top->parent; top = top->parent);
		if (top->window != NULL && top->window->toolbar != NULL)
			ro_toolbar_throb(top->window->toolbar);
	}
}


/**
 * Update the toolbar buttons for a given browser window to reflect the
 * current state of its contents.
 *
 * Note that the parameters to this function are arranged so that it can be
 * supplied to the toolbar module as an button state update callback.
 *
 * \param *g			The browser window to update.
 */

void ro_gui_window_update_toolbar_buttons(struct gui_window *g)
{
	struct browser_window	*bw;
	hlcache_handle		*h;
	struct toolbar		*toolbar;

	if (g == NULL || g->toolbar == NULL)
		return;

	bw = g->bw;
	h = bw->current_content;
	toolbar = g->toolbar;

	ro_toolbar_set_button_shaded_state(toolbar, TOOLBAR_BUTTON_RELOAD,
			!browser_window_reload_available(bw));

	ro_toolbar_set_button_shaded_state(toolbar, TOOLBAR_BUTTON_STOP,
			!browser_window_stop_available(bw));

	ro_toolbar_set_button_shaded_state(toolbar, TOOLBAR_BUTTON_BACK,
			!browser_window_back_available(bw));

	ro_toolbar_set_button_shaded_state(toolbar, TOOLBAR_BUTTON_FORWARD,
			!browser_window_forward_available(bw));

	ro_toolbar_set_button_shaded_state(toolbar, TOOLBAR_BUTTON_UP,
			!ro_gui_window_up_available(bw));

	ro_toolbar_set_button_shaded_state(toolbar, TOOLBAR_BUTTON_SEARCH,
			h == NULL || (content_get_type(h) != CONTENT_HTML &&
				content_get_type(h) != CONTENT_TEXTPLAIN));

	ro_toolbar_set_button_shaded_state(toolbar, TOOLBAR_BUTTON_SCALE,
			h == NULL);

	ro_toolbar_set_button_shaded_state(toolbar, TOOLBAR_BUTTON_PRINT,
			h == NULL);

	ro_toolbar_set_button_shaded_state(toolbar, TOOLBAR_BUTTON_SAVE_SOURCE,
			h == NULL);

	ro_toolbar_update_urlsuggest(toolbar);
}


/**
 * Update a window to reflect a change in toolbar size: used as a callback by
 * the toolbar module when a toolbar height changes.
 *
 * \param *data			void pointer the window's gui_window struct
 */

void ro_gui_window_update_toolbar(void *data)
{
	struct gui_window *g = (struct gui_window *) data;

	if (g != NULL)
		gui_window_update_extent(g);
}


/**
 * Save a new toolbar button configuration: used as a callback by the toolbar
 * module when a buttonbar edit has finished.
 *
 * \param *data			void pointer to the window's gui_window struct
 * \param *config		pointer to a malloc()'d button config string.
 */

void ro_gui_window_save_toolbar_buttons(void *data, char *config)
{
	if (option_toolbar_browser != NULL)
		free(option_toolbar_browser);
	option_toolbar_browser = config;
	ro_gui_save_options();
}


/**
 * Update a window and its toolbar to reflect a new theme: used as a callback
 * by the toolbar module when a theme change affects a toolbar.
 *
 * \param *data			void pointer to the window's gui_window struct
 * \param ok			true if the bar still exists; else false.
 */

void ro_gui_window_update_theme(void *data, bool ok)
{
	struct gui_window *g = (struct gui_window *) data;

	if (g != NULL && g->toolbar != NULL) {
		if (ok) {
			gui_window_update_extent(g);
		} else {
			g->toolbar = NULL;
		}
	}
}


/*
 * General Window Support
 */

/**
 * Import text file into window or its toolbar
 *
 * \param  g	     gui window containing textarea
 * \param  filename  pathname of file to be imported
 * \param  toolbar   true iff imported to toolbar rather than main window
 * \return true iff successful
 */

bool ro_gui_window_import_text(struct gui_window *g, const char *filename,
		bool toolbar)
{
	fileswitch_object_type obj_type;
	os_error *error;
	char *buf, *utf8_buf;
	int size;
	utf8_convert_ret ret;

	error = xosfile_read_stamped(filename, &obj_type, NULL, NULL,
			&size, NULL, NULL);
	if (error) {
		LOG(("xosfile_read_stamped: 0x%x:%s",
			error->errnum, error->errmess));
		warn_user("FileError", error->errmess);
		return true;  /* was for us, but it didn't work! */
	}

	buf = malloc(size);
	if (!buf) {
		warn_user("NoMemory", NULL);
		return true;
	}

	error = xosfile_load_stamped(filename, (byte*)buf,
			NULL, NULL, NULL, NULL, NULL);
	if (error) {
		LOG(("xosfile_load_stamped: 0x%x:%s",
			error->errnum, error->errmess));
		warn_user("LoadError", error->errmess);
		free(buf);
		return true;
	}

	ret = utf8_from_local_encoding(buf, size, &utf8_buf);
	if (ret != UTF8_CONVERT_OK) {
		/* bad encoding shouldn't happen */
		assert(ret != UTF8_CONVERT_BADENC);
		LOG(("utf8_from_local_encoding failed"));
		free(buf);
		warn_user("NoMemory", NULL);
		return true;
	}
	size = strlen(utf8_buf);

	if (toolbar) {
		const char *ep = utf8_buf + size;
		const char *sp;
		char *p = utf8_buf;

		/* skip leading whitespace */
		while (isspace(*p)) p++;

		sp = p;
		while (*p && *p != '\r' && *p != '\n')
			p += utf8_next(p, ep - p, 0);
		*p = '\0';

		if (p > sp)
			ro_gui_window_launch_url(g, sp);
	}
	else
		browser_window_paste_text(g->bw, utf8_buf, size, true);

	free(buf);
	free(utf8_buf);
	return true;
}


/**
 * Clones a browser window's options.
 *
 * \param  new_bw  the new browser window
 * \param  old_bw  the browser window to clone from, or NULL for default
 */

void ro_gui_window_clone_options(struct browser_window *new_bw,
		struct browser_window *old_bw)
{
	struct gui_window *old_gui = NULL;
	struct gui_window *new_gui;

	assert(new_bw);

	/*	Get our GUIs
	*/
	new_gui = new_bw->window;

	if (old_bw)
		old_gui = old_bw->window;

	/*	Clone the basic options
	*/
	if (!old_gui) {
		new_bw->scale = ((float)option_scale) / 100;
		new_gui->option.background_images = option_background_images;
		new_gui->option.buffer_animations = option_buffer_animations;
		new_gui->option.buffer_everything = option_buffer_everything;
	} else {
		new_gui->option = old_gui->option;
	}

	/*	Set up the toolbar
	*/
	if (new_gui->toolbar) {
		ro_toolbar_set_display_buttons(new_gui->toolbar,
				option_toolbar_show_buttons);
		ro_toolbar_set_display_url(new_gui->toolbar,
				option_toolbar_show_address);
		ro_toolbar_set_display_throbber(new_gui->toolbar,
				option_toolbar_show_throbber);
		if ((old_gui) && (old_gui->toolbar)) {
			ro_toolbar_set_display_buttons(new_gui->toolbar,
					ro_toolbar_get_display_buttons(
						old_gui->toolbar));
			ro_toolbar_set_display_url(new_gui->toolbar,
					ro_toolbar_get_display_url(
						old_gui->toolbar));
			ro_toolbar_set_display_throbber(new_gui->toolbar,
					ro_toolbar_get_display_throbber(
						old_gui->toolbar));
			ro_toolbar_process(new_gui->toolbar, -1, true);
		}
	}
}


/**
 * Makes a browser window's options the default.
 *
 * \param  bw  the browser window to read options from
 */

void ro_gui_window_default_options(struct browser_window *bw)
{
	struct gui_window *gui;

	assert(bw);

	/*	Get our GUI
	*/
	gui = bw->window;
	if (!gui) return;

	/*	Save the basic options
	*/
	option_scale = bw->scale * 100;
	option_buffer_animations = gui->option.buffer_animations;
	option_buffer_everything = gui->option.buffer_everything;

	/*	Set up the toolbar
	*/
	if (gui->toolbar != NULL) {
		option_toolbar_show_buttons =
				ro_toolbar_get_display_buttons(gui->toolbar);
		option_toolbar_show_address =
				ro_toolbar_get_display_url(gui->toolbar);
		option_toolbar_show_throbber =
				ro_toolbar_get_display_throbber(gui->toolbar);
	}
	if (gui->status_bar != NULL)
		option_toolbar_status_width =
				ro_gui_status_bar_get_width(gui->status_bar);
}


/*
 * Custom Menu Support
 */

/**
 * Prepare or reprepare a form select menu, setting up the menu handle
 * global in the process.
 *
 * \param *bw		The browser window to contain the menu.
 * \param *control	The form control needing a menu.
 * \return		true if the menu is OK to be opened; else false.
 */

bool ro_gui_window_prepare_form_select_menu(struct browser_window *bw,
		struct form_control *control)
{
	unsigned int i, entries;
	char *text_convert, *temp;
	struct form_option *option;
	bool reopen = true;
	utf8_convert_ret err;

	assert(control);

	for (entries = 0, option = control->data.select.items; option;
			option = option->next)
		entries++;
	if (entries == 0) {
		ro_gui_menu_closed();
		return false;
	}

	if ((gui_form_select_menu) && (control != gui_form_select_control)) {
		for (i = 0; ; i++) {
			free(gui_form_select_menu->entries[i].data.
					indirected_text.text);
			if (gui_form_select_menu->entries[i].menu_flags &
					wimp_MENU_LAST)
				break;
		}
		free(gui_form_select_menu->title_data.indirected_text.text);
		free(gui_form_select_menu);
		gui_form_select_menu = 0;
	}

	if (!gui_form_select_menu) {
		reopen = false;
		gui_form_select_menu = malloc(wimp_SIZEOF_MENU(entries));
		if (!gui_form_select_menu) {
			warn_user("NoMemory", 0);
			ro_gui_menu_closed();
			return false;
		}
		err = utf8_to_local_encoding(messages_get("SelectMenu"), 0,
				&text_convert);
		if (err != UTF8_CONVERT_OK) {
			/* badenc should never happen */
			assert(err != UTF8_CONVERT_BADENC);
			LOG(("utf8_to_local_encoding failed"));
			warn_user("NoMemory", 0);
			ro_gui_menu_closed();
			return false;
		}
		gui_form_select_menu->title_data.indirected_text.text =
				text_convert;
		ro_gui_menu_init_structure(gui_form_select_menu, entries);
	}

	for (i = 0, option = control->data.select.items; option;
			i++, option = option->next) {
		gui_form_select_menu->entries[i].menu_flags = 0;
		if (option->selected)
			gui_form_select_menu->entries[i].menu_flags =
					wimp_MENU_TICKED;
		if (!reopen) {

			/* convert spaces to hard spaces to stop things
			 * like 'Go Home' being treated as if 'Home' is a
			 * keyboard shortcut and right aligned in the menu.
			 */

			temp = cnv_space2nbsp(option->text);
			if (!temp) {
				LOG(("cnv_space2nbsp failed"));
				warn_user("NoMemory", 0);
				ro_gui_menu_closed();
				return false;
			}

			err = utf8_to_local_encoding(temp,
				0, &text_convert);
			if (err != UTF8_CONVERT_OK) {
				/* A bad encoding should never happen,
				 * so assert this */
				assert(err != UTF8_CONVERT_BADENC);
				LOG(("utf8_to_enc failed"));
				warn_user("NoMemory", 0);
				ro_gui_menu_closed();
				return false;
			}

			free(temp);

			gui_form_select_menu->entries[i].data.indirected_text.text =
					text_convert;
			gui_form_select_menu->entries[i].data.indirected_text.size =
					strlen(gui_form_select_menu->entries[i].
					data.indirected_text.text) + 1;
		}
	}

	gui_form_select_menu->entries[0].menu_flags |=
			wimp_MENU_TITLE_INDIRECTED;
	gui_form_select_menu->entries[i - 1].menu_flags |= wimp_MENU_LAST;

	return true;
}

/**
 * Process selections from a form select menu, passing them back to the core.
 *
 * \param *g		The browser window affected by the menu.
 * \param *selection	The menu selection.
 */

void ro_gui_window_process_form_select_menu(struct gui_window *g,
		wimp_selection *selection)
{
	assert(g != NULL);

	if (selection->items[0] >= 0)
		form_select_process_selection(g->bw->current_content,
				gui_form_select_control,
				selection->items[0]);
}


/*
 * Window and Toolbar Lookup
 */

/**
 * Convert a RISC OS window handle to a gui_window.
 *
 * \param  w  RISC OS window handle
 * \return  pointer to a structure if found, 0 otherwise
 */

struct gui_window *ro_gui_window_lookup(wimp_w window)
{
	struct gui_window *g;
	for (g = window_list; g; g = g->next)
		if (g->window == window)
			return g;
	return 0;
}


/**
 * Convert a toolbar RISC OS window handle to a gui_window.
 *
 * \param  w		RISC OS window handle of a toolbar
 * \return		pointer to a structure if found, 0 otherwise
 */

struct gui_window *ro_gui_toolbar_lookup(wimp_w window)
{
	struct gui_window	*g = NULL;
	struct toolbar		*toolbar;
	wimp_w			parent;

	toolbar = ro_toolbar_window_lookup(window);

	if (toolbar != NULL) {
		parent = ro_toolbar_get_parent_window(toolbar);
		g = ro_gui_window_lookup(parent);
	}

	return g;
}


/*
 * Core to RISC OS Conversions
 */

/**
 * Convert x,y screen co-ordinates into window co-ordinates.
 *
 * \param  g	gui window
 * \param  x	x ordinate
 * \param  y	y ordinate
 * \param  pos  receives position in window co-ordinatates
 * \return true iff conversion successful
 */

bool ro_gui_window_to_window_pos(struct gui_window *g, int x, int y,
		os_coord *pos)
{
	wimp_window_state state;
	os_error *error;

	assert(g);

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x:%s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}
	pos->x = (x - (state.visible.x0 - state.xscroll)) / 2 / g->bw->scale;
	pos->y = ((state.visible.y1 - state.yscroll) - y) / 2 / g->bw->scale;
	return true;
}


/**
 * Convert x,y window co-ordinates into screen co-ordinates.
 *
 * \param  g	gui window
 * \param  x	x ordinate
 * \param  y	y ordinate
 * \param  pos  receives position in screen co-ordinatates
 * \return true iff conversion successful
 */

bool ro_gui_window_to_screen_pos(struct gui_window *g, int x, int y,
		os_coord *pos)
{
	wimp_window_state state;
	os_error *error;

	assert(g);

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x:%s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}
	pos->x = (x * 2 * g->bw->scale) + (state.visible.x0 - state.xscroll);
	pos->y = (state.visible.y1 - state.yscroll) - (y * 2 * g->bw->scale);
	return true;
}


/*
 * Miscellaneous Functions
 *
 * \TODO -- These items might well belong elsewhere.
 */

/**
 * Returns the state of the mouse buttons and modifiers keys for a
 * mouse action, suitable for passing to the OS-independent
 * browser window/ treeview/ etc code.
 *
 * \param  buttons		Wimp button state.
 * \param  type			Wimp work-area/icon type for decoding.
 * \return			NetSurf core button state.
 */

browser_mouse_state ro_gui_mouse_click_state(wimp_mouse_state buttons,
		wimp_icon_flags type)
{
	browser_mouse_state state = 0; /* Blank state with nothing set */

	switch (type) {
	case wimp_BUTTON_CLICK_DRAG:		/* Used for browser window */
		/* Handle single clicks. */

		/* We fire core PRESS and CLICK events together for "action on
		 * press" behaviour. */
		if (buttons & (wimp_CLICK_SELECT)) /* Select click */
			state |= BROWSER_MOUSE_PRESS_1 | BROWSER_MOUSE_CLICK_1;
		if (buttons & (wimp_CLICK_ADJUST)) /* Adjust click */
			state |= BROWSER_MOUSE_PRESS_2 | BROWSER_MOUSE_CLICK_2;
		break;
	case wimp_BUTTON_DOUBLE_CLICK_DRAG:	/* Used for treeview window */
		/* Handle single and double clicks. */

		/* Single clicks: Fire PRESS and CLICK events together
		 * for "action on press" behaviour. */
		if (buttons & (wimp_SINGLE_SELECT)) /* Select single click */
			state |= BROWSER_MOUSE_PRESS_1 | BROWSER_MOUSE_CLICK_1;
		if (buttons & (wimp_SINGLE_ADJUST)) /* Adjust single click */
			state |= BROWSER_MOUSE_PRESS_2 | BROWSER_MOUSE_CLICK_2;

		/* Double clicks: Fire PRESS, CLICK, and DOUBLE_CLICK
		 * events together for "action on 2nd press" behaviour. */
		if (buttons & (wimp_DOUBLE_SELECT)) /* Select double click */
			state |= BROWSER_MOUSE_PRESS_1 | BROWSER_MOUSE_CLICK_1 |
					BROWSER_MOUSE_DOUBLE_CLICK;
		if (buttons & (wimp_DOUBLE_ADJUST)) /* Adjust double click */
			state |= BROWSER_MOUSE_PRESS_2 | BROWSER_MOUSE_CLICK_2 |
					BROWSER_MOUSE_DOUBLE_CLICK;
		break;
	}

	/* Check if a drag has started */
	if (buttons & (wimp_DRAG_SELECT)) {
		/* A drag was _started_ with Select; Fire DRAG_1. */
		state |= BROWSER_MOUSE_DRAG_1;
		mouse_drag_select = true;
	}
	if (buttons & (wimp_DRAG_ADJUST)) {
		/* A drag was _started_ with Adjust; Fire DRAG_2. */
		state |= BROWSER_MOUSE_DRAG_2;
		mouse_drag_adjust = true;
	}

	/* Set modifier key state */
	if (ro_gui_shift_pressed()) state |= BROWSER_MOUSE_MOD_1;
	if (ro_gui_ctrl_pressed())  state |= BROWSER_MOUSE_MOD_2;
	if (ro_gui_alt_pressed())   state |= BROWSER_MOUSE_MOD_3;

	return state;
}


/**
 * Returns the state of the mouse buttons and modifiers keys whilst
 * dragging, for passing to the OS-independent browser window/ treeview/
 * etc code
 *
 * \param  buttons		Wimp button state.
 * \param  type			Wimp work-area/icon type for decoding.
 * \return			NetSurf core button state.
 */

browser_mouse_state ro_gui_mouse_drag_state(wimp_mouse_state buttons,
		wimp_icon_flags type)
{
	browser_mouse_state state = 0; /* Blank state with nothing set */

	/* If mouse buttons aren't held, turn off drags */
	if (!(buttons & (wimp_CLICK_SELECT) || buttons & (wimp_CLICK_ADJUST))) {
		mouse_drag_select = false;
		mouse_drag_adjust = false;
	}

	/* If there's a drag happening, set DRAG_ON and record which button
	 * the drag is happening with, i.e. HOLDING_1 or HOLDING_2 */
	if (mouse_drag_select) {
		state |= BROWSER_MOUSE_DRAG_ON | BROWSER_MOUSE_HOLDING_1;
	}
	if (mouse_drag_adjust) {
		state |= BROWSER_MOUSE_DRAG_ON | BROWSER_MOUSE_HOLDING_2;
	}

	/* Set modifier key state */
	if (ro_gui_shift_pressed()) state |= BROWSER_MOUSE_MOD_1;
	if (ro_gui_ctrl_pressed())  state |= BROWSER_MOUSE_MOD_2;
	if (ro_gui_alt_pressed())   state |= BROWSER_MOUSE_MOD_3;

	return state;
}


/**
 * Returns true iff one or more Shift keys is held down
 */

bool ro_gui_shift_pressed(void)
{
	int shift = 0;
	xosbyte1(osbyte_SCAN_KEYBOARD, 0 ^ 0x80, 0, &shift);
	return (shift == 0xff);
}


/**
 * Returns true iff one or more Ctrl keys is held down
 */

bool ro_gui_ctrl_pressed(void)
{
	int ctrl = 0;
	xosbyte1(osbyte_SCAN_KEYBOARD, 1 ^ 0x80, 0, &ctrl);
	return (ctrl == 0xff);
}


/**
 * Returns true iff one or more Alt keys is held down
 */

bool ro_gui_alt_pressed(void)
{
	int alt = 0;
	xosbyte1(osbyte_SCAN_KEYBOARD, 2 ^ 0x80, 0, &alt);
	return (alt == 0xff);
}

