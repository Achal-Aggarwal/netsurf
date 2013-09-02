/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/plotters.h"
#include "desktop/textinput.h"
#include "desktop/tree.h"
#include "desktop/textinput.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "atari/gui.h"
#include "atari/treeview.h"
#include "atari/plot/plot.h"
#include "atari/misc.h"
#include "atari/gemtk/gemtk.h"
#include "cflib.h"

enum treeview_area_e {
	TREEVIEW_AREA_WORK = 0,
	TREEVIEW_AREA_TOOLBAR,
	TREEVIEW_AREA_CONTENT
};

extern int mouse_hold_start[3];
extern browser_mouse_state bmstate;
extern short last_drag_x;
extern short last_drag_y;
extern long atari_plot_flags;
extern int atari_plot_vdi_handle;

static void atari_treeview_resized(struct tree *tree,int w,int h, void *pw);
static void atari_treeview_scroll_visible(int y, int h, void *pw);
static void atari_treeview_get_dimensions(int *width, int *height, void *pw);
static void atari_treeview_get_grect(NSTREEVIEW tree,
									enum treeview_area_e mode, GRECT *dest);

static const struct treeview_table atari_tree_callbacks = {
	atari_treeview_request_redraw,
	atari_treeview_resized,
	atari_treeview_scroll_visible,
	atari_treeview_get_dimensions
};

static void __CDECL on_mbutton_event(NSTREEVIEW tv, EVMULT_OUT *ev_out,
									short msg[8]);
static void __CDECL on_keybd_event(NSTREEVIEW tv, EVMULT_OUT *ev_out,
									short msg[8]);
static void __CDECL on_redraw_event(NSTREEVIEW tv, EVMULT_OUT *ev_out,
									short msg[8]);

static short handle_event(GUIWIN *win, EVMULT_OUT *ev_out, short msg[8])
{

	NSTREEVIEW tv = (NSTREEVIEW) gemtk_wm_get_user_data(win);

    if( (ev_out->emo_events & MU_MESAG) != 0 ) {
        // handle message
        switch (msg[0]) {

        case WM_REDRAW:
			on_redraw_event(tv, ev_out, msg);
            break;

		case WM_SIZED:
		case WM_FULLED:
			//atari_treeview_resized(tv->tree, tv->extent.x, tv->extent.y, tv);
			break;

        default:
            break;
        }
    }
    if( (ev_out->emo_events & MU_KEYBD) != 0 ) {
        on_keybd_event(tv, ev_out, msg);
    }
    if( (ev_out->emo_events & MU_BUTTON) != 0 ) {
        LOG(("Treeview click at: %d,%d\n", ev_out->emo_mouse.p_x,
             ev_out->emo_mouse.p_y));
        on_mbutton_event(tv, ev_out, msg);
    }

    if(tv != NULL && tv->user_func != NULL){
		tv->user_func(win, ev_out, msg);
    }

    return(0);
}

static void __CDECL on_keybd_event(NSTREEVIEW tv, EVMULT_OUT *ev_out,
									short msg[8])
{
	bool r=false;
	long kstate = 0;
	long kcode = 0;
	long ucs4;
	long ik;
	unsigned short nkc = 0;
	unsigned short nks = 0;
	unsigned char ascii;

	kstate = ev_out->emo_kmeta;
	kcode = ev_out->emo_kreturn;
	nkc= gem_to_norm( (short)kstate, (short)kcode );
	ascii = (nkc & 0xFF);
	ik = nkc_to_input_key( nkc, &ucs4 );

	if( ik == 0 ){
		if (ascii >= 9 ) {
            r = tree_keypress( tv->tree, ucs4 );
		}
	} else {
		r = tree_keypress( tv->tree, ik );
	}
}


static void __CDECL on_redraw_event(NSTREEVIEW tv, EVMULT_OUT *ev_out,
									short msg[8])
{
	GRECT work, clip;
	struct gemtk_wm_scroll_info_s *slid;

	if( tv == NULL )
		return;

	gemtk_wm_get_grect(tv->window, GEMTK_WM_AREA_CONTENT, &work);
	slid = gemtk_wm_get_scroll_info(tv->window);

	clip = work;
	if ( !rc_intersect( (GRECT*)&msg[4], &clip ) ) return;
	clip.g_x -= work.g_x;
	clip.g_y -= work.g_y;
	if( clip.g_x < 0 ) {
		clip.g_w = work.g_w + clip.g_x;
		clip.g_x = 0;
	}
	if( clip.g_y < 0 ) {
		clip.g_h = work.g_h + clip.g_y;
		clip.g_y = 0;
	}
	if( clip.g_h > 0 && clip.g_w > 0 ) {
		// TODO: get slider values
		atari_treeview_request_redraw((slid->x_pos*slid->x_unit_px) + clip.g_x,
									(slid->y_pos*slid->y_unit_px) + clip.g_y,
									clip.g_w, clip.g_h, tv
		);
	}
}

static void __CDECL on_mbutton_event(NSTREEVIEW tv, EVMULT_OUT *ev_out,
									short msg[8])
{
	struct gemtk_wm_scroll_info_s *slid;
	GRECT work;
	short mx, my;

	if(tv == NULL)
		return;

	gemtk_wm_get_grect(tv->window, GEMTK_WM_AREA_CONTENT, &work);
	slid = gemtk_wm_get_scroll_info(tv->window);
	mx = ev_out->emo_mouse.p_x;
	my = ev_out->emo_mouse.p_y;

	/* mouse click relative origin: */

	short origin_rel_x = (mx-work.g_x) +
							(slid->x_pos*slid->x_unit_px);
	short origin_rel_y = (my-work.g_y) +
							(slid->y_pos*slid->y_unit_px);

	if( origin_rel_x >= 0 && origin_rel_y >= 0
		&& mx < work.g_x + work.g_w
		&& my < work.g_y + work.g_h )
	{
		int bms;
		bool ignore=false;
		short cur_rel_x, cur_rel_y, dummy, mbut;

		if (ev_out->emo_mclicks == 2) {
			tree_mouse_action(tv->tree,
							BROWSER_MOUSE_CLICK_1 | BROWSER_MOUSE_DOUBLE_CLICK,
							origin_rel_x, origin_rel_y );
			return;
		}

		graf_mkstate(&cur_rel_x, &cur_rel_x, &mbut, &dummy);
		if( (mbut&1) == 0 ){
			bms = BROWSER_MOUSE_CLICK_1 | BROWSER_MOUSE_PRESS_1;
			if(ev_out->emo_mclicks == 2 ) {
				bms = BROWSER_MOUSE_DOUBLE_CLICK;
			}
			tree_mouse_action(tv->tree, bms, origin_rel_x, origin_rel_y );
		} else {
			/* button still pressed */

			short prev_x = origin_rel_x;
			short prev_y = origin_rel_y;

			cur_rel_x = origin_rel_x;
			cur_rel_y = origin_rel_y;

			gem_set_cursor(&gem_cursors.hand);

			tv->startdrag.x = origin_rel_x;
			tv->startdrag.y = origin_rel_y;

			tree_mouse_action( tv->tree,
								BROWSER_MOUSE_DRAG_1 | BROWSER_MOUSE_DRAG_ON ,
								cur_rel_x, cur_rel_y );
			do{
				if( abs(prev_x-cur_rel_x) > 5 || abs(prev_y-cur_rel_y) > 5 ){
					tree_mouse_action( tv->tree,
								BROWSER_MOUSE_HOLDING_1 | BROWSER_MOUSE_DRAG_ON,
								cur_rel_x, cur_rel_y);
					prev_x = cur_rel_x;
					prev_y = cur_rel_y;
				}

				if( tv->redraw )
					atari_treeview_redraw( tv );
				/* sample mouse button state: */
				graf_mkstate(&cur_rel_x, &cur_rel_y, &mbut, &dummy);
				cur_rel_x = (cur_rel_x-work.g_x)+(slid->x_pos*slid->x_unit_px);
				cur_rel_y = (cur_rel_y-work.g_y)+(slid->y_pos*slid->y_unit_px);
			} while( mbut & 1 );

			tree_drag_end(tv->tree, 0, tv->startdrag.x, tv->startdrag.y,
							cur_rel_x, cur_rel_y );
			gem_set_cursor(&gem_cursors.arrow);
		}
	}
}

NSTREEVIEW atari_treeview_create(uint32_t flags, GUIWIN *win,
								gemtk_wm_event_handler_f user_func)
{
	struct gemtk_wm_scroll_info_s *slid;

	if( win == NULL )
		return( NULL );
	NSTREEVIEW new = malloc(sizeof(struct atari_treeview));
	if (new == NULL)
		return NULL;
	memset( new, 0, sizeof(struct atari_treeview));
	new->tree = tree_create(flags, &atari_tree_callbacks, new);
	if (new->tree == NULL) {
		free(new);
		return NULL;
	}
	new->window = win;
	new->user_func = user_func;

	gemtk_wm_set_event_handler(win, handle_event);
	gemtk_wm_set_user_data(win, (void*)new);

	slid = gemtk_wm_get_scroll_info(new->window);
	slid->y_unit_px = 16;
	slid->x_unit_px = 16;

	return(new);
}

void atari_treeview_open( NSTREEVIEW tv )
{
	if( tv->window != NULL ) {
		gemtk_wm_link(tv->window);

	}
}

void atari_treeview_close(NSTREEVIEW tv)
{
	if(tv->window != NULL) {
		gemtk_wm_unlink(tv->window);
	}
}

void atari_treeview_destroy( NSTREEVIEW tv )
{
	if( tv != NULL ){
		tv->disposing = true;
		LOG(("tree: %p", tv));
		if( tv->tree != NULL ) {
			tree_delete(tv->tree);
			tv->tree = NULL;
		}
		free( tv );
	}
}

bool atari_treeview_mevent( NSTREEVIEW tv, browser_mouse_state bms, int x, int y)
{
	GRECT work;
	struct gemtk_wm_scroll_info_s *slid;

	if( tv == NULL )
		return ( false );

	gemtk_wm_get_grect(tv->window, GEMTK_WM_AREA_CONTENT, &work);
	slid = gemtk_wm_get_scroll_info(tv->window);

	int rx = (x-work.g_x)+(slid->x_pos*slid->x_unit_px);
	int ry = (y-work.g_y)+(slid->y_pos*slid->y_unit_px);

	tree_mouse_action(tv->tree, bms, rx, ry);

	tv->click.x = rx;
	tv->click.y = ry;

	return( true );
}



void atari_treeview_redraw(NSTREEVIEW tv)
{
	if (tv != NULL) {
		if( tv->redraw && ((atari_plot_flags & PLOT_FLAG_OFFSCREEN) == 0) ) {

			short todo[4];
			GRECT work;
			short handle = gemtk_wm_get_handle(tv->window);
			struct gemtk_wm_scroll_info_s *slid;

			gemtk_wm_get_grect(tv->window, GEMTK_WM_AREA_CONTENT, &work);
			slid = gemtk_wm_get_scroll_info(tv->window);

			struct redraw_context ctx = {
				.interactive = true,
				.background_images = true,
				.plot = &atari_plotters
			};
			plot_set_dimensions(work.g_x, work.g_y, work.g_w, work.g_h);
			if (plot_lock() == false)
				return;

			if( wind_get(handle, WF_FIRSTXYWH,
							&todo[0], &todo[1], &todo[2], &todo[3] )!=0 ) {
				while (todo[2] && todo[3]) {

					short pxy[4];
					pxy[0] = todo[0];
					pxy[1] = todo[1];
					pxy[2] = todo[0] + todo[2]-1;
					pxy[3] = todo[1] + todo[3]-1;
					vs_clip(atari_plot_vdi_handle, 1, (short*)&pxy);

					/* convert screen to treeview coords: */
					todo[0] = todo[0] - work.g_x + slid->x_pos*slid->x_unit_px;
					todo[1] = todo[1] - work.g_y + slid->y_pos*slid->y_unit_px;
					if( todo[0] < 0 ){
						todo[2] = todo[2] + todo[0];
						todo[0] = 0;
					}
					if( todo[1] < 0 ){
						todo[3] = todo[3] + todo[1];
						todo[1] = 0;
					}

					// TODO: get slider values
					if (rc_intersect((GRECT *)&tv->rdw_area,(GRECT *)&todo)) {
						tree_draw(tv->tree, -(slid->x_pos*slid->x_unit_px),
										-(slid->y_pos*slid->y_unit_px),
							todo[0], todo[1], todo[2], todo[3], &ctx
						);
					}
					vs_clip(atari_plot_vdi_handle, 0, (short*)&pxy);
					if (wind_get(handle, WF_NEXTXYWH,
							&todo[0], &todo[1], &todo[2], &todo[3])==0) {
						break;
					}
				}
			} else {
				plot_unlock();
				return;
			}
			plot_unlock();
			tv->redraw = false;
			tv->rdw_area.g_x = 65000;
			tv->rdw_area.g_y = 65000;
			tv->rdw_area.g_w = -1;
			tv->rdw_area.g_h = -1;
		} else {
			/* just copy stuff from the offscreen buffer */
		}
	}
}


/**
 * Callback to force a redraw of part of the treeview window.
 *
 * \param  x		Min X Coordinate of area to be redrawn.
 * \param  y		Min Y Coordinate of area to be redrawn.
 * \param  width	Width of area to be redrawn.
 * \param  height	Height of area to be redrawn.
 * \param  pw		The treeview object to be redrawn.
 */
void atari_treeview_request_redraw(int x, int y, int w, int h, void *pw)
{
	if ( pw != NULL ) {
		NSTREEVIEW tv = (NSTREEVIEW) pw;
		if( tv->redraw == false ){
			tv->redraw = true;
			tv->rdw_area.g_x = x;
			tv->rdw_area.g_y = y;
			tv->rdw_area.g_w = w;
			tv->rdw_area.g_h = h;
		} else {
			/* merge the redraw area to the new area.: */
			int newx1 = x+w;
			int newy1 = y+h;
			int oldx1 = tv->rdw_area.g_x + tv->rdw_area.g_w;
			int oldy1 = tv->rdw_area.g_y + tv->rdw_area.g_h;
			tv->rdw_area.g_x = MIN(tv->rdw_area.g_x, x);
			tv->rdw_area.g_y = MIN(tv->rdw_area.g_y, y);
			tv->rdw_area.g_w = ( oldx1 > newx1 ) ? oldx1 - tv->rdw_area.g_x : newx1 - tv->rdw_area.g_x;
			tv->rdw_area.g_h = ( oldy1 > newy1 ) ? oldy1 - tv->rdw_area.g_y : newy1 - tv->rdw_area.g_y;
		}
		// dbg_grect("atari_treeview_request_redraw", &tv->rdw_area);
	}
}


/**
 * Callback to notify us of a new overall tree size.
 *
 * \param  tree		The tree being resized.
 * \param  width	The new width of the window.
 * \param  height	The new height of the window.
 * \param  *pw		The treeview object to be resized.
 */

void atari_treeview_resized(struct tree *tree, int width, int height, void *pw)
{
	GRECT area;
	if (pw != NULL) {
		NSTREEVIEW tv = (NSTREEVIEW) pw;
		if( tv->disposing )
			return;
		tv->extent.x = width;
		tv->extent.y = height;
		struct gemtk_wm_scroll_info_s *slid = gemtk_wm_get_scroll_info(tv->window);
		gemtk_wm_get_grect(tv->window, GEMTK_WM_AREA_CONTENT, &area);
		slid->x_units = (width/slid->x_unit_px);
		slid->y_units = (height/slid->y_unit_px);
		/*printf("units content: %d, units viewport: %d\n", (height/slid->y_unit_px),
					(area.g_h/slid->y_unit_px));*/
		gemtk_wm_update_slider(tv->window, GEMTK_WM_VH_SLIDER);
	}
}


/**
 * Callback to request that a section of the tree is scrolled into view.
 *
 * \param  y			The Y coordinate of top of the area in NS units.
 * \param  height		The height of the area in NS units.
 * \param  *pw			The treeview object affected.
 */

void atari_treeview_scroll_visible(int y, int height, void *pw)
{
	/* we don't support dragging outside the treeview */
	/* so we don't need to implement this? */
}

static void atari_treeview_get_grect(NSTREEVIEW tv, enum treeview_area_e mode,
									GRECT *dest)
{

	if (mode == TREEVIEW_AREA_CONTENT) {
		gemtk_wm_get_grect(tv->window, GEMTK_WM_AREA_CONTENT, dest);
	}
	else if (mode == TREEVIEW_AREA_TOOLBAR) {
		gemtk_wm_get_grect(tv->window, GEMTK_WM_AREA_TOOLBAR, dest);
	}
}

/**
 * Callback to return the tree window dimensions to the treeview system.
 *
 * \param  *width		Return the window width.
 * \param  *height		Return the window height.
 * \param  *pw			The treeview object to use.
 */

void atari_treeview_get_dimensions(int *width, int *height,
		void *pw)
{
	if (pw != NULL && (width != NULL || height != NULL)) {
		NSTREEVIEW tv = (NSTREEVIEW) pw;
		GRECT work;
		atari_treeview_get_grect(tv, TREEVIEW_AREA_CONTENT, &work);
		*width = work.g_w;
		*height = work.g_h;
	}
}
