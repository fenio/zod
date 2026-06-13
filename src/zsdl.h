#ifndef _ZSDL_H_
#define _ZSDL_H_

#include <SDL3/SDL.h>
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "zaudio.h"
#include "zvideo.h"
#include "zsdl_helpers.h"
#include <string>
#include "SDL_rotozoom.h"
#include "zsdl_opengl.h"

using namespace std;

enum sound_setting
{
	SOUND_0, SOUND_25, SOUND_50, SOUND_75, SOUND_100, MAX_SOUND_SETTINGS
};

const string sound_setting_string[MAX_SOUND_SETTINGS] = 
{
	"0%", "25%", "50%", "75%", "100%"
};

class SDL_RotoZoomSurface
{
public:
	SDL_RotoZoomSurface();

	void LoadBaseImage(string filename);
	SDL_Surface *GetImage(int angle, double zoom);
	SDL_Surface *CreateImage(int angle, double zoom);

private:
	SDL_Surface *base_surface;
	SDL_Surface *the_surface[360][100];
};

class SDL_ZoomSurface
{
public:
	SDL_ZoomSurface();

	void LoadBaseImage(string filename);
	SDL_Surface *GetImage(double zoom);
	SDL_Surface *CreateImage(double zoom);

private:
	SDL_Surface *base_surface;
	SDL_Surface *zoom_surface[100];
};

class SDL_RotateSurface
{
public:
	SDL_RotateSurface();

	void LoadBaseImage(string filename);
	SDL_Surface *GetImage(int angle);

private:
	SDL_Surface *base_surface;
	SDL_Surface *rotated_surface[360];
};

SDL_Surface *ZSDL_ConvertImage(SDL_Surface *src);
SDL_Surface *ZSDL_IMG_Load(string filename);
void ZSDL_SetDataRoot(const char *root);
std::string ZSDL_DataPath(const std::string &f);
SDL_Surface *IMG_Load_Error(string filename);
ZAudio_Music *ZSDL_LoadMusic(string filename);
ZAudio_Sound *ZSDL_LoadSound(string filename);
SDL_Surface *CopyImage(SDL_Surface *original);
SDL_Surface *CopyImageShifted(SDL_Surface *original, int x, int y);
void put32pixel(SDL_Surface *surface, int x, int y, SDL_Color color);
SDL_Color get32pixel(SDL_Surface *surface, int x, int y);
int ZSDL_PlayMusic(ZAudio_Music *music, int eh);
int ZSDL_PlayChannel(int ch, ZAudio_Sound *wav, int repeat);
void ZSDL_SetMusicOn(bool iton);
void draw_box(SDL_Surface *surface, SDL_Rect dim, SDL_Color color, int max_x, int max_y);
void draw_selection_box(SDL_Surface *surface, SDL_Rect dim, SDL_Color color, int max_x, int max_y);
int AngleFromLoc(float dx, float dy);
SDL_Surface *ZSDL_NewSurface(int w, int h);
void ZSDL_ModifyBlack(SDL_Surface *surface);
void ZSDL_BlitSurface(SDL_Surface *src, int fx, int fy, int fw, int fh, SDL_Surface *dest, int x, int y);
void ZSDL_BlitHitSurface(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect, bool render_hit = true);
void ZSDL_BlitTileSurface(SDL_Surface *src, int fx, int fy, SDL_Surface *dest, int x, int y);
void ZSDL_DestroySurface(SDL_Surface *&surface);
void ZSDL_Quit();

#endif
