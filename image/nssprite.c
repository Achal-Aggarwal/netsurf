 /*
 * Copyright 2008 James Shaw <js102@zepler.net>
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
 * Content for image/x-riscos-sprite (librosprite implementation).
 *
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <librosprite.h>
#include "utils/config.h"
#include "content/content_protected.h"
#include "desktop/plotters.h"
#include "image/bitmap.h"
#include "image/nssprite.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/talloc.h"
#include "utils/utils.h"

typedef struct nssprite_content {
	struct content base;

	struct rosprite_area* sprite_area;
} nssprite_content;


#define ERRCHK(x) do { \
	rosprite_error err = x; \
	if (err == ROSPRITE_EOF) { \
		LOG(("Got ROSPRITE_EOF when loading sprite file")); \
		return false; \
	} else if (err == ROSPRITE_BADMODE) { \
		LOG(("Got ROSPRITE_BADMODE when loading sprite file")); \
		return false; \
	} else if (err == ROSPRITE_OK) { \
	} else { \
		return false; \
	} \
} while(0)




static nserror nssprite_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	nssprite_content *sprite;
	nserror error;

	sprite = talloc_zero(0, nssprite_content);
	if (sprite == NULL)
		return NSERROR_NOMEM;

	error = content__init(&sprite->base, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		talloc_free(sprite);
		return error;
	}

	*c = (struct content *) sprite;

	return NSERROR_OK;
}

/**
 * Convert a CONTENT_SPRITE for display.
 *
 * No conversion is necessary. We merely read the sprite dimensions.
 */

static bool nssprite_convert(struct content *c)
{
	nssprite_content *nssprite = (nssprite_content *) c;
	union content_msg_data msg_data;

	struct rosprite_mem_context* ctx;

	const char *data;
	unsigned long size;

	data = content__get_source_data(c, &size);

	ERRCHK(rosprite_create_mem_context((uint8_t *) data, size, &ctx));

	struct rosprite_area* sprite_area;
	ERRCHK(rosprite_load(rosprite_mem_reader, ctx, &sprite_area));
	rosprite_destroy_mem_context(ctx);
	nssprite->sprite_area = sprite_area;

	assert(sprite_area->sprite_count > 0);

	struct rosprite* sprite = sprite_area->sprites[0];

	c->bitmap = bitmap_create(sprite->width, sprite->height, BITMAP_NEW);
	if (!c->bitmap) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	unsigned char* imagebuf = bitmap_get_buffer(c->bitmap);
	if (!imagebuf) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	unsigned int row_width = bitmap_get_rowstride(c->bitmap);

	memcpy(imagebuf, sprite->image, row_width * sprite->height); // TODO: avoid copying entire image buffer

	/* reverse byte order of each word */
	for (uint32_t y = 0; y < sprite->height; y++) {
		for (uint32_t x = 0; x < sprite->width; x++) {
			int offset = 4 * (y * sprite->width + x);
			uint32_t r = imagebuf[offset+3];
			uint32_t g = imagebuf[offset+2];
			uint32_t b = imagebuf[offset+1];
			uint32_t a = imagebuf[offset];
			imagebuf[offset] = r;
			imagebuf[offset+1] = g;
			imagebuf[offset+2] = b;
			imagebuf[offset+3] = a;
		}
	}

	c->width = sprite->width;
	c->height = sprite->height;
	bitmap_modified(c->bitmap);

	content_set_ready(c);
	content_set_done(c);

	return true;
}


/**
 * Destroy a CONTENT_SPRITE and free all resources it owns.
 */

static void nssprite_destroy(struct content *c)
{
	nssprite_content *sprite = (nssprite_content *) c;

	if (sprite->sprite_area != NULL)
		rosprite_destroy_sprite_area(sprite->sprite_area);
	if (c->bitmap != NULL)
		bitmap_destroy(c->bitmap);
}


/**
 * Redraw a CONTENT_SPRITE.
 */

static bool nssprite_redraw(struct content *c, struct content_redraw_data *data,
		const struct rect *clip, const struct redraw_context *ctx)
{
	bitmap_flags_t flags = BITMAPF_NONE;

	if (data->repeat_x)
		flags |= BITMAPF_REPEAT_X;
	if (data->repeat_y)
		flags |= BITMAPF_REPEAT_Y;

	return ctx->plot->bitmap(data->x, data->y, data->width, data->height,
			c->bitmap, data->background_colour, flags);
}


static nserror nssprite_clone(const struct content *old, struct content **newc)
{
	nssprite_content *sprite;
	nserror error;

	sprite = talloc_zero(0, nssprite_content);
	if (sprite == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old, &sprite->base);
	if (error != NSERROR_OK) {
		content_destroy(&sprite->base);
		return error;
	}

	/* Simply replay convert */
	if (old->status == CONTENT_STATUS_READY ||
			old->status == CONTENT_STATUS_DONE) {
		if (nssprite_convert(&sprite->base) == false) {
			content_destroy(&sprite->base);
			return NSERROR_CLONE_FAILED;
		}
	}

	*newc = (struct content *) sprite;

	return NSERROR_OK;
}

static content_type nssprite_content_type(lwc_string *mime_type)
{
	return CONTENT_IMAGE;
}

static const content_handler nssprite_content_handler = {
	.create = nssprite_create,
	.data_complete = nssprite_convert,
	.destroy = nssprite_destroy,
	.redraw = nssprite_redraw,
	.clone = nssprite_clone,
	.type = nssprite_content_type,
	.no_share = false,
};

static const char *nssprite_types[] = {
	"image/x-riscos-sprite"
};

CONTENT_FACTORY_REGISTER_TYPES(nssprite, nssprite_types, nssprite_content_handler);
