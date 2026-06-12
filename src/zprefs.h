#ifndef _ZPREFS_H_
#define _ZPREFS_H_

// Player-local preferences, persisted to ~/.zod_prefs and editable from the
// in-game Options menu. Load order: defaults, then the prefs file, then the
// env vars (ZOD_CLASSIC_MOUSE / ZOD_SMOOTH) - so env always wins a launch.

// Classic (original-Z) mouse: left click orders and drops the selection,
// right click cancels. Off = RTS-style right-click orders.
extern bool zod_classic_mouse;

void ZPrefs_Load();
void ZPrefs_Save();

#endif
