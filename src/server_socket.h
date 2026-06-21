#ifndef _SERVERSOCKET_H_
#define _SERVERSOCKET_H_

#include <vector>

#include "constants.h"
#include "socket_handler.h"
#include "event_handler.h"

#ifdef _WIN32 //if windows
//Use winsock2 explicitly (matches the linked ws2_32) instead of the
//deprecated winsock 1.1 that <windows.h> pulled in implicitly. winsock2
//must precede any <windows.h>; WIN32_LEAN_AND_MEAN (set globally in CMake)
//keeps a stray <windows.h> from dragging the old <winsock.h> back in.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <direct.h>		//win for _mkdir/_getcwd
#else
#include <sys/types.h>		//lin
#include <sys/socket.h>		//lin
#include <netinet/in.h>		//lin
#include <netdb.h>		//lin
#include <sys/stat.h>		//lin
#include <sys/ioctl.h>		//lin
#include <arpa/inet.h>
#endif

using namespace std;

class ZServer;

class ServerSocket
{
	public:
		ServerSocket();
		int Start(int port = 8000, bool localhost_only = true);	//#158: loopback-only unless explicitly hosting
		void SetEventList(list<Event*> *event_list_);
		void SetEventHandler(EventHandler<ZServer> *ehandler_) { ehandler = ehandler_; }
		int Process();
		
		int SendMessage(int player, int pack_id, const char *data, int size);
		int SendMessageAll(int pack_id, const char *data, int size);
		int SendMessageAscii(int player, int pack_id, const char *data);
		
		int PlayersConnected();
		
		SocketHandler* GetHandler(int id);
	private:
		int Bind(bool localhost_only = true);
		int Listen();
		int CreateSocket();
		int CheckConnects();
		int CheckData();
		
		int port;
		int bound;
		int started;
		
		int listen_socket;
		vector<SocketHandler*> client_socket;
		list<Event*> *event_list;
		EventHandler<ZServer> *ehandler;
};

#endif
