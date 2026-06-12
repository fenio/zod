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
	test_destroyable_barrier();

	printf("\n%d checks, %d failed\n", g_checks, g_fails);
	return g_fails ? 1 : 0;
}
