/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
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
 * Target independent plotting (GDK / GTK+ and Cairo implementation).
 * Can use either GDK drawing primitives (which are mostly passed straight
 * to X to process, and thus accelerated) or Cairo drawing primitives (much
 * higher quality, not accelerated).  Cairo's fast enough, so it defaults
 * to using it if it is available.  It does this by checking for the
 * CAIRO_VERSION define that the cairo headers set.
 */

#include <math.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include "desktop/plotters.h"
#include "gtk/font_pango.h"
#include "gtk/gtk_plotters.h"
#include "gtk/gtk_scaffolding.h"
#include "render/font.h"
#include "utils/log.h"
#include "desktop/options.h"
#include "gtk/options.h"
#include "gtk/gtk_bitmap.h"

#ifndef CAIRO_VERSION
#error "nsgtk requires cairo"
#endif

GtkWidget *current_widget;
GdkDrawable *current_drawable;
GdkGC *current_gc;
cairo_t *current_cr;

static bool nsgtk_plot_clg(colour c);
static bool nsgtk_plot_rectangle(int x0, int y0, int width, int height,
		int line_width, colour c, bool dotted, bool dashed);
static bool nsgtk_plot_line(int x0, int y0, int x1, int y1, int width,
		colour c, bool dotted, bool dashed);
static bool nsgtk_plot_polygon(int *p, unsigned int n, colour fill);
static bool nsgtk_plot_path(float *p, unsigned int n, colour fill, float width,
                    colour c, float *transform);
static bool nsgtk_plot_fill(int x0, int y0, int x1, int y1, colour c);
static bool nsgtk_plot_clip(int clip_x0, int clip_y0,
		int clip_x1, int clip_y1);
static bool nsgtk_plot_text(int x, int y, const struct css_style *style,
		const char *text, size_t length, colour bg, colour c);
static bool nsgtk_plot_disc(int x, int y, int radius, colour c, bool filled);
static bool nsgtk_plot_arc(int x, int y, int radius, int angle1, int angle2,
    		colour c);
static bool nsgtk_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg, struct content *content);
static bool nsgtk_plot_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bool repeat_x, bool repeat_y, struct content *content);
static void nsgtk_set_solid(void);	/**< Set for drawing solid lines */
static void nsgtk_set_dotted(void);	/**< Set for drawing dotted lines */
static void nsgtk_set_dashed(void);	/**< Set for drawing dashed lines */

static GdkRectangle cliprect;
static float nsgtk_plot_scale = 1.0;

struct plotter_table plot;

const struct plotter_table nsgtk_plotters = {
	nsgtk_plot_clg,
	nsgtk_plot_rectangle,
	nsgtk_plot_line,
	nsgtk_plot_polygon,
	nsgtk_plot_fill,
	nsgtk_plot_clip,
	nsgtk_plot_text,
	nsgtk_plot_disc,
	nsgtk_plot_arc,
	nsgtk_plot_bitmap,
	nsgtk_plot_bitmap_tile,
	NULL,
	NULL,
	NULL,
	nsgtk_plot_path,
	true
};


bool nsgtk_plot_clg(colour c)
{
	return true;
}

bool nsgtk_plot_rectangle(int x0, int y0, int width, int height,
		int line_width, colour c, bool dotted, bool dashed)
{
	nsgtk_set_colour(c);
        if (dotted)
                nsgtk_set_dotted();
        else if (dashed)
                nsgtk_set_dashed();
        else
                nsgtk_set_solid();

	if (line_width == 0)
		line_width = 1;

	cairo_set_line_width(current_cr, line_width);
	cairo_rectangle(current_cr, x0, y0, width, height);
	cairo_stroke(current_cr);

	return true;
}


bool nsgtk_plot_line(int x0, int y0, int x1, int y1, int width,
		colour c, bool dotted, bool dashed)
{
	nsgtk_set_colour(c);
	if (dotted)
		nsgtk_set_dotted();
	else if (dashed)
		nsgtk_set_dashed();
	else
		nsgtk_set_solid();

	if (width == 0)
		width = 1;

	cairo_set_line_width(current_cr, width);
	cairo_move_to(current_cr, x0, y0 - 0.5);
	cairo_line_to(current_cr, x1, y1 - 0.5);
	cairo_stroke(current_cr);

	return true;
}


bool nsgtk_plot_polygon(int *p, unsigned int n, colour fill)
{
	unsigned int i;

	nsgtk_set_colour(fill);
	nsgtk_set_solid();

	cairo_set_line_width(current_cr, 0);
	cairo_move_to(current_cr, p[0], p[1]);
	for (i = 1; i != n; i++) {
		cairo_line_to(current_cr, p[i * 2], p[i * 2 + 1]);
	}
	cairo_fill(current_cr);
	cairo_stroke(current_cr);

	return true;
}


bool nsgtk_plot_fill(int x0, int y0, int x1, int y1, colour c)
{
	nsgtk_set_colour(c);
	nsgtk_set_solid();

	cairo_set_line_width(current_cr, 0);
	cairo_rectangle(current_cr, x0, y0, x1 - x0, y1 - y0);
	cairo_fill(current_cr);
	cairo_stroke(current_cr);

	return true;
}


bool nsgtk_plot_clip(int clip_x0, int clip_y0,
		int clip_x1, int clip_y1)
{
	cairo_reset_clip(current_cr);
	cairo_rectangle(current_cr, clip_x0, clip_y0,
			clip_x1 - clip_x0, clip_y1 - clip_y0);
	cairo_clip(current_cr);

	cliprect.x = clip_x0;
	cliprect.y = clip_y0;
	cliprect.width = clip_x1 - clip_x0;
	cliprect.height = clip_y1 - clip_y0;
	gdk_gc_set_clip_rectangle(current_gc, &cliprect);

	return true;
}


bool nsgtk_plot_text(int x, int y, const struct css_style *style,
		const char *text, size_t length, colour bg, colour c)
{
	return nsfont_paint(style, text, length, x, y, c);
}


bool nsgtk_plot_disc(int x, int y, int radius, colour c, bool filled)
{
	nsgtk_set_colour(c);
	nsgtk_set_solid();

	if (filled)
		cairo_set_line_width(current_cr, 0);
	else
		cairo_set_line_width(current_cr, 1);

	cairo_arc(current_cr, x, y, radius, 0, M_PI * 2);

	if (filled)
		cairo_fill(current_cr);

	cairo_stroke(current_cr);

	return true;
}

bool nsgtk_plot_arc(int x, int y, int radius, int angle1, int angle2, colour c)
{
	nsgtk_set_colour(c);
	nsgtk_set_solid();

	cairo_set_line_width(current_cr, 1);
	cairo_arc(current_cr, x, y, radius,
			(angle1 + 90) * (M_PI / 180),
			(angle2 + 90) * (M_PI / 180));
	cairo_stroke(current_cr);

	return true;
}

static bool nsgtk_plot_pixbuf(int x, int y, int width, int height,
                              GdkPixbuf *pixbuf, colour bg)
{
	/* XXX: This currently ignores the background colour supplied.
	 * Does this matter?
	 */

	if (width == 0 || height == 0)
		return true;

	if (gdk_pixbuf_get_width(pixbuf) == width &&
			gdk_pixbuf_get_height(pixbuf) == height) {
		gdk_draw_pixbuf(current_drawable, current_gc,
				pixbuf,
				0, 0,
				x, y,
				width, height,
				GDK_RGB_DITHER_MAX, 0, 0);

	} else {
		GdkPixbuf *scaled;
		scaled = gdk_pixbuf_scale_simple(pixbuf,
				width, height,
				option_render_resample ? GDK_INTERP_BILINEAR
							: GDK_INTERP_NEAREST);
		if (!scaled)
			return false;

		gdk_draw_pixbuf(current_drawable, current_gc,
				scaled,
				0, 0,
				x, y,
				width, height,
				GDK_RGB_DITHER_MAX, 0, 0);

		g_object_unref(scaled);
	}

	return true;
}

bool nsgtk_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg, struct content *content)
{
	GdkPixbuf *pixbuf = gtk_bitmap_get_primary(bitmap);
	return nsgtk_plot_pixbuf(x, y, width, height, pixbuf, bg);
}

bool nsgtk_plot_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bool repeat_x, bool repeat_y, struct content *content)
{
	int doneheight = 0, donewidth = 0;
        GdkPixbuf *primary;
	GdkPixbuf *pretiled;

	if (!(repeat_x || repeat_y)) {
		/* Not repeating at all, so just pass it on */
		return nsgtk_plot_bitmap(x,y,width,height,bitmap,bg,content);
	}

        if (repeat_x && !repeat_y)
                pretiled = gtk_bitmap_get_pretile_x(bitmap);
        if (repeat_x && repeat_y)
                pretiled = gtk_bitmap_get_pretile_xy(bitmap);
        if (!repeat_x && repeat_y)
                pretiled = gtk_bitmap_get_pretile_y(bitmap);
        primary = gtk_bitmap_get_primary(bitmap);
        /* use the primary and pretiled widths to scale the w/h provided */
        width *= gdk_pixbuf_get_width(pretiled);
        width /= gdk_pixbuf_get_width(primary);
        height *= gdk_pixbuf_get_height(pretiled);
        height /= gdk_pixbuf_get_height(primary);

	if (y > cliprect.y)
		doneheight = (cliprect.y - height) + ((y - cliprect.y) % height);
	else
		doneheight = y;

	while (doneheight < (cliprect.y + cliprect.height)) {
		if (x > cliprect.x)
			donewidth = (cliprect.x - width) + ((x - cliprect.x) % width);
		else
			donewidth = x;
		while (donewidth < (cliprect.x + cliprect.width)) {
			nsgtk_plot_pixbuf(donewidth, doneheight,
                                          width, height, pretiled, bg);
			donewidth += width;
			if (!repeat_x) break;
		}
		doneheight += height;
		if (!repeat_y) break;
	}


	return true;
}

bool nsgtk_plot_path(float *p, unsigned int n, colour fill, float width,
                colour c, float *transform)
{
	unsigned int i;
	cairo_matrix_t old_ctm, n_ctm;

	if (n == 0)
		return true;

	if (p[0] != PLOTTER_PATH_MOVE) {
		LOG(("Path does not start with move"));
		return false;
	}


	/* Save CTM */
	cairo_get_matrix(current_cr, &old_ctm);

	/* Set up line style and width */
	cairo_set_line_width(current_cr, 1);
	nsgtk_set_solid();

	/* Load new CTM */
	n_ctm.xx = transform[0];
	n_ctm.yx = transform[1];
	n_ctm.xy = transform[2];
	n_ctm.yy = transform[3];
	n_ctm.x0 = transform[4];
	n_ctm.y0 = transform[5];

	cairo_set_matrix(current_cr, &n_ctm);

	/* Construct path */
	for (i = 0; i < n; ) {
		if (p[i] == PLOTTER_PATH_MOVE) {
			cairo_move_to(current_cr, p[i+1], p[i+2]);
			i += 3;
		} else if (p[i] == PLOTTER_PATH_CLOSE) {
			cairo_close_path(current_cr);
			i++;
		} else if (p[i] == PLOTTER_PATH_LINE) {
			cairo_line_to(current_cr, p[i+1], p[i+2]);
			i += 3;
		} else if (p[i] == PLOTTER_PATH_BEZIER) {
			cairo_curve_to(current_cr, p[i+1], p[i+2],
					p[i+3], p[i+4],
					p[i+5], p[i+6]);
			i += 7;
		} else {
			LOG(("bad path command %f", p[i]));
			/* Reset matrix for safety */
			cairo_set_matrix(current_cr, &old_ctm);
			return false;
		}
	}

	/* Restore original CTM */
	cairo_set_matrix(current_cr, &old_ctm);

	/* Now draw path */
	if (fill != TRANSPARENT) {
		nsgtk_set_colour(fill);

		if (c != TRANSPARENT) {
			/* Fill & Stroke */
			cairo_fill_preserve(current_cr);
			nsgtk_set_colour(c);
			cairo_stroke(current_cr);
		} else {
			/* Fill only */
			cairo_fill(current_cr);
		}
	} else if (c != TRANSPARENT) {
		/* Stroke only */
		nsgtk_set_colour(c);
		cairo_stroke(current_cr);
	}

	return true;
}

void nsgtk_set_colour(colour c)
{
	int r, g, b;
	GdkColor colour;

	r = c & 0xff;
	g = (c & 0xff00) >> 8;
	b = (c & 0xff0000) >> 16;

	colour.red = r | (r << 8);
	colour.green = g | (g << 8);
	colour.blue = b | (b << 8);
	colour.pixel = (r << 16) | (g << 8) | b;

	gdk_color_alloc(gdk_colormap_get_system(),
			&colour);
	gdk_gc_set_foreground(current_gc, &colour);

	cairo_set_source_rgba(current_cr, r / 255.0,
			g / 255.0, b / 255.0, 1.0);
}

void nsgtk_set_solid()
{
	double dashes = 0;
	
	cairo_set_dash(current_cr, &dashes, 0, 0);
}

void nsgtk_set_dotted()
{
	double cdashes = 1;
	gint8 dashes[] = { 1, 1 };

	cairo_set_dash(current_cr, &cdashes, 1, 0);
}

void nsgtk_set_dashed()
{
	double cdashes = 3;
	gint8 dashes[] = { 3, 3 };

	cairo_set_dash(current_cr, &cdashes, 1, 0);
}

void nsgtk_plot_set_scale(float s)
{
	nsgtk_plot_scale = s;
}

float nsgtk_plot_get_scale(void)
{
	return nsgtk_plot_scale;
}

/** Plot a caret.  It is assumed that the plotters have been set up. */
void nsgtk_plot_caret(int x, int y, int h)
{
	GdkColor colour;

	colour.red = 0;
	colour.green = 0;
	colour.blue = 0;
	colour.pixel = 0;
	gdk_color_alloc(gdk_colormap_get_system(),
			&colour);
	gdk_gc_set_foreground(current_gc, &colour);

	gdk_draw_line(current_drawable, current_gc,
			x, y,
			x, y + h - 1);
}

