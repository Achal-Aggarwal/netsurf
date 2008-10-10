/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
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

#include <stdbool.h>
#include <Application.h>
#include <FilePanel.h>
#include <Window.h>

#define CALLED() fprintf(stderr, "%s()\n", __FUNCTION__);

extern bool gui_in_multitask;
#if 0 /* GTK */
//extern GladeXML *gladeWindows;
//extern char *glade_file_location;
#endif
extern char *options_file_location;

class NSBrowserApplication : public BApplication {
public:
		NSBrowserApplication();
virtual	~NSBrowserApplication();

virtual void	MessageReceived(BMessage *message);
virtual void	RefsReceived(BMessage *message);
virtual void	ArgvReceived(int32 argc, char **argv);

virtual void	AboutRequested();
virtual bool	QuitRequested();
};



extern BWindow *wndAbout;

extern BWindow *wndTooltip;
#if 0 /* GTK */
//extern GtkLabel *labelTooltip;
#endif

extern BFilePanel *wndOpenFile;

void nsbeos_pipe_message(BMessage *message, BView *_this, struct gui_window *gui);
void nsbeos_pipe_message_top(BMessage *message, BWindow *_this, struct beos_scaffolding *scaffold);

void nsbeos_gui_view_source(struct content *content, struct selection *selection);

