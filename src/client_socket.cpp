#include "client_socket.h"
#include "common.h"
#include <SDL3/SDL.h>
#ifndef _WIN32
#include <unistd.h>
#endif

using namespace COMMON;

ClientSocket::ClientSocket()
{
	s = -1;
	shandler = NULL;
	event_list = NULL;
}

int ClientSocket::SendMessage(int pack_id, const char *data, int size)
{
	if(shandler) return shandler->SendMessage(pack_id, data, size);
	else return 0;
}

int ClientSocket::SendMessageAscii(int pack_id, const char *data)
{
	if(shandler) return shandler->SendMessageAscii(pack_id, data);
	else return 0;
}

void ClientSocket::SetEventList(list<Event*> *event_list_)
{
	event_list = event_list_;
}

int ClientSocket::Start(const char *address_, int port_)
{
	address = address_;
	port = port_;
	
	if(!CreateSocket()) return 0;
	
	if(!Connect()) return 0;
	
	return 1;
}

void ClientSocket::ClearConnection()
{
	if(shandler)
	{
		delete shandler;
		shandler = NULL;
	}

	s = -1;
}

int ClientSocket::Process()
{
	char *message;
	int size;
	int pack_id;
	if(!shandler) return 0;
	
	//not connected, free it up
	if(!shandler->Connected())
	{
		ClearConnection();
		
		event_list->push_back(new Event(OTHER_EVENT, DISCONNECT_EVENT, 0, NULL, 0));
	}
	else if(shandler->DoRecv())
	{
		while(shandler->DoFastProcess(&message, &size, &pack_id))
			event_list->push_back(new Event(TCP_EVENT, pack_id, 0, message, size));
		shandler->ResetFastProcess();

		/*
		while(shandler->DoProcess(&message, &size, &pack_id))
		{
			event_list->push_back(new Event(TCP_EVENT, pack_id, 0, message, size));
 			//printf("ClientSocket::Process:got packet id:%d\n", pack_id);
		}*/
	}

	/*
	while(shandler->PacketAvailable() && shandler->GetPacket(&message, &size, &pack_id))
		event_list->push_back(new Event(TCP_EVENT, pack_id, 0, message, size));	
	*/

	return 1;
}

SocketHandler* ClientSocket::GetHandler()
{
	return shandler;
}

int ClientSocket::Connect()
{
	struct sockaddr_in c_in;
	struct hostent * host;
	
	host = gethostbyname(address.c_str());

	if(!host)
	{
		printf("could not resolve host '%s'\n", address.c_str());
		return 0;
	}

	// Only accept an IPv4 result: this is an AF_INET socket, and on IPv6-first
	// systems (e.g. Android) gethostbyname("localhost") can hand back a 16-byte
	// ::1, which would overflow the 4-byte sin_addr and aim connect() at
	// garbage - the server's accept() then never fires.
	if(host->h_addrtype != AF_INET || host->h_length != 4)
	{
		SDL_Log("ClientSocket: '%s' resolved to a non-IPv4 address (type=%d len=%d); cannot connect",
		        address.c_str(), host->h_addrtype, host->h_length);
		return 0;
	}

	memset((char*)&c_in, 0, sizeof(c_in));
	memcpy((char*)&c_in.sin_addr, (char*)host->h_addr, host->h_length);
	c_in.sin_family = AF_INET;
	c_in.sin_port = htons(port);
	SDL_Log("ClientSocket: resolved '%s' to %s, connecting on port %d",
	        address.c_str(), inet_ntoa(c_in.sin_addr), port);

	{
		double first_failure = current_time();
		while(connect(s, (struct sockaddr *)&c_in, sizeof c_in) < 0)
		{
			if(current_time() - first_failure > 15.0)
			{
				printf("could not connect to '%s'\n", address.c_str());
				return 0;
			}
			// On BSD/macOS a failed connect() leaves the socket in a state
			// where subsequent connect()s on the same fd can hang. Close and
			// recreate before retrying.
#ifdef _WIN32
			closesocket(s);
#else
			close(s);
#endif
			s = -1;
			if(!CreateSocket()) return 0;
			uni_pause(50);
		}
	}
	
	shandler = new SocketHandler(s,c_in);
	
	event_list->push_back(new Event(OTHER_EVENT, CONNECT_EVENT, 0, NULL, 0));
	
	return 1;
}

int ClientSocket::CreateSocket()
{
	if(s != -1) return 1;
	
#ifdef _WIN32
	WSADATA wsaData;
	
	WSAStartup(0x202,&wsaData);
#endif
	
	if((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
	{
		printf("ClientSocket::CreateSocket:error in socket creation\n");
		return 0;
	}
	
	return 1;
}
