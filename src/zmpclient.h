#ifndef _ZMPCLIENT_H_
#define _ZMPCLIENT_H_

#include <string>
#include <vector>

// Client for the match orchestrator (see orchestrator/). Talks JSON/HTTP to it
// and hands the menus simple structs - the menus never touch HTTP or JSON. The
// orchestrator address comes from $ZOD_ORCHESTRATOR (default 127.0.0.1:8080),
// so "everything local" needs no config.

struct MatchInfo
{
	std::string id;
	std::string name;
	std::string map;
	std::string host;
	int port;
	int players;

	MatchInfo() : port(-1), players(-1) {}
};

// GET /matches. Returns the live matches; ok=false on any transport/parse error.
bool ZMP_ListMatches(std::vector<MatchInfo> &out);

// POST /matches with {map, bots}. On success fills `out` (incl. host/port to
// join) and returns true.
bool ZMP_CreateMatch(const std::string &map, const std::vector<std::string> &bots, MatchInfo &out);

// GET /maps. The .map filenames the orchestrator will accept (the in-game create
// form needs these - the client only knows campaign display names, not files).
bool ZMP_ListMaps(std::vector<std::string> &out);

// The orchestrator address the menus would connect to (for display).
std::string ZMP_OrchestratorAddress();

#endif
