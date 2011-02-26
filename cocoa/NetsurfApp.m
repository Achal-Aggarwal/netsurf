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

#import "cocoa/NetsurfApp.h"
#import "cocoa/gui.h"
#import "cocoa/plotter.h"

#import "desktop/gui.h"
#import "content/urldb.h"
#import "content/fetch.h"
#import "css/utils.h"
#import "desktop/gui.h"
#import "desktop/history_core.h"
#import "desktop/mouse.h"
#import "desktop/netsurf.h"
#import "desktop/options.h"
#import "desktop/plotters.h"
#import "desktop/save_complete.h"
#import "desktop/selection.h"
#import "desktop/textinput.h"
#import "render/html.h"
#import "utils/url.h"
#import "utils/log.h"
#import "utils/messages.h"
#import "utils/utils.h"
#import "css/utils.h"

#ifndef NETSURF_HOMEPAGE
#define NETSURF_HOMEPAGE "http://www.netsurf-browser.org/welcome/"
#endif


@implementation NetSurfApp

@synthesize frontTab;

- (void) loadOptions;
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	[defaults registerDefaults: [NSDictionary dictionaryWithObjectsAndKeys: 
								 cocoa_get_user_path( @"Cookies" ), kCookiesFileOption,
								 cocoa_get_user_path( @"URLs" ), kURLsFileOption,
								 [NSString stringWithUTF8String: NETSURF_HOMEPAGE], kHomepageURLOption,
								 nil]];
	
	
	if (NULL == option_cookie_file) {
		option_cookie_file = strdup( [[defaults objectForKey: kCookiesFileOption] UTF8String] );
	}
	
	if (NULL == option_cookie_jar) {
		option_cookie_jar = strdup( option_cookie_file );
	}
	
	if (NULL == option_homepage_url) {
		option_homepage_url = strdup( [[defaults objectForKey: kHomepageURLOption] UTF8String] );
	}

	urldb_load( [[defaults objectForKey: kURLsFileOption] UTF8String] );
	urldb_load_cookies( option_cookie_file );
	
	cocoa_update_scale_factor();
}

- (void) saveOptions;
{
	urldb_save_cookies( option_cookie_file );
	urldb_save( [[[NSUserDefaults standardUserDefaults] objectForKey: kURLsFileOption] UTF8String] );
}

- (void) run;
{
	[self finishLaunching];
	[self loadOptions];
	netsurf_main_loop();
	[self saveOptions];
}

-(void) terminate: (id)sender;
{
	[[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationWillTerminateNotification object:self];
	
	netsurf_quit = true;
	[self postEvent: [NSEvent otherEventWithType: NSApplicationDefined location: NSZeroPoint 
								   modifierFlags: 0 timestamp: 0 windowNumber: 0 context: NULL 
										 subtype: 0 data1: 0 data2: 0]  
			atStart: YES];
}

@end

#pragma mark -

static NSString *cocoa_get_preferences_path( void )
{
	NSArray *paths = NSSearchPathForDirectoriesInDomains( NSApplicationSupportDirectory, NSUserDomainMask, YES );
	NSCAssert( [paths count] >= 1, @"Where is the application support directory?" );
	
	NSString *netsurfPath = [[paths objectAtIndex: 0] stringByAppendingPathComponent: @"NetSurf"];
	
	NSFileManager *fm = [NSFileManager defaultManager];
	BOOL isDirectory = NO;
	BOOL exists = [fm fileExistsAtPath: netsurfPath isDirectory: &isDirectory];
	
	if (!exists) {
		exists = [fm createDirectoryAtPath: netsurfPath attributes: nil];
		isDirectory = YES;
	}
	if (!(exists && isDirectory)) {
		die( "Cannot create netsurf preferences directory" );
	}
	
	return netsurfPath;
}

NSString *cocoa_get_user_path( NSString *fileName ) 
{
	return [cocoa_get_preferences_path() stringByAppendingPathComponent: fileName];
}

static const char *cocoa_get_options_file( void )
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	[defaults registerDefaults: [NSDictionary dictionaryWithObjectsAndKeys: 
								 cocoa_get_user_path( @"Options" ), kOptionsFileOption,
								 nil]];
	
	return [[defaults objectForKey: kOptionsFileOption] UTF8String];
}

static NSApplication *cocoa_prepare_app( void )
{
	if (NSApp != nil) return NSApp;
	
	NSDictionary *infoDictionary = [[NSBundle mainBundle] infoDictionary];
	Class principalClass =  NSClassFromString([infoDictionary objectForKey:@"NSPrincipalClass"]);
	NSCAssert([principalClass respondsToSelector:@selector(sharedApplication)], @"Principal class must implement sharedApplication.");
	[principalClass sharedApplication];
	
	NSString *mainNibName = [infoDictionary objectForKey:@"NSMainNibFile"];
	NSNib *mainNib = [[NSNib alloc] initWithNibNamed:mainNibName bundle:[NSBundle mainBundle]];
	[mainNib instantiateNibWithOwner:NSApp topLevelObjects:nil];
	[mainNib release];
	
	return NSApp;
}

void cocoa_autorelease( void )
{
	static NSAutoreleasePool *pool = nil;
	[pool release];
	pool = [[NSAutoreleasePool alloc] init];
}

int main( int argc, char **argv )
{
	cocoa_autorelease();
		
	const char * const messages = [[[NSBundle mainBundle] pathForResource: @"Messages" ofType: @""] UTF8String];
	const char * const options = cocoa_get_options_file();
	
	netsurf_init(&argc, &argv, options, messages);
	
    [cocoa_prepare_app() run];
	
	netsurf_exit();
	
	return 0;
}
