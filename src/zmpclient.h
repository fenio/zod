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
	int capacity;   //max humans = the map's player slots

	MatchInfo() : port(-1), players(-1), capacity(0) {}
};

// A map the orchestrator can spawn, with its player-slot count (from --map-info).
struct MapMeta
{
	std::string name;
	int players;

	MapMeta() : players(0) {}
};

// GET /matches. Returns the live matches; ok=false on any transport/parse error.
bool ZMP_ListMatches(std::vector<MatchInfo> &out);

// POST /matches with {map, bots}. On success fills `out` (incl. host/port to
// join) and returns true.
bool ZMP_CreateMatch(const std::string &map, const std::vector<std::string> &bots, MatchInfo &out);

// POST /matchmake. Returns the open match to drop a "play with someone" joiner
// into (the orchestrator seeds one if needed). Fills `out` (host/port) on success.
bool ZMP_Matchmake(MatchInfo &out);

// GET /maps. The maps the orchestrator can spawn, each with its player-slot count
// (the create form needs the filenames; the count drives the "(NP)" label).
bool ZMP_ListMaps(std::vector<MapMeta> &out);

// The orchestrator address the menus would connect to (for display).
std::string ZMP_OrchestratorAddress();

#endif
