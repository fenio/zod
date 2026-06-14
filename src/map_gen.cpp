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
enum { JEEP_V, LIGHT_V, MEDIUM_V };
enum { GRUNT_R };

static const char *terrain_names[5] = { "desert", "volcanic", "arctic", "jungle", "city" };

// ---- generator state ----

static int W = 80, H = 100;
static int enemies = 1;
static int terrain = 0;
static int tech = 0;   //building level 0-5: how advanced production is
static unsigned int seed = 0;
static string out_path = "maps/random.map";
static string assets_dir = "assets";

static palette_tile_info tinfo[MAX_PLANET_TILES];
static vector<int> ground_tiles, starter_tiles, water_tiles, road_tiles;

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
		if(!tinfo[i].is_passable) continue;
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
			int wp = 0, wp4 = 0;
			for(int d = 0; d < 8; d++)
			{
				int nx = x + dx8[d], ny = y + dy8[d];
				char nk = (nx < 0 || ny < 0 || nx >= b.width || ny >= b.height) ? 'g' : K[ny * b.width + nx];
				if(nk == 'w') wp |= (1 << d);
			}
			if(wp & (1<<1)) wp4 |= 1; //N
			if(wp & (1<<3)) wp4 |= 2; //W
			if(wp & (1<<4)) wp4 |= 4; //E
			if(wp & (1<<6)) wp4 |= 8; //S

			unsigned short tt = t[y * b.width + x];
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

	// fort zones: spread corners (TL, BR, TR, BL) — symmetric-ish for 2-4 teams
	int corner_zone[4] = { 0, (int)zones.size() - 1, cols - 1, (int)zones.size() - cols };
	for(int t = 0; t < enemies + 1; t++)
		fort_zone.push_back(corner_zone[t]);
}

static void make_roads()
{
	if(road_tiles.empty()) return;

	int cx = W / 2, cy = H / 2;
	for(size_t t = 0; t < fort_zone.size(); t++)
	{
		map_zone &z = zones[fort_zone[t]];
		int x = z.x + z.w / 2, y = z.y + z.h / 2;

		// L-shape: horizontal then vertical, 2 tiles wide; never through structures
		for(int i = (x < cx ? x : cx); i <= (x < cx ? cx : x); i++)
			for(int d = 0; d < 2; d++)
				if(y + d < H && !reserved[idx(i, y + d)])
					{ kind[idx(i, y + d)] = 'r'; tiles[idx(i, y + d)] = road_tiles[rnd(road_tiles.size())]; }
		for(int j = (y < cy ? y : cy); j <= (y < cy ? cy : y); j++)
			for(int d = 0; d < 2; d++)
				if(cx + d < W && !reserved[idx(cx + d, j)])
					{ kind[idx(cx + d, j)] = 'r'; tiles[idx(cx + d, j)] = road_tiles[rnd(road_tiles.size())]; }
	}
}

static void make_water()
{
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

			int wp = 0, wp4 = 0;
			for(int d = 0; d < 8; d++)
			{
				int nx = x + dx8[d], ny = y + dy8[d];
				char nk = (nx < 0 || ny < 0 || nx >= W || ny >= H) ? 'g' : kind[idx(nx, ny)];
				if(nk == 'w') wp |= (1 << d);
			}
			if(wp & (1<<1)) wp4 |= 1;
			if(wp & (1<<3)) wp4 |= 2;
			if(wp & (1<<4)) wp4 |= 4;
			if(wp & (1<<6)) wp4 |= 8;

			char c = kind[i];
			if(c == 'w' || (c == 'g' && wp))
			{
				int k8 = (c << 8) | wp, k4 = (c << 8) | wp4;
				if(water_tab.count(k8)) { tiles[i] = sample_tab(water_tab[k8]); continue; }
				if(water_tab4.count(k4)) { tiles[i] = sample_tab(water_tab4[k4]); continue; }
				if(c == 'w') { tiles[i] = water_tiles.empty() ? tiles[i] : water_tiles[rnd(water_tiles.size())]; continue; }
				//unmatched shore: plain ground below
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
	return kind[idx(x,y)] == 'w' || solid[idx(x,y)];
}

static bool vehicle_open(int x, int y)
{
	return !tile_blocked(x,y) && !tile_blocked(x+1,y) && !tile_blocked(x,y+1) && !tile_blocked(x+1,y+1);
}

static void carve_cell(int x, int y)
{
	if(x < 0 || y < 0 || x >= W || y >= H) return;
	int i = idx(x, y);
	if(kind[i] == 'w') set_ground(x, y);
	if(solid[i] && rock_at[i] >= 0) { obj_dead[rock_at[i]] = true; solid[i] = false; rock_at[i] = -1; }
}

static void ensure_connectivity()
{
	map_zone &z0 = zones[fort_zone[0]];
	int sx = z0.x + z0.w / 2, sy = z0.y + z0.h / 2 + 6; //below the fort
	int cx = W / 2, cy = H / 2;

	for(int pass = 0; pass < 4; pass++)
	{
		vector<bool> seen(W * H, false);
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

	// starting squad outside the footprint, on the fort's open side
	int uy = top_half ? fy + 10 : fy - 2;
	for(int i = 0; i < 3; i++) add_object(fx + 3 + i, uy, t + 1, ROBOT_OBJECT, GRUNT_R);
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
		int cx = z.x + z.w / 2, cy = z.y + z.h / 2;

		// flag marks the capturable zone
		reserve_rect(cx, cy, 1, 1);
		add_object(cx, cy, 0, MAP_ITEM_OBJECT, FLAG_ITEM);

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

static void place_scatter()
{
	// rock clusters: a rock OBJECT at (x,y) blocks the tile at (x, y+2) -
	// its base - so choose the blocking tile and place the object above it
	int clusters = (W * H) / 700;
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
		for(int k = 0, tries2 = 0; k < (n) && tries2 < 200; tries2++) { \
			int x = 3 + rnd(W - 6), y = 3 + rnd(H - 6); int i = idx(x, y); \
			if(kind[i] == 'g' && !reserved[i] && (cond)) { reserved[i] = true; place; k++; } }
	(void)tries;
	SPOT(true, (W*H)/900, add_object(x, y, 0, MAP_ITEM_OBJECT, HUT_ITEM))
	SPOT(true, 5, add_object(x, y, 1, MAP_ITEM_OBJECT, GRENADES_ITEM))
	SPOT(x < W - 4 && y < H - 4 && area_free(x, y, 2, 2)
		&& kind[idx(x+1,y)] == 'g' && kind[idx(x,y+1)] == 'g' && kind[idx(x+1,y+1)] == 'g',
		enemies + 1, { reserve_rect(x, y, 2, 2); add_object(x, y, 0, VEHICLE_OBJECT, JEEP_V); })
	#undef SPOT
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
	int terrain_, int tech_, unsigned int seed_, const std::string &assets_dir_)
{
	//reset all module state (this can be called repeatedly in-engine)
	out_path = out_path_;
	enemies = enemies_;
	W = w_;
	H = h_;
	terrain = terrain_;
	tech = tech_;
	seed = seed_;
	assets_dir = assets_dir_;

	ground_tiles.clear(); starter_tiles.clear(); water_tiles.clear(); road_tiles.clear();
	water_tab.clear(); water_tab4.clear(); ground_tab.clear(); road_tab.clear();
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

	tiles.resize(W * H);
	kind.assign(W * H, 'g');
	reserved.assign(W * H, false);
	solid.assign(W * H, false);
	keepout.assign(W * H, false);
	rock_at.assign(W * H, -1);
	for(int y = 0; y < H; y++) for(int x = 0; x < W; x++) set_ground(x, y);

	make_zones();

	// reserve structure spots before terrain features so water/rocks keep clear
	for(int t = 0; t < enemies + 1; t++) place_team(t);
	place_zone_contents();

	make_roads();
	make_water();
	place_scatter();

	obj_dead.assign(objects.size(), false);
	ensure_connectivity();

	retile();

	return write_map();
}

} //namespace MapGen
