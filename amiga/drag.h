/*
 * Copyright 2010 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_DRAG_H
#define AMIGA_DRAG_H
#include "amiga/gui.h"

#define AMI_DRAG_THRESHOLD 10

int drag_save;
void *drag_save_data;
struct gui_window *drag_save_gui;

void ami_drag_save(struct Window *win);
void ami_drag_icon_show(struct Window *win, char *type);
void ami_drag_icon_close(struct Window *win);
void ami_drag_icon_move(void);
BOOL ami_drag_in_progress(void);

struct gui_window_2 *ami_window_at_pointer(void);
#endif
