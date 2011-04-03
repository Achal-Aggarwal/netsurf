/*
 * Copyright 2007 Rob Kendrick <rjek@netsurf-browser.org>
 * Copyright 2004-2007 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 John Tytgat <joty@netsurf-browser.org>
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

#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>
#include "desktop/netsurf.h"

#include "utils/utils.h"
#include "utils/log.h"

nserror nslog_init(nslog_ensure_t *ensure, int *pargc, char **argv)
{
	nserror ret = NSERROR_OK;

	if (((*pargc) > 1) && 
	    (argv[1][0] == '-') && 
	    (argv[1][1] == 'v') && 
	    (argv[1][2] == 0)) {
		int argcmv;
		for (argcmv = 2; argcmv < (*pargc); argcmv++) {
			argv[argcmv - 1] = argv[argcmv];
		}
		(*pargc)--;

		/* ensure we actually show logging */
		verbose_log = true;
		
		/* ensure stderr is available */
		if (ensure != NULL) {
			if (ensure(stderr) == false) {
				/* failed to ensure output */
				ret = NSERROR_INIT_FAILED;
			}
		}
	}
	return ret;
}

#ifndef NDEBUG

const char *nslog_gettime(void)
{
	static struct timeval start_tv;
	static char buff[32];

	struct timeval tv;
        struct timeval now_tv;

	if (!timerisset(&start_tv)) {
		gettimeofday(&start_tv, NULL);		
	}
        gettimeofday(&now_tv, NULL);

	timeval_subtract(&tv, &now_tv, &start_tv);

        snprintf(buff, sizeof(buff),"(%ld.%ld)", 
			(long)tv.tv_sec, (long)tv.tv_usec);

        return buff;
}

void nslog_log(const char *format, ...)
{
	if (verbose_log) {
		va_list ap;

		va_start(ap, format);

		vfprintf(stderr, format, ap);

		va_end(ap);
	}
}

#endif

