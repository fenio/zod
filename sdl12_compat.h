#ifndef _SDL12_COMPAT_H_
#define _SDL12_COMPAT_H_

// Thin SDL1.2 → SDL2 compatibility shim for the Zod Engine port.
// Provides the small subset of removed/renamed APIs the codebase uses.

#include <SDL.h>
#include <string>

// Removed surface flags — map to 0 (SDL2 ignores most legacy creation flags
// when working with window surfaces, which is what this codebase does).
#ifndef SDL_HWSURFACE
#define SDL_HWSURFACE   0x00000000
#endif
#ifndef SDL_SWSURFACE
#define SDL_SWSURFACE   0x00000000
#endif
#ifndef SDL_DOUBLEBUF
#define SDL_DOUBLEBUF   0x00000000
#endif
#ifndef SDL_ANYFORMAT
#define SDL_ANYFORMAT   0x00000000
#endif
#ifndef SDL_SRCALPHA
#define SDL_SRCALPHA    0x00010000
#endif

// Window flag remappings (these legacy names are used as SDL_SetVideoMode flags).
#ifndef SDL_RESIZABLE
#define SDL_RESIZABLE   SDL_WINDOW_RESIZABLE
#endif
#ifndef SDL_FULLSCREEN
#define SDL_FULLSCREEN  SDL_WINDOW_FULLSCREEN
#endif
#ifndef SDL_OPENGL
#define SDL_OPENGL      SDL_WINDOW_OPENGL
#endif

// SDL_GRAB_* enum is gone.
#ifndef SDL_GRAB_OFF
#define SDL_GRAB_OFF    0
#endif
#ifndef SDL_GRAB_ON
#define SDL_GRAB_ON     1
#endif
#ifndef SDL_GRAB_QUERY
#define SDL_GRAB_QUERY  -1
#endif

// Single global window managed by the shim.
extern SDL_Window *g_compat_window;

// Replacement for SDL_SetVideoMode: creates/resizes the window and returns
// its surface. Width/height of 0 leaves them unchanged on resize calls.
SDL_Surface *SDL_SetVideoMode(int w, int h, int bpp, Uint32 flags);

// Replacement for SDL_Flip: present the window surface.
int SDL_Flip(SDL_Surface *screen);

// Removed window-manager calls — wrap to SDL2 equivalents.
void SDL_WM_SetCaption(const char *title, const char *icon);
void SDL_WM_SetIcon(SDL_Surface *icon, Uint8 *mask);
int  SDL_WM_GrabInput(int mode);

// Removed input init helpers — no-ops in SDL2 (text input/key repeat are
// handled automatically or via SDL_StartTextInput; this codebase only used
// them for the obsolete UNICODE-in-keysym mechanism).
static inline int SDL_EnableUNICODE(int /*enable*/) { return 0; }
static inline int SDL_EnableKeyRepeat(int /*delay*/, int /*interval*/) { return 0; }

// Old default-repeat constants.
#ifndef SDL_DEFAULT_REPEAT_DELAY
#define SDL_DEFAULT_REPEAT_DELAY    500
#endif
#ifndef SDL_DEFAULT_REPEAT_INTERVAL
#define SDL_DEFAULT_REPEAT_INTERVAL 30
#endif

// SDL_CreateThread gained a name parameter in SDL2. Redirect 2-arg calls.
#define SDL_CreateThread(fn, data) SDL_CreateThread((fn), #fn, (data))

// SDL2's SDL_CreateRGBSurface validates the RGBA masks against its set of
// known SDL_PIXELFORMAT_* values and fails with "Unknown pixel format" if
// they don't match. The Zod Engine's original code passes a non-standard
// 32-bit mask layout (R=0xFF000000, G=0x0000FF00, B=0x00FF0000, A=0x000000FF)
// that SDL2 rejects. When that happens, retry with SDL_CreateRGBSurfaceWithFormat
// using ARGB8888 — which is always supported and renders correctly through
// SDL_BlitSurface format conversion.
static inline SDL_Surface *zod_CreateRGBSurface_compat(
    Uint32 flags, int w, int h, int depth,
    Uint32 rmask, Uint32 gmask, Uint32 bmask, Uint32 amask)
{
    SDL_Surface *s = SDL_CreateRGBSurface(flags, w, h, depth, rmask, gmask, bmask, amask);
    if (!s && depth == 32)
        s = SDL_CreateRGBSurfaceWithFormat(flags, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    return s;
}
#define SDL_CreateRGBSurface(flags, w, h, depth, r, g, b, a) \
    zod_CreateRGBSurface_compat((flags), (w), (h), (depth), (r), (g), (b), (a))

// Window-target versions of these became required in SDL2.
static inline void SDL_GL_SwapBuffers(void)
{
    if (g_compat_window) SDL_GL_SwapWindow(g_compat_window);
}
static inline void SDL_WarpMouse(int x, int y)
{
    if (g_compat_window) SDL_WarpMouseInWindow(g_compat_window, x, y);
}

// SDL_DisplayFormatAlpha was removed; closest equivalent is ConvertSurfaceFormat.
// SDL_ConvertSurfaceFormat can fail (returns NULL) when the source has an
// unusual mask layout; in that case fall back to ConvertSurface using a
// freshly-constructed ARGB8888 format descriptor.
static inline SDL_Surface *SDL_DisplayFormatAlpha(SDL_Surface *src)
{
    if (!src) return NULL;
    SDL_Surface *out = SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_ARGB8888, 0);
    if (out) return out;
    SDL_PixelFormat *fmt = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
    if (!fmt) return NULL;
    out = SDL_ConvertSurface(src, fmt, 0);
    SDL_FreeFormat(fmt);
    return out;
}

// SDL_DisplayFormat was removed; in SDL2 we just convert to a 32-bit RGB format.
static inline SDL_Surface *SDL_DisplayFormat(SDL_Surface *src)
{
    if (!src) return NULL;
    SDL_Surface *out = SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_RGB888, 0);
    if (out) return out;
    SDL_PixelFormat *fmt = SDL_AllocFormat(SDL_PIXELFORMAT_RGB888);
    if (!fmt) return NULL;
    out = SDL_ConvertSurface(src, fmt, 0);
    SDL_FreeFormat(fmt);
    return out;
}

// SDL_SRCCOLORKEY flag removed — pass SDL_TRUE to SDL_SetColorKey instead.
#ifndef SDL_SRCCOLORKEY
#define SDL_SRCCOLORKEY SDL_TRUE
#endif

// SDL_RLEACCEL is gone — folded into SDL_SetSurfaceRLE; map to 0 here so it
// can still be OR'd into legacy flag arguments without changing meaning.
#ifndef SDL_RLEACCEL
#define SDL_RLEACCEL 0
#endif

// SDL_SetAlpha was removed. SDL2 uses two calls: SetSurfaceAlphaMod for the
// alpha value, and SetSurfaceBlendMode to enable blending.
static inline int SDL_SetAlpha(SDL_Surface *s, Uint32 flags, Uint8 alpha)
{
    if (!s) return -1;
    SDL_SetSurfaceBlendMode(s, (flags & SDL_SRCALPHA) ? SDL_BLENDMODE_BLEND
                                                       : SDL_BLENDMODE_NONE);
    return SDL_SetSurfaceAlphaMod(s, alpha);
}


#endif
