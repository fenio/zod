#ifndef _PACKET_IO_H_
#define _PACKET_IO_H_

#include <string.h>

// Alignment-safe accessors for the 32-bit ints in network packet buffers.
//
// The packet code used to do `((int*)buf)[i]` / `*(int*)(buf+n)`. Those packet
// buffers arrive off the socket at arbitrary byte offsets, so the int* they
// produce is frequently misaligned - undefined behavior (flagged by UBSan),
// and an actual fault on stricter architectures. memcpy compiles to the same
// load/store but is defined for any alignment, and the byte layout is identical
// to the old casts, so the on-wire format is unchanged.
//
// Offsets are in BYTES (an old `[i]` becomes offset i*4).

static inline int PacketGetInt(const char *buf, int byte_offset)
{
	int v;
	memcpy(&v, buf + byte_offset, sizeof(v));
	return v;
}

static inline void PacketSetInt(char *buf, int byte_offset, int v)
{
	memcpy(buf + byte_offset, &v, sizeof(v));
}

#endif
