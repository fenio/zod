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
