#include "zmpclient.h"
#include "zhttp.h"

#include <cstdlib>
#include <cstdio>

using namespace std;

// --- tiny JSON readers, just enough for the orchestrator's flat, controlled
// responses (no nesting beyond a string array, no escaped quotes in values). Not
// a general JSON parser. ---

// Split a top-level array "[ {..}, {..} ]" into its object substrings.
static vector<string> json_split_array(const string &s)
{
	vector<string> out;
	size_t i = s.find('[');
	if(i == string::npos) return out;

	int depth = 0;
	bool in_str = false;
	size_t obj_start = string::npos;

	for(i++; i < s.size(); i++)
	{
		char c = s[i];
		if(in_str) { if(c == '"') in_str = false; continue; }
		if(c == '"') { in_str = true; continue; }

		if(c == '{') { if(depth == 0) obj_start = i; depth++; }
		else if(c == '}') { if(--depth == 0 && obj_start != string::npos) { out.push_back(s.substr(obj_start, i - obj_start + 1)); obj_start = string::npos; } }
		else if(c == ']' && depth == 0) break;
	}
	return out;
}

// "key":"value" -> value
static bool json_str(const string &obj, const string &key, string &out)
{
	string pat = "\"" + key + "\"";
	size_t k = obj.find(pat);
	if(k == string::npos) return false;
	size_t colon = obj.find(':', k + pat.size());
	if(colon == string::npos) return false;
	size_t q1 = obj.find('"', colon + 1);
	if(q1 == string::npos) return false;
	size_t q2 = obj.find('"', q1 + 1);
	if(q2 == string::npos) return false;
	out = obj.substr(q1 + 1, q2 - q1 - 1);
	return true;
}

// "key": <number> -> int
static bool json_int(const string &obj, const string &key, int &out)
{
	string pat = "\"" + key + "\"";
	size_t k = obj.find(pat);
	if(k == string::npos) return false;
	size_t colon = obj.find(':', k + pat.size());
	if(colon == string::npos) return false;
	size_t p = colon + 1;
	while(p < obj.size() && (obj[p] == ' ' || obj[p] == '\t')) p++;
	if(p >= obj.size()) return false;
	out = atoi(obj.c_str() + p);
	return true;
}

// Parse a flat JSON string array: ["a","b","c"] -> {a,b,c}. No escapes (our
// filenames have none).
static vector<string> json_string_array(const string &s)
{
	vector<string> out;
	size_t i = s.find('[');
	if(i == string::npos) return out;

	bool in_str = false;
	string cur;
	for(i++; i < s.size(); i++)
	{
		char c = s[i];
		if(in_str)
		{
			if(c == '"') { out.push_back(cur); cur.clear(); in_str = false; }
			else cur += c;
		}
		else if(c == '"') in_str = true;
		else if(c == ']') break;
	}
	return out;
}

static void orchestrator_target(string &host, int &port)
{
	// Default to the public PoC server so Multiplayer works out of the box.
	// Override with ZOD_ORCHESTRATOR=host[:port] (e.g. 127.0.0.1:8080 for a local
	// orchestrator).
	host = "z.0f.ee";
	port = 8080;

	const char *e = getenv("ZOD_ORCHESTRATOR");
	if(e && e[0])
	{
		string s = e;
		size_t c = s.find(':');
		if(c == string::npos) host = s;
		else { host = s.substr(0, c); port = atoi(s.c_str() + c + 1); }
	}
}

string ZMP_OrchestratorAddress()
{
	string h;
	int p;
	orchestrator_target(h, p);
	char buf[160];
	snprintf(buf, sizeof(buf), "%s:%d", h.c_str(), p);
	return buf;
}

bool ZMP_ListMatches(vector<MatchInfo> &out)
{
	out.clear();

	string host;
	int port;
	orchestrator_target(host, port);

	ZHttpResponse r = ZHttp_Get(host, port, "/matches");
	if(!r.ok || r.status != 200) return false;

	vector<string> objs = json_split_array(r.body);
	for(size_t i = 0; i < objs.size(); i++)
	{
		MatchInfo m;
		json_str(objs[i], "id", m.id);
		json_str(objs[i], "name", m.name);
		json_str(objs[i], "map", m.map);
		json_str(objs[i], "host", m.host);
		json_int(objs[i], "port", m.port);
		json_int(objs[i], "players", m.players);
		out.push_back(m);
	}
	return true;
}

bool ZMP_CreateMatch(const string &map, const vector<string> &bots, MatchInfo &out)
{
	string host;
	int port;
	orchestrator_target(host, port);

	string body = "{\"map\":\"" + map + "\",\"bots\":[";
	for(size_t i = 0; i < bots.size(); i++)
	{
		if(i) body += ",";
		body += "\"" + bots[i] + "\"";
	}
	body += "]}";

	ZHttpResponse r = ZHttp_Post(host, port, "/matches", body);
	if(!r.ok || (r.status != 201 && r.status != 200)) return false;

	json_str(r.body, "id", out.id);
	json_str(r.body, "host", out.host);
	json_str(r.body, "map", out.map);
	json_int(r.body, "port", out.port);
	json_int(r.body, "players", out.players);

	return out.port > 0 && !out.host.empty();
}

bool ZMP_ListMaps(vector<string> &out)
{
	out.clear();

	string host;
	int port;
	orchestrator_target(host, port);

	ZHttpResponse r = ZHttp_Get(host, port, "/maps");
	if(!r.ok || r.status != 200) return false;

	out = json_string_array(r.body);
	return true;
}
