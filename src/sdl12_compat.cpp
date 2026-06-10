#include "sdl12_compat.h"
#include <stdio.h>
#include <stdlib.h>   // getenv, for the look-tuning env vars

SDL_Window *g_compat_window = NULL;

// Modern scaled-framebuffer rendering: the game draws to an off-screen
// surface at its (low) logical resolution, and we GPU-scale that up to a
// large window via an SDL_Renderer. This makes the 1990s pixel art big and
// crisp on today's high-resolution / Retina displays instead of being a
// tiny, OS-upscaled (blurry) region. The whole change is contained here, so
// none of the game's blitting code has to know about it.
static SDL_Renderer *g_renderer       = NULL;
static SDL_Texture  *g_frame_tex      = NULL;  // GPU texture we present
static SDL_Surface  *g_frame_surface  = NULL;  // CPU surface the game blits to
static int           g_logical_w      = 0;
static int           g_logical_h      = 0;
static int           g_scanlines      = 0;   // ZOD_SCANLINES: CRT-style overlay
static int           g_scanline_alpha = 64;  // ZOD_SCANLINES value = darkness 0-255

static void zod_destroy_scaler()
{
    if (g_frame_tex)     { SDL_DestroyTexture(g_frame_tex); g_frame_tex = NULL; }
    if (g_frame_surface) { SDL_DestroySurface(g_frame_surface); g_frame_surface = NULL; }
}

SDL_Surface *SDL_SetVideoMode(int w, int h, int /*bpp*/, Uint32 flags)
{
    if (w <= 0) w = 800;
    if (h <= 0) h = 600;

    bool fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;

    // --- look-tuning knobs (env vars, so they can be compared without rebuilds) ---
    // ZOD_FILTER:    1 = linear/smooth (default), 0 = nearest/crisp
    // ZOD_INTEGER:   1 = force perfectly-even integer scaling (may add bars)
    // ZOD_SCANLINES: 0 = off (default), or 1-255 = CRT scanline darkness
    const char *e_filter = getenv("ZOD_FILTER");   // applied per-texture below (SDL3)
    const char *e_scan = getenv("ZOD_SCANLINES");
    if (e_scan) { g_scanline_alpha = atoi(e_scan); g_scanlines = g_scanline_alpha > 0; }

    if (!g_compat_window)
    {
        // Size the window to fill ~90% of the desktop while preserving the
        // game's aspect ratio, so the logical framebuffer scales up nicely.
        int win_w = w, win_h = h;
        const SDL_DisplayMode *dm = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
        if (dm && dm->w > 0 && dm->h > 0)
        {
            double sx = (double)(dm->w * 9 / 10) / w;
            double sy = (double)(dm->h * 9 / 10) / h;
            double s  = sx < sy ? sx : sy;
            if (s < 1.0) s = 1.0;
            win_w = (int)(w * s);
            win_h = (int)(h * s);
        }

        Uint32 wflags = SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE;
        if (fullscreen) wflags |= SDL_WINDOW_FULLSCREEN;

        g_compat_window = SDL_CreateWindow("Zod Engine", win_w, win_h, wflags);
        if (!g_compat_window) return NULL;
        SDL_SetWindowPosition(g_compat_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

        g_renderer = SDL_CreateRenderer(g_compat_window, NULL);
        if (!g_renderer) return NULL;
        SDL_SetRenderVSync(g_renderer, 1);
    }
    else
    {
        SDL_SetWindowFullscreen(g_compat_window, fullscreen);
    }

    // (Re)build the logical framebuffer + texture when the resolution changes.
    if (w != g_logical_w || h != g_logical_h || !g_frame_surface || !g_frame_tex)
    {
        zod_destroy_scaler();
        g_logical_w = w;
        g_logical_h = h;

        // Aspect-correct (or integer) scaling; also maps mouse events into this space.
        SDL_SetRenderLogicalPresentation(g_renderer, w, h,
            getenv("ZOD_INTEGER") ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
                                  : SDL_LOGICAL_PRESENTATION_LETTERBOX);
        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);

        g_frame_tex = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888,
                                        SDL_TEXTUREACCESS_STREAMING, w, h);
        // SDL3 sets scale quality per-texture (SDL_HINT_RENDER_SCALE_QUALITY is gone).
        SDL_SetTextureScaleMode(g_frame_tex,
            (e_filter && e_filter[0] == '0') ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);

        g_frame_surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    }

    return g_frame_surface;
}

int SDL_Flip(SDL_Surface * /*screen*/)
{
    if (!g_renderer || !g_frame_tex || !g_frame_surface) return -1;

    SDL_UpdateTexture(g_frame_tex, NULL, g_frame_surface->pixels, g_frame_surface->pitch);
    SDL_RenderClear(g_renderer);
    SDL_RenderTexture(g_renderer, g_frame_tex, NULL, NULL);

    // Optional CRT-style scanlines: a translucent dark line over every other
    // logical row (drawn in logical space, so it scales with the image).
    if (g_scanlines)
    {
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, (Uint8)g_scanline_alpha);
        for (int y = 0; y < g_logical_h; y += 2)
            SDL_RenderLine(g_renderer, 0.0f, (float)y, (float)(g_logical_w - 1), (float)y);
    }

    SDL_RenderPresent(g_renderer);
    return 0;
}

// The game passes logical (render) coordinates; map them to window coordinates
// for the scaled framebuffer so warps land where the cursor visually is.
// (Without this, middle-button drag-scroll mis-warps and the deltas skew.)
void SDL_WarpMouse(int x, int y)
{
    if (!g_compat_window) return;
    if (g_renderer)
    {
        float wx = (float)x, wy = (float)y;
        SDL_RenderCoordinatesToWindow(g_renderer, (float)x, (float)y, &wx, &wy);
        SDL_WarpMouseInWindow(g_compat_window, wx, wy);
    }
    else
        SDL_WarpMouseInWindow(g_compat_window, (float)x, (float)y);
}

void SDL_WM_SetCaption(const char *title, const char * /*icon*/)
{
    if (g_compat_window && title) SDL_SetWindowTitle(g_compat_window, title);
}

void SDL_WM_SetIcon(SDL_Surface *icon, Uint8 * /*mask*/)
{
    if (g_compat_window && icon) SDL_SetWindowIcon(g_compat_window, icon);
}

int SDL_WM_GrabInput(int mode)
{
    if (!g_compat_window) return SDL_GRAB_OFF;
    if (mode == SDL_GRAB_QUERY)
        return SDL_GetWindowMouseGrab(g_compat_window) ? SDL_GRAB_ON : SDL_GRAB_OFF;
    SDL_SetWindowMouseGrab(g_compat_window, mode ? true : false);
    return mode;
}
