#include "sdl12_compat.h"
#include <stdio.h>

SDL_Window *g_compat_window = NULL;

SDL_Surface *SDL_SetVideoMode(int w, int h, int /*bpp*/, Uint32 flags)
{
    Uint32 wflags = SDL_WINDOW_SHOWN;
    if (flags & SDL_WINDOW_RESIZABLE)  wflags |= SDL_WINDOW_RESIZABLE;
    if (flags & SDL_WINDOW_FULLSCREEN) wflags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    if (flags & SDL_WINDOW_OPENGL)     wflags |= SDL_WINDOW_OPENGL;

    if (!g_compat_window)
    {
        g_compat_window = SDL_CreateWindow("Zod Engine",
                                           SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED,
                                           w > 0 ? w : 800,
                                           h > 0 ? h : 600,
                                           wflags);
        if (!g_compat_window) return NULL;
    }
    else if (w > 0 && h > 0)
    {
        SDL_SetWindowSize(g_compat_window, w, h);
    }

    return SDL_GetWindowSurface(g_compat_window);
}

int SDL_Flip(SDL_Surface * /*screen*/)
{
    if (!g_compat_window) return -1;
    return SDL_UpdateWindowSurface(g_compat_window);
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
        return SDL_GetWindowGrab(g_compat_window) ? SDL_GRAB_ON : SDL_GRAB_OFF;
    SDL_SetWindowGrab(g_compat_window, mode ? SDL_TRUE : SDL_FALSE);
    return mode;
}
