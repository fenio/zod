#ifndef _ZAUDIO_H_
#define _ZAUDIO_H_

// Native SDL3 audio for the Zod Engine, over SDL3_mixer's track-based MIX_ API.
//
// The engine was written against SDL2_mixer's chunk/channel/music model; SDL3_
// mixer (3.x) replaced that with a track-based MIX_ API. This module presents
// the small channel-based surface the engine needs (load a sound, play it on a
// channel pool, stream one music track) on top of the new API. Sound effects
// play on a pool of tracks emulating SDL2_mixer "channels"; music gets its own
// dedicated track. Volumes are the engine's legacy 0..ZAUDIO_MAX_VOLUME ints;
// MIX_ gains are 0.0..1.0 floats, mapped here.

#include <SDL3_mixer/SDL_mixer.h>

// Loaded audio handles. Both are a MIX_Audio underneath; the two names keep the
// engine's sound-effect vs. music distinction readable.
typedef MIX_Audio ZAudio_Sound;
typedef MIX_Audio ZAudio_Music;

#define ZAUDIO_MAX_VOLUME 128

bool          ZAudio_Init(void);
bool          ZAudio_Open(int frequency, Uint16 format, int channels, int chunksize);
void          ZAudio_Close(void);
bool          ZAudio_QuerySpec(int *frequency, Uint16 *format, int *channels);
int           ZAudio_AllocateChannels(int numchans);
ZAudio_Sound *ZAudio_LoadSound(const char *file);
ZAudio_Music *ZAudio_LoadMusic(const char *file);
int           ZAudio_PlayChannel(int channel, ZAudio_Sound *sound, int loops);
void          ZAudio_HaltChannel(int channel);
bool          ZAudio_ChannelPlaying(int channel);
void          ZAudio_SetChannelVolume(int channel, int volume);
bool          ZAudio_PlayMusic(ZAudio_Music *music, int loops);
void          ZAudio_SetMusicVolume(int volume);
bool          ZAudio_SetMusicPosition(double position);

#endif
