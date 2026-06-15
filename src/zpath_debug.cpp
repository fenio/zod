#include "zpath_debug.h"
#include "zobject.h"
#include "constants.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int zpath_log_level = 0;

static FILE *log_file = NULL;
static SDL_Mutex *log_mutex = NULL;
static Uint64 log_start_ticks = 0;

void ZPathLog_Init()
{
	const char *e = getenv("ZOD_PATHLOG");

	if(!e || !e[0] || e[0] == '0') return;

	zpath_log_level = (e[0] == '2') ? 2 : 1;

	const char *filename = getenv("ZOD_PATHLOG_FILE");
	if(!filename || !filename[0]) filename = "zod_path.log";

	if(!strcmp(filename, "-"))
		log_file = stdout;
	else
	{
		log_file = fopen(filename, "w");
		if(!log_file)
		{
			printf("ZPathLog_Init::could not open '%s', logging to stdout instead\n", filename);
			log_file = stdout;
		}
	}

	log_mutex = SDL_CreateMutex();
	log_start_ticks = SDL_GetTicks();

	{
		char date[64];
		time_t now = time(NULL);
		strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", localtime(&now));
		ZPathLog("pathfinding log started %s (ZOD_PATHLOG=%d)", date, zpath_log_level);
	}

	if(log_file != stdout)
		printf("pathfinding debug log -> %s (ZOD_PATHLOG_FILE=- for stdout)\n", filename);
}

void ZPathLog(const char *format, ...)
{
	va_list ap;

	if(!log_file) return;

	SDL_LockMutex(log_mutex);

	fprintf(log_file, "[%8.3f] ", (SDL_GetTicks() - log_start_ticks) / 1000.0);

	va_start(ap, format);
	vfprintf(log_file, format, ap);
	va_end(ap);

	fputc('\n', log_file);
	//flush every line so the log survives a crash and tail -f stays live
	fflush(log_file);

	SDL_UnlockMutex(log_mutex);
}

// ---- always-on diagnostic log (zod_diag.log) ----

static FILE *diag_file = NULL;
static SDL_Mutex *diag_mutex = NULL;
static Uint64 diag_start_ticks = 0;
static std::string diag_path;

void ZDiag_Init(const char *version)
{
	// prefer the game dir (cwd, where a portable Windows build lives so the file
	// sits right next to the .exe); fall back to $HOME for read-only installs
	// (e.g. a Homebrew Cellar), so we always end up with a writable log.
	const char *candidates[2] = { "zod_diag.log", NULL };
	std::string home_path;
	const char *home = getenv("HOME");
	if(home && home[0]) { home_path = std::string(home) + "/zod_diag.log"; candidates[1] = home_path.c_str(); }

	for(int i = 0; i < 2 && !diag_file; i++)
	{
		if(!candidates[i]) continue;
		diag_file = fopen(candidates[i], "w");
		if(diag_file) diag_path = candidates[i];
	}
	if(!diag_file) return;

	diag_mutex = SDL_CreateMutex();
	diag_start_ticks = SDL_GetTicks();

	const char *os =
#if defined(_WIN32)
		"windows";
#elif defined(__ANDROID__)
		"android";
#elif defined(__APPLE__)
		"macos";
#else
		"linux";
#endif

	char date[64];
	time_t now = time(NULL);
	strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", localtime(&now));

	ZDiag("zod diagnostic log");
	ZDiag("started %s | version %s | os %s", date, version ? version : "?", os);
	ZDiag("(in-game: select a misbehaving unit and press F12 or '\\' to append its state)");
	ZDiag("to report: open an issue at https://github.com/fenio/zod/issues and attach this file");

	printf("diagnostic log -> %s\n", diag_path.c_str());
}

void ZDiag(const char *format, ...)
{
	va_list ap;

	if(!diag_file) return;

	if(diag_mutex) SDL_LockMutex(diag_mutex);

	fprintf(diag_file, "[%8.3f] ", (SDL_GetTicks() - diag_start_ticks) / 1000.0);

	va_start(ap, format);
	vfprintf(diag_file, format, ap);
	va_end(ap);

	fputc('\n', diag_file);
	fflush(diag_file);

	if(diag_mutex) SDL_UnlockMutex(diag_mutex);
}

const char *ZDiag_Path() { return diag_path.c_str(); }

std::string ZPathLog_UnitDesc(ZObject *obj)
{
	char buf[160];
	int cx, cy;

	if(!obj) return "(null object)";

	obj->GetCenterCords(cx, cy);

	snprintf(buf, sizeof(buf), "%s %s#%d (%d,%d)t(%d,%d)",
		team_type_string[obj->GetOwner()].c_str(),
		obj->GetObjectName().c_str(), obj->GetRefID(),
		cx, cy, cx / 16, cy / 16);

	return buf;
}

const char *ZPathLog_WPModeName(int mode)
{
	switch(mode)
	{
	case MOVE_WP: return "move";
	case FORCE_MOVE_WP: return "force_move";
	case ENTER_WP: return "enter";
	case ATTACK_WP: return "attack";
	case CRANE_REPAIR_WP: return "crane_repair";
	case UNIT_REPAIR_WP: return "unit_repair";
	case AGRO_WP: return "agro";
	case ENTER_FORT_WP: return "enter_fort";
	case DODGE_WP: return "dodge";
	case PICKUP_GRENADES_WP: return "pickup_grenades";
	}

	return "unknown";
}
