#import <Cocoa/Cocoa.h>

#include "mac_dock_icon.h"

// Set the Dock icon for the running process. Works for a bare executable (no
// .app bundle), which otherwise shows the generic "exec" Dock icon, since
// SDL_SetWindowIcon doesn't touch the Dock on macOS.
extern "C" void MacSetDockIcon(const char *png_path)
{
	if(!png_path) return;

	@autoreleasepool
	{
		NSString *path = [NSString stringWithUTF8String:png_path];
		NSImage *img = [[NSImage alloc] initWithContentsOfFile:path];
		if(img)
			[[NSApplication sharedApplication] setApplicationIconImage:img];
	}
}
