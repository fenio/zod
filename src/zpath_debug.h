#ifndef _ZPATH_DEBUG_H_
#define _ZPATH_DEBUG_H_

#include <string>

// Optional pathfinding / movement debug log, for collecting evidence when a
// unit gets stuck or takes a bad route. Off by default; enable with:
//
//   ZOD_PATHLOG=1        log orders, A* requests/results, routes, blocks,
//                        arrivals and a "no progress" stall watchdog
//   ZOD_PATHLOG=2        additionally log every leg of every route
//   ZOD_PATHLOG_FILE=f   write to f instead of ./zod_path.log ("-" = stdout)
//
// All hooks live on the server side (the authority on movement), so the log
// is the ground truth of what the engine decided, not what the client drew.
// ZPathLog() is thread-safe: the A* search runs on worker threads.

class ZObject;

extern int zpath_log_level;

#define ZPATH_LOG_ON      (zpath_log_level >= 1)
#define ZPATH_LOG_VERBOSE (zpath_log_level >= 2)

// Read the env vars and open the log; call once at startup, before any
// pathfinding threads exist. Does nothing when ZOD_PATHLOG is unset/0.
void ZPathLog_Init();

// printf-style; prepends a timestamp and appends a newline.
void ZPathLog(const char *format, ...);

// "red medium#412 (672,500)t(42,31)" — team, unit, ref_id, pixel + tile pos.
std::string ZPathLog_UnitDesc(ZObject *obj);

const char *ZPathLog_WPModeName(int mode);

#endif
