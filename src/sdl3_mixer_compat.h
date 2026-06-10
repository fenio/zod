#ifndef _SDL3_MIXER_COMPAT_H_
#define _SDL3_MIXER_COMPAT_H_

// SDL3_mixer (3.x) replaced SDL2_mixer's chunk/channel/music API with a new
// track-based MIX_* API. This shim re-implements the small SDL2_mixer surface
// the Zod Engine uses on top of the new API, so the audio engines run
// unchanged. Both legacy handle types are just MIX_Audio underneath.

#include <SDL3_mixer/SDL_mixer.h>

typedef MIX_Audio Mix_Chunk;
typedef MIX_Audio Mix_Music;

// Legacy constants used by the codebase (the old init flags are now no-ops).
#undef AUDIO_S16
#ifndef AUDIO_S16
#define AUDIO_S16 SDL_AUDIO_S16LE
#endif
#ifndef MIX_INIT_MOD
#define MIX_INIT_MOD 0
#endif
#ifndef MIX_INIT_OGG
#define MIX_INIT_OGG 0
#endif
#ifndef MIX_MAX_VOLUME
#define MIX_MAX_VOLUME 128
#endif

// Mix_GetError was always just SDL_GetError.
#define Mix_GetError SDL_GetError

int        Mix_Init(int flags);
int        Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize);
void       Mix_CloseAudio(void);
int        Mix_QuerySpec(int *frequency, Uint16 *format, int *channels);
int        Mix_AllocateChannels(int numchans);
Mix_Chunk *Mix_LoadWAV(const char *file);
Mix_Music *Mix_LoadMUS(const char *file);
int        Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops);
int        Mix_HaltChannel(int channel);
int        Mix_Playing(int channel);
int        Mix_Volume(int channel, int volume);
int        Mix_PlayMusic(Mix_Music *music, int loops);
int        Mix_VolumeMusic(int volume);
int        Mix_SetMusicPosition(double position);

#endif
