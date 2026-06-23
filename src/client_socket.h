#ifndef _CLIENTSOCKET_H_
#define _CLIENTSOCKET_H_

#include <string>
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

class ClientSocket
{
	public:
		ClientSocket();
		
		int Start(const char *address_ = "localhost", int port_ = 2137);
		void SetEventList(list<Event*> *event_list_);
		int Process();
		
		int SendMessage(int pack_id, const char *data, int size);
		int SendMessageAscii(int pack_id, const char *data);
		
		SocketHandler* GetHandler();
		void ClearConnection();
	private:
		int CreateSocket();
		int Connect();
		
		string address;
		int port;
		int s;
		
		SocketHandler* shandler;
		list<Event*> *event_list;
};

#endif
