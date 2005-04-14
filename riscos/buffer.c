/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004, 2005 Richard Wilson <info@tinct.net>
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <swis.h>
#include "oslib/colourtrans.h"
#include "oslib/os.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "oslib/wimpreadsysinfo.h"
#include "netsurf/riscos/buffer.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/tinct.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
//#define NDEBUG
#define BUFFER_EXCLUSIVE_USER_REDRAW "Only support pure user redraw (faster)"
//#define BUFFER_EMULATE_32BPP "Redirect to a 32bpp sprite and plot with Tinct"

static void ro_gui_buffer_free(void);


/** The buffer characteristics
*/
static osspriteop_area *buffer = NULL;
static char buffer_name[] = "scr_buffer\0\0";

/** The current clip area
*/
static os_box clipping;

/** The current save area
*/
static osspriteop_save_area *save_area;
static osspriteop_area *context1;
static osspriteop_id context2;
static osspriteop_save_area *context3;

/** The current sprite mode
*/
static os_mode mode;


/**
 * Opens a buffer for writing to.
 *
 * The ro_plot_origin_ variables are updated to reflect the new screen origin,
 * so the variables should be set before calling this function, and not
 * changed until after ro_gui_buffer_close() has been called.
 *
 * \param redraw the current WIMP redraw area to buffer
 */
void ro_gui_buffer_open(wimp_draw *redraw) {
	int size;
	int total_size;
	os_coord sprite_size;
	int bpp, word_width;
	bool palette;
	os_error *error;
	int palette_size = 0;
#ifdef BUFFER_EXCLUSIVE_USER_REDRAW
	osspriteop_header *header;
#endif

	/*	Close any open buffer
	*/
	if (buffer)
		ro_gui_buffer_close();

	/*	Store our clipping region
	*/
	clipping = redraw->clip;

	/*	Stop bad rectangles
	*/
	if ((clipping.x1 < clipping.x0) ||
			(clipping.y1 < clipping.y0)) {
		LOG(("Invalid clipping rectangle (%i, %i) to (%i,%i)",
			clipping.x0, clipping.y0,
			clipping.x1, clipping.y1));
		return;
	}

	/*	Work out how much buffer we need
	*/
	sprite_size.x = clipping.x1 - clipping.x0 + 1;
	sprite_size.y = clipping.y1 - clipping.y0 + 1;
	ro_convert_os_units_to_pixels(&sprite_size, (os_mode)-1);
	if (sprite_size.y == 1) /* work around SpriteExtend bug */
		sprite_size.y = 2;

#ifdef BUFFER_EMULATE_32BPP
	bpp = 5;
	palette = false;
#else
	/*	Get the screen depth as we can't use palettes for >8bpp
	*/
	xos_read_mode_variable((os_mode)-1, os_MODEVAR_LOG2_BPP, &bpp, 0);
	palette = (bpp < 4);
#endif

	/*	Get our required buffer size
	*/
	word_width = ((sprite_size.x << bpp) + 31) >> 5;
	if (palette)
		palette_size = ((1 << (1 << bpp)) << 3);
	total_size = sizeof(osspriteop_area) + sizeof(osspriteop_header) +
			(word_width * sprite_size.y * 4) + palette_size;
	buffer = (osspriteop_area *)malloc(total_size);
	if (!buffer) {
		LOG(("Failed to allocate memory"));
		ro_gui_buffer_free();
		return;
	}
	buffer->size = total_size;
	buffer->first = 16;

#ifdef BUFFER_EMULATE_32BPP
	mode = tinct_SPRITE_MODE;
#else
	if (bpp == 32) {
		mode = tinct_SPRITE_MODE;
	} else {
		if ((error = xwimpreadsysinfo_wimp_mode(&mode)) != NULL) {
			LOG(("Error reading mode '%s'", error->errmess));
			ro_gui_buffer_free();
			return;
		}
	}
#endif

#ifdef BUFFER_EXCLUSIVE_USER_REDRAW
	/*	Create the sprite manually so we don't waste time clearing the
		background.
	*/
	buffer->sprite_count = 1;
	buffer->used = total_size;
	header = (osspriteop_header *)(buffer + 1);
	header->size = total_size - sizeof(osspriteop_area);
	memcpy(header->name, buffer_name, 12);
	header->width = word_width - 1;
	header->height = sprite_size.y - 1;
	header->left_bit = 0;
	header->right_bit = ((sprite_size.x << bpp) - 1) & 31;
	header->image = sizeof(osspriteop_header) + palette_size;
	header->mask = header->image;
	header->mode = mode;
	if (palette)
		xcolourtrans_read_palette((osspriteop_area *)mode,
			(osspriteop_id)colourtrans_CURRENT_MODE,
			(os_palette *)(header + 1), palette_size,
			(colourtrans_palette_flags)
				colourtrans_FLASHING_PALETTE, 0);
#else
	/*	Read the current contents of the screen
	*/
	buffer->sprite_count = 0;
	buffer->used = 16;
	if ((error = xosspriteop_get_sprite_user_coords(osspriteop_NAME,
			buffer, buffer_name, palette,
			clipping.x0, clipping.y0,
			clipping.x1, clipping.y1)) != NULL) {
		LOG(("Grab error '%s'", error->errmess));
		ro_gui_buffer_free();
		return;
	}
#endif
	/*	Allocate OS_SpriteOp save area
	*/
	if ((error = xosspriteop_read_save_area_size(osspriteop_PTR,
			buffer, (osspriteop_id)(buffer + 1), &size)) != NULL) {
		LOG(("Save area error '%s'", error->errmess));
		ro_gui_buffer_free();
		return;
	}
	if ((save_area = malloc((size_t)size)) == NULL) {
		ro_gui_buffer_free();
		return;
	}
	save_area->a[0] = 0;

	/*	Switch output to sprite
	*/
	if ((error = xosspriteop_switch_output_to_sprite(osspriteop_PTR,
			buffer, (osspriteop_id)(buffer + 1), save_area, 0,
			(int *)&context1, (int *)&context2,
			(int *)&context3)) != NULL) {
		LOG(("Switching error '%s'", error->errmess));
		free(save_area);
		ro_gui_buffer_free();
		return;
	}

	/*	Emulate an origin as the FontManager doesn't respect it in
		most cases.
	*/
	ro_plot_origin_x -= clipping.x0;
	ro_plot_origin_y -= clipping.y0;
}


/**
 * Closes any open buffer and flushes the contents to screen
 */
void ro_gui_buffer_close(void) {

	/*	Check we have an open buffer
	*/
	if (!buffer)
		return;

	/*	Remove any previous redirection
	*/
	ro_plot_origin_x += clipping.x0;
	ro_plot_origin_y += clipping.y0;
	xosspriteop_switch_output_to_sprite(osspriteop_PTR,
			context1, context2, context3,
			0, 0, 0, 0);
	free(save_area);

	/*	Plot the contents to screen
	*/
	if (mode == tinct_SPRITE_MODE)
		_swix(Tinct_Plot, _IN(2) | _IN(3) | _IN(4) | _IN(7),
				(char *)(buffer + 1),
				clipping.x0, clipping.y0,
				option_fg_plot_style);
	else
		xosspriteop_put_sprite_user_coords(osspriteop_PTR,
			buffer, (osspriteop_id)(buffer + 1),
			clipping.x0, clipping.y0, (os_action)0);
	ro_gui_buffer_free();
}


/**
 * Releases any buffer memory depending on cache constraints.
 */
static void ro_gui_buffer_free(void) {
	free(buffer);
	buffer = NULL;
}
