/*
 * Copyright 2011 Sven Weidauer <sven.weidauer@gmail.com>
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

#ifdef WITH_APPLE_IMAGE

#import "cocoa/apple_image.h"

#include "utils/config.h"
#include "content/content_protected.h"
#include "image/bitmap.h"
#include "desktop/plotters.h"
#include "utils/talloc.h"
#include "utils/utils.h"
#include "utils/schedule.h"

typedef struct apple_image_content {
	struct content base;
    NSUInteger frames;
    NSUInteger currentFrame;
    int *frameTimes;
} apple_image_content;

static nserror apple_image_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c);
static bool apple_image_convert(struct content *c);
static void apple_image_destroy(struct content *c);
static bool apple_image_redraw(struct content *c, struct content_redraw_data *data,
		const struct rect *clip);
static nserror apple_image_clone(const struct content *old, 
		struct content **newc);
static content_type apple_image_content_type(lwc_string *mime_type);

static const content_handler apple_image_content_handler = {
	.create = apple_image_create,
	.data_complete = apple_image_convert,
	.destroy = apple_image_destroy,
	.redraw = apple_image_redraw,
	.clone = apple_image_clone,
	.type = apple_image_content_type,
	.no_share = false
};

static nserror register_for_type( NSString *mime )
{
	const char *type = [mime UTF8String];
	/* nsgif has priority since it supports animated GIF */
#ifdef WITH_GIF
	if (strcmp(type, "image/gif") == 0)
		return NSERROR_OK;
#endif

	lwc_string *string = NULL;
	lwc_error lerror = lwc_intern_string( type, strlen( type ), &string );
	if (lerror != lwc_error_ok) return NSERROR_NOMEM;

	nserror error = content_factory_register_handler( string, &apple_image_content_handler );
	lwc_string_unref( string );

	if (error != NSERROR_OK) return error;
	
	return NSERROR_OK;
}

nserror apple_image_init(void)
{
	NSArray *utis = [NSBitmapImageRep imageTypes];
	for (NSString *uti in utis) {
		NSDictionary *declaration = [(NSDictionary *)UTTypeCopyDeclaration( (CFStringRef)uti ) autorelease];
		id mimeTypes = [[declaration objectForKey: (NSString *)kUTTypeTagSpecificationKey] objectForKey: (NSString *)kUTTagClassMIMEType];
		
		if (mimeTypes == nil) continue;
		
		if (![mimeTypes isKindOfClass: [NSArray class]]) {
			mimeTypes = [NSArray arrayWithObject: mimeTypes];
		}
		
		for (NSString *mime in mimeTypes) {
			nserror error = register_for_type( mime );
			if (error != NSERROR_OK) return error;
		} 
	}
	
	return NSERROR_OK;
}

void apple_image_fini(void)
{
}

nserror apple_image_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	apple_image_content *ai;
	nserror error;

	ai = talloc_zero(0, apple_image_content);
	if (ai == NULL)
		return NSERROR_NOMEM;

	error = content__init(&ai->base, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		talloc_free(ai);
		return error;
	}

	*c = (struct content *) ai;

	return NSERROR_OK;
}


static void animate_image_cb( void *ptr )
{
    struct apple_image_content *ai = ptr;
    ++ai->currentFrame;
    if (ai->currentFrame >= ai->frames) ai->currentFrame = 0;
    
    [(NSBitmapImageRep *)ai->base.bitmap setProperty: NSImageCurrentFrame withValue: [NSNumber numberWithUnsignedInteger: ai->currentFrame]];
    bitmap_modified( ai->base.bitmap );
    
    union content_msg_data data;
    data.redraw.full_redraw = true;
    data.redraw.x = data.redraw.object_x = 0;
    data.redraw.y = data.redraw.object_y = 0;
    data.redraw.width = data.redraw.object_width = ai->base.width;
    data.redraw.height = data.redraw.object_height = ai->base.height;
	data.redraw.object = &ai->base;
    content_broadcast( &ai->base, CONTENT_MSG_REDRAW, data );

    schedule( ai->frameTimes[ai->currentFrame], animate_image_cb, ai );
}

/**
 * Convert a CONTENT_APPLE_IMAGE for display.
 */

bool apple_image_convert(struct content *c)
{
	unsigned long size;
	const char *bytes = content__get_source_data(c, &size);

	NSData *data = [NSData dataWithBytesNoCopy: (char *)bytes length: size freeWhenDone: NO];
	NSBitmapImageRep *image = [[NSBitmapImageRep imageRepWithData: data] retain];

	if (image == nil) {
		union content_msg_data msg_data;
		msg_data.error = "cannot decode image";
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	
	c->width = [image pixelsWide];
	c->height = [image pixelsHigh];
	c->bitmap = (void *)image;

	NSString *url = [NSString stringWithUTF8String: llcache_handle_get_url( content_get_llcache_handle( c ) )];
	NSString *title = [NSString stringWithFormat: @"%@ (%dx%d)", [url lastPathComponent], c->width, c->height];
	content__set_title(c, [title UTF8String] );
	
	content_set_ready(c);
	content_set_done(c);
	content_set_status(c, "");
    
    struct apple_image_content *ai = (struct apple_image_content *)c;
    NSUInteger frames = [[image valueForProperty: NSImageFrameCount] unsignedIntegerValue];
    if (frames > 1) {
        ai->frames = frames;
        ai->currentFrame = 0;
        ai->frameTimes = talloc_zero_array( ai, int, ai->frames );
        for (NSUInteger i = 0; i < frames; i++) {
            [image setProperty: NSImageCurrentFrame withValue: [NSNumber numberWithUnsignedInteger: i]];
            ai->frameTimes[i] = 100 * [[image valueForProperty: NSImageCurrentFrameDuration] floatValue];
        }
        [image setProperty: NSImageCurrentFrame withValue: [NSNumber numberWithUnsignedInteger: 0]];
        schedule( ai->frameTimes[0], animate_image_cb, ai );
    }
	
	return true;
}


void apple_image_destroy(struct content *c)
{
	[(id)c->bitmap release];
	c->bitmap = NULL;
    schedule_remove( animate_image_cb, c );
}


nserror apple_image_clone(const struct content *old, struct content **newc)
{
	apple_image_content *ai;
	nserror error;

	ai = talloc_zero(0, apple_image_content);
	if (ai == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old, &ai->base);
	if (error != NSERROR_OK) {
		content_destroy(&ai->base);
		return error;
	}

	if (old->status == CONTENT_STATUS_READY ||
		old->status == CONTENT_STATUS_DONE) {
		ai->base.width = old->width;
		ai->base.height = old->height;
		ai->base.bitmap = (void *)[(id)old->bitmap retain];
	}

	*newc = (struct content *) ai;
	
	return NSERROR_OK;
}

content_type apple_image_content_type(lwc_string *mime_type)
{
	return CONTENT_IMAGE;
}

/**
 * Redraw a CONTENT_APPLE_IMAGE with appropriate tiling.
 */

bool apple_image_redraw(struct content *c, struct content_redraw_data *data,
		const struct rect *clip)
{
	bitmap_flags_t flags = BITMAPF_NONE;

	if (data->repeat_x)
		flags |= BITMAPF_REPEAT_X;
	if (data->repeat_y)
		flags |= BITMAPF_REPEAT_Y;

	return plot.bitmap(data->x, data->y, data->width, data->height,
			c->bitmap, data->background_colour, flags);
}

#endif /* WITH_APPLE_IMAGE */
