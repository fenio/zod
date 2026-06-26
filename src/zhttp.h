#ifndef _ZHTTP_H_
#define _ZHTTP_H_

#include <string>

// Minimal blocking HTTP/1.1 client, just enough to talk to the local match
// orchestrator. PoC-grade: localhost JSON, "Connection: close" so we can read
// until EOF (no Content-Length parsing), and a short timeout so a down
// orchestrator can't wedge the caller. Not a general-purpose HTTP client.
struct ZHttpResponse
{
	bool ok;          // a response line was received and parsed
	int status;       // HTTP status (e.g. 200), or 0 if the request failed outright
	std::string body; // response body (the JSON)
};

ZHttpResponse ZHttp_Get(const std::string &host, int port, const std::string &path);
ZHttpResponse ZHttp_Post(const std::string &host, int port, const std::string &path,
                         const std::string &json_body);

#endif
