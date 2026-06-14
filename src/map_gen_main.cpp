// CLI front-end for the random map generator. The generation logic lives in
// map_gen.cpp (namespace MapGen), shared with the in-game "Generate Map" menu.
//
//   zod_mapgen -e <enemies 1-3> [-w tiles] [-h tiles] [-t terrain 0-4]
//              [-L tech 0-5] [-v neutral-vehicles] [-s seed] [-o out.map]

#include "map_gen_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

int main(int argc, char **argv)
{
	int enemies = 1, w = 80, h = 100, terrain = 0, tech = 0, vehicles = -1;
	unsigned int seed = 1234567;
	std::string out_path = "maps/random.map";

	for(int i = 1; i < argc; i++)
	{
		if(!strcmp(argv[i], "-e") && i + 1 < argc) enemies = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-w") && i + 1 < argc) w = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-h") && i + 1 < argc) h = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-t") && i + 1 < argc) terrain = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-L") && i + 1 < argc) tech = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-v") && i + 1 < argc) vehicles = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-s") && i + 1 < argc) seed = (unsigned)atoi(argv[++i]);
		else if(!strcmp(argv[i], "-o") && i + 1 < argc) out_path = argv[++i];
		else { printf("usage: %s -e <enemies 1-3> [-w tiles] [-h tiles] [-t terrain 0-4] [-L tech 0-5] [-v neutral-vehicles] [-s seed] [-o out.map]\n", argv[0]); return 1; }
	}

	return MapGen::Generate(out_path, enemies, w, h, terrain, tech, seed, vehicles) ? 0 : 1;
}
