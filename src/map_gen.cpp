// zod_mapgen — standalone random skirmish map generator for the Zod Engine.
//
// Writes ordinary .map files the game loads like any hand-made map; it shares
// no code with the engine (the structs below mirror zmap.h's on-disk layout),
// so it cannot affect existing behavior.
//
//   zod_mapgen -e <enemies 1-3> [-w tiles] [-h tiles] [-t terrain 0-4]
//              [-s seed] [-o out.map]
//
// v1 algorithm: zone grid with one fort zone per team in spread corners,
// weighted ground fill from the palette's starter tiles, L-shaped roads from
// each fort toward the center, random-walk water blobs (vehicle connectivity
// re-carved afterwards), neutral factories/radar/repair + a flag per zone,
// rock clusters, huts, grenade boxes and a few neutral jeeps.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <map>

#include "map_gen_core.h"

using namespace std;

namespace MapGen {

// ---- on-disk structures (must match zmap.h exactly; natural alignment) ----

struct map_basics
{
	unsigned short width;
	unsigned short height;
	char map_name[50];
	unsigned char player_count;
	unsigned short object_count;
	unsigned char terrain_type;
	unsigned short zone_count;
};

struct map_zone
{
	unsigned short x, y, w, h; //in tile form
};

struct map_object
{
	unsigned short x, y; //in tile form
	char owner;
	unsigned char object_type;
	unsigned char object_id;
	char blevel;
	unsigned short extra_links;
	int health_percent;
};

// tileinfo entries are written with #pragma pack(1) in the engine
#pragma pack(1)
struct palette_tile_info
{
	bool is_water;
	bool is_passable;
	bool is_usable;
	bool is_road;
	bool is_effect;
	bool is_water_effect;
	unsigned short next_tile_in_effect;
	bool takes_tank_tracks;
	short crater_type;
	bool is_starter_tile;
};
#pragma pack()

#define MAX_PLANET_TILES (20*24)

// object/item ids (constants.h)
enum { ROCK_OBJECT, BRIDGE_OBJECT, BUILDING_OBJECT, CANNON_OBJECT, VEHICLE_OBJECT, ROBOT_OBJECT, ANIMAL_OBJECT, MAP_ITEM_OBJECT };
enum { FORT_FRONT, FORT_BACK, RADAR, REPAIR, ROBOT_FACTORY, VEHICLE_FACTORY };
enum { FLAG_ITEM, ROCK_ITEM, GRENADES_ITEM, ROCKETS_ITEM, HUT_ITEM };
enum { GATLING_C };
enum { JEEP_V, LIGHT_V, MEDIUM_V, HEAVY_V, APC_V, MISSILE_V, CRANE_V };
enum { GRUNT_R };

static const char *terrain_names[5] = { "desert", "volcanic", "arctic", "jungle", "city" };

// ---- generator state ----

static int W = 80, H = 100;
static int enemies = 1;
static int terrain = 0;
static int tech = 0;   //building level 0-5: how advanced production is
static int n_vehicles = -1;  //neutral/orphaned vehicles to scatter; -1 = auto
static unsigned int seed = 0;
static string out_path = "maps/random.map";
static string assets_dir = "assets";

static palette_tile_info tinfo[MAX_PLANET_TILES];
static vector<int> ground_tiles, starter_tiles, water_tiles, road_tiles, rock_tiles;

static vector<unsigned short> tiles;       // W*H tile indices
static vector<char> kind;                  // per tile: 'g' ground, 'w' water, 'r' road
static vector<bool> reserved;              // building/flag footprints: keep clear
static vector<bool> solid;                 // engine-impassable: buildings, rocks
static vector<bool> keepout;               // no water/rocks here: clean apron around buildings
static vector<map_zone> zones;
static vector<map_object> objects;
static vector<int> fort_zone;              // zone index per team (team i = owner i+1)

static int idx(int x, int y) { return y * W + x; }
static int rnd(int n) { return n > 0 ? rand() % n : 0; }

static void set_ground(int x, int y)
{
	if(x < 0 || y < 0 || x >= W || y >= H) return;
	kind[idx(x,y)] = 'g';
	tiles[idx(x,y)] = (rnd(100) < 85 || ground_tiles.empty())
		? starter_tiles[rnd(starter_tiles.size())]
		: ground_tiles[rnd(ground_tiles.size())];
}

static void add_object(int x, int y, int owner, int ot, int oid, int blevel = 0)
{
	map_object o;
	memset(&o, 0, sizeof(o));
	o.x = x; o.y = y;
	o.owner = (char)owner;
	o.object_type = (unsigned char)ot;
	o.object_id = (unsigned char)oid;
	o.blevel = (char)blevel;
	o.extra_links = 0;
	o.health_percent = 100;
	objects.push_back(o);
}

// clear (force ground) and reserve a rectangle for a structure; is_solid
// marks the footprint the engine will treat as impassable
static void reserve_rect(int x, int y, int w, int h, bool is_solid = false)
{
	for(int j = y - 1; j < y + h + 1; j++)
		for(int i = x - 1; i < x + w + 1; i++)
		{
			if(i < 0 || j < 0 || i >= W || j >= H) continue;
			set_ground(i, j);
			reserved[idx(i,j)] = true;
			if(is_solid && j >= y && i >= x && j < y + h && i < x + w) solid[idx(i,j)] = true;
		}

	// clean apron: keep water/rocks (and their shoreline tiles) off the area
	// immediately around a structure, so buildings aren't ringed by lakes or
	// mud at their entrance. Wider for solid buildings, minimal for flags.
	int m = is_solid ? 3 : 1;
	for(int j = y - m; j < y + h + m; j++)
		for(int i = x - m; i < x + w + m; i++)
			if(i >= 0 && j >= 0 && i < W && j < H) keepout[idx(i,j)] = true;
}

static bool area_free(int x, int y, int w, int h)
{
	if(x < 1 || y < 1 || x + w >= W - 1 || y + h >= H - 1) return false;
	for(int j = y; j < y + h; j++)
		for(int i = x; i < x + w; i++)
			if(reserved[idx(i,j)]) return false;
	return true;
}

// ---- pieces ----

static bool load_tileinfo()
{
	string path = assets_dir + "/planets/" + terrain_names[terrain] + ".tileinfo";
	FILE *fp = fopen(path.c_str(), "rb");
	if(!fp) { fprintf(stderr, "cannot open %s (run from the game directory or pass assets via cwd)\n", path.c_str()); return false; }
	if(fread(tinfo, sizeof(palette_tile_info), MAX_PLANET_TILES, fp) != MAX_PLANET_TILES)
		{ fclose(fp); fprintf(stderr, "short read on %s\n", path.c_str()); return false; }
	fclose(fp);

	for(int i = 0; i < MAX_PLANET_TILES; i++)
	{
		if(!tinfo[i].is_usable) continue;
		if(tinfo[i].is_effect || tinfo[i].is_water_effect) continue;

		if(tinfo[i].is_water) { water_tiles.push_back(i); continue; }
		if(!tinfo[i].is_passable) { rock_tiles.push_back(i); continue; } //impassable terrain
		if(tinfo[i].is_road) { road_tiles.push_back(i); continue; }

		if(tinfo[i].is_starter_tile) starter_tiles.push_back(i);
		else ground_tiles.push_back(i);
	}

	if(starter_tiles.empty()) starter_tiles = ground_tiles;
	if(starter_tiles.empty()) { fprintf(stderr, "palette has no usable ground tiles\n"); return false; }

	printf("palette %s: %d starter, %d ground, %d water, %d road tiles\n", terrain_names[terrain],
		(int)starter_tiles.size(), (int)ground_tiles.size(), (int)water_tiles.size(), (int)road_tiles.size());
	return true;
}

// ---- tile grammar learned from the shipped hand-made maps ----
//
// For every map of the same terrain, record which tile index the original
// mappers used for each water-neighborhood pattern (8 surrounding cells,
// bit set = water) and how often each plain-ground tile occurs. Sampling
// from those tables reproduces their shorelines and calm ground mix.


static std::map<int, std::map<unsigned short, int> > water_tab; // key: center('w' or 'g')<<8 | pattern
static std::map<int, std::map<unsigned short, int> > water_tab4; // 4-bit N/W/E/S fallback
static std::map<int, std::map<unsigned short, int> > rock_tab;  // key: center('x' or 'g')<<8 | rock-pattern
static std::map<int, std::map<unsigned short, int> > rock_tab4; // 4-bit N/W/E/S fallback
static std::map<unsigned short, int> ground_tab;
static std::map<int, std::map<unsigned short, int> > road_tab; // 4-bit N/W/E/S road-neighbor pattern

static char kind_of_tile(unsigned short t)
{
	if(t >= MAX_PLANET_TILES) return 'x';
	if(tinfo[t].is_water) return 'w';
	if(!tinfo[t].is_passable) return 'x';
	if(tinfo[t].is_road) return 'r';
	return 'g';
}

static void learn_from_map(const char *path)
{
	FILE *fp = fopen(path, "rb");
	if(!fp) return;

	map_basics b;
	if(fread(&b, sizeof(b), 1, fp) != 1 || b.terrain_type != terrain
		|| b.width < 4 || b.height < 4 || b.width > 1000 || b.height > 1000)
		{ fclose(fp); return; }

	fseek(fp, sizeof(map_zone) * b.zone_count + sizeof(map_object) * b.object_count, SEEK_CUR);

	int n = b.width * b.height;
	vector<unsigned short> t(n);
	if((int)fread(&t[0], sizeof(unsigned short), n, fp) != n) { fclose(fp); return; }
	fclose(fp);

	vector<char> K(n);
	for(int i = 0; i < n; i++) K[i] = kind_of_tile(t[i]);

	const int dx8[8] = {-1,0,1,-1,1,-1,0,1}, dy8[8] = {-1,-1,-1,0,0,1,1,1};
	for(int y = 0; y < b.height; y++)
		for(int x = 0; x < b.width; x++)
		{
			char c = K[y * b.width + x];
			int wp = 0, wp4 = 0, rp = 0, rp4 = 0;
			for(int d = 0; d < 8; d++)
			{
				int nx = x + dx8[d], ny = y + dy8[d];
				char nk = (nx < 0 || ny < 0 || nx >= b.width || ny >= b.height) ? 'g' : K[ny * b.width + nx];
				if(nk == 'w') wp |= (1 << d);
				if(nk == 'x') rp |= (1 << d);
			}
			if(wp & (1<<1)) wp4 |= 1; //N
			if(wp & (1<<3)) wp4 |= 2; //W
			if(wp & (1<<4)) wp4 |= 4; //E
			if(wp & (1<<6)) wp4 |= 8; //S
			if(rp & (1<<1)) rp4 |= 1;
			if(rp & (1<<3)) rp4 |= 2;
			if(rp & (1<<4)) rp4 |= 4;
			if(rp & (1<<6)) rp4 |= 8;

			unsigned short tt = t[y * b.width + x];
			// never learn animated/effect tiles (water sparkles, etc.) as terrain
			// pieces — picking one into a rock/ground neighbourhood renders as a
			// bright out-of-place tile
			if(tt < MAX_PLANET_TILES && (tinfo[tt].is_effect || tinfo[tt].is_water_effect))
				continue;
			if(c == 'r')
			{
				int rp = 0;
				if(y > 0 && K[(y-1) * b.width + x] == 'r') rp |= 1;
				if(x > 0 && K[y * b.width + x - 1] == 'r') rp |= 2;
				if(x < b.width - 1 && K[y * b.width + x + 1] == 'r') rp |= 4;
				if(y < b.height - 1 && K[(y+1) * b.width + x] == 'r') rp |= 8;
				road_tab[rp][tt]++;
			}
			else if(c == 'w' || (c == 'g' && wp))
			{
				water_tab[(c << 8) | wp][tt]++;
				water_tab4[(c << 8) | wp4][tt]++;
			}
			else if(c == 'x' || (c == 'g' && rp))
			{
				rock_tab[(c << 8) | rp][tt]++;
				rock_tab4[(c << 8) | rp4][tt]++;
			}
			else if(c == 'g')
				ground_tab[tt]++;
		}
}

static void learn_tiles()
{
	// the campaign maps live next to the generator's output by default
	char path[512];
	const char *prefixes[3] = { "maps/p02_bb_orig%02d.map", "maps/p03_bb_p03m%02d.map", "maps/p04_bb_p04m%02d.map" };
	for(int p = 0; p < 3; p++)
		for(int i = 0; i < 40; i++)
			{ snprintf(path, sizeof(path), prefixes[p], i); learn_from_map(path); }

	// plain-fill set: only the head of the ground distribution - the long
	// tail is feature-edge tiles that look broken in isolation
	{
		int max_n = 0;
		for(std::map<unsigned short, int>::iterator i = ground_tab.begin(); i != ground_tab.end(); ++i)
			if(i->second > max_n) max_n = i->second;
		std::map<unsigned short, int> head;
		for(std::map<unsigned short, int>::iterator i = ground_tab.begin(); i != ground_tab.end(); ++i)
			if(i->second >= max_n * 35 / 100) head[i->first] = i->second;
		if(!head.empty()) ground_tab = head;
	}

	printf("tile grammar: %d shoreline patterns, %d road patterns, %d plain ground tiles\n",
		(int)water_tab.size(), (int)road_tab.size(), (int)ground_tab.size());
}

static unsigned short sample_tab(std::map<unsigned short, int> &m)
{
	long total = 0;
	for(std::map<unsigned short, int>::iterator i = m.begin(); i != m.end(); ++i) total += i->second;
	long r = (long)(rand() % (int)(total > 0 ? total : 1));
	for(std::map<unsigned short, int>::iterator i = m.begin(); i != m.end(); ++i)
		{ r -= i->second; if(r < 0) return i->first; }
	return m.begin()->first;
}

// the single most-common tile for a neighborhood: rock/cliff tiles are
// directional, so a configuration must always get its one canonical piece
// (weighted-random would scatter wrong-facing edges = visual chaos)
static unsigned short mode_tab(std::map<unsigned short, int> &m)
{
	unsigned short best = m.empty() ? 0 : m.begin()->first;
	int bn = -1;
	for(std::map<unsigned short, int>::iterator i = m.begin(); i != m.end(); ++i)
		if(i->second > bn) { bn = i->second; best = i->first; }
	return best;
}

// Our generated geometry produces some neighbourhood configurations the
// originals never used (e.g. a 1-wide rock spur), so an exact 8-bit/4-bit
// lookup misses and the old fallback dropped in a SOLID interior tile — a tile
// that visibly doesn't match its surroundings. Instead pick the learned
// configuration whose 8-neighbour bitmask is closest (fewest differing
// neighbours) and use its canonical piece: always a near-fitting edge, never a
// jarring mismatch. Returns -1 only if nothing with this centre was learned.
static int nearest_tab(std::map<int, std::map<unsigned short, int> > &tab, int c, int p)
{
	// the 4 CARDINAL neighbours (bits 1=N,3=W,4=E,6=S) decide which way a cliff
	// edge faces; diagonals only refine corners. Weight a cardinal mismatch 4x so
	// the closest learned piece always faces the right way (a pure popcount could
	// pick a wrong-facing tile that merely shares diagonal bits).
	const unsigned card = (1<<1)|(1<<3)|(1<<4)|(1<<6);
	int bestd = 1 << 30; unsigned short bestt = 0; bool found = false;
	for(std::map<int, std::map<unsigned short, int> >::iterator i = tab.begin(); i != tab.end(); ++i)
	{
		if((i->first >> 8) != c) continue;
		unsigned diff = (unsigned)((i->first & 0xff) ^ p);
		int d = __builtin_popcount(diff & ~card) + 4 * __builtin_popcount(diff & card);
		if(d < bestd) { bestd = d; bestt = mode_tab(i->second); found = true; }
	}
	return found ? (int)bestt : -1;
}

static void make_zones()
{
	int cols = W / 22; if(cols < 2) cols = 2;
	int rows = H / 26; if(rows < 2) rows = 2;

	for(int r = 0; r < rows; r++)
		for(int c = 0; c < cols; c++)
		{
			map_zone z;
			z.x = 2 + c * (W - 4) / cols;
			z.y = 2 + r * (H - 4) / rows;
			z.w = (W - 4) / cols - 1;
			z.h = (H - 4) / rows - 1;
			zones.push_back(z);
		}

	// fort zones: each team gets a "home" band of the zone grid and the fort drops
	// in a RANDOM zone inside it — so it lands somewhere on the player's side but
	// not always the same corner (the originals vary it). 2 teams split top/bottom
	// halves; 3-4 take distinct quadrants. Bands don't overlap, so starts stay
	// spread and fair.
	int hc = cols / 2, hr = rows / 2;
	#define ZONE_IN(c0, c1, r0, r1) (((r0) + rnd((r1) - (r0) + 1)) * cols + ((c0) + rnd((c1) - (c0) + 1)))
	if(enemies + 1 == 2)
	{
		// DIAGONAL corners: opposite horizontal halves AND top/bottom, so the two
		// bases are well separated both ways. (Picking a random column for each
		// independently let them land in the same column - stacked and too close.)
		bool flip = rnd(2);
		int lc0 = 0, lc1 = hc - 1;             // left-half columns
		int rc0 = cols - hc, rc1 = cols - 1;   // right-half columns
		fort_zone.push_back(ZONE_IN(flip ? lc0 : rc0, flip ? lc1 : rc1, 0, hr - 1));            // top, one side
		fort_zone.push_back(ZONE_IN(flip ? rc0 : lc0, flip ? rc1 : lc1, rows - hr, rows - 1));  // bottom, other side
	}
	else
	{
		int qc0[4] = { 0, cols - hc, 0, cols - hc }, qc1[4] = { hc - 1, cols - 1, hc - 1, cols - 1 };
		int qr0[4] = { 0, 0, rows - hr, rows - hr }, qr1[4] = { hr - 1, hr - 1, rows - 1, rows - 1 };
		int order[4] = { 0, 1, 2, 3 };
		for(int i = 3; i > 0; i--) { int j = rnd(i + 1); int tmp = order[i]; order[i] = order[j]; order[j] = tmp; }
		for(int t = 0; t < enemies + 1; t++)
		{
			int q = order[t];
			fort_zone.push_back(ZONE_IN(qc0[q], qc1[q], qr0[q], qr1[q]));
		}
	}
	#undef ZONE_IN
}

// carve an open (passable ground) disc into the rock
static void carve_disc(int cx, int cy, int r)
{
	for(int y = cy - r; y <= cy + r; y++)
		for(int x = cx - r; x <= cx + r; x++)
		{
			if(x < 1 || y < 1 || x >= W - 1 || y >= H - 1) continue;
			int ddx = x - cx, ddy = y - cy;
			if(ddx * ddx + ddy * ddy <= r * r) kind[idx(x, y)] = 'g';
		}
}

// stamp a thin meandering "vein" of one terrain kind. The originals interleave
// rock and water into fine veins (rock interior 9%, water 2% — almost all of it
// is edge), threaded by narrow open corridors; meandering 1-wide walks reproduce
// that thin, low-interior texture. When guided, drift toward (tx,ty) so the vein
// also connects two points (used to link forts to the centre).
static int kind_count(char ch)
{
	int n = 0;
	for(int i = 0; i < W * H; i++) if(kind[i] == ch) n++;
	return n;
}

static void vein(char ch, int x, int y, int steps, int width, int tx, int ty, bool guided)
{
	for(int s = 0; s < steps; s++)
	{
		for(int j = -width; j <= width; j++)
			for(int i = -width; i <= width; i++)
				if(x + i >= 1 && y + j >= 1 && x + i < W - 1 && y + j < H - 1
					&& i * i + j * j <= width * width)
					kind[idx(x + i, y + j)] = ch;

		if(guided && rnd(100) < 60)         //drift toward the target
		{
			if(rnd(2) && x != tx) x += (tx > x) ? 1 : -1;
			else if(y != ty)      y += (ty > y) ? 1 : -1;
			else if(x != tx)      x += (tx > x) ? 1 : -1;
		}
		else                                //or wander
		{
			int d = rnd(4);
			x += (d == 0) - (d == 1);
			y += (d == 2) - (d == 3);
		}
		if(x < 1) x = 1; if(x > W - 2) x = W - 2;
		if(y < 1) y = 1; if(y > H - 2) y = H - 2;
		if(guided && x == tx && y == ty) break;
	}
}

// Match what the originals actually are (correctly measured over the shipped
// desert maps): a mostly-OPEN closed arena. ~75% open ground, rock only ~11%
// and concentrated as a thick irregular BORDER WALL framing the arena (the
// interior is just ~3% rock), water ~7% in a few ponds, roads ~8% in a grid.
// The rock walls the map shut; the play space inside is open.
static void make_terrain()
{
	for(int i = 0; i < W * H; i++) kind[i] = 'g';

	// --- 1) perimeter rock border ---------------------------------------------
	// Per-side thickness follows a random WALK along the edge, so the inner edge
	// undulates smoothly (per-column random would give a jagged comb). Corners
	// take whichever side is thicker.
	// Per-terrain composition, measured from the shipped originals:
	//   desert 11% rock / 7% water, volcanic 22% / 0% (lava world, no water),
	//   arctic 9% / 13%, jungle 9% / 9%, city ~desert. Border thickness sets the
	//   rock%, interior ridges add the rest, pond target sets the water%.
	// (water targets run ~2-3% above the originals' final % to offset the
	// shoreline-smoothing pass below, which erodes the pond edges back)
	static const int RK_BASE[5] = { 4, 7, 3, 3, 4 };  // border thickness @100px
	static const int WT_PCT[5]  = { 9, 0, 16, 11, 9 }; // target water %
	static const int RIDGES[5]  = { 3, 10, 2, 3, 3 }; // interior ridge count
	int sz = (W < H ? W : H);
	int base = RK_BASE[terrain] * sz / 100; if(base < 2) base = 2;
	int water_target = (W * H * WT_PCT[terrain]) / 100;

	vector<int> topT(W), botT(W), lefT(H), rigT(H);
	int t = base;
	#define WALK(arr, n) { t = base; for(int k = 0; k < (n); k++) { arr[k] = t; t += rnd(3) - 1; if(t < 1) t = 1; if(t > base + 5) t = base + 5; } }
	WALK(topT, W) WALK(botT, W) WALK(lefT, H) WALK(rigT, H)
	#undef WALK
	for(int y = 0; y < H; y++)
		for(int x = 0; x < W; x++)
			if(x < lefT[y] || (W - 1 - x) < rigT[y] || y < topT[x] || (H - 1 - y) < botT[x])
				kind[idx(x, y)] = 'x';

	// --- 2) sparse interior rock ridges (chokepoints; bulk of volcanic's rock) -
	int ridges = RIDGES[terrain] + rnd(3);
	int rlen = (terrain == 1) ? 16 : 8, rwid = (terrain == 1) ? 1 : 0;
	for(int r = 0; r < ridges; r++)
		vein('x', base + 4 + rnd(W - 2 * base - 8), base + 4 + rnd(H - 2 * base - 8),
		     rlen + rnd(12), rwid + rnd(2), 0, 0, false);

	// --- 3) water ponds (interior), added until the terrain's water% is hit ----
	for(int guard = 0; guard < 400 && kind_count('w') < water_target; guard++)
	{
		int x = base + 6 + rnd(W - 2 * base - 12), y = base + 6 + rnd(H - 2 * base - 12);
		int steps = 30 + rnd(40), rad = 2 + rnd(2);
		for(int s = 0; s < steps; s++)
		{
			for(int j = -rad; j <= rad; j++)
				for(int i = -rad; i <= rad; i++)
				{
					int xx = x + i, yy = y + j;
					if(xx >= 1 && yy >= 1 && xx < W - 1 && yy < H - 1
						&& kind[idx(xx,yy)] == 'g' && i*i + j*j <= rad*rad)
						kind[idx(xx, yy)] = 'w';
				}
			x += rnd(3) - 1; y += rnd(3) - 1;
			if(x < base + 2 || y < base + 2 || x > W - base - 2 || y > H - base - 2) break;
		}
	}

	// smooth pond edges (kills lone water/ground specks -> coherent shorelines)
	for(int pass = 0; pass < 2; pass++)
	{
		vector<char> nk = kind;
		for(int y = 1; y < H - 1; y++)
			for(int x = 1; x < W - 1; x++)
			{
				if(kind[idx(x,y)] == 'x') continue;
				int n = 0;
				for(int j = -1; j <= 1; j++)
					for(int i = -1; i <= 1; i++)
						if(kind[idx(x+i, y+j)] == 'w') n++;
				if(kind[idx(x,y)] == 'w') nk[idx(x,y)] = (n >= 4) ? 'w' : 'g';
				else                      nk[idx(x,y)] = (n >= 6) ? 'w' : 'g';
			}
		kind = nk;
	}
}

static bool near_flag(int x, int y, int r)
{
	for(size_t i = 0; i < objects.size(); i++)
		if(objects[i].object_type == MAP_ITEM_OBJECT && objects[i].object_id == FLAG_ITEM)
		{
			int dx = objects[i].x - x, dy = objects[i].y - y;
			if(dx > -r && dx < r && dy > -r && dy < r) return true;
		}
	return false;
}

// lay one 2-wide road cell, skipping structures, flag berths and the rock wall
// (roads may cross water — a paved crossing stands in for a bridge for now)
static void road_put(int x, int y)
{
	if(x < 0 || y < 0 || x >= W || y >= H) return;
	int i = idx(x, y);
	if(reserved[i] || kind[i] == 'x' || near_flag(x, y, 2)) return;
	kind[i] = 'r';
	tiles[i] = road_tiles[rnd(road_tiles.size())];
}

static void road_run(int x0, int y0, int x1, int y1)
{
	if(y0 == y1)
		for(int i = (x0 < x1 ? x0 : x1); i <= (x0 < x1 ? x1 : x0); i++)
			for(int d = 0; d < 2; d++) road_put(i, y0 + d);
	else
		for(int j = (y0 < y1 ? y0 : y1); j <= (y0 < y1 ? y1 : y0); j++)
			for(int d = 0; d < 2; d++) road_put(x0 + d, j);
}

// an L-shaped 2-wide road between two points, with randomised elbow direction
static void road_L(int x0, int y0, int x1, int y1)
{
	if(rnd(2)) { road_run(x0, y0, x1, y0); road_run(x1, y0, x1, y1); }
	else       { road_run(x0, y0, x0, y1); road_run(x0, y1, x1, y1); }
}

static void make_roads()
{
	if(road_tiles.empty()) return;

	// A rigid ring+cross reads as an artificial rectangle. The originals grow an
	// irregular network connecting the buildings. Reproduce that: take every
	// building as a node, connect them with a nearest-neighbour spanning tree
	// (organic branching) of L-roads, then add a few extra loop edges so it isn't
	// a bare tree. Roads are cosmetic overlays on open ground (the whole interior
	// is already passable), so the layout is free to be irregular.
	vector<int> ax, ay;
	for(size_t i = 0; i < objects.size(); i++)
		if(objects[i].object_type == BUILDING_OBJECT)
			{ ax.push_back(objects[i].x + 2); ay.push_back(objects[i].y + 2); }

	int n = (int)ax.size();
	if(n < 2) return;

	vector<bool> conn(n, false);
	conn[0] = true;
	for(int done = 1; done < n; done++)
	{
		int bu = -1, bc = -1, bd = 1 << 30;
		for(int u = 0; u < n; u++)
		{
			if(conn[u]) continue;
			for(int c = 0; c < n; c++)
			{
				if(!conn[c]) continue;
				int d = abs(ax[u] - ax[c]) + abs(ay[u] - ay[c]);
				if(d < bd) { bd = d; bu = u; bc = c; }
			}
		}
		if(bu < 0) break;
		road_L(ax[bc], ay[bc], ax[bu], ay[bu]);
		conn[bu] = true;
	}

	// extra loop edges between random nodes so the network has alternate routes
	int loops = 1 + n / 2;
	for(int l = 0; l < loops; l++)
	{
		int a = rnd(n), b = rnd(n);
		if(a != b) road_L(ax[a], ay[a], ax[b], ay[b]);
	}
}

// Place each neutral zone's capture flag AFTER the roads, at a random open spot
// inside the zone (never on a road, never on rock/water, never the fixed centre)
// — the originals vary flag positions, and a flag must never sit on a road.
static void place_flags()
{
	for(size_t zi = 0; zi < zones.size(); zi++)
	{
		bool is_fort = false;
		for(size_t t = 0; t < fort_zone.size(); t++) if(fort_zone[t] == (int)zi) is_fort = true;
		if(is_fort) continue;

		map_zone &z = zones[zi];
		int fx = -1, fy = -1;

		// prefer a varied random open-ground spot in the zone interior
		for(int a = 0; a < 50; a++)
		{
			int rx = z.x + 3 + rnd(z.w > 6 ? z.w - 6 : 1);
			int ry = z.y + 3 + rnd(z.h > 6 ? z.h - 6 : 1);
			int i = idx(rx, ry);
			if(rx > 0 && ry > 0 && rx < W - 1 && ry < H - 1 && kind[i] == 'g' && !reserved[i])
				{ fx = rx; fy = ry; break; }
		}

		// the random tries can all miss in a cramped or watery zone; scan the whole
		// zone so the flag still lands on open ground (never water/road/rock) rather
		// than the old unchecked centre fallback that could strand it in a lake
		if(fx < 0)
			for(int ry = z.y + 1; ry < z.y + z.h - 1 && fx < 0; ry++)
				for(int rx = z.x + 1; rx < z.x + z.w - 1; rx++)
				{
					int i = idx(rx, ry);
					if(rx > 0 && ry > 0 && rx < W - 1 && ry < H - 1 && kind[i] == 'g' && !reserved[i])
						{ fx = rx; fy = ry; break; }
				}

		// no open ground anywhere in the zone (degenerate): carve the centre to
		// ground so the flag is on dry, reachable land instead of in the water
		if(fx < 0)
		{
			fx = z.x + z.w / 2; fy = z.y + z.h / 2;
			set_ground(fx, fy);
		}
		// reserve the flag cell + a keepout ring WITHOUT clearing terrain (it sits
		// on open ground already; reserve_rect's border would erase an adjacent road)
		reserved[idx(fx, fy)] = true;
		for(int j = -1; j <= 1; j++)
			for(int i = -1; i <= 1; i++)
				if(fx + i >= 0 && fy + j >= 0 && fx + i < W && fy + j < H) keepout[idx(fx + i, fy + j)] = true;
		add_object(fx, fy, 0, MAP_ITEM_OBJECT, FLAG_ITEM);
	}
}

static void make_water()
{
	// make_terrain now lays the water directly, interleaved with the rock as
	// thin veins (the originals' texture), so the old open-field lake pass is
	// disabled — adding lakes here would just flood the narrow corridors.
	return;

	if(water_tiles.empty()) return;

	// blob seeds: thick random walks (3x3 stamps) make coherent lakes
	int blobs = (W * H) / 1600;
	for(int b = 0; b < blobs; b++)
	{
		int x = 4 + rnd(W - 8), y = 4 + rnd(H - 8);
		int steps = 15 + rnd(35);

		for(int s = 0; s < steps; s++)
		{
			for(int j = -1; j <= 1; j++)
				for(int i = -1; i <= 1; i++)
				{
					int ii = idx(x + i, y + j);
					if(kind[ii] == 'g' && !reserved[ii] && !keepout[ii]) kind[ii] = 'w';
				}
			x += rnd(3) - 1; y += rnd(3) - 1;
			if(x < 4) x = 4; if(y < 4) y = 4;
			if(x > W - 5) x = W - 5; if(y > H - 5) y = H - 5;
		}
	}

	// two majority-rule smoothing passes: kills ragged single-tile noise
	for(int pass = 0; pass < 2; pass++)
	{
		vector<char> nk = kind;
		for(int y = 1; y < H - 1; y++)
			for(int x = 1; x < W - 1; x++)
			{
				if(reserved[idx(x,y)] || keepout[idx(x,y)] || kind[idx(x,y)] == 'r') continue;
				int n = 0;
				for(int j = -1; j <= 1; j++)
					for(int i = -1; i <= 1; i++)
						if(kind[idx(x+i, y+j)] == 'w') n++;
				if(kind[idx(x,y)] == 'w') nk[idx(x,y)] = (n >= 4) ? 'w' : 'g';
				else nk[idx(x,y)] = (n >= 6) ? 'w' : kind[idx(x,y)];
			}
		kind = nk;
	}
}

// final tile assignment from the learned grammar: shorelines get the tiles
// the original mappers used for the same water neighborhood, plain ground
// gets their frequency mix, structures sit on clean starter tiles
static void retile()
{
	const int dx8[8] = {-1,0,1,-1,1,-1,0,1}, dy8[8] = {-1,-1,-1,0,0,1,1,1};

	// the single most-common plain-ground tile: the bulk of the ground is this
	// one tile so the terrain reads as cohesive, not a per-tile checkerboard
	unsigned short base_tile = starter_tiles.empty() ? 0 : starter_tiles[0];
	{
		int best = -1;
		for(std::map<unsigned short, int>::iterator it = ground_tab.begin(); it != ground_tab.end(); ++it)
			if(it->second > best) { best = it->second; base_tile = it->first; }
	}

	// the canonical solid-rock interior tile (mode of the fully-rock-surrounded
	// neighborhood), used for the bulk of the rock and as the edge fallback
	unsigned short rock_base = rock_tiles.empty() ? base_tile : rock_tiles[0];
	if(rock_tab.count(('x' << 8) | 0xff)) rock_base = mode_tab(rock_tab[('x' << 8) | 0xff]);

	for(int y = 0; y < H; y++)
		for(int x = 0; x < W; x++)
		{
			int i = idx(x, y);

			if(kind[i] == 'r')
			{
				int rp = 0;
				if(y > 0 && kind[idx(x, y-1)] == 'r') rp |= 1;
				if(x > 0 && kind[idx(x-1, y)] == 'r') rp |= 2;
				if(x < W - 1 && kind[idx(x+1, y)] == 'r') rp |= 4;
				if(y < H - 1 && kind[idx(x, y+1)] == 'r') rp |= 8;
				if(road_tab.count(rp)) tiles[i] = sample_tab(road_tab[rp]);
				continue;
			}

			int wp = 0, wp4 = 0, rkp = 0, rkp4 = 0;
			for(int d = 0; d < 8; d++)
			{
				int nx = x + dx8[d], ny = y + dy8[d];
				char nk = (nx < 0 || ny < 0 || nx >= W || ny >= H) ? 'g' : kind[idx(nx, ny)];
				if(nk == 'w') wp |= (1 << d);
				if(nk == 'x') rkp |= (1 << d);
			}
			if(wp & (1<<1)) wp4 |= 1;
			if(wp & (1<<3)) wp4 |= 2;
			if(wp & (1<<4)) wp4 |= 4;
			if(wp & (1<<6)) wp4 |= 8;
			if(rkp & (1<<1)) rkp4 |= 1;
			if(rkp & (1<<3)) rkp4 |= 2;
			if(rkp & (1<<4)) rkp4 |= 4;
			if(rkp & (1<<6)) rkp4 |= 8;

			char c = kind[i];
			if(c == 'w' || (c == 'g' && wp))
			{
				int k8 = (c << 8) | wp, k4 = (c << 8) | wp4;
				if(water_tab.count(k8)) { tiles[i] = sample_tab(water_tab[k8]); continue; }
				if(water_tab4.count(k4)) { tiles[i] = sample_tab(water_tab4[k4]); continue; }
				int nb = nearest_tab(water_tab, c, wp);
				if(nb >= 0) { tiles[i] = (unsigned short)nb; continue; }
				if(c == 'w') { tiles[i] = water_tiles.empty() ? tiles[i] : water_tiles[rnd(water_tiles.size())]; continue; }
				//unmatched shore: plain ground below
			}

			// rock is a directional autotile set, but with two distinct regimes
			// (learned from the originals): EDGE cells (a cliff face/corner/top,
			// some neighbour not rock) must get the ONE canonical piece for that
			// configuration (mode) so the facings fit together; INTERIOR cells
			// (fully surrounded by rock) are interchangeable rock-body textures —
			// the originals spread ~94 distinct tiles across them, so sampling by
			// frequency gives the varied "mountain" body instead of one repeated
			// column.
			if(c == 'x' || (c == 'g' && rkp))
			{
				bool interior = (c == 'x' && rkp == 0xff);
				int k8 = (c << 8) | rkp, k4 = (c << 8) | rkp4;
				if(rock_tab.count(k8))
				{
					tiles[i] = interior ? sample_tab(rock_tab[k8])
					                    : mode_tab(rock_tab[k8]);
					continue;
				}
				if(rock_tab4.count(k4)) { tiles[i] = mode_tab(rock_tab4[k4]); continue; }
				int nb = nearest_tab(rock_tab, c, rkp);
				if(nb >= 0) { tiles[i] = (unsigned short)nb; continue; }
				if(c == 'x') { tiles[i] = rock_base; continue; }
				//unmatched rock edge: plain ground below
			}

			if(reserved[i] || ground_tab.empty())
				tiles[i] = base_tile;
			else
			{
				// almost all ground is the single dominant tile; only a minority
				// of coarse 5x5 patches introduce ONE variant, and even then it's
				// sprinkled among the base - so variants form soft textured
				// patches instead of high-frequency per-tile noise
				unsigned int patch = ((y / 5) * 131u + (x / 5)) * 2654435761u + seed;
				bool varied = ((patch >> 16) & 0xff) < 58 && ground_tab.size() > 1; //~23% of patches
				if(!varied)
					tiles[i] = base_tile;
				else
				{
					std::map<unsigned short, int>::iterator it = ground_tab.begin();
					std::advance(it, patch % ground_tab.size());
					unsigned int cell = ((unsigned int)(y * W + x)) * 2246822519u ^ patch;
					tiles[i] = ((cell >> 18) & 0x7f) < 50 ? it->first : base_tile; //~39% of the patch
				}
			}
		}
}

// vehicles are 2x2 and can't cross water/rocks/buildings: flood-fill with a
// 2x2 clearance requirement from team 0's fort front and carve land bridges
// (removing rocks in the way) to anything that ended up disconnected
static vector<int> rock_at; // blocking tile -> objects[] index of the rock
static vector<bool> obj_dead;

static bool tile_blocked(int x, int y)
{
	if(x < 0 || y < 0 || x >= W || y >= H) return true;
	return kind[idx(x,y)] == 'w' || kind[idx(x,y)] == 'x' || solid[idx(x,y)];
}

static bool vehicle_open(int x, int y)
{
	return !tile_blocked(x,y) && !tile_blocked(x+1,y) && !tile_blocked(x,y+1) && !tile_blocked(x+1,y+1);
}

static void carve_cell(int x, int y)
{
	if(x < 0 || y < 0 || x >= W || y >= H) return;
	int i = idx(x, y);
	if(kind[i] == 'w' || kind[i] == 'x') set_ground(x, y);
	if(solid[i] && rock_at[i] >= 0) { obj_dead[rock_at[i]] = true; solid[i] = false; rock_at[i] = -1; }
}

static void ensure_connectivity()
{
	map_zone &z0 = zones[fort_zone[0]];
	int sx = z0.x + z0.w / 2, sy = z0.y + z0.h / 2 + 6; //below the fort
	int cx = W / 2, cy = H / 2;

	for(int pass = 0; pass < 4; pass++)
	{
		vector<bool> seen((size_t)W * H, false);
		vector<int> q;
		if(!vehicle_open(sx, sy)) { carve_cell(sx, sy); carve_cell(sx+1, sy); carve_cell(sx, sy+1); carve_cell(sx+1, sy+1); }
		q.push_back(idx(sx, sy));
		seen[q[0]] = true;

		for(size_t qi = 0; qi < q.size(); qi++)
		{
			int x = q[qi] % W, y = q[qi] / W;
			const int dx[4] = {1,-1,0,0}, dy[4] = {0,0,1,-1};
			for(int d = 0; d < 4; d++)
			{
				int nx = x + dx[d], ny = y + dy[d];
				if(nx < 0 || ny < 0 || nx >= W - 1 || ny >= H - 1) continue;
				int ni = idx(nx, ny);
				if(seen[ni] || !vehicle_open(nx, ny)) continue;
				seen[ni] = true;
				q.push_back(ni);
			}
		}

		// every unit, flag and structure front must be vehicle-reachable
		int carved = 0;
		for(size_t oi = 0; oi < objects.size(); oi++)
		{
			if(obj_dead[oi]) continue;
			int ot = objects[oi].object_type, oid = objects[oi].object_id;
			bool care = (ot == VEHICLE_OBJECT || ot == ROBOT_OBJECT
				|| (ot == MAP_ITEM_OBJECT && oid == FLAG_ITEM));
			if(!care) continue;

			int x = objects[oi].x, y = objects[oi].y;
			if(x >= W - 1) x = W - 2;
			if(y >= H - 1) y = H - 2;
			// nearby open spot counts as reachable
			bool ok = false;
			for(int j = -2; j <= 2 && !ok; j++)
				for(int i = -2; i <= 2 && !ok; i++)
					if(x+i >= 0 && y+j >= 0 && x+i < W-1 && y+j < H-1 && seen[idx(x+i, y+j)]) ok = true;
			if(ok) continue;

			// carve a 3-wide land corridor straight toward the map center
			carved++;
			int px = x, py = y;
			while((px != cx || py != cy) && !seen[idx(px, py)])
			{
				for(int j = -1; j <= 1; j++)
					for(int i = -1; i <= 1; i++)
						carve_cell(px + i, py + j);
				if(px != cx) px += (cx > px) ? 1 : -1;
				else if(py != cy) py += (cy > py) ? 1 : -1;
			}
		}

		if(!carved) break;
	}
}

static void place_team(int t)
{
	map_zone &z = zones[fort_zone[t]];
	int fx = z.x + z.w / 2 - 5, fy = z.y + z.h / 2 - 4;
	bool top_half = (fy < H / 2);

	// the fort's engine footprint is 10x9 tiles from its top-left corner
	reserve_rect(fx, fy, 10, 9, true);
	add_object(fx, fy, t + 1, BUILDING_OBJECT, top_half ? FORT_FRONT : FORT_BACK, tech);

	// keep a generous clear apron around the fort (esp. the entrance staging
	// area) so neutral vehicles/huts/rocks never spawn blocking the door
	for(int j = fy - 5; j < fy + 9 + 5; j++)
		for(int i = fx - 5; i < fx + 10 + 5; i++)
			if(i >= 0 && j >= 0 && i < W && j < H) keepout[idx(i, j)] = true;

	// starting squad outside the footprint, on the fort's open side
	int uy = top_half ? fy + 10 : fy - 2;          // cannon/vehicle row, by the door
	// each grunt object spawns a whole (stacked) group, so 3 of them one tile apart
	// at the door crowds ~9 units at the entrance as they fan out. Push the grunts
	// a few tiles further from the fort and spread the spawn points wider so the
	// groups have room to scuffle apart cleanly.
	int guy = top_half ? fy + 11 : fy - 3;       // grunts: just a touch further out
	for(int i = 0; i < 3; i++) add_object(fx + 2 + 2 * i, guy, t + 1, ROBOT_OBJECT, GRUNT_R);
	add_object(fx + 8, uy, t + 1, CANNON_OBJECT, GATLING_C);

	//at higher tech, also start with a vehicle so it is not all grunts
	if(tech >= 4)      add_object(fx, uy, t + 1, VEHICLE_OBJECT, MEDIUM_V);
	else if(tech >= 2) add_object(fx, uy, t + 1, VEHICLE_OBJECT, LIGHT_V);
}

static void place_zone_contents()
{
	for(size_t zi = 0; zi < zones.size(); zi++)
	{
		bool is_fort = false;
		for(size_t t = 0; t < fort_zone.size(); t++) if(fort_zone[t] == (int)zi) is_fort = true;
		if(is_fort) continue;

		map_zone &z = zones[zi];

		// (the capture flag is placed later, after the roads, at a varied spot —
		// see place_flags — so it never lands fixed at the zone centre or on a road)

		// varied zone contents like the originals: some zones dense, some
		// just a flag worth holding (engine footprints: factories 4x5,
		// radar 4x3, repair 5x4)
		int n_buildings = rnd(4); //0-3
		for(int bnum = 0; bnum < n_buildings; bnum++)
		{
			int roll = rnd(100);
			int bw = (roll < 70) ? 4 : 5, bh = (roll < 40) ? 5 : ((roll < 70) ? 3 : 4);
			int oid = (roll < 25) ? ROBOT_FACTORY : (roll < 40) ? VEHICLE_FACTORY : (roll < 70) ? RADAR : REPAIR;
			if(oid == ROBOT_FACTORY || oid == VEHICLE_FACTORY) { bw = 4; bh = 5; }
			else if(oid == RADAR) { bw = 4; bh = 3; }
			else { bw = 5; bh = 4; }

			for(int attempt = 0; attempt < 8; attempt++)
			{
				int bx = z.x + 2 + rnd(z.w > bw + 4 ? z.w - bw - 4 : 1);
				int by = z.y + 2 + rnd(z.h > bh + 4 ? z.h - bh - 4 : 1);
				if(!area_free(bx, by, bw, bh)) continue;
				reserve_rect(bx, by, bw, bh, true);
				add_object(bx, by, 0, BUILDING_OBJECT, oid, tech);
				break;
			}
		}
	}
}

// a neutral/orphaned vehicle type, weighted by tech level: low tech fields light
// vehicles, higher tech mixes in heavier armour (HEAVY/APC/missile launcher)
static int random_neutral_vehicle()
{
	int r = rnd(100);
	if(tech >= 4)      return (r < 22) ? JEEP_V : (r < 42) ? LIGHT_V : (r < 64) ? MEDIUM_V : (r < 84) ? HEAVY_V : (r < 94) ? APC_V : MISSILE_V;
	else if(tech >= 2) return (r < 32) ? JEEP_V : (r < 60) ? LIGHT_V : (r < 84) ? MEDIUM_V : (r < 95) ? HEAVY_V : APC_V;
	else               return (r < 50) ? JEEP_V : (r < 82) ? LIGHT_V : MEDIUM_V;
}

static void place_scatter()
{
	// rock clusters: a rock OBJECT at (x,y) blocks the tile at (x, y+2) -
	// its base - so choose the blocking tile and place the object above it.
	// The interleaved terrain already supplies the rock; keep only a few
	// destructible rock objects (they double as carvable connectivity chokes).
	int clusters = (W * H) / 6000;
	for(int c = 0; c < clusters; c++)
	{
		int x = 4 + rnd(W - 8), y = 4 + rnd(H - 8);
		int n = 4 + rnd(9);
		for(int r = 0; r < n; r++)
		{
			int i = idx(x, y);
			if(y >= 3 && kind[i] == 'g' && !reserved[i] && !keepout[i]
				&& !reserved[idx(x, y-1)] && !reserved[idx(x, y-2)])
			{
				add_object(x, y - 2, 1, MAP_ITEM_OBJECT, ROCK_ITEM);
				rock_at[i] = (int)objects.size() - 1;
				reserved[i] = true;
				solid[i] = true;
				reserved[idx(x, y-1)] = true; //sprite body
				reserved[idx(x, y-2)] = true;
			}
			x += rnd(3) - 1; y += rnd(3) - 1;
			if(x < 4 || y < 4 || x > W - 5 || y > H - 5) break;
		}
	}

	// huts, grenade boxes, neutral jeeps
	int tries;
	#define SPOT(cond, n, place) \
		for(int k = 0, tries2 = 0; k < (n) && tries2 < 400; tries2++) { \
			int x = 3 + rnd(W - 6), y = 3 + rnd(H - 6); int i = idx(x, y); \
			if(kind[i] == 'g' && !reserved[i] && !keepout[i] && (cond)) { reserved[i] = true; place; k++; } }
	(void)tries;
	SPOT(true, (W*H)/900, add_object(x, y, 0, MAP_ITEM_OBJECT, HUT_ITEM))
	SPOT(true, 5, add_object(x, y, 1, MAP_ITEM_OBJECT, GRENADES_ITEM))
	#undef SPOT

	// neutral/orphaned vehicles: spread them EVENLY across the map so no side is
	// favoured - every player should have similar access to hijack them. Enforce
	// a minimum spacing (Poisson-disk style) that relaxes as attempts run out, so
	// they fan out instead of clumping where the random scatter happened to land.
	int veh = (n_vehicles < 0) ? enemies + 1 : n_vehicles;  // <0 = auto
	vector<int> vx, vy;
	int base_sep = (W < H ? W : H) / 3;
	for(int k = 0, tries = 0; k < veh && tries < 6000; tries++)
	{
		int x = 3 + rnd(W - 6), y = 3 + rnd(H - 6), i = idx(x, y);

		if(kind[i] != 'g' || reserved[i] || keepout[i]) continue;
		if(!(x < W - 4 && y < H - 4 && area_free(x, y, 2, 2)
			&& kind[idx(x+1,y)] == 'g' && kind[idx(x,y+1)] == 'g' && kind[idx(x+1,y+1)] == 'g'
			&& !keepout[idx(x+1,y)] && !keepout[idx(x,y+1)] && !keepout[idx(x+1,y+1)])) continue;

		//spacing shrinks toward 0 as tries climb, guaranteeing placement
		int sep = base_sep * (6000 - tries) / 6000;
		bool too_close = false;
		for(size_t v = 0; v < vx.size() && !too_close; v++)
			if(abs(vx[v] - x) < sep && abs(vy[v] - y) < sep) too_close = true;
		if(too_close) continue;

		//a vehicle is a movable UNIT: reserve its 2x2 without clearing terrain
		reserved[i] = reserved[idx(x+1,y)] = reserved[idx(x,y+1)] = reserved[idx(x+1,y+1)] = true;
		add_object(x, y, 0, VEHICLE_OBJECT, random_neutral_vehicle());
		vx.push_back(x); vy.push_back(y);
		k++;
	}
}

static bool write_map()
{
	map_basics b;
	memset(&b, 0, sizeof(b));
	b.width = W; b.height = H;
	snprintf(b.map_name, sizeof(b.map_name), "random_%u", seed);
	int live_objects = 0;
	for(size_t i = 0; i < objects.size(); i++) if(!obj_dead[i]) live_objects++;

	b.player_count = enemies + 1;
	b.object_count = live_objects;
	b.terrain_type = terrain;
	b.zone_count = zones.size();

	FILE *fp = fopen(out_path.c_str(), "wb");
	if(!fp) { fprintf(stderr, "cannot write %s\n", out_path.c_str()); return false; }

	fwrite(&b, sizeof(b), 1, fp);
	for(size_t i = 0; i < zones.size(); i++) fwrite(&zones[i], sizeof(map_zone), 1, fp);
	for(size_t i = 0; i < objects.size(); i++) if(!obj_dead[i]) fwrite(&objects[i], sizeof(map_object), 1, fp);
	for(size_t i = 0; i < tiles.size(); i++) fwrite(&tiles[i], sizeof(unsigned short), 1, fp);
	fclose(fp);

	printf("wrote %s: %dx%d tiles, %d players, %d zones, %d objects, seed %u\n",
		out_path.c_str(), W, H, enemies + 1, (int)zones.size(), live_objects, seed);
	return true;
}

bool Generate(const std::string &out_path_, int enemies_, int w_, int h_,
	int terrain_, int tech_, unsigned int seed_, int vehicles_,
	const std::string &assets_dir_)
{
	//reset all module state (this can be called repeatedly in-engine)
	out_path = out_path_;
	enemies = enemies_;
	W = w_;
	H = h_;
	terrain = terrain_;
	tech = tech_;
	seed = seed_;
	n_vehicles = vehicles_;
	assets_dir = assets_dir_;

	ground_tiles.clear(); starter_tiles.clear(); water_tiles.clear(); road_tiles.clear(); rock_tiles.clear();
	water_tab.clear(); water_tab4.clear(); rock_tab.clear(); rock_tab4.clear(); ground_tab.clear(); road_tab.clear();
	tiles.clear(); kind.clear(); reserved.clear(); solid.clear();
	zones.clear(); objects.clear(); fort_zone.clear();
	rock_at.clear(); obj_dead.clear();

	if(enemies < 1) enemies = 1;
	if(enemies > 3) enemies = 3;
	if(W < 48) W = 48;
	if(H < 48) H = 48;
	if(W > 250) W = 250;
	if(H > 250) H = 250;
	if(terrain < 0 || terrain > 4) terrain = 0;
	if(tech < 0) tech = 0;
	if(tech > 5) tech = 5;

	srand(seed);

	if(!load_tileinfo()) return false;
	learn_tiles();

	tiles.resize((size_t)W * H);
	kind.assign((size_t)W * H, 'g');
	reserved.assign((size_t)W * H, false);
	solid.assign((size_t)W * H, false);
	keepout.assign((size_t)W * H, false);
	rock_at.assign((size_t)W * H, -1);
	for(int y = 0; y < H; y++) for(int x = 0; x < W; x++) set_ground(x, y);

	make_zones();
	make_terrain(); // carve the playable chambers+corridors out of solid rock

	// reserve structure spots (in the open chambers) before water/rocks
	for(int t = 0; t < enemies + 1; t++) place_team(t);
	place_zone_contents();

	// buildings are placed AFTER the terrain veins, so a vein may have left rock
	// or water right at a doorway. Clear every keepout cell (the apron that
	// reserve_rect marks around each structure) back to open ground.
	for(int y = 0; y < H; y++)
		for(int x = 0; x < W; x++)
			if(keepout[idx(x,y)] && (kind[idx(x,y)] == 'x' || kind[idx(x,y)] == 'w'))
				set_ground(x, y);

	make_roads();
	place_flags();   // after roads: varied positions, never on a road
	make_water();
	place_scatter();

	obj_dead.assign(objects.size(), false);
	ensure_connectivity();

	retile();

	return write_map();
}

} //namespace MapGen
