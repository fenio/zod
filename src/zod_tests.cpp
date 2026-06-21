// Standalone functional tests for the Zod engine's load-bearing, hard-to-eyeball
// logic - starting with pathfinding, where most regressions have landed. No test
// framework: a CHECK macro counts pass/fail and main() returns non-zero on any
// failure, so CI (and `make test`) can gate on it.
//
// Built as the `zod_tests` target; links only the pathfinding sources + SDL
// (for the engine's mutex), with tiny stubs for the debug-log symbols so we
// don't drag in the whole engine.

#include "zpath_finding.h"
#include "zpath_finding_astar.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <vector>

using namespace std;

// --- stubs for the pathlog symbols zpath_finding.cpp references ---
int zpath_log_level = 0;
void ZPathLog(const char *, ...) {}

// --- tiny test harness ---
static int g_checks = 0, g_fails = 0;

#define CHECK(cond, msg) do { \
	g_checks++; \
	if(!(cond)) { g_fails++; printf("  FAIL: %s  (%s:%d)\n", (msg), __FILE__, __LINE__); } \
} while(0)

// Build a w x h all-normal map, then run `setup` to carve walls/roads/water.
// Tile coords for SetTileInfo/SetImpassable; pixel coords (tile*16 + 8) for the
// path queries.
static void build_map(ZPath_Finding_Engine &pf, int w, int h)
{
	pf.ResetTileInfo(w, h);
	for(int y = 0; y < h; y++)
		for(int x = 0; x < w; x++)
			pf.SetTileInfo(x, y, PF_NORMAL);
}

static void finalize_map(ZPath_Finding_Engine &pf)
{
	pf.SetTileWideWeights();
	pf.RebuildRegions();
}

// centre-of-tile pixel coordinate
static int px(int tile) { return tile * 16 + 8; }

// Run Find_Path and wait for the worker thread to deliver the route. Returns
// the number of pf_points (0 = direct/unreachable, >0 = an A* route).
static int find_path_blocking(ZPath_Finding_Engine &pf, int stx, int sty, int etx, int ety, bool is_robot)
{
	int id = pf.Find_Path(px(stx), px(sty), px(etx), px(ety), is_robot, false, 1);
	if(id <= 0) return 0; // direct path or unreachable - no thread spawned

	// poll the response list (the worker pushes when A* finishes)
	for(int tries = 0; tries < 2000; tries++)
	{
		int n = -1;
		pf.Lock_List();
		if(pf.GetList().size())
			n = (int)pf.GetList()[0]->pf_point_list.size();
		pf.Unlock_List();

		if(n >= 0) { pf.Clear_Response_List(); return n; }
		SDL_Delay(1);
	}
	printf("  (timed out waiting for A* result)\n");
	return -1;
}

// Run Find_Path and return the route as a list of tile coords, with the start
// tile prepended (Do_Astar's route omits it). Returns false if no A* route ran
// (direct/unreachable). Robot (1x1) paths, so a point's tile == pixel/16.
static bool get_route_tiles(ZPath_Finding_Engine &pf, int stx, int sty, int etx, int ety,
	bool is_robot, vector<ZPath_Finding_AStar::pf_point> &out)
{
	out.clear();
	int id = pf.Find_Path(px(stx), px(sty), px(etx), px(ety), is_robot, false, 1);
	if(id <= 0) return false;

	for(int tries = 0; tries < 2000; tries++)
	{
		bool got = false;
		pf.Lock_List();
		if(pf.GetList().size())
		{
			out.push_back(ZPath_Finding_AStar::pf_point(stx, sty));
			for(size_t k = 0; k < pf.GetList()[0]->pf_point_list.size(); k++)
			{
				ZPath_Finding_AStar::pf_point &p = pf.GetList()[0]->pf_point_list[k];
				out.push_back(ZPath_Finding_AStar::pf_point(p.x / 16, p.y / 16));
			}
			got = true;
		}
		pf.Unlock_List();
		if(got) { pf.Clear_Response_List(); return true; }
		SDL_Delay(1);
	}
	printf("  (timed out waiting for A* result)\n");
	return false;
}

static int iabs(int v) { return v < 0 ? -v : v; }

// The invariant a correct A* route must satisfy: every leg is a straight line
// (orthogonal or perfect diagonal), every tile traced along it is passable, and
// the route ends on the goal. The RemovePoint open-list corruption broke exactly
// this - it emitted a non-adjacent jump leg that cut across impassable water.
static bool route_is_valid(ZPath_Finding_Engine &pf, vector<ZPath_Finding_AStar::pf_point> &route,
	int etx, int ety, bool is_robot, const char *&why)
{
	if(route.size() < 2) { why = "route has fewer than 2 points"; return false; }

	for(size_t k = 1; k < route.size(); k++)
	{
		int ax = route[k-1].x, ay = route[k-1].y;
		int bx = route[k].x,   by = route[k].y;
		int dx = bx - ax, dy = by - ay;

		if(dx != 0 && dy != 0 && iabs(dx) != iabs(dy)) { why = "leg is not a straight line (corrupt parent chain)"; return false; }

		int sx = dx == 0 ? 0 : (dx > 0 ? 1 : -1);
		int sy = dy == 0 ? 0 : (dy > 0 ? 1 : -1);
		int steps = iabs(dx) > iabs(dy) ? iabs(dx) : iabs(dy);
		for(int s = 0; s <= steps; s++)
			if(!pf.TilePassable(ax + sx*s, ay + sy*s, is_robot)) { why = "leg crosses an impassable tile"; return false; }
	}

	if(route.back().x != etx || route.back().y != ety) { why = "route does not end at the goal"; return false; }
	return true;
}

// ----------------------------------------------------------------------------

static void test_passability()
{
	printf("passability / impassable:\n");
	ZPath_Finding_Engine pf;
	build_map(pf, 8, 8);
	pf.SetImpassable(4, 4, true);
	finalize_map(pf);

	CHECK(pf.TilePassable(0, 0, true), "open tile is passable (robot)");
	CHECK(!pf.TilePassable(4, 4, true), "impassable tile is not passable (robot)");
	CHECK(!pf.TilePassable(4, 4, false), "impassable tile is not passable (vehicle)");

	int sx, sy;
	CHECK(pf.WithinImpassable(px(4) - 8, px(4) - 8, 14, 14, sx, sy, true),
		"collision box over the impassable tile is detected");
	CHECK(!pf.WithinImpassable(px(0), px(0), 14, 14, sx, sy, true),
		"collision box on open ground is clear");
}

static void test_direct_path()
{
	printf("direct path:\n");
	ZPath_Finding_Engine pf;
	build_map(pf, 12, 12);
	// a vertical wall at x=6, leaving a gap at y=0 (so regions still connect)
	for(int y = 1; y < 12; y++) pf.SetImpassable(6, y, true);
	finalize_map(pf);

	CHECK(pf.Direct_Path_Possible(px(1), px(5), px(4), px(5), true, false),
		"clear straight line is direct");
	CHECK(!pf.Direct_Path_Possible(px(1), px(5), px(10), px(5), true, false),
		"straight line through a wall is NOT direct");
}

static void test_regions()
{
	printf("regions (reachability):\n");
	ZPath_Finding_Engine pf;
	build_map(pf, 12, 12);
	// a full vertical wall splitting the map into two regions
	for(int y = 0; y < 12; y++) pf.SetImpassable(6, y, true);
	finalize_map(pf);

	CHECK(pf.InSameRegion(px(2), px(5), px(4), px(5), true),
		"two tiles on the same side are in one region");
	CHECK(!pf.InSameRegion(px(2), px(5), px(10), px(5), true),
		"tiles on opposite sides of a full wall are unreachable");
}

static void test_astar_routes()
{
	printf("A* routing:\n");
	ZPath_Finding_Engine pf;
	build_map(pf, 16, 16);
	// a wall at x=8 from y=0..12, leaving a gap at the bottom (y=13..15)
	for(int y = 0; y <= 12; y++) pf.SetImpassable(8, y, true);
	finalize_map(pf);

	int n = find_path_blocking(pf, 2, 2, 13, 2, true);
	CHECK(n > 0, "A* finds a route around a wall (non-empty)");

	// fully walled off -> different regions -> Find_Path returns 0 (no route)
	ZPath_Finding_Engine pf2;
	build_map(pf2, 12, 12);
	for(int y = 0; y < 12; y++) pf2.SetImpassable(6, y, true);
	finalize_map(pf2);
	CHECK(find_path_blocking(pf2, 2, 5, 10, 5, true) == 0,
		"no A* route across a full wall (unreachable)");
}

// A serpentine maze forces a long winding route with heavy open-list churn -
// exactly the conditions under which the RemovePoint index bug corrupted a node.
// The route must stay a continuous, passable, straight-legged path to the goal.
static void test_path_continuity()
{
	printf("A* path continuity (serpentine maze):\n");
	ZPath_Finding_Engine pf;
	const int w = 24, h = 24;
	build_map(pf, w, h);

	// horizontal walls every 3 rows, each leaving a one-tile gap that alternates
	// between the left and right edge - so the only route snakes back and forth.
	for(int row = 2, band = 0; row < h - 2; row += 3, band++)
	{
		int gap = (band % 2 == 0) ? (w - 2) : 1;   // gap near right edge, then left
		for(int x = 0; x < w; x++)
			if(x != gap) pf.SetImpassable(x, row, true);
	}
	finalize_map(pf);

	vector<ZPath_Finding_AStar::pf_point> route;
	bool ran = get_route_tiles(pf, 1, 1, w - 2, h - 2, true, route);
	CHECK(ran, "A* produced a route through the serpentine maze");

	const char *why = "ok";
	CHECK(ran && route_is_valid(pf, route, w - 2, h - 2, true, why),
		"serpentine route is continuous, passable, and reaches the goal");
	if(ran && !route_is_valid(pf, route, w - 2, h - 2, true, why))
		printf("    -> %s\n", why);
}

// Regression guard for the open-list (pf_point_array::RemovePoint) corruption:
// run many queries over a deterministic pseudo-random maze and assert EVERY route
// is a valid continuous path. The bug was intermittent (it needed the removed
// node to be the open list's tail and its tile revisited), so volume is the test.
static void test_path_validity_stress()
{
	printf("A* path validity (randomized stress, regression for open-list corruption):\n");

	unsigned int seed = 99991u;
#define TST_RND() (seed = seed * 1103515245u + 12345u, (int)((seed >> 16) & 0x7fff))

	const int w = 40, h = 40;
	int checked = 0, invalid = 0;
	const char *first_why = "ok";

	for(int trial = 0; trial < 12; trial++)
	{
		ZPath_Finding_Engine pf;
		build_map(pf, w, h);

		// ~22% scattered impassable tiles - lots of detours, lots of open-list ops
		for(int y = 0; y < h; y++)
			for(int x = 0; x < w; x++)
				if(TST_RND() % 100 < 22) pf.SetImpassable(x, y, true);
		finalize_map(pf);

		for(int q = 0; q < 40; q++)
		{
			int sx = TST_RND() % w, sy = TST_RND() % h;
			int ex = TST_RND() % w, ey = TST_RND() % h;
			if(!pf.TilePassable(sx, sy, true) || !pf.TilePassable(ex, ey, true)) continue;
			if(!pf.InSameRegion(px(sx), px(sy), px(ex), px(ey), true)) continue;
			if(sx == ex && sy == ey) continue;

			vector<ZPath_Finding_AStar::pf_point> route;
			if(!get_route_tiles(pf, sx, sy, ex, ey, true, route)) continue; // direct hop

			const char *why = "ok";
			if(!route_is_valid(pf, route, ex, ey, true, why))
			{
				invalid++;
				if(invalid == 1) first_why = why;
			}
			checked++;
		}
	}
#undef TST_RND

	printf("    (%d routes checked)\n", checked);
	CHECK(checked > 100, "stress test exercised a meaningful number of routes");
	CHECK(invalid == 0, "every randomized A* route is a valid continuous path");
	if(invalid) printf("    -> %d invalid route(s); first failure: %s\n", invalid, first_why);
}

static void test_destroyable_barrier()
{
	printf("destroyable barriers (rocks):\n");
	ZPath_Finding_Engine pf;
	build_map(pf, 8, 8);
	pf.SetImpassable(4, 4, true, true);   // destroyable
	pf.SetImpassable(5, 5, true, false);  // solid
	finalize_map(pf);

	CHECK(pf.HasDestroyableBarrier(4, 4), "destroyable barrier is reported destroyable");
	CHECK(!pf.HasDestroyableBarrier(5, 5), "solid barrier is not destroyable");
	CHECK(!pf.HasDestroyableBarrier(0, 0), "open tile is not a barrier");
}

int main()
{
	test_passability();
	test_direct_path();
	test_regions();
	test_astar_routes();
	test_path_continuity();
	test_path_validity_stress();
	test_destroyable_barrier();

	printf("\n%d checks, %d failed\n", g_checks, g_fails);
	return g_fails ? 1 : 0;
}
