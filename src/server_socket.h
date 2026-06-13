#ifndef _SERVERSOCKET_H_
#define _SERVERSOCKET_H_

#include <vector>

#include "constants.h"
#include "socket_handler.h"
#include "event_handler.h"

#ifdef _WIN32 //if windows
//winsock2 must come before any <windows.h>, and WIN32_LEAN_AND_MEAN keeps
//<windows.h> (pulled transitively) from dragging in <rpcndr.h>, whose global
//`byte` typedef is ambiguous with C++17+ std::byte under `using namespace std`.
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
		int Start(int port = 8000);
		void SetEventList(list<Event*> *event_list_);
		void SetEventHandler(EventHandler<ZServer> *ehandler_) { ehandler = ehandler_; }
		int Process();
		
		int SendMessage(int player, int pack_id, const char *data, int size);
		int SendMessageAll(int pack_id, const char *data, int size);
		int SendMessageAscii(int player, int pack_id, const char *data);
		
		int PlayersConnected();
		
		SocketHandler* GetHandler(int id);
	private:
		int Bind();
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
