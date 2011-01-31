/*
 * Copyright 2011 Sven Weidauer <sven.weidauer@gmail.com>
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


#import "cocoa/PreferencesWindowController.h"
#import "cocoa/NetsurfApp.h"
#import "cocoa/gui.h"
#import "cocoa/BrowserViewController.h"

#import "desktop/browser.h"
#import "content/content.h"
#import "desktop/options.h"

@implementation PreferencesWindowController

- init;
{
	if ((self = [super initWithWindowNibName: @"PreferencesWindow"]) == nil) return nil;
	
	return self;
}

- (IBAction) useCurrentPageAsHomepage: (id) sender;
{
	struct browser_window *bw = [[(NetSurfApp *)NSApp frontTab] browser];
	const char *url = content_get_url( bw->current_content );
	[self setHomepageURL: [NSString stringWithUTF8String: url]];
}

- (void) setHomepageURL: (NSString *) newUrl;
{
	free( option_homepage_url );
	option_homepage_url = strdup( [newUrl UTF8String] );
	[[NSUserDefaults standardUserDefaults] setObject: newUrl forKey: kHomepageURLOption];
	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (NSString *) homepageURL;
{
	return [NSString stringWithUTF8String: option_homepage_url];
}

@end
