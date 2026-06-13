#ifdef __ANDROID__

#include "android_assets.h"

#include <SDL3/SDL.h>

#include <string>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>

// Reader thread: pull lines off the pipe that stdout/stderr were redirected to,
// and emit each to logcat. Lets the engine's printf diagnostics show up.
static void *zod_stdio_logcat_thread(void *arg)
{
	int rfd = *(int *)arg;
	char buf[1024];
	std::string line;
	ssize_t n;
	while((n = read(rfd, buf, sizeof(buf) - 1)) > 0)
	{
		buf[n] = 0;
		for(ssize_t i = 0; i < n; i++)
		{
			if(buf[i] == '\n')
			{
				__android_log_write(ANDROID_LOG_INFO, "zod-stdio", line.c_str());
				line.clear();
			}
			else if(buf[i] != '\r')
				line += buf[i];
		}
	}
	return NULL;
}

void AndroidRedirectStdioToLogcat()
{
	static int fds[2];
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	if(pipe(fds) != 0) return;
	dup2(fds[1], STDOUT_FILENO);
	dup2(fds[1], STDERR_FILENO);
	pthread_t t;
	if(pthread_create(&t, NULL, zod_stdio_logcat_thread, &fds[0]) == 0)
		pthread_detach(t);
}

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

	// SDL-based loaders (IMG_Load/TTF/mixer) ignore the chdir and read relative
	// paths from the APK assets, so give them this absolute extracted-data root.
	extern void ZSDL_SetDataRoot(const char *);
	ZSDL_SetDataRoot(root.c_str());

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
