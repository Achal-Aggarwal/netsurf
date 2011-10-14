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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <windom.h>
#include <assert.h>
#include <math.h>
#include <osbind.h>

#include "utils/log.h"
#include "desktop/gui.h"
#include "desktop/history_core.h"
#include "desktop/netsurf.h"
#include "desktop/browser.h"
#include "desktop/mouse.h"
#include "desktop/plotters.h"
#include "desktop/textinput.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "css/css.h"
#include "render/box.h"
#include "render/form.h"
#include "atari/gui.h"
#include "atari/browser_win.h"
#include "atari/browser.h"
#include "atari/misc.h"
#include "atari/global_evnt.h"
#include "atari/res/netsurf.rsh"
#include "atari/browser.h"
#include "atari/toolbar.h"
#include "atari/statusbar.h"
#include "atari/plot/plotter.h"
#include "atari/dragdrop.h"
#include "atari/search.h"
#include "atari/osspec.h"

extern void * h_gem_rsrc;
extern struct gui_window *input_window;
extern GEM_PLOTTER plotter;
extern int mouse_click_time[3];
extern int mouse_hold_start[3];
extern browser_mouse_state bmstate;
extern short last_drag_x;
extern short last_drag_y;

void __CDECL std_szd( WINDOW * win, short buff[8], void * );
void __CDECL std_mvd( WINDOW * win, short buff[8], void * );

/* -------------------------------------------------------------------------- */
/* Module public functions:                                                   */
/* -------------------------------------------------------------------------- */


static void __CDECL evnt_window_arrowed( WINDOW *win, short buff[8], void *data )
{
	bool abs = false;
	LGRECT cwork;
	int value = BROWSER_SCROLL_SVAL;

	if( input_window == NULL ) {
		return;
	}

	browser_get_rect( input_window, BR_CONTENT, &cwork );

	switch( buff[4] ) {
		case WA_UPPAGE:
		case WA_DNPAGE:
				value = cwork.g_h;
			break;


		case WA_LFPAGE:
		case WA_RTPAGE:
				value = cwork.g_w;
			break;

		default:
			break;
	}
	browser_scroll( input_window, buff[4], value, abs );
}

/*
	track the mouse state and
	finally checks for released buttons.
 */
static void window_track_mouse_state( LGRECT * bwrect, bool within, short mx, short my, short mbut, short mkstate ){
	int i = 0;
	int nx, ny;
	struct gui_window * gw = input_window;

	if( !gw ) {
		bmstate = 0;
		mouse_hold_start[0] = 0;
		mouse_hold_start[1] = 0;
		return;
	}

	/* todo: creat function find_browser_window( mx, my ) */
	nx = (mx - bwrect->g_x + gw->browser->scroll.current.x);
	ny = (my - bwrect->g_y + gw->browser->scroll.current.y);

	if( mkstate & (K_RSHIFT | K_LSHIFT) ){
		bmstate |= BROWSER_MOUSE_MOD_1;
	} else {
		bmstate &= ~(BROWSER_MOUSE_MOD_1);
	}
	if( (mkstate & K_CTRL) ){
		bmstate |= BROWSER_MOUSE_MOD_2;
	} else {
		bmstate &= ~(BROWSER_MOUSE_MOD_2);
	}
	if( (mkstate & K_ALT) ){
		bmstate |= BROWSER_MOUSE_MOD_3;
	} else {
		bmstate &= ~(BROWSER_MOUSE_MOD_3);
	}

	if( !(mbut&1) && !(mbut&2) ) {
		if(bmstate & BROWSER_MOUSE_DRAG_ON )
			bmstate &= ~( BROWSER_MOUSE_DRAG_ON );
	}

	/* todo: if we need right button click, increase loop count */
	for( i = 1; i<2; i++ ) {
		if( !(mbut & i) ) {
			if( mouse_hold_start[i-1] > 0 ) {
				mouse_hold_start[i-1] = 0;
				if( i==1 ) {
					LOG(("Drag for %d ended", i));
					bmstate &= ~( BROWSER_MOUSE_HOLDING_1 | BROWSER_MOUSE_DRAG_1 ) ;
					if( within ) {
						/* drag end */
						browser_window_mouse_track(
							gw->browser->bw, 0, nx, ny
						);
					}
				}
				if( i==2 ) {
					bmstate &= ~( BROWSER_MOUSE_HOLDING_2 | BROWSER_MOUSE_DRAG_2 ) ;
					LOG(("Drag for %d ended", i));
					if( within ) {
						/* drag end */
						browser_window_mouse_track(
							gw->browser->bw, 0, nx, ny
						);
					}
				}
			}
		}
	}
}


static void __CDECL evnt_window_m1( WINDOW * win, short buff[8], void * data)
{
	struct gui_window * gw = input_window;
	static bool prev_url = false;
	static bool prev_sb = false;
	short mx, my, mbut, mkstate;
	bool a = false;	/* flags if mouse is within controls or browser canvas */
	bool within = false;
	LGRECT urlbox, bwbox, sbbox;
	int nx, ny; 	/* relative mouse position */


	if( gw == NULL)
		return;

	if( gw != input_window ){
		return;
	}

	graf_mkstate(&mx, &my, &mbut, &mkstate);

	browser_get_rect( gw, BR_CONTENT, &bwbox );
	if( gw->root->toolbar )
		mt_CompGetLGrect(&app, gw->root->toolbar->url.comp, WF_WORKXYWH, &urlbox);
	if( gw->root->statusbar )
		mt_CompGetLGrect(&app, gw->root->statusbar->comp, WF_WORKXYWH, &sbbox);

	if( mx > bwbox.g_x && mx < bwbox.g_x + bwbox.g_w &&
		my > bwbox.g_y &&  my < bwbox.g_y + bwbox.g_h ){
		within = true;
	}

	if( evnt.m1_flag == MO_LEAVE ) {
		if( MOUSE_IS_DRAGGING() ){
			window_track_mouse_state( &bwbox, within, mx, my, mbut, mkstate );
		}
		if( gw->root->toolbar && within == false ) {
			if( (mx > urlbox.g_x && mx < urlbox.g_x + urlbox.g_w ) &&
			 	(my > urlbox.g_y && my < + urlbox.g_y + urlbox.g_h )) {
				gem_set_cursor( &gem_cursors.ibeam );
				prev_url = a = true;
			}
		}
		if( gw->root->statusbar && within == false /* && a == false */ ) {
			if( mx >= sbbox.g_x + (sbbox.g_w-MOVER_WH) && mx <= sbbox.g_x + sbbox.g_w &&
				my >= sbbox.g_y + (sbbox.g_h-MOVER_WH) && my <= sbbox.g_y + sbbox.g_h ) {
				/* mouse within sizer box ( bottom right ) */
				prev_sb = a =  true;
				gem_set_cursor( &gem_cursors.sizenwse );
			}
		}
		if( !a ) {
			if( prev_sb )
				gw->root->statusbar->resize_init = true;
			if( prev_url || prev_sb ) {
				gem_set_cursor( &gem_cursors.arrow );
				prev_url = false;
				prev_sb = false;
			}
			/* report mouse move in the browser window */
			if( within ){
				nx = mx - bwbox.g_x;
				ny = my - bwbox.g_y;
				if( ( abs(mx-last_drag_x)>5 || abs(mx-last_drag_y)>5 ) ||
					!MOUSE_IS_DRAGGING() ){
					browser_window_mouse_track(
						input_window->browser->bw,
						bmstate,
						nx + gw->browser->scroll.current.x,
						ny + gw->browser->scroll.current.y
					);
					if( MOUSE_IS_DRAGGING() ){
						last_drag_x = mx;
						last_drag_y = my;
					}
				}
			}
		}
	} else {
		/* set input window? */
	}
}

int window_create( struct gui_window * gw, struct browser_window * bw, unsigned long inflags)
{
	short buff[8];
	OBJECT * tbtree;
	int err = 0;
	bool tb, sb;
	tb = (inflags & WIDGET_TOOLBAR );
	sb = (inflags & WIDGET_STATUSBAR);
	short w,h, wx, wy, wh, ww;
	int flags = CLOSER | MOVER | NAME | FULLER | SMALLER ;

	gw->root = malloc( sizeof(struct s_gui_win_root) );
	if( gw->root == NULL )
		return( -1 );
	memset( gw->root, 0, sizeof(struct s_gui_win_root) );
	gw->root->title = malloc(atari_sysinfo.aes_max_win_title_len+1);
	gw->root->handle = WindCreate( flags,40, 40, app.w, app.h );
	if( gw->root->handle == NULL ) {
		free( gw->root->title );
		free( gw->root );
		return( -1 );
	}
	gw->root->cmproot = mt_CompCreate(&app, CLT_VERTICAL, 1, 1);
	WindSetPtr( gw->root->handle, WF_COMPONENT, gw->root->cmproot, NULL);

	if( tb ) {
		gw->root->toolbar = tb_create( gw );
		assert( gw->root->toolbar );
		mt_CompAttach( &app, gw->root->cmproot, gw->root->toolbar->comp );

	} else {
		gw->root->toolbar = NULL;
	}

	gw->browser = browser_create( gw, bw, NULL, CLT_HORIZONTAL, 1, 1 );
	mt_CompAttach( &app, gw->root->cmproot,  gw->browser->comp );

	if( sb ) {
		gw->root->statusbar = sb_create( gw );
		mt_CompAttach( &app, gw->root->cmproot, gw->root->statusbar->comp );
	} else {
		gw->root->statusbar = NULL;
	}

	WindSetStr(gw->root->handle, WF_ICONTITLE, (char*)"NetSurf");

	/* Event Handlers: */
	EvntDataAttach( gw->root->handle, WM_CLOSED, evnt_window_close, gw );
	/* capture resize/move events so we can handle that manually */
	EvntDataAttach( gw->root->handle, WM_SIZED, evnt_window_resize, gw );
	if( !option_atari_realtime_move ) {
		EvntDataAttach( gw->root->handle, WM_MOVED, evnt_window_move, gw );
	} else {
		EvntDataAdd( gw->root->handle, WM_MOVED, evnt_window_rt_resize, gw, EV_BOT );
	}
	EvntDataAttach( gw->root->handle, WM_FORCE_MOVE, evnt_window_rt_resize, gw );
	EvntDataAttach( gw->root->handle, AP_DRAGDROP, evnt_window_dd, gw );
	EvntDataAdd( gw->root->handle, WM_DESTROY,evnt_window_destroy, gw, EV_TOP );
	EvntDataAdd( gw->root->handle, WM_ARROWED,evnt_window_arrowed, gw, EV_TOP );
	EvntDataAdd( gw->root->handle, WM_NEWTOP, evnt_window_newtop, gw, EV_BOT);
	EvntDataAdd( gw->root->handle, WM_TOPPED, evnt_window_newtop, gw, EV_BOT);
	EvntDataAttach( gw->root->handle, WM_ICONDRAW, evnt_window_icondraw, gw);
	EvntDataAttach( gw->root->handle, WM_XM1, evnt_window_m1, gw );

	/*
	OBJECT * tbut;
	RsrcGaddr( h_gem_rsrc, R_TREE, FAVICO , &tbut );
	window_set_icon(gw, &tbut[]);
	*/
	/* TODO: check if window is openend as "foreground" window... */
	window_set_focus( gw, BROWSER, gw->browser);
	return (err);
}

int window_destroy( struct gui_window * gw)
{
	short buff[8];
	int err = 0;

	search_destroy( gw );
	if( input_window == gw )
		input_window = NULL;

	if( gw->root ) {
		window_set_icon( gw, NULL );
		if( gw->root->toolbar )
			tb_destroy( gw->root->toolbar );

		if( gw->root->statusbar )
			sb_destroy( gw->root->statusbar );
	}

	search_destroy( gw );

	LOG(("Freeing browser window"));
	if( gw->browser )
		browser_destroy( gw->browser );


	/* destroy the icon: */
	/*window_set_icon(gw, NULL, false );*/

	/* needed? */ /*listRemove( (LINKABLE*)gw->root->cmproot ); */
	LOG(("Freeing root window"));
	if( gw->root ) {
		/* TODO: check if no other browser is bound to this root window! */
		if( gw->root->title )
			free( gw->root->title );
		if( gw->root->cmproot )
			mt_CompDelete( &app, gw->root->cmproot );
		ApplWrite( _AESapid, WM_DESTROY, gw->root->handle->handle, 0, 0, 0, 0);
		EvntWindom( MU_MESAG );
		gw->root->handle = NULL;
		free( gw->root );
		gw->root = NULL;
	}
	return( err );
}



void window_open( struct gui_window * gw)
{
	LGRECT br;
	GRECT dim;
	WindOpen(gw->root->handle, 20, 20, app.w/2, app.h/2 );
	WindSetStr( gw->root->handle, WF_NAME, (char *)"" );
	/* apply focus to the root frame: */
	long lfbuff[8] = { CM_GETFOCUS };
	mt_CompEvntExec( gl_appvar, gw->browser->comp, lfbuff );
	/* recompute the nested component sizes and positions: */
	browser_update_rects( gw );
	mt_WindGetGrect( &app, gw->root->handle, WF_CURRXYWH, (GRECT*)&gw->root->loc);
	browser_get_rect( gw, BR_CONTENT, &br );
	plotter->move( plotter, br.g_x, br.g_y );
	plotter->resize( plotter, br.g_w, br.g_h );
	gw->browser->attached = true;
	if( gw->root->statusbar != NULL ){
		gw->root->statusbar->attached = true;
	}
	snd_rdw( gw->root->handle );
}


void window_set_icon(struct gui_window * gw, struct bitmap * bmp )
{
	/*
    if( gw->icon != NULL ){
        bitmap_destroy( gw->icon );
        gw->icon = NULL;
    }*/
    gw->icon = bmp;
}



/* update back forward buttons (see tb_update_buttons (bug) ) */
void window_update_back_forward( struct gui_window * gw)
{
	tb_update_buttons( gw );
}

static void window_redraw_controls(struct gui_window *gw, uint32_t flags)
{
	LGRECT rect;
	/* redraw sliders manually, dunno why this is needed (mt_WindSlider should do the job anytime)!*/

	browser_get_rect( gw, BR_VSLIDER, &rect);
	ApplWrite( _AESapid, WM_REDRAW,  gw->root->handle->handle,
		rect.g_x, rect.g_y, rect.g_w, rect.g_h );

	browser_get_rect( gw, BR_HSLIDER, &rect);
	ApplWrite( _AESapid, WM_REDRAW,  gw->root->handle->handle,
		rect.g_x, rect.g_y, rect.g_w, rect.g_h );

	/* send redraw to toolbar & statusbar & scrollbars: */
	mt_CompGetLGrect(&app, gw->root->toolbar->comp, WF_WORKXYWH, &rect);
	ApplWrite( _AESapid, WM_REDRAW,  gw->root->handle->handle,
		rect.g_x, rect.g_y, rect.g_w, rect.g_h );
	mt_CompGetLGrect(&app, gw->root->statusbar->comp, WF_WORKXYWH, &rect);
	ApplWrite( _AESapid, WM_REDRAW,  gw->root->handle->handle,
		rect.g_x, rect.g_y, rect.g_w, rect.g_h );
}

void window_set_stauts( struct gui_window * gw , char * text )
{
	if( gw->root == NULL )
		return;

	CMP_STATUSBAR sb = gw->root->statusbar;

	if( sb == NULL || gw->browser->attached == false )
		return;

	sb_set_text( sb, text );
}

/* set focus to an arbitary element */
void window_set_focus( struct gui_window * gw, enum focus_element_type type, void * element )
{
	if( gw->root->focus.type != type || gw->root->focus.element != element ) {
		LOG(("Set focus: %p (%d)\n", element, type));
		gw->root->focus.type = type;
		gw->root->focus.element = element;
	}
}

/* check if the url widget has focus */
bool window_url_widget_has_focus( struct gui_window * gw )
{
	assert( gw );
	assert( gw->root );
	if( gw->root->focus.type == URL_WIDGET && gw->root->focus.element != NULL ) {
		assert( ( &gw->root->toolbar->url == (struct s_url_widget*)gw->root->focus.element ) );
		assert( GUIWIN_VISIBLE(gw) );
		return true;
	}
	return false;
}

/* check if an arbitary window widget / or frame has the focus */
bool window_widget_has_focus( struct gui_window * gw, enum focus_element_type t, void * element )
{
	if( gw == NULL )
		return( false );
	if( element == NULL  ){
		assert( 1 != 0 );
		return( (gw->root->focus.type == t ) );
	}
	assert( gw->root != NULL );
	return( ( element == gw->root->focus.element && t == gw->root->focus.type) );
}

static void __CDECL evnt_window_dd( WINDOW *win, short wbuff[8], void * data )
{
	struct gui_window * gw = (struct gui_window *)data;
	char file[DD_NAMEMAX];
	char name[DD_NAMEMAX];
	char *buff=NULL;
	int dd_hdl;
	int dd_msg; /* pipe-handle */
	long size;
	char ext[32];
	short mx,my,bmstat,mkstat;
	graf_mkstate(&mx, &my, &bmstat, &mkstat);

	if( gw == NULL )
		return;
	if( (win->status & WS_ICONIFY))
		return;

	dd_hdl = ddopen( wbuff[7], DD_OK);
	if( dd_hdl<0)
		return;	/* pipe not open */
	memset( ext, 0, 32);
	strcpy( ext, "ARGS");
	dd_msg = ddsexts( dd_hdl, ext);
	if( dd_msg<0)
		goto error;
	dd_msg = ddrtry( dd_hdl, (char*)&name[0], (char*)&file[0], (char*)&ext[0], &size);
	if( size+1 >= PATH_MAX )
		goto error;
	if( !strncmp( ext, "ARGS", 4) && dd_msg > 0)
	{
		ddreply(dd_hdl, DD_OK);
		buff = (char*)alloca(sizeof(char)*(size+1));
		if( buff != NULL )
		{
			if( Fread(dd_hdl, size, buff ) == size)
			{
				buff[size] = 0;
			}
			LOG(("file: %s, ext: %s, size: %d dropped at: %d,%d\n",
				(char*)buff, (char*)&ext,
				size, mx, my
			));
			{
				int posx, posy;
				struct box *box;
				struct box *file_box = 0;
				hlcache_handle *h;
				int box_x, box_y;
				LGRECT bwrect;
				struct browser_window * bw = gw->browser->bw;
				h = bw->current_content;
				if (!bw->current_content || content_get_type(h) != CONTENT_HTML)
					return;
				browser_get_rect( gw, BR_CONTENT, &bwrect );
				mx = mx - bwrect.g_x;
				my = my - bwrect.g_y;
				if( (mx < 0 || mx > bwrect.g_w) || (my < 0 || my > bwrect.g_h) )
					return;
				box = html_get_box_tree(h);
				box_x = box->margin[LEFT];
				box_y = box->margin[TOP];

				while ((box = box_at_point(box, mx+gw->browser->scroll.current.x, my+gw->browser->scroll.current.y, &box_x, &box_y, &h)))
				{
					if (box->style && css_computed_visibility(box->style) == CSS_VISIBILITY_HIDDEN)
						continue;
					if (box->gadget)
					{
						switch (box->gadget->type)
						{
							case GADGET_FILE:
								file_box = box;
							break;
							/*
							TODO: handle these
							case GADGET_TEXTBOX:
							case GADGET_TEXTAREA:
							case GADGET_PASSWORD:
							text_box = box;
							break;
							*/
							default:
								break;
						}
					}
				} /* end While */
				if ( !file_box )
					return;
				if (file_box) {
					utf8_convert_ret ret;
					char *utf8_fn;

					ret = local_encoding_to_utf8( buff, 0, &utf8_fn);
					if (ret != UTF8_CONVERT_OK) {
						/* A bad encoding should never happen */
						LOG(("utf8_from_local_encoding failed"));
						assert(ret != UTF8_CONVERT_BADENC);
						/* Load was for us - just no memory */
						return;
					}
					/* Found: update form input. */
					free(file_box->gadget->value);
					file_box->gadget->value = utf8_fn;
					/* Redraw box. */
					box_coords(file_box, &posx, &posy);
					browser_schedule_redraw(bw->window,
						posx - gw->browser->scroll.current.x,
						posy - gw->browser->scroll.current.y,
						posx - gw->browser->scroll.current.x + file_box->width,
						posy - gw->browser->scroll.current.y + file_box->height);
				}
			}
		}
	}
error:
	ddclose( dd_hdl);
}

/* -------------------------------------------------------------------------- */
/* Non Public Modul event handlers:                                           */
/* -------------------------------------------------------------------------- */
static void __CDECL evnt_window_destroy( WINDOW *win, short buff[8], void *data )
{
	LOG(("%s\n", __FUNCTION__ ));
}

static void __CDECL evnt_window_close( WINDOW *win, short buff[8], void *data )
{
	struct gui_window * gw = (struct gui_window *) data ;
	if( gw != NULL ) {
		browser_window_destroy( gw->browser->bw );
	}
}


static void __CDECL evnt_window_newtop( WINDOW *win, short buff[8], void *data )
{
	input_window = (struct gui_window *) data;
	LOG(("newtop: iw: %p, win: %p", input_window, win ));
	assert( input_window != NULL );

	/* window_redraw_controls(input_window, 0); */
}

static void __CDECL evnt_window_shaded( WINDOW *win, short buff[8], void *data )
{
	if(buff[0] == WM_SHADED){
		LOG(("WM_SHADED, vis: %d, state: %d", GEMWIN_VISIBLE(win), win->status ));
	}
	if(buff[0] == WM_UNSHADED){

	}
}

static void __CDECL evnt_window_icondraw( WINDOW *win, short buff[8], void * data )
{
	short x,y,w,h;
	struct gui_window * gw = (struct gui_window*)data;

	LOG((""));

	WindClear( win);
	WindGet( win, WF_WORKXYWH, &x, &y, &w, &h);
	if( gw->icon == NULL ) {
		OBJECT * tree;
		RsrcGaddr( h_gem_rsrc, R_TREE, ICONIFY , &tree );
		tree->ob_x = x;
		tree->ob_y = y;
		tree->ob_width = w;
		tree->ob_height = h;
		mt_objc_draw( tree, 0, 8, buff[4], buff[5], buff[6], buff[7], app.aes_global );
	} else {
	    struct rect clip = { 0,0,w,h };
        plotter->move( plotter, x, y );
        plotter->resize( plotter, w, h );
        plotter->clip(plotter, &clip );
        plotter->bitmap_resize( plotter,  gw->icon, w, h  );
        plotter->bitmap(
			plotter,
			( gw->icon->resized ) ? gw->icon->resized : gw->icon,
			0, 0, 0xffffff, BITMAPF_NONE
		);
	}
}

static void __CDECL evnt_window_move( WINDOW *win, short buff[8], void * data )
{
	short mx,my, mb, ks;
	short wx, wy, wh, ww, nx, ny;
	short r;
	short xoff, yoff;
	if( option_atari_realtime_move  ) {
		std_mvd( win, buff, &app );
		evnt_window_rt_resize( win, buff, data );
	} else {
		wind_get( win->handle, WF_CURRXYWH, &wx, &wy, &ww, &wh );
		if( graf_dragbox( ww, wh, wx, wy, app.x-ww, app.y, app.w+ww, app.h+wh, &nx, &ny )){
			buff[4] = nx;
			buff[5] = ny;
			buff[6] = ww;
			buff[7] = wh;
			std_mvd( win, buff, &app );
			evnt_window_rt_resize( win, buff, data );
		}
	}
}

void __CDECL evnt_window_resize( WINDOW *win, short buff[8], void * data )
{
	short wx, wy, wh, ww, nw, nh;
	short r;

	wind_get( win->handle, WF_CURRXYWH, &wx, &wy, &ww, &wh );
	r = graf_rubberbox(wx, wy, 20, 20, &nw, &nh);
	if( nw < 40 && nw < 40 )
		return;
	buff[4] = wx;
	buff[5] = wy;
	buff[6] = nw;
	buff[7] = nh;
	std_szd( win, buff, &app );
	evnt_window_rt_resize( win, buff, data );
}

/* perform the actual resize */
static void __CDECL evnt_window_rt_resize( WINDOW *win, short buff[8], void * data )
{
	short x,y,w,h;
	struct gui_window * gw;
	LGRECT rect;

	if(buff[0] == WM_FORCE_MOVE ) {
		std_mvd(win, buff, &app);
		std_szd(win, buff, &app);
	}

	wind_get( win->handle, WF_CURRXYWH, &x, &y, &w, &h );
	gw = (struct gui_window *)data;

	assert( gw != NULL );

	if(gw->root->loc.g_w != w || gw->root->loc.g_h != h ){
		/* report resize to component interface: */
		browser_update_rects( gw );
		mt_WindGetGrect( &app, gw->root->handle, WF_CURRXYWH, (GRECT*)&gw->root->loc);
		browser_get_rect( gw, BR_CONTENT, &rect );
		if( gw->browser->bw->current_content != NULL )
			browser_window_reformat(gw->browser->bw, false, rect.g_w, rect.g_h );
		gw->root->toolbar->url.scrollx = 0;
		window_redraw_controls(gw, 0);
		/* TODO: recalculate scroll position, instead of zeroing? */
	} else {
		if(gw->root->loc.g_x != x || gw->root->loc.g_y != y ){
       			mt_WindGetGrect( &app, gw->root->handle, WF_CURRXYWH, (GRECT*)&gw->root->loc);
			browser_update_rects( gw );
        	}
	}
}
