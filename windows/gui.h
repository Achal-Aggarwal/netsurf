/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
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

#ifndef _NETSURF_WINDOWS_GUI_H_
#define _NETSURF_WINDOWS_GUI_H_

#include <windows.h>
#include "desktop/gui.h"
#include "windows/localhistory.h"

extern HINSTANCE hinstance;

/* bounding box */
typedef struct bbox_s {
        int x0;
        int y0;
        int x1;
        int y1;
} bbox_t;

struct nsws_pointers {
	HCURSOR		hand;
	HCURSOR		ibeam;
	HCURSOR		cross;
	HCURSOR		sizeall;
	HCURSOR		sizewe;
	HCURSOR		sizens;
	HCURSOR		sizenesw;
	HCURSOR		sizenwse;
	HCURSOR		wait;
	HCURSOR		appstarting;
	HCURSOR		no;
	HCURSOR		help;
	HCURSOR		arrow;
};

struct browser_mouse {
       struct gui_window *gui;
       struct box *box;
       
       double pressed_x;
       double pressed_y;
       bool waiting;
       browser_mouse_state state;
};

extern struct gui_window *window_list;
extern char *options_file_location;

HWND gui_window_main_window(struct gui_window *);
HWND gui_window_toolbar(struct gui_window *);
HWND gui_window_urlbar(struct gui_window *);
HWND gui_window_statusbar(struct gui_window *);
HWND gui_window_drawingarea(struct gui_window *);
struct nsws_localhistory *gui_window_localhistory(struct gui_window *);
void gui_window_set_localhistory(struct gui_window *,
		struct nsws_localhistory *);

RECT *gui_window_redraw_rect(struct gui_window *);
RECT *gui_window_clip_rect(struct gui_window *w);

int gui_window_voffset(struct gui_window *);
int gui_window_width(struct gui_window *);
int gui_window_height(struct gui_window *);
int gui_window_scrollingx(struct gui_window *w);
int gui_window_scrollingy(struct gui_window *w);

struct gui_window *gui_window_iterate(struct gui_window *);
struct browser_window *gui_window_browser_window(struct gui_window *);
struct nsws_pointers *nsws_get_pointers(void);
HICON nsws_window_get_ico(bool);

#endif 
