#ifndef _MAC_DOCK_ICON_H_
#define _MAC_DOCK_ICON_H_

// macOS only: set the running app's Dock icon at runtime (NSApp
// applicationIconImage). SDL_SetWindowIcon does NOT affect the Dock on macOS -
// without this, a bare (non-.app-bundle) executable shows the generic "exec"
// icon. No-op / not declared on other platforms.
#ifdef __APPLE__
extern "C" void MacSetDockIcon(const char *png_path);
#endif

#endif
