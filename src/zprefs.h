#ifndef _ZPREFS_H_
#define _ZPREFS_H_

#include <string>

// Player-local preferences, persisted to ~/.zod_prefs and editable from the
// in-game Options menu. Load order: defaults, then the prefs file, then the
// env vars (ZOD_CLASSIC_MOUSE / ZOD_SMOOTH) - so env always wins a launch.

// Classic (original-Z) mouse: left click orders and drops the selection,
// right click cancels. Off = RTS-style right-click orders.
extern bool zod_classic_mouse;

// Audio volume (a sound_setting index, 0..MAX_SOUND_SETTINGS-1) and game speed
// multiplier, remembered across sessions so the Options menu values stick (#73).
extern int zod_volume_setting;
extern double zod_game_speed;
extern double zod_menu_scale;   //#196: UI scale for menus + resume bar

// Bot AI difficulty (an index into the profiles in zbot.cpp). NORMAL == the
// original behavior, and is the default - so a regular campaign with no -D flag
// and no Options change plays exactly as before. Steers *decision quality*
// (think rate, how many units it orders, how fast, how aggressively), never unit
// stats. Read live by the bot thread.
enum bot_difficulty { BOT_DIFF_EASY, BOT_DIFF_NORMAL, BOT_DIFF_HARD, BOT_DIFF_EXPERT, MAX_BOT_DIFFICULTY };
extern int zod_bot_difficulty;
extern const char *bot_difficulty_name[MAX_BOT_DIFFICULTY];

//#75: max combat units per team. Adjustable in Options (UNIT_LIMIT_MIN..MAX);
//read at the start of each game, so a change takes effect from the next game.
extern int zod_max_units_per_team;

//#246: the player's display name, persisted so it's not "Player" for everyone in
//multiplayer. Editable in Options. Empty here means "unset" - main defaults it to
//the OS login name on first run; an explicit -n on the command line still wins.
extern std::string zod_player_name;

void ZPrefs_Load();
void ZPrefs_Save();

#endif
