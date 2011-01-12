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
#ifndef _GEM_PLOTTER_API_H_
#define _GEM_PLOTTER_API_H_
#include <windom.h>



#ifndef ceilf
#define ceilf(x) (float)ceil((double)x)
#endif

#ifdef TEST_PLOTTER
#define verbose_log 1
#define LOG(x) do { if (verbose_log) (printf(__FILE__ " %s %i: ", __PRETTY_FUNCTION__, __LINE__), printf x, fputc('\n', stdout)); } while (0)
#endif

#define MAX_FRAMEBUFS 0x010
#define C2P (1<<0)	/* C2P convert buffer 1 to buffer 2 */
/* TODO: implement offscreen buffer switch */
/* Plotter Flags: */
#define PLOT_FLAG_OFFSCREEN 0x01
#define PLOT_FLAG_LOCKED 	0x02
#define PLOT_FLAG_DITHER 	0x04
#define PLOT_FLAG_TRANS		0x08

/* Error codes: */
#define ERR_BUFFERSIZE_EXCEEDS_SCREEN 1
#define ERR_NO_MEM 2
#define ERR_PLOTTER_NOT_AVAILABLE 3

static const char * plot_error_codes[] =
{
	"None",
	"ERR_BUFFERSIZE_EXCEEDS_SCREEN",
	"ERR_NO_MEM",
	"ERR_PLOTTER_NOT_AVAILABLE"
};

/* Grapics & Font Plotter Objects: */
typedef struct s_font_plotter * FONT_PLOTTER;
typedef struct s_gem_plotter * GEM_PLOTTER;
typedef struct s_font_plotter * GEM_FONT_PLOTTER; /* for public use ... */
struct s_font_plotter
{
	char * name;
	int flags;
	int vdi_handle;
	void * priv_data;
	GEM_PLOTTER plotter;
	
	bool (*str_width)(FONT_PLOTTER self, const plot_font_style_t *fstyle,
						const char * str, size_t length, int * width);
	bool (*str_split)(FONT_PLOTTER self, const plot_font_style_t *fstyle,
						const char *string, size_t length,
						int x, size_t *char_offset, int *actual_x);
	bool (*pixel_position)(FONT_PLOTTER self, const plot_font_style_t *fstyle,
						const char *string, size_t length,
						int x, size_t *char_offset, int *actual_x);
	void (*text)(FONT_PLOTTER self, int x, int y, const char *text, size_t length, const plot_font_style_t *fstyle);
	void (*dtor)(FONT_PLOTTER self );
};


struct s_clipping {
	short x0;
	short y0;
	short x1;
	short y1;
};

struct s_vdi_sysinfo {
	short vdi_handle;			/* vdi handle 					*/
	short scr_w;				/* resolution horz. 			*/
	short scr_h;				/* resolution vert. 			*/
	short scr_bpp;				/* bits per pixel 				*/
	int colors;					/* 0=hiclor, 2=mono				*/
	unsigned long hicolors;		/* if colors = 0				*/
	short pixelsize;			/* bytes per pixel 				*/
	unsigned short pitch;		/* row pitch 					*/
	unsigned short vdiformat;	/* pixel format 				*/
	unsigned short clut;		/* type of clut support 		*/
	void * screen;				/* pointer to screen, or NULL	*/
	unsigned long  screensize;	/* size of screen (in bytes)	*/
	unsigned long  mask_r;		/* color masks 					*/
	unsigned long  mask_g;
	unsigned long  mask_b;
	unsigned long  mask_a;
	short maxintin;				/* maximum pxy items 			*/
	short maxpolycoords;		/* max coords for p_line etc.	*/
	unsigned long EdDiVersion;	/* EdDi Version or 0 			*/
	bool rasterscale;			/* raster scaling support		*/
};



struct s_frame_buf
{
	short x;
	short y;
	short w;
	short h;
	short vis_x;	/* visible rectangle of the screen buffer */
	short vis_y;	/* coords are relative to framebuffer location */
	short vis_w;
	short vis_h;
	int size;
	bool swapped;
	void * mem;
};


struct s_gem_plotter
{
	char * name;         /* name that identifies the Plotter */
	unsigned long flags;
	int vdi_handle;
	struct s_vdi_sysinfo * scr;
	void * priv_data;
	int bpp_virt;     	/* bit depth of framebuffer */
	struct s_clipping clipping;
	struct s_frame_buf fbuf[MAX_FRAMEBUFS];
	int cfbi; 			/* current framebuffer index */

	FONT_PLOTTER font_plotter;
	int (*dtor)(GEM_PLOTTER self);
	int (*resize)(GEM_PLOTTER self, int w, int h);
	int (*move)(GEM_PLOTTER self, short x, short y );
	void * (*lock)(GEM_PLOTTER self);
	void * (*create_framebuffer)(GEM_PLOTTER self);
	void * (*switch_to_framebuffer)(GEM_PLOTTER self);
	int (*unlock)(GEM_PLOTTER self);
	int (*update_region)(GEM_PLOTTER self, GRECT region);
	int (*update_screen_region)( GEM_PLOTTER self, GRECT region );
	int (*update_screen)(GEM_PLOTTER self);
	int (*put_pixel)(GEM_PLOTTER self, int x, int y, int color );
	int (*copy_rect)(GEM_PLOTTER self, GRECT src, GRECT dst );
	int (*clip)(GEM_PLOTTER self, int x0, int y0, int x1, int y1);
	int (*arc)(GEM_PLOTTER self, int x, int y, int radius, int angle1, int angle2, const plot_style_t * pstyle);
	int (*disc)(GEM_PLOTTER self, int x, int y, int radius, const plot_style_t * pstyle);
	int (*line)(GEM_PLOTTER self, int x0, int y0, int x1,	int y1, const plot_style_t * pstyle);
	int (*rectangle)(GEM_PLOTTER self, int x0, int y0, int x1, int y1, const plot_style_t * pstyle);
	int (*polygon)(GEM_PLOTTER self, const int *p, unsigned int n,  const plot_style_t * pstyle);
	int (*path)(GEM_PLOTTER self, const float *p, unsigned int n, int fill, float width, int c, const float transform[6]);
	int (*bitmap_resize) ( GEM_PLOTTER self, struct bitmap * bm, int nw, int nh );
	int (*bitmap)(GEM_PLOTTER self, struct bitmap * bmp, int x, int y,
					unsigned long bg, unsigned long flags );
	int (*text)(GEM_PLOTTER self, int x, int y, const char *text, size_t length, const plot_font_style_t *fstyle);
};


struct s_driver_table_entry
{
	char * name;
	int (*ctor)( GEM_PLOTTER self );
	int flags;
	int max_bpp;
};

struct s_font_driver_table_entry
{
	char * name;
	int (*ctor)( FONT_PLOTTER self );
	int flags;
};

typedef struct s_driver_table_entry * PLOTTER_INFO;
typedef struct s_font_driver_table_entry * FONT_PLOTTER_INFO;

/* get index to driver in driver list by name */
static int drvrname_idx( char * name );

/* get s_driver_table_entry from driver table */
struct s_driver_table_entry * get_screen_driver_entry(char * name);

/* get s_font_driver_table_entry from driver table */
struct s_font_driver_table_entry * get_font_driver_entry(char * name);

/* fill screen / sys info */
struct s_vdi_sysinfo * read_vdi_sysinfo(short vdih, struct s_vdi_sysinfo * info );

/*
   Create an new plotter object
   Error Values:
      -1 no mem
      -2 error configuring plotter
      -3 Plotter not available
*/
GEM_PLOTTER new_plotter(int vdihandle, char * name,
	GRECT *, int virt_bpp, unsigned long flags, FONT_PLOTTER font_renderer,
	int * error);

/*
   Create an new font plotter object
   Error Values:
      -1 no mem
      -2 error configuring font plotter
      -3 Font Plotter not available
*/
FONT_PLOTTER new_font_plotter(int vdihandle, char * name, unsigned long flags, int * error );

/* free the plotter resources */
int delete_plotter( GEM_PLOTTER p );
int delete_font_plotter( FONT_PLOTTER p );


/* calculate size of intermediate buffer */
int calc_chunked_buffer_size(int x, int y, int stride, int bpp);

/* calculates the pixel offset from x,y pos */
int get_pixel_offset( int x, int y, int stride, int bpp );

/* Recalculate visible parts of the framebuffer */
void update_visible_rect( GEM_PLOTTER p );

/* resolve possible visible parts of the framebuffer in screen coords */
bool fbrect_to_screen( GEM_PLOTTER self, GRECT box, GRECT * ret );

/* translate an error number */
const char* plotter_err_str(int i) ;

void dump_font_drivers(void);
void dump_plot_drivers(void);
void dump_vdi_info(short);

/* convert an rgb color to vdi1000 color */
void rgb_to_vdi1000( unsigned char * in, unsigned short * out );

/* convert an rgb color to an index into the web palette */
short rgb_to_666_index(unsigned char r, unsigned char g, unsigned char b);

/* shared / static methods ... */
int plotter_get_clip( GEM_PLOTTER self, struct s_clipping * out );
int plotter_std_clip(GEM_PLOTTER self,int x0, int y0, int x1, int y1);
void plotter_vdi_clip( GEM_PLOTTER self, bool set);

#define PLOTTER_IS_LOCKED(plotter) ( plotter->private_flags & PLOTTER_FLAG_LOCKED )
#define FILL_PLOTTER_VTAB( p ) \
   p->dtor = dtor;\
   p->resize= resize;\
   p->move = move;\
   p->lock = lock;\
   p->unlock = unlock;\
   p->update_region = update_region;\
   p->update_screen_region = update_screen_region;\
   p->update_screen = update_screen;\
   p->put_pixel = put_pixel;\
   p->copy_rect = copy_rect; \
   p->clip = clip;\
   p->arc = arc;\
   p->disc = disc;\
   p->line = line;\
   p->rectangle = rectangle;\
   p->polygon = polygon;\
   p->path = path;\
   p->bitmap = bitmap;\
   p->text = text;\


#define FILL_FONT_PLOTTER_VTAB( p ) \
	p->dtor = dtor;\
	p->str_width = str_width;\
	p->str_split = str_split;\
	p->pixel_position = pixel_position;\
	p->text = text;\

#define CURFB( p ) \
	p->fbuf[p->cfbi]

#define FIRSTFB( p ) \
	p->fbuf[0]

#define OFFSET_WEB_PAL 16
#define OFFSET_FONT_PAL 232
#define RGB_TO_VDI(c) rgb_to_666_index( (c&0xFF),(c&0xFF00)>>8,(c&0xFF0000)>>16)+OFFSET_WEB_PAL
#define ABGR_TO_RGB(c)  ( ((c&0xFF)<<16) | (c&0xFF00) | ((c&0xFF0000)>>16) ) << 8 

#endif