#include "zprefs.h"
#include "constants.h"	//#75: DEFAULT_MAX_UNITS_PER_TEAM + UNIT_LIMIT bounds

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

//bot AI difficulty - default NORMAL keeps the original behavior (opt-in)
int zod_bot_difficulty = BOT_DIFF_NORMAL;
const char *bot_difficulty_name[MAX_BOT_DIFFICULTY] = { "Easy", "Normal", "Hard", "Expert" };
int zod_max_units_per_team = DEFAULT_MAX_UNITS_PER_TEAM;	//#75

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
			else if(!strcmp(key, "bot_difficulty"))
			{
				zod_bot_difficulty = atoi(eq + 1);
				if(zod_bot_difficulty < 0 || zod_bot_difficulty >= MAX_BOT_DIFFICULTY)
					zod_bot_difficulty = BOT_DIFF_NORMAL;
			}
			else if(!strcmp(key, "max_units_per_team"))
			{
				zod_max_units_per_team = atoi(eq + 1);
				if(zod_max_units_per_team < UNIT_LIMIT_MIN) zod_max_units_per_team = UNIT_LIMIT_MIN;
				if(zod_max_units_per_team > UNIT_LIMIT_MAX) zod_max_units_per_team = UNIT_LIMIT_MAX;
			}
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
	fprintf(fp, "bot_difficulty=%d\n", zod_bot_difficulty);
	fprintf(fp, "max_units_per_team=%d\n", zod_max_units_per_team);
	fclose(fp);
}
