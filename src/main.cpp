#include <stdio.h>

#ifdef __ANDROID__
// On Android the process entry point is provided by SDL's Java activity (JNI),
// which calls our main() as SDL_main. Including this header does the
// `#define main SDL_main` rename. Guarded out on desktop builds.
#include <SDL3/SDL_main.h>
#include "android_assets.h"
#endif

#ifdef _WIN32

//xgetopt stuff
//this is only needed for some compilers
#include "xgetopt.h"
int optind, opterr;
TCHAR *optarg;
//end xgetopt

#else
#include <unistd.h>
#endif


#include <sys/stat.h>
#include <string>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define ZOD_CHDIR _chdir
#else
#include <unistd.h>
#define ZOD_CHDIR chdir
#endif

#include "main.h"
#include "zpath_debug.h"
#include "zvideo.h"
#include "zprefs.h"
#include "common.h"
#include "constants.h"
#include "zsdl.h"
#include "zplayer.h"
#include "zbot.h"
#include "zserver.h"
#include "ztray.h"

// Compile-time install data dir (package builds set e.g.
// -D ZOD_DATADIR=\"/opt/homebrew/share/zod\"); empty when unset.
#ifndef ZOD_DATADIR
#define ZOD_DATADIR ""
#endif

void display_help(char *shell_command);
void display_version();
int run_server_thread(void *nothing);
int run_bot_thread(void *nothing);
void run_player_thread();
void run_tray_app();

input_options starting_conditions;

char bot_bypass_data[MAX_BOT_BYPASS_SIZE];
int bot_bypass_size;

// The game loads ~485 assets by paths relative to the working directory.
// That's fine when run from the source tree, but breaks once installed
// (binary in bin/, assets in share/). Locate the data dir and chdir into it
// so the relative paths resolve regardless of how/where it was launched.
static bool dir_has_assets(const std::string &dir)
{
	struct stat st;
	std::string probe = dir + "/assets";
	return stat(probe.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static std::string get_exe_dir()
{
	char buf[4096];
#ifdef __APPLE__
	uint32_t size = sizeof(buf);
	if(_NSGetExecutablePath(buf, &size) != 0) return "";
#elif defined(_WIN32)
	DWORD n = GetModuleFileNameA(NULL, buf, sizeof(buf));
	if(n == 0 || n >= sizeof(buf)) return "";
	buf[n] = 0;
#else
	ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
	if(n <= 0) return "";
	buf[n] = 0;
#endif
	std::string p(buf);
	size_t slash = p.find_last_of("/\\");   // handle Windows backslashes too
	return slash == std::string::npos ? "" : p.substr(0, slash);
}

static void chdir_to_data_dir()
{
	// 1. explicit override
	const char *env = getenv("ZOD_DATA");
	if(env && dir_has_assets(env)) { if(ZOD_CHDIR(env)){} return; }

	// 2. already in a dir that has assets/ (running from the source tree)
	if(dir_has_assets(".")) return;

	std::string exe = get_exe_dir();
	if(!exe.empty())
	{
		// 3. assets sit right next to the binary (Windows zip / portable layout)
		if(dir_has_assets(exe)) { if(ZOD_CHDIR(exe.c_str())){} return; }
		// 4. installed layout: <prefix>/bin/zod -> <prefix>/share/zod/assets
		std::string share = exe + "/../share/zod";
		if(dir_has_assets(share)) { if(ZOD_CHDIR(share.c_str())){} return; }
	}

	// 5. compiled-in install datadir (set by package builds)
	std::string datadir = ZOD_DATADIR;
	if(!datadir.empty() && dir_has_assets(datadir)) { if(ZOD_CHDIR(datadir.c_str())){} return; }

	// else: leave cwd as-is (assets/ had better be here)
}

#ifndef __ANDROID__
// undo SDL's `#define main SDL_main` on desktop (we want a plain main there).
// On Android we KEEP the rename: SDL's Java activity calls our SDL_main via JNI.
#undef main
#endif
int main(int argc, char **argv)
{
#ifdef __ANDROID__
	//make the engine's printf/stderr diagnostics visible in logcat
	AndroidRedirectStdioToLogcat();
#endif
#ifdef __ANDROID__
	//no real cwd on Android: unpack the APK-bundled game data to writable
	//storage and cd into it, so the engine's fopen/ifstream/SDL_LoadBMP calls
	//resolve assets/ maps/ map_list.txt the same as on desktop
	AndroidExtractAssetsAndChdir();
#else
	//find the game data before anything tries to load an asset
	chdir_to_data_dir();
#endif

	// --map-info <mapfile>: print how many player slots the map has (distinct
	// teams with a fort placed) as JSON, then exit. Used to build a matchmaking
	// map manifest. Loads map DATA only - no SDL/graphics (same read path the
	// dedicated server uses), so it's cheap and headless.
	if(argc >= 3 && string(argv[1]) == "--map-info")
	{
		ZMap m;
		if(!m.Read(argv[2]))
		{
			fprintf(stderr, "map-info: cannot read map '%s'\n", argv[2]);
			return 1;
		}

		bool team_has_fort[MAX_TEAM_TYPES] = {false};
		for(vector<map_object>::iterator i = m.GetObjectList().begin(); i != m.GetObjectList().end(); ++i)
			if(i->object_type == BUILDING_OBJECT &&
				(i->object_id == FORT_FRONT || i->object_id == FORT_BACK) &&
				i->owner >= 0 && i->owner < MAX_TEAM_TYPES)
				team_has_fort[(int)i->owner] = true;

		int players = 0;
		for(int t = 0; t < MAX_TEAM_TYPES; t++)
			if(team_has_fort[t]) players++;

		string mp = argv[2];
		size_t slash = mp.find_last_of("/\\");
		string base = (slash == string::npos) ? mp : mp.substr(slash + 1);
		printf("{\"map\":\"%s\",\"players\":%d}\n", base.c_str(), players);
		return 0;
	}

	SDL_Thread *server_thread;

	printf("Welcome to the Zod Engine (%s)\n", ZOD_VERSION);

	//optional pathfinding debug log (ZOD_PATHLOG); init before
	//the server / pathfinding threads exist
	ZPathLog_Init();

	//always-on diagnostic log (zod_diag.log) for bug reports
	ZDiag_Init(ZOD_VERSION);

	//player preferences (mouse mode, smoothing) + env overrides
	ZPrefs_Load();

	if(argc<=1) starting_conditions.setdefaults();
	
	//read in the arguments
	starting_conditions.getoptions(argc, argv);
	
	//make sure there is nothing conflicting, 
	//like we are trying to make a dedicated server that is supposed to connect to another server
	starting_conditions.checkoptions();
	
	//init this for the bots
	ZCore::CreateRandomBotBypassData(bot_bypass_data, bot_bypass_size);

	//now see what we have
	if(starting_conditions.read_display_version) display_version();
	if(starting_conditions.read_display_help) display_help(argv[0]);
	
	//now what do we really run?
	else if(starting_conditions.read_run_tray)
	{
		run_tray_app();
	}
	else if(starting_conditions.read_is_dedicated)
	{
		//run only a server
		//server_thread = SDL_CreateThread(run_server_thread, NULL);
		run_server_thread(NULL);
	}
	else if(starting_conditions.read_connect_address)
	{
		//connect to a server
		run_player_thread();
	}
	else
	{
		//run a server, then connect to it
		server_thread = SDL_CreateThread(run_server_thread, "server", NULL);
		run_player_thread();
	}

	return 1;
}

int run_server_thread(void *nothing)
{
	int i;
	vector<SDL_Thread*> bot_thread;
	ZServer zserver;

	//#79 follow-up: keep the default single-player opponent (a BLUE bot) unless the
	//user set up bots themselves (-b) or this is a network/dedicated launch. Like
	//menu-first, this default was only applied with zero arguments, so any flag -
	//even just -r/-w - left you with no AI opponent (campaign maps rely on it;
	//generated maps spawn their own bots in GenerateAndStartMap).
	{
		bool any_bot = false;
		for(i=0;i<MAX_TEAM_TYPES;i++)
			if(starting_conditions.read_start_bot[i]) any_bot = true;
		if(!any_bot && !starting_conditions.read_is_dedicated
			&& !starting_conditions.read_connect_address)
			starting_conditions.read_start_bot[BLUE_TEAM] = true;
	}

	for(i=0;i<MAX_TEAM_TYPES;i++)
		if(starting_conditions.read_start_bot[i])
		{
			zserver.InitBot(i);

			//int *team = new int;
			//printf("start bot for team %s\n", team_type_string[i].c_str());

			//*team = i;
			//bot_thread.push_back(SDL_CreateThread(run_bot_thread, (void*)team));
		}
	
	if(starting_conditions.read_map_name)
		zserver.SetMapName(starting_conditions.map_name);
	else if(starting_conditions.read_map_list)
		zserver.SetMapList(starting_conditions.map_list);

	//#79 follow-up: start on the menu by default for a normal launch. Only skip it
	//when the user asked for something specific to load - a map (-m), a map list
	//(-l), a dedicated server (-d), or a multiplayer connection (-c). Previously the
	//menu default was only applied with zero arguments, so any flag - even just -r
	//or -w - dropped you straight into the campaign.
	if(!starting_conditions.read_map_name && !starting_conditions.read_map_list
		&& !starting_conditions.read_is_dedicated && !starting_conditions.read_connect_address)
		starting_conditions.menu_first = true;

	//#79: idle on the menu instead of auto-loading the first map (the map list is
	//still read, so Play Campaign / the map picker work from the menu)
	if(starting_conditions.menu_first)
		zserver.SetMenuFirst(true);

	if(starting_conditions.read_settings)
		zserver.SetSettingsFilename(starting_conditions.settings_filename);
	if(starting_conditions.read_p_settings)
		zserver.SetPerpetualSettingsFilename(starting_conditions.p_settings_filename);

	zserver.SetBotBypassData(bot_bypass_data, bot_bypass_size);

	//#158: only an explicit host exposes a network port. A dedicated server (-d) is
	//inherently for remote clients; a normal host-and-play launch opts in with -L.
	//Everything else (plain singleplayer) binds 127.0.0.1 only.
	zserver.SetAllowRemoteClients(starting_conditions.read_is_dedicated || starting_conditions.read_allow_remote);

	zserver.Setup();
	zserver.Run();

	return 1;
}

int run_bot_thread(void *nothing)
{
	int *team;
	ZBot zbot;

	team = (int*)nothing;
	
	zbot.SetDesiredTeam((team_type)*team);
	if(starting_conditions.read_connect_address)
		zbot.SetRemoteAddress(starting_conditions.connect_address);

	zbot.SetBotBypassData(bot_bypass_data, bot_bypass_size);

	zbot.Setup();
	zbot.Run();

	return 1;
}

void run_player_thread()
{
	ZPlayer zplayer;
	
	zplayer.DisableCursor(starting_conditions.read_disable_zcursor);
	zplayer.SetSoundsOff(starting_conditions.read_sound_off);
	zplayer.SetMusicOff(starting_conditions.read_music_off);
	zplayer.SetWindowed(starting_conditions.read_is_windowed);
	zplayer.SetUseOpenGL(!starting_conditions.read_opengl_off);
	if(starting_conditions.read_player_team)
		zplayer.SetDesiredTeam((team_type)starting_conditions.team);
	if(starting_conditions.read_player_name)
		zplayer.SetPlayerName(starting_conditions.player_name);
	if(starting_conditions.read_loginname)
		zplayer.SetLoginName(starting_conditions.loginname);
	if(starting_conditions.read_password)
		zplayer.SetLoginPassword(starting_conditions.password);
	if(starting_conditions.read_connect_address)
		zplayer.SetRemoteAddress(starting_conditions.connect_address);
	{
		int rw = starting_conditions.read_resolution ? starting_conditions.resolution_width : 800;
		int rh = starting_conditions.read_resolution ? starting_conditions.resolution_height : 600;

		//No explicit -r: adapt the logical width to the display's aspect
		//ratio. Same height = same pixel size; the map viewport absorbs all
		//the extra width (the HUD anchors to the right edge), and fullscreen
		//fills the screen with no bars. -r WxH still forces any resolution.
		if(!starting_conditions.resolution_explicit)
		{
			int dw, dh;

			if(ZVideo_GetDesktopSize(dw, dh))
			{
#ifdef __ANDROID__
				//#140: the activity is locked landscape, but SDL reports the display
				//in its natural (portrait) orientation, so the aspect math came out
				//tall/narrow and clamped to 4:3 - pillarboxing the landscape screen
				//with black bars left and right. Use the landscape dimensions.
				if(dh > dw) { int t = dw; dw = dh; dh = t; }
#endif
				int aw = (int)(((long)rh * dw) / dh);

				if(aw < rw) aw = rw;                  //never narrower than the classic 4:3
				if(aw > (rh * 8) / 3) aw = (rh * 8) / 3; //sanity cap (~21:9)
				rw = aw & ~1;

				printf("display %dx%d: logical view %dx%d (use -r to override)\n", dw, dh, rw, rh);
			}
		}

		zplayer.SetDimensions(rw, rh);
		zplayer.SetAutoAspect(!starting_conditions.resolution_explicit);
	}
	
	zplayer.Setup();
	zplayer.Run();
}

void run_tray_app()
{
	ZTray ztray;

	if(starting_conditions.read_connect_address)
		ztray.SetRemoteAddress(starting_conditions.connect_address);

	ztray.Setup();
	ztray.Run();
}
 
void display_help(char *shell_command)
{
	printf("\n==================================================================\n");
	printf("Command list...\n");
	printf("-c ip_address        - game host address\n");
	printf("-m filename          - map to be used\n");
	printf("-l filename          - map list to be used\n");
	printf("-z filename          - settings file to be used\n");
	printf("-e filename          - main server settings file to be used\n");
	printf("-n player_name       - your player name\n");
	printf("-g login_name        - your login name\n");
	printf("-i login_password    - your login password\n");
	printf("-t team              - your team\n");
	printf("-b team              - connect a bot player\n");
	printf("-D difficulty        - bot AI difficulty: easy|normal|hard|expert (default normal)\n");
	printf("-w                   - run game in windowed mode\n");
	printf("-r resolution        - resolution to run the game at\n");
	printf("-d                   - run a dedicated server\n");
	printf("-L                   - allow LAN/remote clients to connect (host a network game while you play;\n");
	printf("                       without it a normal game binds 127.0.0.1 only and opens no network port)\n");
	printf("-h                   - display command help\n");
	printf("-s                   - no sound\n");
	printf("-u                   - no music\n");
	printf("-o                   - no opengl\n");
	printf("-k                   - use faster and blander cursor\n");
	printf("-v                   - display version and credits\n");
	printf("-a                   - run shell based tray app\n");
	
	printf("\nExample usage...\n");
	printf("%s -c localhost -r 800x600 -w\n", shell_command);
	printf("%s -m level1.map -b 1 -p 1\n", shell_command);
	printf("==================================================================\n");
}

void display_version()
{
#ifndef DISABLE_OPENGL
	printf("\nZod: A Zed Engine, Version Alpha\n");
#else
	printf("\nZod: A Zed Engine, Version Alpha (OpenGL Disabled)\n");
#endif
	printf("By Michael Bok\n");
	printf("Please visit http://zod.sourceforge.net/ and http://zzone.lewe.com/\n");
}

void input_options::setdefaults()
{
	//No args (e.g. double-clicked / installed launch): start a local
	//single-player vs a bot, instead of the old default of dialing the long-dead
	//nighsoft online server.
	printf("no arguments set: starting on the menu (red vs blue bot, map_list.txt, windowed 800x600)\n");

	//local game — no connect address, so main() runs a server + connects us
	read_player_name = true;
	player_name = "Player";

	read_player_team = true;
	player_team_str = "red";
	team = RED_TEAM;

	//#79: land on the menu with no map loaded; the campaign list is still read so
	//Play Campaign works, the game just doesn't auto-start a map.
	read_map_list = true;
	map_list = "map_list.txt";
	menu_first = true;

	//give us an opponent (idles until the player picks a map)
	read_start_bot[BLUE_TEAM] = true;

	resolution = "800x600";
	resolution_width = 800;
	resolution_height = 600;
	read_resolution = true;

	read_is_windowed = true;

	read_opengl_off = true;
}
 
int input_options::checkoptions()
{
	//nothing?
	if(!read_connect_address &&
		!read_map_name &&
		!read_is_windowed &&
		!read_resolution &&
		!read_is_dedicated &&
		!read_display_help &&
		!read_display_version &&
		!read_map_list)
	{
		read_display_help = true;
	}
	
	if(read_connect_address && read_is_dedicated)
	{
		printf("cannot be a dedicated server and have a connect address\n");
		return 0;
	}
	
	if(read_is_dedicated && read_resolution)
	{
		printf("cannot be a dedicated server and have a screen resolution\n");
		return 0;
	}
	
	if(read_is_dedicated && read_is_windowed)
	{
		printf("cannot be a dedicated server and be in windowed mode\n");
		return 0;
	}
	
	if(read_connect_address && (read_map_name || read_map_list))
	{
		printf("cannot have a connect address and set the map\n");
		return 0;
	}

	if(read_map_name && read_map_list)
	{
		printf("cannot a read map list and a specific map file\n");
		return 0;
	}

	if(read_run_tray && !read_connect_address)
	{
		printf("need a connect address to run the tray app\n");
		return 0;
	}
	
	return 1;
}
 
int input_options::getoptions(int argc, char **argv)
{
	int c;
	int i;
	int temp_int;
	extern char *optarg;
	extern int optind;

	while ((c = getopt(argc, argv, "c:m:l:n:t:b:z:e:g:i:wr:dhvksuoaMD:L")) != -1)
	{
		switch(c)
		{
			case 'M':	//#79: start on the menu, no map auto-loaded
				menu_first = true;
				break;
			case 'D':	//#difficulty: optional bot AI difficulty (default stays Normal)
			{
				if(!optarg) return 0;
				std::string s = optarg;
				for(size_t k=0;k<s.size();k++) s[k] = tolower((unsigned char)s[k]);
				if(s == "easy") zod_bot_difficulty = BOT_DIFF_EASY;
				else if(s == "normal") zod_bot_difficulty = BOT_DIFF_NORMAL;
				else if(s == "hard") zod_bot_difficulty = BOT_DIFF_HARD;
				else if(s == "expert") zod_bot_difficulty = BOT_DIFF_EXPERT;
				else { int d = atoi(optarg); if(d >= 0 && d < MAX_BOT_DIFFICULTY) zod_bot_difficulty = d; }
				break;
			}
			case 'c':
				if(!optarg) return 0;
				read_connect_address = true;
				connect_address = optarg;
				break;
			case 'm':
				if(!optarg) return 0;
				read_map_name = true;
				map_name = optarg;
				break;

			case 'l':
				if(!optarg) return 0;
				read_map_list = true;
				map_list = optarg;
				break;
				
			case 'n':
				if(!optarg) return 0;
				read_player_name = true;
				player_name = optarg;
				break;

			case 'z':
				if(!optarg) return 0;
				read_settings = true;
				settings_filename = optarg;
				break;

			case 'e':
				if(!optarg) return 0;
				read_p_settings = true;
				p_settings_filename = optarg;
				break;

			case 'g':
				if(!optarg) return 0;
				read_loginname = true;
				loginname = optarg;
				break;

			case 'i':
				if(!optarg) return 0;
				read_password = true;
				password = optarg;
				break;
				
			case 't':
				if(!optarg) return 0;
				read_player_team = true;
				player_team_str = optarg;

				for(i=0;i<MAX_TEAM_TYPES;i++)
					if(team_type_string[i] == player_team_str)
						break;

				if(i!=MAX_TEAM_TYPES)
					team = i;
				else
					printf("could not find the team '%s', perhaps try lowercase?\n", player_team_str.c_str());
				break;
				
			case 'b':
				if(!optarg) return 0;
				//read_bot_count = true;
				//bot_count = atoi(optarg);
				if(!optarg) return 0;

				for(i=0;i<MAX_TEAM_TYPES;i++)
					if(team_type_string[i] == optarg)
						break;

				if(i!=MAX_TEAM_TYPES)
					read_start_bot[i] = true;
				else
					printf("could not find the team '%s', perhaps try lowercase?\n", optarg);
				break;
				
			case 'r':
				if(!optarg) return 0;
				resolution = optarg;
				
				temp_int = resolution.find('x');
				
				if(temp_int != string::npos)
				{
					resolution_width = atoi(resolution.substr(0, temp_int).c_str());
					resolution_height = atoi(resolution.substr(temp_int+1, 10).c_str());
					read_resolution = true;
					resolution_explicit = true; //user chose; don't auto-adapt
				}
				else
					read_resolution = false;
				
				break;
				
			case 'w':
				read_is_windowed = true;
				break;
				
			case 'd':
				read_is_dedicated = true;
				break;

			case 'L':	//#158: opt in to LAN/remote clients on a normal host-and-play launch
				read_allow_remote = true;
				break;

			case 'h':
				read_display_help = true;
				break;
				
			case 'v':
				read_display_version = true;
				break;

			case 's':
				read_sound_off = true;
				break;

			case 'u':
				read_music_off = true;
				break;

			case 'k':
				read_disable_zcursor = true;
				break;

			case 'o':
				read_opengl_off = true;
				break;

			case 'a':
				read_run_tray = true;
				break;
				
			case '?':
				printf("unrecognized option -%c\n", c);
				read_display_help = true;
				return 0;
				break;
				
				
		}
		
	}

	return 1;
}
