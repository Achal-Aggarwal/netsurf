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

#ifndef AMIGA_FILETYPE_H
#define AMIGA_FILETYPE_H
#include <stdbool.h>
#include <libwapcaplet/libwapcaplet.h>
#include "content/content_type.h"
#include "utils/errors.h"
#include <datatypes/datatypes.h>

struct hlcache_handle;
struct ami_mime_entry;

nserror ami_mime_init(const char *mimefile);
void ami_mime_free(void);
void ami_mime_entry_free(struct ami_mime_entry *mimeentry);

struct Node *ami_mime_from_datatype(struct DataType *dt,
		lwc_string **mimetype, struct Node *start_node);

const char *ami_content_type_to_file_type(content_type type);
void ami_datatype_to_mimetype(struct DataType *dtn, char *mimetype);
bool ami_mime_compare(struct hlcache_handle *c, const char *type);
#endif
