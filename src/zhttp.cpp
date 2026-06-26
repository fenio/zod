#include "zhttp.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include <cstring>
#include <cstdio>
#include <cstdlib>

using namespace std;

#ifdef _WIN32
typedef SOCKET zsock_t;
#define ZSOCK_BAD INVALID_SOCKET
static void zsock_close(zsock_t s) { closesocket(s); }
#else
typedef int zsock_t;
#define ZSOCK_BAD (-1)
static void zsock_close(zsock_t s) { close(s); }
#endif

// Resolve an IPv4 host (dotted-quad or name) into a sockaddr_in.
static bool resolve_host(const string &host, int port, struct sockaddr_in &out)
{
	memset(&out, 0, sizeof(out));
	out.sin_family = AF_INET;
	out.sin_port = htons((unsigned short)port);

	unsigned long ip = inet_addr(host.c_str());
	if(ip != INADDR_NONE)
	{
		out.sin_addr.s_addr = ip;
		return true;
	}

	struct hostent *he = gethostbyname(host.c_str());
	if(!he || he->h_addrtype != AF_INET || he->h_length != 4) return false;
	memcpy(&out.sin_addr, he->h_addr_list[0], 4);
	return true;
}

// Cap blocking time so a down orchestrator can't wedge the UI thread.
static void set_timeout(zsock_t s, int ms)
{
#ifdef _WIN32
	DWORD tv = ms;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
	setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#else
	struct timeval tv;
	tv.tv_sec = ms / 1000;
	tv.tv_usec = (ms % 1000) * 1000;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
	setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#endif
}

static ZHttpResponse do_request(const char *method, const string &host, int port,
                                const string &path, const string &body)
{
	ZHttpResponse resp;
	resp.ok = false;
	resp.status = 0;

	struct sockaddr_in addr;
	if(!resolve_host(host, port, addr)) return resp;

	zsock_t s = socket(AF_INET, SOCK_STREAM, 0);
	if(s == ZSOCK_BAD) return resp;
	set_timeout(s, 2000);

	if(connect(s, (struct sockaddr*)&addr, sizeof(addr)) != 0) { zsock_close(s); return resp; }

	// "Connection: close" -> the server closes after responding, so we can read
	// the whole body until EOF without parsing Content-Length / chunking.
	char hdr[600];
	int hn = snprintf(hdr, sizeof(hdr),
		"%s %s HTTP/1.1\r\n"
		"Host: %s:%d\r\n"
		"Connection: close\r\n"
		"Content-Type: application/json\r\n"
		"Content-Length: %d\r\n\r\n",
		method, path.c_str(), host.c_str(), port, (int)body.size());
	if(hn <= 0) { zsock_close(s); return resp; }

	string req(hdr, (size_t)hn);
	req += body;

	if(send(s, req.data(), (int)req.size(), 0) < 0) { zsock_close(s); return resp; }

	string raw;
	char buf[2048];
	for(;;)
	{
		int r = (int)recv(s, buf, sizeof(buf), 0);
		if(r <= 0) break;
		raw.append(buf, (size_t)r);
	}
	zsock_close(s);

	if(raw.empty()) return resp;

	// status line: "HTTP/1.1 200 OK"
	size_t sp = raw.find(' ');
	if(sp != string::npos) resp.status = atoi(raw.c_str() + sp + 1);

	// body follows the blank line
	size_t hdr_end = raw.find("\r\n\r\n");
	if(hdr_end != string::npos) resp.body = raw.substr(hdr_end + 4);

	resp.ok = (resp.status > 0);
	return resp;
}

ZHttpResponse ZHttp_Get(const string &host, int port, const string &path)
{
	return do_request("GET", host, port, path, "");
}

ZHttpResponse ZHttp_Post(const string &host, int port, const string &path, const string &json_body)
{
	return do_request("POST", host, port, path, json_body);
}
