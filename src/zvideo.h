#ifndef _ZVIDEO_H_
#define _ZVIDEO_H_

#include <SDL3/SDL.h>

// Native SDL3 window + scaled-framebuffer manager for the Zod Engine.
//
// The game renders into one off-screen surface at its (low) logical resolution;
// this module GPU-upscales that surface to a large window via an SDL_Renderer,
// so the pixel art stays crisp and HiDPI/Retina-aware instead of being a tiny,
// OS-blurred region. All of that is contained here — the rest of the engine
// just draws into the surface ZVideo_SetMode() returns and calls ZVideo_Present().

// Create (or resize) the window and return the off-screen framebuffer surface
// the game draws into. w/h are the logical (render) resolution.
SDL_Surface *ZVideo_SetMode(int w, int h, bool fullscreen);

// Upload the framebuffer, GPU-upscale it into the window, and swap.
void ZVideo_Present();

// Save the current game frame to a PNG file. Returns true on success.
bool ZVideo_SaveScreenshot(const char *path);

// Window-manager bits.
void ZVideo_SetCaption(const char *title);
// Borderless desktop fullscreen on/off at runtime (Alt+Enter).
void ZVideo_SetFullscreen(bool fullscreen);
bool ZVideo_GetFullscreen();

// Desktop resolution (for adapting the logical view to the display aspect).
bool ZVideo_GetDesktopSize(int &w, int &h);
// The renderer's REAL output size in pixels (desktop modes are estimates -
// scaled modes and notched displays differ from the fullscreen drawable).
bool ZVideo_GetOutputSize(int &w, int &h);
void ZVideo_SetIcon(SDL_Surface *icon);
void ZVideo_SetGrab(bool grab);
bool ZVideo_GetGrab();

// Warp the OS pointer; x/y are in logical (render) coordinates and are mapped
// to window coordinates for the scaled framebuffer.
void ZVideo_WarpMouse(int x, int y);

// Rewrite a polled event's mouse coordinates from window space into logical
// (render) space — call right after SDL_PollEvent so input lines up with the
// scaled framebuffer (essential in letterboxed fullscreen). No-op at 1:1.
void ZVideo_ConvertEventCoords(SDL_Event *ev);

#endif
