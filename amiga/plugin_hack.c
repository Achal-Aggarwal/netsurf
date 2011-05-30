/*
 * Copyright 2011 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
 * DataTypes picture handler (implementation)
*/

#include "amiga/filetype.h"
#include "amiga/plugin_hack.h"
#include "content/content_protected.h"
#include "desktop/plotters.h"
#include "render/box.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/talloc.h"

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>

typedef struct amiga_plugin_hack_content {
	struct content base;
} amiga_plugin_hack_content;

static nserror amiga_plugin_hack_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c);
static bool amiga_plugin_hack_convert(struct content *c);
static void amiga_plugin_hack_reformat(struct content *c, int width, int height);
static void amiga_plugin_hack_destroy(struct content *c);
static bool amiga_plugin_hack_redraw(struct content *c, int x, int y,
		int width, int height, const struct rect *clip,
		float scale, colour background_colour,
		bool repeat_x, bool repeat_y);
static void amiga_plugin_hack_open(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params);
static void amiga_plugin_hack_close(struct content *c);
static nserror amiga_plugin_hack_clone(const struct content *old, struct content **newc);
static content_type amiga_plugin_hack_content_type(lwc_string *mime_type);

static const content_handler amiga_plugin_hack_content_handler = {
	.create = amiga_plugin_hack_create,
	.data_complete = amiga_plugin_hack_convert,
	.reformat = amiga_plugin_hack_reformat,
	.destroy = amiga_plugin_hack_destroy,
	.redraw = amiga_plugin_hack_redraw,
	.open = amiga_plugin_hack_open,
	.close = amiga_plugin_hack_close,
	.clone = amiga_plugin_hack_clone,
	.type = amiga_plugin_hack_content_type,
	.no_share = false,
};

nserror amiga_plugin_hack_init(void)
{
	struct Node *node = NULL;
	lwc_string *type;
	nserror error;

	do {
		node = ami_mime_has_cmd(&type, node);

		if(node)
		{
			printf("plugin_hack registered %s\n",lwc_string_data(type));

			error = content_factory_register_handler(type, 
				&amiga_plugin_hack_content_handler);

			if (error != NSERROR_OK)
				return error;
		}

	}while (node != NULL);

	return NSERROR_OK;
}

void amiga_plugin_hack_fini(void)
{
	/* Nothing to do */
}

nserror amiga_plugin_hack_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	amiga_plugin_hack_content *plugin;
	nserror error;

	plugin = talloc_zero(0, amiga_plugin_hack_content);
	if (plugin == NULL)
		return NSERROR_NOMEM;

	error = content__init(&plugin->base, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		talloc_free(plugin);
		return error;
	}

	*c = (struct content *) plugin;

	return NSERROR_OK;
}

bool amiga_plugin_hack_convert(struct content *c)
{
	LOG(("amiga_plugin_hack_convert"));

	content_set_ready(c);
	content_set_done(c);

	content_set_status(c, "");

	return true;
}

void amiga_plugin_hack_destroy(struct content *c)
{
	amiga_plugin_hack_content *plugin = (amiga_plugin_hack_content *) c;

	LOG(("amiga_plugin_hack_destroy"));

	return;
}

bool amiga_plugin_hack_redraw(struct content *c, int x, int y,
	int width, int height, const struct rect *clip,
	float scale, colour background_colour,
	bool repeat_x, bool repeat_y)
{
	static plot_style_t pstyle;

	LOG(("amiga_plugin_hack_redraw"));

	plot.rectangle(x, y, x + width, y + height, plot_style_fill_red);
	return plot.text(x, y+20, lwc_string_data(content__get_mime_type(c)),
		lwc_string_length(content__get_mime_type(c)), plot_style_font);

}

/**
 * Handle a window containing a CONTENT_PLUGIN being opened.
 *
 * \param  c       content that has been opened
 * \param  bw      browser window containing the content
 * \param  page    content of type CONTENT_HTML containing c, or 0 if not an
 *                 object within a page
 * \param  box     box containing c, or 0 if not an object
 * \param  params  object parameters, or 0 if not an object
 */
void amiga_plugin_hack_open(struct content *c, struct browser_window *bw,
	struct content *page, struct box *box,
	struct object_params *params)
{
	LOG(("amiga_plugin_hack_open %s", content__get_url(c)));

	c->width = box->width;
	c->height = box->height;

	return;
}

void amiga_plugin_hack_close(struct content *c)
{
	LOG(("amiga_plugin_hack_close"));
	return;
}

void amiga_plugin_hack_reformat(struct content *c, int width, int height)
{
	LOG(("amiga_plugin_hack_reformat"));

	c->width = width;
	c->height = height;

	return;
}

nserror amiga_plugin_hack_clone(const struct content *old, struct content **newc)
{
	amiga_plugin_hack_content *plugin;
	nserror error;

	LOG(("amiga_plugin_hack_clone"));

	plugin = talloc_zero(0, amiga_plugin_hack_content);
	if (plugin == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old, &plugin->base);
	if (error != NSERROR_OK) {
		content_destroy(&plugin->base);
		return error;
	}

	/* We "clone" the old content by replaying conversion */
	if (old->status == CONTENT_STATUS_READY || 
			old->status == CONTENT_STATUS_DONE) {
		if (amiga_plugin_hack_convert(&plugin->base) == false) {
			content_destroy(&plugin->base);
			return NSERROR_CLONE_FAILED;
		}
	}

	*newc = (struct content *) plugin;

	return NSERROR_OK;
}

content_type amiga_plugin_hack_content_type(lwc_string *mime_type)
{
	return CONTENT_PLUGIN;
}

void amiga_plugin_hack_execute(struct hlcache_handle *c)
{
	lwc_string *mimetype;
	lwc_string *plugincmd;
	struct Node *node;
	char *full_cmd;
	BPTR in, out;

	if(c == NULL) return;

	mimetype = content_get_mime_type(c);
	node = ami_mime_to_plugincmd(mimetype, &plugincmd, NULL);

	if(node && plugincmd)
	{
		full_cmd = ASPrintf("%s %s", lwc_string_data(plugincmd), content_get_url(c));

		if(full_cmd)
		{
			LOG(("Attempting to execute %s", full_cmd));

			in = Open("NIL:", MODE_OLDFILE);
			out = Open("NIL:", MODE_NEWFILE);

			SystemTags(full_cmd,
				SYS_Input, in,
				SYS_Output, out,
				SYS_Error, out,
				SYS_Asynch, TRUE,
				NP_Name, "NetSurf External Process",
				TAG_DONE);

			FreeVec(full_cmd);
		}
	}
}