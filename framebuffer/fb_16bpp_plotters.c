/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
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

#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "utils/log.h"
#include "utils/utf8.h"
#include "desktop/plotters.h"

#include "framebuffer/fb_gui.h"
#include "framebuffer/fb_plotters.h"
#include "framebuffer/fb_bitmap.h"
#include "framebuffer/fb_font.h"

static inline uint16_t *
fb_16bpp_get_xy_loc(int x, int y)
{
        return (void *)(framebuffer->ptr + 
                            (y * framebuffer->linelen) + 
                            (x << 1));
}


#define SIGN(x)  ((x<0) ?  -1  :  ((x>0) ? 1 : 0))

static bool fb_16bpp_line(int x0, int y0, int x1, int y1, int width,
			colour c, bool dotted, bool dashed)
{
        int w;
        uint16_t ent;
        uint16_t *pvideo;

        int x, y, i;
        int dx, dy, sdy;
        int dxabs, dyabs;

        /*LOG(("%d, %d, %d, %d, %d, 0x%lx, %d, %d",
	  x0,y0,x1,y1,width,c,dotted,dashed));*/

        if (y1 > fb_plot_ctx.y1)
                return true;
        if (y0 < fb_plot_ctx.y0)
                return true;

        ent = ((c & 0xF8) << 8) |
              ((c & 0xFC00 ) >> 5) |
              ((c & 0xF80000) >> 19);

        if (y0 == y1) {
                /* horizontal line special cased */
                if (!fb_plotters_clip_rect_ctx(&x0, &y0, &x1, &y1))
			return true; /* line outside clipping */

                /*LOG(("horiz: %d, %d, %d, %d, %d, 0x%lx, %d, %d",
		  x0,y0,x1,y1,width,c,dotted,dashed));*/

                pvideo = fb_16bpp_get_xy_loc(x0, y0);

                w = x1 - x0;
                while (w-- > 0) {
                        *(pvideo + w) = ent;
                }
                return true;
        } else {
                /* standard bresenham line */
                if (!fb_plotters_clip_line_ctx(&x0, &y0, &x1, &y1))
                        return true; /* line outside clipping */

                //LOG(("%d, %d, %d, %d", x0,y0,x1,y1));

                /* the horizontal distance of the line */
                dx = x1 - x0;
                dxabs = abs (dx);

                /* the vertical distance of the line */
                dy = y1 - y0;
                dyabs = abs (dy);

                sdy = dx ? SIGN(dy) * SIGN(dx) : SIGN(dy);

                if (dx >= 0)
                        pvideo = fb_16bpp_get_xy_loc(x0, y0);
                else
                        pvideo = fb_16bpp_get_xy_loc(x1, y1);

                x = dyabs >> 1;
                y = dxabs >> 1;

                if (dxabs >= dyabs) { 
                        /* the line is more horizontal than vertical */
                        for (i = 0; i <= dxabs; i++) {
                                *pvideo = ent;

                                pvideo++;
                                y += dyabs;
                                if (y >= dxabs) {
                                        y -= dxabs;
                                        pvideo += sdy * (framebuffer->linelen>>1);
                                }
                        }
                } else {
                        /* the line is more vertical than horizontal */
                        for (i = 0; i <= dyabs; i++) {
                                *pvideo = ent;
                                pvideo += sdy * (framebuffer->linelen >> 1);

                                x += dxabs;
                                if (x >= dyabs) {
                                        x -= dyabs;
                                        pvideo++;
                                }
                        }
                }
                
        }



	return true;
}

static bool fb_16bpp_rectangle(int x0, int y0, int width, int height,
			int line_width, colour c, bool dotted, bool dashed)
{
        fb_16bpp_line(x0, y0, x0 + width, y0, line_width, c, dotted, dashed);
        fb_16bpp_line(x0, y0 + height, x0 + width, y0 + height, line_width, c, dotted, dashed);
        fb_16bpp_line(x0, y0, x0, y0 + height, line_width, c, dotted, dashed);
        fb_16bpp_line(x0 + width, y0, x0 + width, y0 + height, line_width, c, dotted, dashed);
	return true;
}

static bool fb_16bpp_polygon(const int *p, unsigned int n, colour fill)
{
        return fb_plotters_polygon(p, n, fill, fb_16bpp_line);
}


static bool fb_16bpp_fill(int x0, int y0, int x1, int y1, colour c)
{
        int w;
        int y;
        uint16_t ent;
        uint16_t *pvideo;

        if (!fb_plotters_clip_rect_ctx(&x0, &y0, &x1, &y1))
                return true; /* fill lies outside current clipping region */

        ent = ((c & 0xF8) << 8) |
              ((c & 0xFC00 ) >> 5) |
              ((c & 0xF80000) >> 19);

        pvideo = fb_16bpp_get_xy_loc(x0, y0);

        for (y = y0; y < y1; y++) {
                w = x1 - x0;
                while (w-- > 0) {
                        *(pvideo + w) = ent;
                }
                pvideo += (framebuffer->linelen >> 1);
        }

	return true;
}

static bool fb_16bpp_clg(colour c)
{
        /* LOG(("c %lx", c)); */
        fb_16bpp_fill(fb_plot_ctx.x0,
                         fb_plot_ctx.y0,
                         fb_plot_ctx.x1,
                         fb_plot_ctx.y1,
                         c);
	return true;
}

#ifdef FB_USE_FREETYPE
static bool fb_16bpp_text(int x, int y, const struct css_style *style,
			const char *text, size_t length, colour bg, colour c)
{
        return false;
}
#else
static bool fb_16bpp_text(int x, int y, const struct css_style *style,
			const char *text, size_t length, colour bg, colour c)
{
        const struct fb_font_desc* fb_font = fb_get_font(style);
        const uint32_t *font_data;
        int xloop, yloop;
        uint32_t row;
        size_t chr;

        uint16_t *pvideo;
        uint16_t fgcol;
        uint16_t bgcol;

	unsigned char *buffer = NULL;
        int x0,y0,x1,y1;
	int xoff, yoff; /* x and y offset into image */
        int height = fb_font->height;

        /* aquire thge text in local font encoding */
	utf8_to_font_encoding(fb_font, text, length, (char **)&buffer);
	if (!buffer) 
                return true;
        length = strlen((char *)buffer);


        /* y is given to the fonts baseline we need it to the fonts top */
        y-=((fb_font->height * 75)/100);

        y+=1; /* the coord is the bottom-left of the pixels offset by 1 to make
               *   it work since fb coords are the top-left of pixels 
               */

        /* The part of the text displayed is cropped to the current context. */
        x0 = x;
        y0 = y;
        x1 = x + (fb_font->width * length);
        y1 = y + fb_font->height;

        if (!fb_plotters_clip_rect_ctx(&x0, &y0, &x1, &y1))
                return true; /* text lies outside current clipping region */

        /* find width and height to plot */
        if (height > (y1 - y0))
                height = (y1 - y0);

	xoff = x0 - x;
	yoff = y0 - y;

        fgcol = ((c & 0xF8) << 8) |
              ((c & 0xFC00 ) >> 5) |
              ((c & 0xF80000) >> 19);

        bgcol = ((bg & 0xF8) << 8) |
              ((bg & 0xFC00 ) >> 5) |
              ((bg & 0xF80000) >> 19);


        /*LOG(("x %d, y %d, style %p, txt %.*s , len %d, bg 0x%lx, fg 0x%lx",
          x,y,style,length,text,length,bg,c));*/

        for (chr = 0; chr < length; chr++, x += fb_font->width) {
                if ((x + fb_font->width) > x1)
                        break;

                if (x < x0) 
                        continue;

                pvideo = fb_16bpp_get_xy_loc(x, y0);

                /* move our font-data to the correct position */
                font_data = fb_font->data + (buffer[chr] * fb_font->height);

                for (yloop = 0; yloop < height; yloop++) {
                        row = font_data[yoff + yloop];
                        for (xloop = fb_font->width; xloop > 0 ; xloop--) {
                                if ((row & 1) != 0)
                                        *(pvideo + xloop) = fgcol;
                                row = row >> 1;
                        }
                        pvideo += (framebuffer->linelen >> 1);
                }
        }

	free(buffer);
	return true;
}
#endif

static bool fb_16bpp_disc(int x, int y, int radius, colour c, bool filled)
{
        LOG(("x %d, y %d, r %d, c 0x%lx, fill %d",
             x, y, radius, (unsigned long)c, filled));

	return true;
}

static bool fb_16bpp_arc(int x, int y, int radius, int angle1, int angle2,
                         colour c)
{
        LOG(("x %d, y %d, r %d, a1 %d, a2 %d, c 0x%lx",
             x, y, radius, angle1, angle2, (unsigned long)c));
	return true;
}

static inline colour fb_16bpp_to_colour(uint16_t pixel)
{
        return ((pixel & 0x1F) << 19) | 
              ((pixel & 0x7E0) << 5) |
              ((pixel & 0xF800) >> 8);
}

static bool fb_16bpp_bitmap(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg,
                        struct content *content)
{
        uint16_t *pvideo;
        colour *pixel = (colour *)bitmap->pixdata;
        colour abpixel; /* alphablended pixel */
        int xloop, yloop;
        int x0,y0,x1,y1;
	int xoff, yoff; /* x and y offset into image */

        /* LOG(("x %d, y %d, width %d, height %d, bitmap %p, content %p", 
           x,y,width,height,bitmap,content));*/

        /* TODO here we should scale the image from bitmap->width to width, for
         * now simply crop. 
         */
        if (width > bitmap->width)
                width = bitmap->width;

        if (height > bitmap->height)
                height = bitmap->height;

        /* The part of the scaled image actually displayed is cropped to the
         * current context. 
         */
        x0 = x;
        y0 = y;
        x1 = x + width;
        y1 = y + height;

        if (!fb_plotters_clip_rect_ctx(&x0, &y0, &x1, &y1))
                return true;

        if (height > (y1 - y0))
                height = (y1 - y0);

        if (width > (x1 - x0))
                width = (x1 - x0);

	xoff = x0 - x;
	yoff = y0 - y;

        /* plot the image */
        pvideo = fb_16bpp_get_xy_loc(x0, y0);

        for (yloop = 0; yloop < height; yloop++) {
                for (xloop = 0; xloop < width; xloop++) {
                        abpixel = pixel[((yoff + yloop) * bitmap->width) + xloop + xoff];
                        if ((abpixel & 0xFF000000) != 0) {
                                if ((abpixel & 0xFF000000) != 0xFF) {
                                        abpixel = fb_plotters_ablend(abpixel, 
                                         fb_16bpp_to_colour(*(pvideo + xloop)));
                                }

                                *(pvideo + xloop) =
                                        ((abpixel & 0xF8) << 8) |
                                        ((abpixel & 0xFC00 ) >> 5) |
                                        ((abpixel & 0xF80000) >> 19);

                        }
                }
                pvideo += (framebuffer->linelen >> 1);
        }

	return true;
}

static bool fb_16bpp_bitmap_tile(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg,
			bool repeat_x, bool repeat_y,
                             struct content *content)
{
        return fb_plotters_bitmap_tile(x, y, width, height, 
                                       bitmap, bg, repeat_x, repeat_y,
                                       content, fb_16bpp_bitmap);
}

static bool fb_16bpp_flush(void)
{
        LOG(("optional"));
	return true;
}

static bool fb_16bpp_path(const float *p, unsigned int n, colour fill, float width,
			colour c, const float transform[6])
{
        LOG(("%f, %d, 0x%lx, %f, 0x%lx, %f",
             *p, n, (unsigned long)fill, width, (unsigned long)c, *transform));
	return true;
}


const struct plotter_table framebuffer_16bpp_plot = {
	.clg = fb_16bpp_clg,
	.rectangle = fb_16bpp_rectangle,
	.line = fb_16bpp_line,
	.polygon = fb_16bpp_polygon,
	.fill = fb_16bpp_fill,
	.clip = fb_clip,
	.text = fb_16bpp_text,
	.disc = fb_16bpp_disc,
	.arc = fb_16bpp_arc,
	.bitmap = fb_16bpp_bitmap,
	.bitmap_tile = fb_16bpp_bitmap_tile,
	.flush = fb_16bpp_flush,
	.path = fb_16bpp_path,
        .option_knockout = true,
};

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
