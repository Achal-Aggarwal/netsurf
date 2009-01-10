/*
 * Copyright 2004-2007 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
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
 * Save dialog and drag and drop saving (implementation).
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "oslib/dragasprite.h"
#include "oslib/osbyte.h"
#include "oslib/osfile.h"
#include "oslib/osmodule.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "desktop/netsurf.h"
#include "desktop/save_text.h"
#include "desktop/selection.h"
#include "image/bitmap.h"
#include "render/box.h"
#include "render/form.h"
#include "riscos/dialog.h"
#include "riscos/gui.h"
#include "riscos/menus.h"
#include "riscos/message.h"
#include "riscos/options.h"
#include "riscos/query.h"
#include "riscos/save.h"
#include "riscos/save_complete.h"
#include "riscos/save_draw.h"
#include "riscos/save_pdf.h"
#include "riscos/textselection.h"
#include "riscos/thumbnail.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utf8.h"
#include "utils/utils.h"

//typedef enum
//{
//	QueryRsn_Quit,
//	QueryRsn_Abort,
//	QueryRsn_Overwrite
//} query_reason;


/**todo - much of the state information for a save should probably be moved into a structure
          now since we could have multiple saves outstanding */

static gui_save_type gui_save_current_type;
static struct content *gui_save_content = NULL;
static struct selection *gui_save_selection = NULL;
static int gui_save_filetype;
static query_id gui_save_query;
static bool gui_save_send_dataload;
static wimp_message gui_save_message;
static bool gui_save_close_after = true;

static bool dragbox_active = false;  /** in-progress Wimp_DragBox/DragASprite op */
static bool using_dragasprite = true;
static bool saving_from_dialog = true;
static osspriteop_area *saveas_area = NULL;
static wimp_w gui_save_sourcew = (wimp_w)-1;
#define LEAFNAME_MAX 200
static char save_leafname[LEAFNAME_MAX];

/** Current save directory (updated by and used for dialog-based saving) */
static char *save_dir = NULL;
static size_t save_dir_len;

typedef enum { LINK_ACORN, LINK_ANT, LINK_TEXT } link_format;

static bool ro_gui_save_complete(struct content *c, char *path);
static bool ro_gui_save_content(struct content *c, char *path, bool force_overwrite);
static void ro_gui_save_done(void);
static void ro_gui_save_bounced(wimp_message *message);
static void ro_gui_save_object_native(struct content *c, char *path);
static bool ro_gui_save_link(struct content *c, link_format format, char *path);
static void ro_gui_save_set_state(struct content *c, gui_save_type save_type,
		char *leaf_buf, char *icon_buf);
static bool ro_gui_save_create_thumbnail(struct content *c, const char *name);
static void ro_gui_save_overwrite_confirmed(query_id, enum query_response res, void *p);
static void ro_gui_save_overwrite_cancelled(query_id, enum query_response res, void *p);

static const query_callback overwrite_funcs =
{
	ro_gui_save_overwrite_confirmed,
	ro_gui_save_overwrite_cancelled
};


/** An entry in gui_save_table. */
struct gui_save_table_entry {
	int filetype;
	const char *name;
};

/** Table of filetypes and default filenames. Must be in sync with
 * gui_save_type (riscos/gui.h). A filetype of 0 indicates the content should
 * be used.
 */
static const struct gui_save_table_entry gui_save_table[] = {
	/* GUI_SAVE_SOURCE,              */ {     0, "SaveSource" },
	/* GUI_SAVE_DRAW,                */ { 0xaff, "SaveDraw" },
	/* GUI_SAVE_PDF,                 */ { 0xadf, "SavePDF" },
	/* GUI_SAVE_TEXT,                */ { 0xfff, "SaveText" },
	/* GUI_SAVE_COMPLETE,            */ { 0xfaf, "SaveComplete" },
	/* GUI_SAVE_OBJECT_ORIG,         */ {     0, "SaveObject" },
	/* GUI_SAVE_OBJECT_NATIVE,       */ { 0xff9, "SaveObject" },
	/* GUI_SAVE_LINK_URI,            */ { 0xf91, "SaveLink" },
	/* GUI_SAVE_LINK_URL,            */ { 0xb28, "SaveLink" },
	/* GUI_SAVE_LINK_TEXT,           */ { 0xfff, "SaveLink" },
	/* GUI_SAVE_HOTLIST_EXPORT_HTML, */ { 0xfaf, "Hotlist" },
	/* GUI_SAVE_HISTORY_EXPORT_HTML, */ { 0xfaf, "History" },
	/* GUI_SAVE_TEXT_SELECTION,      */ { 0xfff, "SaveSelection" },
};


/**
 * Create the saveas dialogue from the given template, and the sprite area
 * necessary for our thumbnail (full page save)
 *
 * \param  template_name  name of template to be used
 * \return window handle of created dialogue
 */

wimp_w ro_gui_saveas_create(const char *template_name)
{
	const int sprite_size = (68 * 68 * 4) + ((68 * 68) / 8);  /* 32bpp with mask */
	int area_size = sizeof(osspriteop_area) + sizeof(osspriteop_header) +
			256 * 8 + sprite_size;
	wimp_window *window;
	os_error *error;
	wimp_icon *icons;
	wimp_w w;

	window = ro_gui_dialog_load_template(template_name);
	assert(window);

	icons = window->icons;

	error = xosmodule_alloc(area_size, (void**)&saveas_area);
	if (error) {
		LOG(("xosmodule_alloc: 0x%x: %s", error->errnum, error->errmess));
		xwimp_close_template();
		die(error->errmess);
	}
	else {
		saveas_area->size = area_size;
		saveas_area->first = 16;

		error = xosspriteop_clear_sprites(osspriteop_USER_AREA, saveas_area);
		if (error) {
			LOG(("xosspriteop_clear_sprites: 0x%x: %s",
				error->errnum, error->errmess));
			warn_user("MiscError", error->errmess);

			xosmodule_free(saveas_area);
			saveas_area = NULL;
		}
	}

	assert((icons[ICON_SAVE_ICON].flags &
		(wimp_ICON_TEXT | wimp_ICON_SPRITE | wimp_ICON_INDIRECTED)) ==
		(wimp_ICON_SPRITE | wimp_ICON_INDIRECTED));
	icons[ICON_SAVE_ICON].data.indirected_sprite.area = saveas_area;

	/* create window */
	error = xwimp_create_window(window, &w);
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		xwimp_close_template();
		die(error->errmess);
	}

	/* the window definition is copied by the wimp and may be freed */
	free(window);

	return w;
}


/**
 * Clean-up function that releases our sprite area and memory.
 */

void ro_gui_saveas_quit(void)
{
	if (saveas_area) {
		os_error *error = xosmodule_free(saveas_area);
		if (error) {
			LOG(("xosmodule_free: 0x%x: %s", error->errnum, error->errmess));
			warn_user("MiscError", error->errmess);
		}
		saveas_area = NULL;
	}

	free(save_dir);
	save_dir = NULL;
}

/**
 * Prepares the save box to reflect gui_save_type and a content, and
 * opens it.
 *
 * \param  save_type  type of save
 * \param  c          content to save
 * \param  sub_menu   open dialog as a sub menu, otherwise persistent
 * \param  x          x position, for sub_menu true only
 * \param  y          y position, for sub_menu true only
 * \param  parent     parent window for persistent box, for sub_menu false only
 */

void ro_gui_save_prepare(gui_save_type save_type, struct content *c)
{
	char name_buf[FILENAME_MAX];
	size_t leaf_offset = 0;
	char icon_buf[20];

	assert((save_type == GUI_SAVE_HOTLIST_EXPORT_HTML) ||
			(save_type == GUI_SAVE_HISTORY_EXPORT_HTML) || c);

	gui_save_current_type = save_type;
	gui_save_content = c;

	if (save_dir) {
		leaf_offset = save_dir_len;
		memcpy(name_buf, save_dir, leaf_offset);
		name_buf[leaf_offset++] = '.';
	}

	ro_gui_save_set_state(c, save_type, name_buf + leaf_offset, icon_buf);

	ro_gui_set_icon_sprite(dialog_saveas, ICON_SAVE_ICON, saveas_area,
			icon_buf);

	ro_gui_set_icon_string(dialog_saveas, ICON_SAVE_PATH, name_buf, true);
	ro_gui_wimp_event_memorise(dialog_saveas);
}

/**
 * Starts a drag for the save dialog
 *
 * \param  pointer  mouse position info from Wimp
 */
void ro_gui_save_start_drag(wimp_pointer *pointer)
{
	if (pointer->buttons & (wimp_DRAG_SELECT | wimp_DRAG_ADJUST)) {
		const char *sprite = ro_gui_get_icon_string(pointer->w, pointer->i);
		int x = pointer->pos.x, y = pointer->pos.y;
		wimp_window_state wstate;
		wimp_icon_state istate;
		/* start the drag from the icon's exact location, rather than the pointer */
		istate.w = wstate.w = pointer->w;
		istate.i = pointer->i;
		if (!xwimp_get_window_state(&wstate) && !xwimp_get_icon_state(&istate)) {
			x = (istate.icon.extent.x1 + istate.icon.extent.x0)/2 +
					wstate.visible.x0 - wstate.xscroll;
			y = (istate.icon.extent.y1 + istate.icon.extent.y0)/2 +
					wstate.visible.y1 - wstate.yscroll;
		}
		gui_current_drag_type = GUI_DRAG_SAVE;
		gui_save_sourcew = pointer->w;
		saving_from_dialog = true;
		gui_save_close_after = !(pointer->buttons & wimp_DRAG_ADJUST);
		ro_gui_drag_icon(x, y, sprite);
	}
}


/**
 * Handle OK click/keypress in the save dialog.
 *
 * \param  w  window handle of save dialog
 * \return true on success, false on failure
 */
bool ro_gui_save_ok(wimp_w w)
{
	const char *name = ro_gui_get_icon_string(w, ICON_SAVE_PATH);
	wimp_pointer pointer;
	char path[256];

	if (!strrchr(name, '.')) {
		warn_user("NoPathError", NULL);
		return false;
	}

	ro_gui_convert_save_path(path, sizeof path, name);
	gui_save_sourcew = w;
	saving_from_dialog = true;
	gui_save_close_after = xwimp_get_pointer_info(&pointer)
						|| !(pointer.buttons & wimp_CLICK_ADJUST);
	if (!ro_gui_save_content(gui_save_content, path, !option_confirm_overwrite)) {
		memcpy(&gui_save_message.data.data_xfer.file_name, path, 1 + strlen(path));
		gui_save_send_dataload = false;
		return false;
	}
	return true;
}


/**
 * Initiates drag saving of an object directly from a browser window
 *
 * \param  save_type  type of save
 * \param  c          content to save
 * \param  g          gui window
 */

void gui_drag_save_object(gui_save_type save_type, struct content *c,
		struct gui_window *g)
{
	wimp_pointer pointer;
	char icon_buf[20];
	os_error *error;

	/* Close the save window because otherwise we need two contexts
	*/
	xwimp_create_menu(wimp_CLOSE_MENU, 0, 0);
	ro_gui_dialog_close(dialog_saveas);

	gui_save_sourcew = g->window;
	saving_from_dialog = false;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	ro_gui_save_set_state(c, save_type, save_leafname, icon_buf);

	gui_current_drag_type = GUI_DRAG_SAVE;

	ro_gui_drag_icon(pointer.pos.x, pointer.pos.y, icon_buf);
}


/**
 * Initiates drag saving of a selection from a browser window
 *
 * \param  s  selection object
 * \param  g  gui window
 */

void gui_drag_save_selection(struct selection *s, struct gui_window *g)
{
	wimp_pointer pointer;
	char icon_buf[20];
	os_error *error;

	/* Close the save window because otherwise we need two contexts
	*/
	xwimp_create_menu(wimp_CLOSE_MENU, 0, 0);
	ro_gui_dialog_close(dialog_saveas);

	gui_save_sourcew = g->window;
	saving_from_dialog = false;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	gui_save_selection = s;

	ro_gui_save_set_state(NULL, GUI_SAVE_TEXT_SELECTION, save_leafname,
			icon_buf);

	gui_current_drag_type = GUI_DRAG_SAVE;

	ro_gui_drag_icon(pointer.pos.x, pointer.pos.y, icon_buf);
}


/**
 * Start drag of icon under the pointer.
 */

void ro_gui_drag_icon(int x, int y, const char *sprite)
{
	os_error *error;
	wimp_drag drag;
	int r2;

	drag.initial.x0 = x - 34;
	drag.initial.y0 = y - 34;
	drag.initial.x1 = x + 34;
	drag.initial.y1 = y + 34;

	if (sprite && (xosbyte2(osbyte_READ_CMOS, 28, 0, &r2) || (r2 & 2))) {
		osspriteop_area *area = (osspriteop_area*)1;

		/* first try our local sprite area in case it's a thumbnail sprite */
		if (saveas_area) {
			error = xosspriteop_select_sprite(osspriteop_USER_AREA,
					saveas_area, (osspriteop_id)sprite, NULL);
			if (error) {
				if (error->errnum != error_SPRITE_OP_DOESNT_EXIST) {
					LOG(("xosspriteop_select_sprite: 0x%x: %s",
						error->errnum, error->errmess));
					warn_user("MiscError", error->errmess);
				}
			}
			else
				area = saveas_area;
		}

		error = xdragasprite_start(dragasprite_HPOS_CENTRE |
				dragasprite_VPOS_CENTRE |
				dragasprite_BOUND_POINTER |
				dragasprite_DROP_SHADOW,
				area, sprite, &drag.initial, 0);

		if (!error) {
			using_dragasprite = true;
			dragbox_active = true;
			return;
		}

		LOG(("xdragasprite_start: 0x%x: %s",
				error->errnum, error->errmess));
	}

	drag.type = wimp_DRAG_USER_FIXED;
	drag.bbox.x0 = -0x8000;
	drag.bbox.y0 = -0x8000;
	drag.bbox.x1 = 0x7fff;
	drag.bbox.y1 = 0x7fff;

	using_dragasprite = false;
	error = xwimp_drag_box(&drag);

	if (error) {
		LOG(("xwimp_drag_box: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("DragError", error->errmess);
	}
	else
		dragbox_active = true;
}


/**
 * Convert a ctrl-char terminated pathname possibly containing spaces
 * to a NUL-terminated one containing only hard spaces.
 *
 * \param  dp   destination buffer to receive pathname
 * \param  len  size of destination buffer
 * \param  p    source pathname, ctrl-char terminated
 */

void ro_gui_convert_save_path(char *dp, size_t len, const char *p)
{
	char *ep = dp + len - 1;	/* leave room for NUL */

	assert(p <= dp || p > ep);	/* in-situ conversion /is/ allowed */

	while (dp < ep && *p >= ' ')	/* ctrl-char terminated */
	{
		*dp++ = (*p == ' ') ? 160 : *p;
		p++;
	}
	*dp = '\0';
}


void ro_gui_drag_box_cancel(void)
{
	if (dragbox_active) {
		os_error *error;
		if (using_dragasprite) {
			error = xdragasprite_stop();
			if (error) {
				LOG(("xdragasprite_stop: 0x%x: %s",
					error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
			}
		}
		else {
			error = xwimp_drag_box(NULL);
			if (error) {
				LOG(("xwimp_drag_box: 0x%x: %s",
					error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
			}
		}
		dragbox_active = false;
	}
}


/**
 * Handle User_Drag_Box event for a drag from the save dialog or browser window.
 */

void ro_gui_save_drag_end(wimp_dragged *drag)
{
	const char *name;
	wimp_pointer pointer;
	wimp_message message;
	os_error *error;
	char *dp, *ep;
	char *local_name = NULL;
	utf8_convert_ret err;

	if (dragbox_active)
		ro_gui_drag_box_cancel();

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* perform hit-test if the destination is the same as the source window;
		we want to allow drag-saving from a page into the input fields within
		the page, but avoid accidental replacements of the current page */
	if (gui_save_sourcew != (wimp_w)-1 && pointer.w == gui_save_sourcew) {
		int dx = (drag->final.x1 + drag->final.x0)/2;
		int dy = (drag->final.y1 + drag->final.y0)/2;
		struct gui_window *g;
		bool dest_ok = false;
		os_coord pos;

		g = ro_gui_window_lookup(gui_save_sourcew);

		if (g && ro_gui_window_to_window_pos(g, dx, dy, &pos)) {
			struct content *content = g->bw->current_content;

			if (content && content->type == CONTENT_HTML) {
				struct box *box = content->data.html.layout;
				int box_x, box_y;

				/* Consider the margins of the html page now */
				box_x = box->margin[LEFT];
				box_y = box->margin[TOP];

				while (!dest_ok && (box = box_at_point(box, pos.x, pos.y,
									&box_x, &box_y, &content))) {
					if (box->style &&
							box->style->visibility == CSS_VISIBILITY_HIDDEN)
						continue;

					if (box->gadget) {
						switch (box->gadget->type) {
							case GADGET_FILE:
							case GADGET_TEXTBOX:
							case GADGET_TEXTAREA:
							case GADGET_PASSWORD:
								dest_ok = true;
								break;

							default:	/* appease compiler */
								break;
						}
					}
				}
			}
		}
		if (!dest_ok) {
			/* cancel the drag operation */
			gui_current_drag_type = GUI_DRAG_NONE;
			return;
		}
	}

	if (!saving_from_dialog) {
		/* saving directly from browser window, choose a
		 * name based upon the URL */
		err = utf8_to_local_encoding(save_leafname, 0, &local_name);
		if (err != UTF8_CONVERT_OK) {
			/* badenc should never happen */
			assert(err != UTF8_CONVERT_BADENC);
			local_name = NULL;
		}
		name = local_name ? local_name : save_leafname;
	}
	else {
		char *dot;

		/* saving from dialog, grab leafname from icon */
		name = ro_gui_get_icon_string(gui_save_sourcew, ICON_SAVE_PATH);
		dot = strrchr(name, '.');
		if (dot)
			name = dot + 1;
	}

	dp = message.data.data_xfer.file_name;
	ep = dp + sizeof message.data.data_xfer.file_name;

	if (gui_save_current_type == GUI_SAVE_COMPLETE) {
		message.data.data_xfer.file_type = 0x2000;
		if (*name != '!') *dp++ = '!';
	} else
		message.data.data_xfer.file_type = gui_save_filetype;

	ro_gui_convert_save_path(dp, ep - dp, name);

/* \todo - we're supposed to set this if drag-n-drop used */
	message.your_ref = 0;

	message.action = message_DATA_SAVE;
	message.data.data_xfer.w = pointer.w;
	message.data.data_xfer.i = pointer.i;
	message.data.data_xfer.pos.x = pointer.pos.x;
	message.data.data_xfer.pos.y = pointer.pos.y;
	message.data.data_xfer.est_size = 1000;
	message.size = 44 + ((strlen(message.data.data_xfer.file_name) + 4) &
			(~3u));

	ro_message_send_message_to_window(wimp_USER_MESSAGE_RECORDED, &message,
			pointer.w, pointer.i, ro_gui_save_bounced, NULL);

	free(local_name);
}



/**
 * Send DataSave message on behalf of clipboard code and remember that it's the
 * clipboard contents we're being asked for when the DataSaveAck reply arrives
 */

void ro_gui_send_datasave(gui_save_type save_type,
		wimp_full_message_data_xfer *message, wimp_t to)
{
	/* Close the save window because otherwise we need two contexts
	*/
	xwimp_create_menu(wimp_CLOSE_MENU, 0, 0);
	ro_gui_dialog_close(dialog_saveas);

	if (ro_message_send_message(wimp_USER_MESSAGE_RECORDED, (wimp_message*)message,
			to, ro_gui_save_bounced)) {
		gui_save_current_type = save_type;
		gui_save_sourcew = (wimp_w)-1;
		saving_from_dialog = false;
		gui_current_drag_type = GUI_DRAG_SAVE;
	}
}


/**
 * Handle lack of Message_DataSaveAck for drags, saveas dialogs and clipboard code
 */

void ro_gui_save_bounced(wimp_message *message)
{
	gui_current_drag_type = GUI_DRAG_NONE;
}


/**
 * Handle Message_DataSaveAck for a drag from the save dialog or browser window,
 * or Clipboard protocol.
 */

void ro_gui_save_datasave_ack(wimp_message *message)
{
	char *path = message->data.data_xfer.file_name;
	struct content *c = gui_save_content;
	bool force_overwrite;

	switch (gui_save_current_type) {
		case GUI_SAVE_HOTLIST_EXPORT_HTML:
		case GUI_SAVE_HISTORY_EXPORT_HTML:
		case GUI_SAVE_TEXT_SELECTION:
		case GUI_SAVE_CLIPBOARD_CONTENTS:
			break;

		default:
			if (!gui_save_content) {
				LOG(("unexpected DataSaveAck: gui_save_content not set"));
				return;
			}
			break;
	}

	if (saving_from_dialog)
		ro_gui_set_icon_string(gui_save_sourcew, ICON_SAVE_PATH,
				path, true);

	gui_save_send_dataload = true;
	memcpy(&gui_save_message, message, sizeof(gui_save_message));

	/* if saving/pasting to another application, don't request user
	   confirmation; a ScrapFile almost certainly exists already */
	if (message->data.data_xfer.est_size == -1)
		force_overwrite = true;
	else
		force_overwrite = !option_confirm_overwrite;

	if (ro_gui_save_content(c, path, force_overwrite))
		ro_gui_save_done();
}



/**
 * Does the actual saving
 *
 * \param  c               content to save (or NULL for other)
 * \param  path            path to save as
 * \param  force_overwrite true iff required to overwrite without prompting
 * \return true on success,
 *         false on (i) error and error reported
 *               or (ii) deferred awaiting user confirmation
 */

bool ro_gui_save_content(struct content *c, char *path, bool force_overwrite)
{
	os_error *error;

	/* does the user want to check for collisions when saving? */
	if (!force_overwrite) {
		fileswitch_object_type obj_type;
		/* check whether the destination file/dir already exists */
		error = xosfile_read_stamped(path, &obj_type,
				NULL, NULL, NULL, NULL, NULL);
		if (error) {
			LOG(("xosfile_read_stamped: 0x%x:%s", error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
			return false;
		}

		switch (obj_type) {
			case osfile_NOT_FOUND:
				break;

			case osfile_IS_FILE:
				gui_save_query = query_user("OverwriteFile", NULL, &overwrite_funcs, NULL,
								messages_get("Replace"), messages_get("DontReplace"));
//				gui_save_query_rsn = QueryRsn_Overwrite;
				return false;

			default:
				error = xosfile_make_error(path, obj_type);
				assert(error);
				warn_user("SaveError", error->errmess);
				return false;
		}
	}

	switch (gui_save_current_type) {
#ifdef WITH_DRAW_EXPORT
		case GUI_SAVE_DRAW:
			return save_as_draw(c, path);
#endif
#ifdef WITH_PDF_EXPORT
		case GUI_SAVE_PDF:
			return save_as_pdf(c, path);
#endif
		case GUI_SAVE_TEXT:
			save_as_text(c, path);
			xosfile_set_type(path, 0xfff);
			break;
#ifdef WITH_SAVE_COMPLETE
		case GUI_SAVE_COMPLETE:
			assert(c);
			if (c->type == CONTENT_HTML) {
				if (strcmp(path, "<Wimp$Scrap>"))
					return ro_gui_save_complete(c, path);

				/* we can't send a whole directory to another application,
				 * so just send the HTML source */
				gui_save_current_type = GUI_SAVE_SOURCE;
			}
			else
				gui_save_current_type = GUI_SAVE_OBJECT_ORIG;	/* \todo do this earlier? */
			/* no break */
#endif
		case GUI_SAVE_SOURCE:
		case GUI_SAVE_OBJECT_ORIG:
			error = xosfile_save_stamped(path,
					ro_content_filetype(c),
					c->source_data,
					c->source_data + c->source_size);
			if (error) {
				LOG(("xosfile_save_stamped: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("SaveError", error->errmess);
				return false;
			}
			break;

		case GUI_SAVE_OBJECT_NATIVE:
			ro_gui_save_object_native(c, path);
			break;

		case GUI_SAVE_LINK_URI:
			return ro_gui_save_link(c, LINK_ACORN, path);

		case GUI_SAVE_LINK_URL:
			return ro_gui_save_link(c, LINK_ANT, path);

		case GUI_SAVE_LINK_TEXT:
			return ro_gui_save_link(c, LINK_TEXT, path);

		case GUI_SAVE_HOTLIST_EXPORT_HTML:
			if (!options_save_tree(hotlist_tree, path, "NetSurf hotlist"))
				return false;
			error = xosfile_set_type(path, 0xfaf);
			if (error)
				LOG(("xosfile_set_type: 0x%x: %s",
						error->errnum, error->errmess));
			break;
		case GUI_SAVE_HISTORY_EXPORT_HTML:
			if (!options_save_tree(global_history_tree, path, "NetSurf history"))
				return false;
			error = xosfile_set_type(path, 0xfaf);
			if (error)
				LOG(("xosfile_set_type: 0x%x: %s",
						error->errnum, error->errmess));
			break;

		case GUI_SAVE_TEXT_SELECTION:
			if (!selection_save_text(gui_save_selection, path))
				return false;
			xosfile_set_type(path, 0xfff);
			break;

		case GUI_SAVE_CLIPBOARD_CONTENTS:
			return ro_gui_save_clipboard(path);

		default:
			LOG(("Unexpected content type: %d, path %s", gui_save_current_type, path));
			return false;
	}
	return true;
}


/**
 * Save completed, inform recipient and close our 'save as' dialog.
 */

void ro_gui_save_done(void)
{
	os_error *error;

	if (gui_save_send_dataload) {
		/* Ack successful save with message_DATA_LOAD */
		wimp_message *message = &gui_save_message;
		message->action = message_DATA_LOAD;
		message->your_ref = message->my_ref;
		error = xwimp_send_message(wimp_USER_MESSAGE, message,
				message->sender);
		if (error) {
			LOG(("xwimp_send_message: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
		}
	}

	if (saving_from_dialog) {
		/* */
		char *sp = gui_save_message.data.data_xfer.file_name;
		char *ep = sp + sizeof(gui_save_message.data.data_xfer.file_name);
		char *lastdot = NULL;
		char *p = sp;

		while (p < ep && *p >= 0x20) {
			if (*p == '.') lastdot = p;
			p++;
		}
		if (lastdot) {
			/* remember the directory */
			char *new_dir = realloc(save_dir, (lastdot+1)-sp);
			if (new_dir) {
				save_dir_len = lastdot - sp;
				memcpy(new_dir, sp, save_dir_len);
				new_dir[save_dir_len] = '\0';
				save_dir = new_dir;
			}
		}

		if (gui_save_close_after) {
			/*	Close the save window */
			ro_gui_dialog_close(dialog_saveas);
			error = xwimp_create_menu(wimp_CLOSE_MENU, 0, 0);
			if (error) {
				LOG(("xwimp_create_menu: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("MenuError", error->errmess);
			}
		}
	}

	if (!saving_from_dialog || gui_save_close_after)
		gui_save_content = 0;
}


/**
 * Prepare an application directory and save_complete() to it.
 *
 * \param  c     content of type CONTENT_HTML to save
 * \param  path  path to save as
 * \return  true on success, false on error and error reported
 */

#define WIDTH 64
#define HEIGHT 64
#define SPRITE_SIZE (16 + 44 + ((WIDTH / 2 + 3) & ~3) * HEIGHT / 2)

#ifdef WITH_SAVE_COMPLETE

bool ro_gui_save_complete(struct content *c, char *path)
{
	osspriteop_header *sprite;
	char name[12];
	char buf[256];
	FILE *fp;
	os_error *error;
	size_t len;
	char *dot;
	int i;

	/* Create dir */
	error = xosfile_create_dir(path, 0);
	if (error) {
		LOG(("xosfile_create_dir: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
		return false;
	}

	/* Save !Run file */
	snprintf(buf, sizeof buf, "%s.!Run", path);
	fp = fopen(buf, "w");
	if (!fp) {
		LOG(("fopen(): errno = %i", errno));
		warn_user("SaveError", strerror(errno));
		return false;
	}
	fprintf(fp, "IconSprites <Obey$Dir>.!Sprites\n");
	fprintf(fp, "Filer_Run <Obey$Dir>.index\n");
	fclose(fp);
	error = xosfile_set_type(buf, 0xfeb);
	if (error) {
		LOG(("xosfile_set_type: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
		return false;
	}

	/* Make sure the sprite name matches the directory name, because
	   the user may have renamed the directory since we created the
	   thumbnail sprite */

	dot = strrchr(path, '.');
	if (dot) dot++; else dot = path;
	len = strlen(dot);
	if (len >= 12) len = 12;

	sprite = (osspriteop_header*)((byte*)saveas_area + saveas_area->first);
	memcpy(name, sprite->name, 12);  /* remember original name */
	memcpy(sprite->name, dot, len);
	memset(sprite->name + len, 0, 12 - len);
	for (i = 0; i < 12; i++) /* convert to lower case */
		if (sprite->name[i] != '\0')
			sprite->name[i] = tolower(sprite->name[i]);

	/* Create !Sprites */
	snprintf(buf, sizeof buf, "%s.!Sprites", path);

	error = xosspriteop_save_sprite_file(osspriteop_NAME, saveas_area, buf);
	if (error) {
		LOG(("xosspriteop_save_sprite_file: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
	        return false;
	}

	/* restore sprite name in case the save fails and we need to try again */
	memcpy(sprite->name, name, 12);

	/* save URL file with original URL */
	snprintf(buf, sizeof buf, "%s.URL", path);
	if (!ro_gui_save_link(c, LINK_ANT, buf))
		return false;

	return save_complete(c, path);
}

#endif

void ro_gui_save_object_native(struct content *c, char *path)
{
	switch (c->type) {
#ifdef WITH_JPEG
		case CONTENT_JPEG:
#endif
#if defined(WITH_MNG) || defined(WITH_PNG)
		case CONTENT_PNG:
#endif
#ifdef WITH_MNG
		case CONTENT_JNG:
		case CONTENT_MNG:
#endif
#ifdef WITH_GIF
		case CONTENT_GIF:
#endif
#ifdef WITH_BMP
		case CONTENT_BMP:
		case CONTENT_ICO:
#endif
		{
			unsigned flags = (os_version == 0xA9) ? BITMAP_SAVE_FULL_ALPHA : 0;
			bitmap_save(c->bitmap, path, flags);
		}
		break;

#ifdef WITH_SPRITE
		case CONTENT_SPRITE: {
			os_error *error;
			error = xosfile_save_stamped(path,
					ro_content_filetype(c),
					c->source_data,
					c->source_data + c->source_size);
			if (error) {
				LOG(("xosfile_save_stamped: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("SaveError", error->errmess);
			}
		}
		break;
#endif

		default:
			break;
	}
}


/**
 * Save a link file.
 *
 * \param  c       content to save link to
 * \param  format  format of link file
 * \param  path    pathname for link file
 * \return  true on success, false on failure and reports the error
 */

bool ro_gui_save_link(struct content *c, link_format format, char *path)
{
	FILE *fp = fopen(path, "w");

	if (!fp) {
		warn_user("SaveError", strerror(errno));
		return false;
	}

	switch (format) {
		case LINK_ACORN: /* URI */
			fprintf(fp, "%s\t%s\n", "URI", "100");
			fprintf(fp, "\t# NetSurf %s\n\n", netsurf_version);
			fprintf(fp, "\t%s\n", c->url);
			if (c->title)
				fprintf(fp, "\t%s\n", c->title);
			else
				fprintf(fp, "\t*\n");
			break;
		case LINK_ANT: /* URL */
		case LINK_TEXT: /* Text */
			fprintf(fp, "%s\n", c->url);
			break;
	}

	fclose(fp);

	switch (format) {
		case LINK_ACORN: /* URI */
			xosfile_set_type(path, 0xf91);
			break;
		case LINK_ANT: /* URL */
			xosfile_set_type(path, 0xb28);
			break;
		case LINK_TEXT: /* Text */
			xosfile_set_type(path, 0xfff);
			break;
	}

	return true;
}


/**
 * Suggest a leafname and sprite name for the given content.
 *
 * \param  c          content being saved
 * \param  save_type  type of save operation being performed
 * \param  leaf_buf   buffer to receive suggested leafname, length at least
 *                    LEAFNAME_MAX
 * \param  icon_buf   buffer to receive sprite name, length at least 13
 */

void ro_gui_save_set_state(struct content *c, gui_save_type save_type,
		char *leaf_buf, char *icon_buf)
{
	/* filename */
	const char *name = gui_save_table[save_type].name;
	url_func_result res;
	bool done = false;
	char *nice = NULL;
	utf8_convert_ret err;
	char *local_name;
	size_t i;

	/* parameters that we need to remember */
	gui_save_current_type = save_type;
	gui_save_content = c;

	/* suggest a filetype based upon the content */
	gui_save_filetype = gui_save_table[save_type].filetype;
	if (!gui_save_filetype)
		gui_save_filetype = ro_content_filetype(c);

	/* leafname */
	if (c && (res = url_nice(c->url, &nice, option_strip_extensions)) ==
			URL_FUNC_OK) {
		for (i = 0; nice[i]; i++) {
			if (nice[i] == '.')
				nice[i] = '/';
			else if (nice[i] <= ' ' ||
					strchr(":*#$&@^%\\", nice[i]))
				nice[i] = '_';
		}
		name = nice;
	} else {
		name = messages_get(name);
	}

	/* filename is utf8 */
	strncpy(leaf_buf, name, LEAFNAME_MAX);
	leaf_buf[LEAFNAME_MAX - 1] = 0;

	err = utf8_to_local_encoding(name, 0, &local_name);
	if (err != UTF8_CONVERT_OK) {
		/* badenc should never happen */
		assert(err != UTF8_CONVERT_BADENC);
		local_name = NULL;
	}

	name = local_name ? local_name : name;

	/* sprite name used for icon and dragging */
	if (save_type == GUI_SAVE_COMPLETE) {
		int index;

		/* Paint gets confused with uppercase characters and we need to
		   convert spaces to hard spaces */
		icon_buf[0] = '!';
		for (index = 0; index < 11 && name[index]; ) {
			char ch = name[index];
			if (ch == ' ')
				icon_buf[++index] = 0xa0;
			else
				icon_buf[++index] = tolower(ch);
		}
		memset(&icon_buf[index + 1], 0, 11 - index);
		icon_buf[12] = '\0';

		if (ro_gui_save_create_thumbnail(c, icon_buf))
			done = true;
	}

	if (!done) {
		osspriteop_header *sprite;
		os_error *error;

		sprintf(icon_buf, "file_%.3x", gui_save_filetype);

		error = ro_gui_wimp_get_sprite(icon_buf, &sprite);
		if (error && error->errnum == error_SPRITE_OP_DOESNT_EXIST) {
			/* try the 'unknown' filetype sprite as a fallback */
			memcpy(icon_buf, "file_xxx", 9);
			error = ro_gui_wimp_get_sprite(icon_buf, &sprite);
		}

		if (error) {
			LOG(("ro_gui_wimp_get_sprite: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("MiscError", error->errmess);
		} else {
			/* the sprite area should always be large enough for
			 * file_xxx sprites */
			assert(sprite->size <= saveas_area->size -
					saveas_area->first);

			memcpy((byte*)saveas_area + saveas_area->first,
					sprite,
					sprite->size);

			saveas_area->sprite_count = 1;
			saveas_area->used = saveas_area->first + sprite->size;
		}
	}

	free(local_name);
	free(nice);
}



/**
 * Create a thumbnail sprite for the page being saved.
 *
 * \param  c     content to be converted
 * \param  name  sprite name to use
 * \return true iff successful
 */

bool ro_gui_save_create_thumbnail(struct content *c, const char *name)
{
	osspriteop_header *sprite_header;
	struct bitmap *bitmap;
	osspriteop_area *area;

	bitmap = bitmap_create(34, 34, BITMAP_NEW | BITMAP_OPAQUE | BITMAP_CLEAR_MEMORY);
	if (!bitmap) {
		LOG(("Thumbnail initialisation failed."));
		return false;
	}
	thumbnail_create(c, bitmap, NULL);
	area = thumbnail_convert_8bpp(bitmap);
	bitmap_destroy(bitmap);
	if (!area) {
		LOG(("Thumbnail conversion failed."));
		return false;
	}

	sprite_header = (osspriteop_header *)(area + 1);
	strncpy(sprite_header->name, name, 12);


	/* we can't resize the saveas sprite area because it may move and we have
	   no elegant way to update the window definition on all OS versions */
	assert(sprite_header->size <= saveas_area->size - saveas_area->first);

	memcpy((byte*)saveas_area + saveas_area->first,
		sprite_header, sprite_header->size);

	saveas_area->sprite_count = 1;
	saveas_area->used = saveas_area->first + sprite_header->size;

	free(area);

	return true;
}


/**
 * User has opted not to overwrite the existing file.
 */

void ro_gui_save_overwrite_cancelled(query_id id, enum query_response res, void *p)
{
	if (!saving_from_dialog) {
//		ro_gui_save_prepare(gui_save_current_type, gui_save_content);
//		ro_gui_dialog_open_persistent(g->window, dialog_saveas, true);
	}
}


/**
 * Overwrite of existing file confirmed, proceed with the save.
 */

void ro_gui_save_overwrite_confirmed(query_id id, enum query_response res, void *p)
{
	if (ro_gui_save_content(gui_save_content, gui_save_message.data.data_xfer.file_name, true))
		ro_gui_save_done();
}
