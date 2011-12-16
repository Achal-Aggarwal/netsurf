/*
 * Copyright 2008 - 2011 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifdef __amigaos4__

#include <proto/popupmenu.h>
#include <proto/intuition.h>
#include <proto/asl.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include <reaction/reaction_macros.h>

#include "amiga/context_menu.h"
#include "amiga/clipboard.h"
#include "amiga/bitmap.h"
#include "amiga/file.h"
#include "amiga/filetype.h"
#include "amiga/gui.h"
#include "amiga/history_local.h"
#include "amiga/iff_dr2d.h"
#include "amiga/options.h"
#include "amiga/plugin_hack.h"
#include "amiga/theme.h"
#include "amiga/utf8.h"
#include "desktop/history_core.h"
#include "desktop/hotlist.h"
#include "desktop/selection.h"
#include "desktop/searchweb.h"
#include "desktop/textinput.h"
#include "render/box.h"
#include "render/form.h"
#include "utils/utf8.h"
#include "utils/messages.h"
#include "utils/utils.h"

#include <string.h>

static uint32 ami_context_menu_hook(struct Hook *hook,Object *item,APTR reserved);
static bool ami_context_menu_history(const struct history *history, int x0, int y0,
	int x1, int y1, const struct history_entry *entry, void *user_data);

static uint32 ami_popup_hook(struct Hook *hook,Object *item,APTR reserved);

enum {
	CMID_SELECTFILE,
	CMID_COPYURL,
	CMID_URLOPEN,
	CMID_URLOPENWIN,
	CMID_URLOPENTAB,
	CMID_URLHOTLIST,
	CMID_SAVEURL,
	CMID_SHOWOBJ,
	CMID_COPYOBJ,
	CMID_CLIPOBJ,
	CMID_SAVEOBJ,
	CMID_SAVEIFFOBJ,
	CMID_RELOADOBJ,
	CMID_SELALL,
	CMID_SELCLEAR,
	CMID_SELCUT,
	CMID_SELCOPY,
	CMID_SELPASTE,
	CMID_SELSEARCH,
	CMID_SELSAVE,
	CMID_FRAMEWIN,
	CMID_FRAMETAB,
	CMID_FRAMESHOW,
	CMID_FRAMERELOAD,
	CMID_FRAMECOPYURL,
	CMID_FRAMESAVE,
	CMID_FRAMESAVECOMPLETE,
	CMID_PLUGINCMD,
	CMID_NAVHOME,
	CMID_NAVBACK,
	CMID_NAVFORWARD,
	CMID_NAVRELOAD,
	CMID_NAVSTOP,
	CMID_PAGEOPEN,
	CMID_PAGESAVE,
	CMID_PAGESAVECOMPLETE,
	CMID_PAGEHOTLIST,
	CMID_PAGECLOSE,

	CMID_TREE_EXPAND,
	CMID_TREE_COLLAPSE,
	CMID_TREE_LAUNCH,
	CMID_TREE_NEWFOLDER,
	CMID_TREE_NEWITEM,
	CMID_TREE_SETDEFAULT,
	CMID_TREE_CLEARDEFAULT,
	CMID_TREE_DELETE,
	CMID_TREE_EDITLINK,
	CMID_TREE_EDITFOLDER,
	CMID_TREE_ADDHOTLIST,

	CMSUB_OBJECT,
	CMSUB_URL,
	CMSUB_SEL,
	CMSUB_PAGE,
	CMSUB_FRAME,
	CMSUB_NAVIGATE,
	CMID_HISTORY,
	CMID_LAST
};

struct Library  *PopupMenuBase = NULL;
struct PopupMenuIFace *IPopupMenu = NULL;
static char *ctxmenulab[CMID_LAST];
static Object *ctxmenuobj = NULL;
static struct Hook ctxmenuhook;

void ami_context_menu_init(void)
{
	if(PopupMenuBase = OpenLibrary("popupmenu.class",0))
	{
		IPopupMenu = (struct PopupMenuIFace *)GetInterface(PopupMenuBase,"main",1,NULL);
	}

	ctxmenulab[CMID_SELECTFILE] = ami_utf8_easy((char *)messages_get("SelectFile"));

	ctxmenulab[CMID_SHOWOBJ] = ami_utf8_easy((char *)messages_get("ObjShow"));
	ctxmenulab[CMID_RELOADOBJ] = ami_utf8_easy((char *)messages_get("ObjReload"));
	ctxmenulab[CMID_COPYOBJ] = ami_utf8_easy((char *)messages_get("CopyURL"));
	ctxmenulab[CMID_CLIPOBJ] = ami_utf8_easy((char *)messages_get("CopyClip"));
	ctxmenulab[CMID_SAVEOBJ] = ami_utf8_easy((char *)messages_get("SaveAs"));
	ctxmenulab[CMID_SAVEIFFOBJ] = ami_utf8_easy((char *)messages_get("SaveIFF"));

	ctxmenulab[CMID_PAGEOPEN] = ami_utf8_easy((char *)messages_get("OpenFile"));
	ctxmenulab[CMID_PAGESAVE] = ami_utf8_easy((char *)messages_get("SaveAs"));
	ctxmenulab[CMID_PAGESAVECOMPLETE] = ami_utf8_easy((char *)messages_get("SaveComplete"));
	ctxmenulab[CMID_PAGEHOTLIST] = ami_utf8_easy((char *)messages_get("HotlistAdd"));
	ctxmenulab[CMID_PAGECLOSE] = ami_utf8_easy((char *)messages_get("Close"));

	ctxmenulab[CMID_FRAMEWIN] = ami_utf8_easy((char *)messages_get("FrameNewWin"));
	ctxmenulab[CMID_FRAMETAB] = ami_utf8_easy((char *)messages_get("FrameNewTab"));
	ctxmenulab[CMID_FRAMESHOW] = ami_utf8_easy((char *)messages_get("FrameOnly"));
	ctxmenulab[CMID_FRAMESAVE] = ami_utf8_easy((char *)messages_get("SaveAs"));
	ctxmenulab[CMID_FRAMESAVECOMPLETE] = ami_utf8_easy((char *)messages_get("SaveComplete"));
	ctxmenulab[CMID_FRAMECOPYURL] = ami_utf8_easy((char *)messages_get("CopyURL"));
	ctxmenulab[CMID_FRAMERELOAD] = ami_utf8_easy((char *)messages_get("ObjReload"));

	ctxmenulab[CMID_SAVEURL] = ami_utf8_easy((char *)messages_get("LinkDload"));
	ctxmenulab[CMID_URLOPEN] = ami_utf8_easy((char *)messages_get("Open"));
	ctxmenulab[CMID_URLOPENWIN] = ami_utf8_easy((char *)messages_get("LinkNewWin"));
	ctxmenulab[CMID_URLOPENTAB] = ami_utf8_easy((char *)messages_get("LinkNewTab"));
	ctxmenulab[CMID_URLHOTLIST] = ami_utf8_easy((char *)messages_get("HotlistAdd"));
	ctxmenulab[CMID_COPYURL] = ami_utf8_easy((char *)messages_get("CopyURL"));

	ctxmenulab[CMID_NAVHOME] = ami_utf8_easy((char *)messages_get("Home"));
	ctxmenulab[CMID_NAVBACK] = ami_utf8_easy((char *)messages_get("Back"));
	ctxmenulab[CMID_NAVFORWARD] = ami_utf8_easy((char *)messages_get("Forward"));
	ctxmenulab[CMID_NAVRELOAD] = ami_utf8_easy((char *)messages_get("ObjReload"));
	ctxmenulab[CMID_NAVSTOP] = ami_utf8_easy((char *)messages_get("Stop"));

	ctxmenulab[CMID_SELCUT] = ami_utf8_easy((char *)messages_get("CutNS"));
	ctxmenulab[CMID_SELCOPY] = ami_utf8_easy((char *)messages_get("CopyNS"));
	ctxmenulab[CMID_SELPASTE] = ami_utf8_easy((char *)messages_get("PasteNS"));
	ctxmenulab[CMID_SELALL] = ami_utf8_easy((char *)messages_get("SelectAllNS"));
	ctxmenulab[CMID_SELCLEAR] = ami_utf8_easy((char *)messages_get("ClearNS"));
	ctxmenulab[CMID_SELSEARCH] = ami_utf8_easy((char *)messages_get("SearchWeb"));
	ctxmenulab[CMID_SELSAVE] = ami_utf8_easy((char *)messages_get("SaveAs"));

	ctxmenulab[CMID_PLUGINCMD] = ami_utf8_easy((char *)messages_get("ExternalApp"));

	ctxmenulab[CMSUB_PAGE] = ami_utf8_easy((char *)messages_get("Page"));
	ctxmenulab[CMSUB_FRAME] = ami_utf8_easy((char *)messages_get("Frame"));
	ctxmenulab[CMSUB_OBJECT] = ami_utf8_easy((char *)messages_get("Object"));
	ctxmenulab[CMSUB_NAVIGATE] = ami_utf8_easy((char *)messages_get("Navigate"));
	ctxmenulab[CMSUB_URL] = ami_utf8_easy((char *)messages_get("Link"));
	ctxmenulab[CMSUB_SEL] = ami_utf8_easy((char *)messages_get("Selection"));

	/* Back button */
	ctxmenulab[CMID_HISTORY] = ami_utf8_easy((char *)messages_get("HistLocalNS"));

	/* treeviews */
	ctxmenulab[CMID_TREE_EXPAND] = ami_utf8_easy((char *)messages_get("Expand"));
	ctxmenulab[CMID_TREE_COLLAPSE] = ami_utf8_easy((char *)messages_get("Collapse"));
	ctxmenulab[CMID_TREE_LAUNCH] = ami_utf8_easy((char *)messages_get("TreeLaunch"));
	ctxmenulab[CMID_TREE_NEWFOLDER] = ami_utf8_easy((char *)messages_get("TreeNewFolder"));
	ctxmenulab[CMID_TREE_NEWITEM] = ami_utf8_easy((char *)messages_get("New"));
	ctxmenulab[CMID_TREE_SETDEFAULT] = ami_utf8_easy((char *)messages_get("TreeDefault"));
	ctxmenulab[CMID_TREE_CLEARDEFAULT] = ami_utf8_easy((char *)messages_get("TreeClear"));
	ctxmenulab[CMID_TREE_DELETE] = ami_utf8_easy((char *)messages_get("TreeDelete"));
	ctxmenulab[CMID_TREE_EDITLINK] = ami_utf8_easy((char *)messages_get("EditLink"));
	ctxmenulab[CMID_TREE_EDITFOLDER] = ami_utf8_easy((char *)messages_get("EditFolder"));
	ctxmenulab[CMID_TREE_ADDHOTLIST] = ami_utf8_easy((char *)messages_get("HotlistAdd"));

}

void ami_context_menu_add_submenu(Object *ctxmenuobj, ULONG cmsub, void *userdata)
{
	/*
	 * CMSUB_PAGE      - userdata = hlcache_object *
	 * CMSUB_FRAME     - userdata = hlcache_object *
	 * CMSUB_URL       - userdata = char *
	 * CMSUB_OBJECT    - userdata = hlcache_object *
	 * CMSUB_SEL       - userdata = browser_window *
	 * CMSUB_NAVIGATE  - userdata = browser_window * (only for menu construction)
	 * CMID_SELECTFILE - userdata = box *
	 */

	struct browser_window *bw = NULL;

	switch(cmsub)
	{
		case CMSUB_PAGE:
			IDoMethod(ctxmenuobj, PM_INSERT,
				NewObject(POPUPMENU_GetItemClass(), NULL,
					PMIA_Title, (ULONG)ctxmenulab[CMSUB_PAGE],
					PMSIMPLESUB,
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_PAGEOPEN],
							PMIA_ID, CMID_PAGEOPEN,
							PMIA_UserData, userdata,
							PMIA_CommKey, "O",
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_PAGESAVE],
							PMIA_ID, CMID_PAGESAVE,
							PMIA_UserData, userdata,
							PMIA_CommKey, "S",
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_PAGESAVECOMPLETE],
							PMIA_ID, CMID_PAGESAVECOMPLETE,
							PMIA_UserData, userdata,
							PMIA_Disabled, (content_get_type(userdata) != CONTENT_HTML),
						TAG_DONE),
						PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, ~0,
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_PAGECLOSE],
							PMIA_ID, CMID_PAGECLOSE,
							PMIA_UserData, userdata,
							PMIA_CommKey, "K",
						TAG_DONE),
						PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, ~0,
						TAG_DONE),
						PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_PAGEHOTLIST],
							PMIA_ID, CMID_PAGEHOTLIST,
							PMIA_UserData, nsurl_access(hlcache_handle_get_url(userdata)),
							PMIA_CommKey, "B",
						TAG_DONE),
					TAG_DONE),
				TAG_DONE),
			~0);
		break;

		case CMSUB_FRAME:
			IDoMethod(ctxmenuobj,PM_INSERT,
				NewObject(POPUPMENU_GetItemClass(), NULL,
					PMIA_Title, (ULONG)ctxmenulab[CMSUB_FRAME],
					PMSIMPLESUB,
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_FRAMEWIN],
							PMIA_ID, CMID_FRAMEWIN,
							PMIA_UserData, nsurl_access(hlcache_handle_get_url(userdata)),
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_FRAMETAB],
							PMIA_ID, CMID_FRAMETAB,
							PMIA_UserData, nsurl_access(hlcache_handle_get_url(userdata)),
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_FRAMESHOW],
							PMIA_ID, CMID_FRAMESHOW,
							PMIA_UserData, userdata,
						TAG_DONE),
						PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, ~0,
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_FRAMERELOAD],
							PMIA_ID, CMID_FRAMERELOAD,
							PMIA_UserData, userdata,
						TAG_DONE),
						PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, ~0,
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_FRAMECOPYURL],
							PMIA_ID, CMID_FRAMECOPYURL,
							PMIA_UserData, nsurl_access(hlcache_handle_get_url(userdata)),
						TAG_DONE),
						PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, ~0,
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_FRAMESAVE],
							PMIA_ID, CMID_FRAMESAVE,
							PMIA_UserData, userdata,
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_FRAMESAVECOMPLETE],
							PMIA_ID, CMID_FRAMESAVECOMPLETE,
							PMIA_UserData, userdata,
							PMIA_Disabled, (content_get_type(userdata) != CONTENT_HTML),
						TAG_DONE),
					TAG_DONE),
				TAG_DONE),
			~0);
		break;

		case CMSUB_NAVIGATE:
			IDoMethod(ctxmenuobj, PM_INSERT,
				NewObject(POPUPMENU_GetItemClass(), NULL,
					PMIA_Title, (ULONG)ctxmenulab[CMSUB_NAVIGATE],
					PMSIMPLESUB,
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_NAVHOME],
							PMIA_ID, CMID_NAVHOME,
							PMIA_UserData, userdata,
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_NAVBACK],
							PMIA_ID, CMID_NAVBACK,
							PMIA_UserData, userdata,
							PMIA_Disabled, !browser_window_back_available(userdata),
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_NAVFORWARD],
							PMIA_ID, CMID_NAVFORWARD,
							PMIA_UserData, userdata,
							PMIA_Disabled, !browser_window_forward_available(userdata),
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_NAVRELOAD],
							PMIA_ID, CMID_NAVRELOAD,
							PMIA_UserData, userdata,
							PMIA_CommKey, "R",
							PMIA_Disabled, !browser_window_reload_available(userdata),
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_NAVSTOP],
							PMIA_ID, CMID_NAVSTOP,
							PMIA_UserData, userdata,
							PMIA_Disabled, !browser_window_stop_available(userdata),
						TAG_DONE),
					TAG_DONE),
				TAG_DONE),
			~0);
		break;

		case CMSUB_URL:
			IDoMethod(ctxmenuobj,PM_INSERT,
				NewObject(POPUPMENU_GetItemClass(), NULL,
					PMIA_Title, (ULONG)ctxmenulab[CMSUB_URL],
					PMSIMPLESUB,
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_URLOPEN],
							PMIA_ID, CMID_URLOPEN,
							PMIA_UserData, userdata,
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_URLOPENWIN],
							PMIA_ID, CMID_URLOPENWIN,
							PMIA_UserData, userdata,
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_URLOPENTAB],
							PMIA_ID, CMID_URLOPENTAB,
							PMIA_UserData, userdata,
						TAG_DONE),
						PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, ~0,
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_COPYURL],
							PMIA_ID, CMID_COPYURL,
							PMIA_UserData, userdata,
						TAG_DONE),
						PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_URLHOTLIST],
							PMIA_ID, CMID_URLHOTLIST,
							PMIA_UserData, userdata,
						TAG_DONE),
						PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, ~0,
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_SAVEURL],
							PMIA_ID, CMID_SAVEURL,
							PMIA_UserData, userdata,
						TAG_DONE),
					TAG_DONE),
				TAG_DONE),
			~0);
		break;

		case CMSUB_OBJECT:
			IDoMethod(ctxmenuobj, PM_INSERT,
				NewObject(POPUPMENU_GetItemClass(), NULL,
					PMIA_Title, (ULONG)ctxmenulab[CMSUB_OBJECT],
					PMSIMPLESUB,
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_SHOWOBJ],
							PMIA_ID, CMID_SHOWOBJ,
							PMIA_UserData, userdata,
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_RELOADOBJ],
							PMIA_ID, CMID_RELOADOBJ,
							PMIA_UserData, userdata,
						TAG_DONE),
						PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, ~0,
						TAG_DONE),
						PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_COPYOBJ],
							PMIA_ID, CMID_COPYOBJ,
							PMIA_UserData, nsurl_access(hlcache_handle_get_url(userdata)),
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_CLIPOBJ],
							PMIA_ID, CMID_CLIPOBJ,
							PMIA_UserData, userdata,
							PMIA_Disabled, (content_get_type(userdata) != CONTENT_IMAGE),
						TAG_DONE),
						PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, ~0,
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_SAVEOBJ],
							PMIA_ID, CMID_SAVEOBJ,
							PMIA_UserData, userdata,
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_SAVEIFFOBJ],
							PMIA_ID, CMID_SAVEIFFOBJ,
							PMIA_UserData, userdata,
							PMIA_Disabled, (content_get_type(userdata) != CONTENT_IMAGE),
						TAG_DONE),
						PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, ~0,
						TAG_DONE),
						PMA_AddItem, NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_PLUGINCMD],
							PMIA_ID, CMID_PLUGINCMD,
							PMIA_UserData, userdata,
							PMIA_Disabled, !ami_mime_content_to_cmd(userdata),
						TAG_DONE),
					TAG_DONE),
				TAG_DONE),
			~0);
		break;

		case CMSUB_SEL:
			bw = userdata;
			BOOL disabled_readonly = selection_read_only(browser_window_get_selection(bw));
			BOOL disabled_noselection = !browser_window_has_selection(bw);

			IDoMethod(ctxmenuobj,PM_INSERT,
				NewObject(POPUPMENU_GetItemClass(), NULL,
					PMIA_Title, (ULONG)ctxmenulab[CMSUB_SEL],
					PMIA_SubMenu, NewObject(POPUPMENU_GetClass(), NULL,
						PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_SELCUT],
							PMIA_ID,CMID_SELCUT,
							PMIA_Disabled, disabled_noselection && disabled_readonly,
							PMIA_CommKey, "X",
						TAG_DONE),
						PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_SELCOPY],
							PMIA_ID,CMID_SELCOPY,
							PMIA_Disabled, disabled_noselection,
							PMIA_CommKey, "C",
						TAG_DONE),
						PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_SELPASTE],
							PMIA_ID,CMID_SELPASTE,
							PMIA_Disabled, (bw->window->c_h == 0),
							PMIA_CommKey, "V",
						TAG_DONE),
						PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_SELALL],
							PMIA_ID,CMID_SELALL,
							PMIA_CommKey, "A",
						TAG_DONE),
						PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_SELCLEAR],
							PMIA_ID,CMID_SELCLEAR,
							PMIA_Disabled, disabled_noselection,
							PMIA_CommKey, "Z",
						TAG_DONE),
						PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, ~0,
						TAG_DONE),
						PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_SELSEARCH],
							PMIA_ID,CMID_SELSEARCH,
							PMIA_Disabled, disabled_noselection,
						TAG_DONE),
						PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_SELSAVE],
							PMIA_ID,CMID_SELSAVE,
							PMIA_Disabled, disabled_noselection,
						TAG_DONE),
					TAG_DONE),
				TAG_DONE),
			~0);
		break;

		case CMID_SELECTFILE:
			IDoMethod(ctxmenuobj,PM_INSERT,
				NewObject(POPUPMENU_GetItemClass(), NULL,
					PMIA_Title, (ULONG)ctxmenulab[CMID_SELECTFILE],
					PMIA_ID, CMID_SELECTFILE,
					PMIA_UserData, userdata,
				TAG_DONE),
			~0);
		break;
	}
}

void ami_context_menu_free(void)
{
	int i;

	if(ctxmenuobj) DisposeObject(ctxmenuobj);

	for(i=0;i<CMID_LAST;i++)
	{
		ami_utf8_free(ctxmenulab[i]);
	}

	if(IPopupMenu) DropInterface((struct Interface *)IPopupMenu);
	if(PopupMenuBase) CloseLibrary(PopupMenuBase);
}

BOOL ami_context_menu_mouse_trap(struct gui_window_2 *gwin, BOOL trap)
{
	int top, left, width, height;

	if(option_context_menu == false) return FALSE;

	if((option_kiosk_mode == false) && (trap == FALSE))
	{
		if(browser_window_back_available(gwin->bw) &&
				ami_gadget_hit(gwin->objects[GID_BACK],
				gwin->win->MouseX, gwin->win->MouseY))
			trap = TRUE;

		if(browser_window_forward_available(gwin->bw) &&
				ami_gadget_hit(gwin->objects[GID_FORWARD],
				gwin->win->MouseX, gwin->win->MouseY))
			trap = TRUE;
	}

	if(gwin->rmbtrapped == trap) return trap;

	SetWindowAttr(gwin->win, WA_RMBTrap, (APTR)(ULONG)trap, sizeof(BOOL));
	gwin->rmbtrapped = trap;

	return trap;
}

void ami_context_menu_show(struct gui_window_2 *gwin,int x,int y)
{
	struct hlcache_handle *cc = gwin->bw->current_content;
	struct box *curbox;
	int box_x=0;
	int box_y=0;
	bool no_more_menus = false;
	bool menuhascontent = false;
	ULONG ret = 0;
	struct contextual_content ccdata;

	if(!cc) return;
	if(ctxmenuobj) DisposeObject(ctxmenuobj);

	ctxmenuhook.h_Entry = ami_context_menu_hook;
	ctxmenuhook.h_SubEntry = NULL;
	ctxmenuhook.h_Data = gwin;

    ctxmenuobj = NewObject( POPUPMENU_GetClass(), NULL,
                        PMA_MenuHandler, &ctxmenuhook,
						TAG_DONE);

	if(gwin->bw && gwin->bw->history &&
		ami_gadget_hit(gwin->objects[GID_BACK],
			gwin->win->MouseX, gwin->win->MouseY))
	{
		gwin->temp = 0;
		history_enumerate_back(gwin->bw->history, ami_context_menu_history, gwin);

		IDoMethod(ctxmenuobj, PM_INSERT,
			NewObject(POPUPMENU_GetItemClass(), NULL,
				PMIA_Title, ~0,
			TAG_DONE),
		~0);

		IDoMethod(ctxmenuobj, PM_INSERT,
			NewObject(POPUPMENU_GetItemClass(), NULL,
				PMIA_Title, (ULONG)ctxmenulab[CMID_HISTORY],
				PMIA_ID, CMID_HISTORY,
				PMIA_UserData, NULL,
			TAG_DONE),
		~0);

		menuhascontent = true;
	}
	else if(gwin->bw && gwin->bw->history &&
		ami_gadget_hit(gwin->objects[GID_FORWARD],
			gwin->win->MouseX, gwin->win->MouseY))
	{
		gwin->temp = 0;
		history_enumerate_forward(gwin->bw->history, ami_context_menu_history, gwin);

		IDoMethod(ctxmenuobj, PM_INSERT,
			NewObject(POPUPMENU_GetItemClass(), NULL,
				PMIA_Title, ~0,
			TAG_DONE),
		~0);

		IDoMethod(ctxmenuobj, PM_INSERT,
			NewObject(POPUPMENU_GetItemClass(), NULL,
				PMIA_Title, (ULONG)ctxmenulab[CMID_HISTORY],
				PMIA_ID, CMID_HISTORY,
				PMIA_UserData, NULL,
			TAG_DONE),
		~0);

		menuhascontent = true;
	}
	else
	{
		if(content_get_type(cc) == CONTENT_HTML)
		{
			curbox = html_get_box_tree(gwin->bw->current_content);

			while(curbox = box_at_point(curbox, x, y, &box_x, &box_y, &cc))
			{
				if (curbox->style &&
					css_computed_visibility(curbox->style) == CSS_VISIBILITY_HIDDEN)
				continue;

				if (curbox->gadget)
				{
					switch (curbox->gadget->type)
					{
						case GADGET_FILE:
							ami_context_menu_add_submenu(ctxmenuobj, CMID_SELECTFILE, curbox);
							menuhascontent = true;
							no_more_menus = true;
						break;
					}
				}
			}
		}

		if(no_more_menus == false)
		{
			browser_window_get_contextual_content(gwin->bw, x, y, &ccdata);

			ami_context_menu_add_submenu(ctxmenuobj, CMSUB_PAGE, cc);
			menuhascontent = true;

			if(ccdata.main && (ccdata.main != cc))
			{
				ami_context_menu_add_submenu(ctxmenuobj, CMSUB_FRAME, ccdata.main);
				menuhascontent = true;
			}

			if(ccdata.link_url)
			{
				ami_context_menu_add_submenu(ctxmenuobj, CMSUB_URL, (char *)ccdata.link_url);
				menuhascontent = true;
			}

			if(ccdata.object)
			{
				ami_context_menu_add_submenu(ctxmenuobj, CMSUB_OBJECT, ccdata.object);
				menuhascontent = true;
			}

			ami_context_menu_add_submenu(ctxmenuobj, CMSUB_NAVIGATE, gwin->bw);
			menuhascontent = true;

			if(content_get_type(cc) == CONTENT_HTML ||
				content_get_type(cc) == CONTENT_TEXTPLAIN)
			{
				ami_context_menu_add_submenu(ctxmenuobj, CMSUB_SEL, gwin->bw);
				menuhascontent = true;
			}
		}
	}

	if(!menuhascontent) return;

	gui_window_set_pointer(gwin->bw->window, GUI_POINTER_DEFAULT);

	IDoMethod(ctxmenuobj, PM_OPEN, gwin->win);
}

static uint32 ami_context_menu_hook(struct Hook *hook,Object *item,APTR reserved)
{
    int32 itemid = 0;
	struct gui_window_2 *gwin = hook->h_Data;
	APTR userdata = NULL;
	struct browser_window *bw;
	struct hlcache_handle *object;
	const char *source_data;
	ULONG source_size;
	struct bitmap *bm;

    if(GetAttrs(item,PMIA_ID,&itemid,
					PMIA_UserData,&userdata,
					TAG_DONE))
    {
		switch(itemid)
		{
			case CMID_SELECTFILE:
				if(AslRequestTags(filereq,
					ASLFR_TitleText,messages_get("NetSurf"),
					ASLFR_Screen,scrn,
					ASLFR_DoSaveMode,FALSE,
					TAG_DONE))
				{
					struct box *box = userdata;
					char *utf8_fn;
					char fname[1024];
					int x,y;

					strlcpy(fname,filereq->fr_Drawer,1024);
					AddPart(fname,filereq->fr_File,1024);

					if(utf8_from_local_encoding(fname,0,&utf8_fn) != UTF8_CONVERT_OK)
					{
						warn_user("NoMemory","");
						break;
					}

					free(box->gadget->value);
					box->gadget->value = utf8_fn;

					box_coords(box, (int *)&x, (int *)&y);
					ami_do_redraw_limits(gwin->bw->window, 
						gwin->bw->window->shared->bw,
						x,y,
						x + box->width,
						y + box->height);
				}
			break;

			case CMID_PAGEOPEN:
				ami_file_open(gwin);
			break;

			case CMID_PAGECLOSE:
				browser_window_destroy(gwin->bw);
			break;

			case CMID_URLHOTLIST:
			case CMID_PAGEHOTLIST:
				hotlist_add_page(userdata);
			break;

			case CMID_FRAMECOPYURL:
			case CMID_COPYURL:
			case CMID_COPYOBJ:
				ami_easy_clipboard((char *)userdata);
			break;

			case CMID_FRAMEWIN:
			case CMID_URLOPENWIN:
				bw = browser_window_create(userdata, gwin->bw,
					nsurl_access(hlcache_handle_get_url(gwin->bw->current_content)), true, false);
			break;

			case CMID_FRAMETAB:
			case CMID_URLOPENTAB:
				bw = browser_window_create(userdata, gwin->bw,
					nsurl_access(hlcache_handle_get_url(gwin->bw->current_content)), true, true);
			break;

			case CMID_FRAMESAVE:
			case CMID_SAVEURL:
				browser_window_download(gwin->bw, userdata,
					nsurl_access(hlcache_handle_get_url(gwin->bw->current_content)));
			break;

			case CMID_FRAMESHOW:
			case CMID_SHOWOBJ:
				browser_window_go(gwin->bw, nsurl_access(hlcache_handle_get_url(userdata)),
					nsurl_access(hlcache_handle_get_url(gwin->bw->current_content)), true);
			break;

			case CMID_URLOPEN:
				browser_window_go(gwin->bw, userdata,
					nsurl_access(hlcache_handle_get_url(gwin->bw->current_content)), true);
			break;

			case CMID_FRAMERELOAD:
			case CMID_RELOADOBJ:
				object = (struct hlcache_handle *)userdata;
				content_invalidate_reuse_data(object);
				browser_window_reload(gwin->bw, false);
			break;

			case CMID_CLIPOBJ:
				object = (struct hlcache_handle *)userdata;
				if((bm = content_get_bitmap(object)))
				{
					bm->url = (char *)nsurl_access(hlcache_handle_get_url(object));
					bm->title = (char *)content_get_title(object);
					ami_easy_clipboard_bitmap(bm);
				}
#ifdef WITH_NS_SVG
				else if(ami_mime_compare(object, "svg") == true)
				{
					ami_easy_clipboard_svg(object);
				}
#endif
			break;

			case CMID_SAVEOBJ:
			case CMID_PAGESAVE:
				ami_file_save_req(AMINS_SAVE_SOURCE, gwin,
					(struct hlcache_handle *)userdata, NULL);
			break;

			case CMID_PAGESAVECOMPLETE:
			case CMID_FRAMESAVECOMPLETE:
				ami_file_save_req(AMINS_SAVE_COMPLETE, gwin,
					(struct hlcache_handle *)userdata, NULL);
			break;

			case CMID_SAVEIFFOBJ:
				ami_file_save_req(AMINS_SAVE_IFF, gwin,
					(struct hlcache_handle *)userdata, NULL);
			break;

			case CMID_PLUGINCMD:
				amiga_plugin_hack_execute((struct hlcache_handle *)userdata);
			break;

			case CMID_HISTORY:
				if(userdata == NULL)
				{
					ami_history_open(gwin->bw, gwin->bw->history);
				}
				else
				{
					history_go(gwin->bw, gwin->bw->history,
						(struct history_entry *)userdata, false);
				}
			break;

			case CMID_NAVHOME:
				browser_window_go(gwin->bw, option_homepage_url, NULL, true);
			break;

			case CMID_NAVBACK:
				ami_gui_history(gwin, true);
			break;

			case CMID_NAVFORWARD:
				ami_gui_history(gwin, false);
			break;

			case CMID_NAVSTOP:
				if(browser_window_stop_available(gwin->bw))
					browser_window_stop(gwin->bw);
			break;

			case CMID_NAVRELOAD:
				if(browser_window_reload_available(gwin->bw))
					browser_window_reload(gwin->bw, true);
			break;

			case CMID_SELCUT:
				browser_window_key_press(gwin->bw, KEY_CUT_SELECTION);
			break;

			case CMID_SELCOPY:
				browser_window_key_press(gwin->bw, KEY_COPY_SELECTION);
				browser_window_key_press(gwin->bw, KEY_CLEAR_SELECTION);
			break;

			case CMID_SELPASTE:
				browser_window_key_press(gwin->bw, KEY_PASTE);
			break;

			case CMID_SELALL:
				browser_window_key_press(gwin->bw, KEY_SELECT_ALL);
				gui_start_selection(gwin->bw->window);
			break;

			case CMID_SELCLEAR:
				browser_window_key_press(gwin->bw, KEY_CLEAR_SELECTION);
			break;

			case CMID_SELSAVE:
				ami_file_save_req(AMINS_SAVE_SELECTION, gwin, NULL,
					browser_window_get_selection(gwin->bw));
			break;

			case CMID_SELSEARCH:
			{
				struct ami_text_selection *sel;
				char *url;

				if(sel = ami_selection_to_text(gwin))
				{
					url = search_web_from_term(sel->text);
					browser_window_go(gwin->bw, url, NULL, true);

					FreeVec(sel);
				}
			}
			break;
		}
    }

    return itemid;
}

static bool ami_context_menu_history(const struct history *history, int x0, int y0,
	int x1, int y1, const struct history_entry *entry, void *user_data)
{
	struct gui_window_2 *gwin = (struct gui_window_2 *)user_data;

	gwin->temp++;
	if(gwin->temp > 10) return false;

	IDoMethod(ctxmenuobj, PM_INSERT,
		NewObject(POPUPMENU_GetItemClass(), NULL,
			PMIA_Title, (ULONG)history_entry_get_title(entry),
			PMIA_ID, CMID_HISTORY,
			PMIA_UserData, entry,
		TAG_DONE),
	~0);

	return true;
}

static uint32 ami_popup_hook(struct Hook *hook,Object *item,APTR reserved)
{
	int32 itemid = 0;
	struct gui_window *gwin = hook->h_Data;

	if(GetAttr(PMIA_ID, item, &itemid))
	{
		form_select_process_selection(gwin->shared->bw->current_content,gwin->shared->control,itemid);
	}

	return itemid;
}

void gui_create_form_select_menu(struct browser_window *bw,
		struct form_control *control)
{
	/* TODO: PMIA_Title memory leaks as we don't free the strings.
	 * We use the core menu anyway, but in future when popupmenu.class
	 * improves we will probably start using this again.
	 */

	struct gui_window *gwin = bw->window;
	struct form_option *opt = control->data.select.items;
	ULONG i = 0;

	if(ctxmenuobj) DisposeObject(ctxmenuobj);

	ctxmenuhook.h_Entry = ami_popup_hook;
	ctxmenuhook.h_SubEntry = NULL;
	ctxmenuhook.h_Data = gwin;

	gwin->shared->control = control;

    ctxmenuobj = PMMENU(ami_utf8_easy(control->name)),
                        PMA_MenuHandler, &ctxmenuhook, End;

	while(opt)
	{
		IDoMethod(ctxmenuobj, PM_INSERT,
			NewObject( POPUPMENU_GetItemClass(), NULL,
				PMIA_Title, (ULONG)ami_utf8_easy(opt->text),
				PMIA_ID, i,
				PMIA_CheckIt, TRUE,
				PMIA_Checked, opt->selected,
				TAG_DONE),
			~0);

		opt = opt->next;
		i++;
	}

	gui_window_set_pointer(gwin, GUI_POINTER_DEFAULT); // Clear the menu-style pointer

	IDoMethod(ctxmenuobj, PM_OPEN, gwin->shared->win);
}

#else

void ami_context_menu_init(void)
{
}

void ami_context_menu_free(void)
{
}

BOOL ami_context_menu_mouse_trap(struct gui_window_2 *gwin, BOOL trap)
{
	return FALSE;
}

void ami_context_menu_show(struct gui_window_2 *gwin, int x, int y)
{
}

void gui_create_form_select_menu(struct browser_window *bw,
		struct form_control *control)
{
}
#endif
