#ifndef _MAP_GEN_CORE_H_
#define _MAP_GEN_CORE_H_

#include <string>

// Random skirmish map generator core, callable from both the standalone
// zod_mapgen tool and the in-game "Generate Map" menu. Writes an ordinary
// .map file the engine loads like any hand-made map.
//
// enemies 1-3, terrain 0-4 (desert/volcanic/arctic/jungle/city); w/h in tiles.
// vehicles is how many neutral/orphaned vehicles to scatter (-1 = auto, i.e.
// enemies+1). assets_dir is where the palette .tileinfo and campaign maps live
// (it learns shoreline/road/ground tile usage from the shipped maps). Returns
// true on success.

namespace MapGen
{
	bool Generate(const std::string &out_path, int enemies, int w, int h,
		int terrain, int tech, unsigned int seed, int vehicles = -1,
		const std::string &assets_dir = "assets");
}

#endif
