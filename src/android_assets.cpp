#ifdef __ANDROID__

#include "android_assets.h"

#include <SDL3/SDL.h>

#include <string>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>

// Create every directory along `path` (which is a FILE path - the last
// component, after the final '/', is not created).
static void mkdir_parents(const std::string &path)
{
	std::string cur;
	for(size_t i = 0; i < path.size(); i++)
	{
		cur += path[i];
		if(path[i] == '/')
			mkdir(cur.c_str(), 0755);
	}
}

bool AndroidExtractAssetsAndChdir()
{
	const char *base = SDL_GetAndroidInternalStoragePath();
	if(!base)
	{
		SDL_Log("zod: no Android internal storage path");
		return false;
	}

	std::string root = std::string(base) + "/gamedata";
	mkdir(root.c_str(), 0755);

	// A marker file records which build last extracted, so we re-extract when
	// the app is updated but skip the (slow) copy on every normal launch.
	std::string marker = root + "/.extracted-" ZOD_VERSION;
	struct stat st;
	bool already = (stat(marker.c_str(), &st) == 0);

	if(!already)
	{
		// The build bundles a newline-separated list of every data file
		// (relative to assets/zod/) at assets/zod/filelist.txt. On Android a
		// relative SDL path reads straight from the APK's asset manager.
		size_t fl_size = 0;
		void *fl = SDL_LoadFile("zod/filelist.txt", &fl_size);
		if(!fl)
		{
			SDL_Log("zod: filelist.txt not found in APK assets");
			return false;
		}
		std::string list((const char *)fl, fl_size);
		SDL_free(fl);

		int copied = 0, missing = 0;
		size_t pos = 0;
		while(pos < list.size())
		{
			size_t nl = list.find('\n', pos);
			std::string rel = list.substr(pos, (nl == std::string::npos) ? std::string::npos : nl - pos);
			pos = (nl == std::string::npos) ? list.size() : nl + 1;

			while(!rel.empty() && (rel.back() == '\r' || rel.back() == ' ' || rel.back() == '\t'))
				rel.pop_back();
			if(rel.empty()) continue;

			std::string asset = "zod/" + rel;
			size_t sz = 0;
			void *data = SDL_LoadFile(asset.c_str(), &sz);
			if(!data) { missing++; continue; }

			std::string out = root + "/" + rel;
			mkdir_parents(out);
			FILE *f = fopen(out.c_str(), "wb");
			if(f)
			{
				if(sz) fwrite(data, 1, sz, f);
				fclose(f);
				copied++;
			}
			SDL_free(data);
		}
		SDL_Log("zod: extracted %d files (%d missing) to %s", copied, missing, root.c_str());

		FILE *m = fopen(marker.c_str(), "wb");
		if(m) fclose(m);
	}

	if(chdir(root.c_str()) != 0)
	{
		SDL_Log("zod: chdir to %s failed", root.c_str());
		return false;
	}
	return true;
}

#endif
