#ifndef _ZSDL_HELPERS_H_
#define _ZSDL_HELPERS_H_

#include <SDL3/SDL.h>

// Small native-SDL3 surface helpers used across the engine. These replace the
// old SDL-1.2-style calls (SDL_CreateRGBSurface with masks, SDL_SetAlpha) the
// codebase was written against; the implementations live in zsdl.cpp.

// Create a blank surface from a legacy depth + RGBA mask description. SDL3
// creates surfaces from a pixel-format enum, so the masks are resolved via
// SDL_GetPixelFormatForMasks; the engine's non-standard 32-bit ARGB layout has
// no named format and falls back to ARGB8888 (which is what it always rendered
// as). Drop-in for every old SDL_CreateRGBSurface(flags,w,h,depth,r,g,b,a) call
// minus the obsolete flags argument.
SDL_Surface *ZSDL_CreateSurface(int w, int h, int depth,
    Uint32 rmask, Uint32 gmask, Uint32 bmask, Uint32 amask);

// Enable per-pixel alpha blending on a surface and set its alpha modulation.
// (SDL2/1.2's SDL_SetAlpha rolled both into one call; SDL3 splits them.)
void ZSDL_SetSurfaceAlpha(SDL_Surface *s, Uint8 alpha);

#endif
