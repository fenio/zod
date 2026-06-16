#include "zprefs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

using namespace std;

bool zod_classic_mouse = true;

//#73: remembered Options values. Defaults match the in-code defaults - full
//volume (SOUND_100 == 4, see enum sound_setting in zsdl.h) and normal speed.
int zod_volume_setting = 4;
double zod_game_speed = 1.0;

//defined in zobject.cpp (the render-smoothing master toggle)
extern bool zod_render_smoothing;

static string prefs_path()
{
	const char *home = getenv("HOME");
	if(!home || !home[0]) home = getenv("USERPROFILE"); //Windows
	if(!home || !home[0]) return ".zod_prefs";          //last resort: cwd

	return string(home) + "/.zod_prefs";
}

void ZPrefs_Load()
{
	FILE *fp = fopen(prefs_path().c_str(), "r");

	if(fp)
	{
		char line[256];

		while(fgets(line, sizeof(line), fp))
		{
			char *eq = strchr(line, '=');
			if(!eq) continue;
			*eq = 0;

			const char *key = line;
			bool value = (eq[1] == '1');

			if(!strcmp(key, "classic_mouse")) zod_classic_mouse = value;
			else if(!strcmp(key, "render_smoothing")) zod_render_smoothing = value;
			else if(!strcmp(key, "volume")) zod_volume_setting = atoi(eq + 1);
			else if(!strcmp(key, "game_speed")) zod_game_speed = atof(eq + 1);
		}

		fclose(fp);
	}

	//env vars override the file for this launch
	{
		const char *e = getenv("ZOD_CLASSIC_MOUSE");
		if(e && e[0]) zod_classic_mouse = (e[0] != '0');
	}
	{
		const char *e = getenv("ZOD_SMOOTH");
		if(e && e[0] == '0') zod_render_smoothing = false;
	}
}

void ZPrefs_Save()
{
	FILE *fp = fopen(prefs_path().c_str(), "w");

	if(!fp) return;

	fprintf(fp, "classic_mouse=%d\n", zod_classic_mouse ? 1 : 0);
	fprintf(fp, "render_smoothing=%d\n", zod_render_smoothing ? 1 : 0);
	fprintf(fp, "volume=%d\n", zod_volume_setting);
	fprintf(fp, "game_speed=%g\n", zod_game_speed);
	fclose(fp);
}
