#include "sdl3_mixer_compat.h"
#include <vector>

// One mixer device; SFX play on a pool of tracks (emulating SDL2_mixer
// "channels"); music gets its own dedicated track. SDL2 volumes were 0..128;
// MIX_ gains are 0.0..1.0 floats.
static MIX_Mixer            *g_mixer = NULL;
static std::vector<MIX_Track*> g_channels;
static MIX_Track            *g_music = NULL;

int Mix_Init(int flags)
{
    MIX_Init();
    return flags;
}

int Mix_OpenAudio(int /*frequency*/, Uint16 /*format*/, int /*channels*/, int /*chunksize*/)
{
    if (!MIX_Init()) return -1;
    g_mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (!g_mixer) return -1;
    g_music = MIX_CreateTrack(g_mixer);
    return 0;
}

void Mix_CloseAudio(void)
{
    for (MIX_Track *t : g_channels) if (t) MIX_DestroyTrack(t);
    g_channels.clear();
    if (g_music) { MIX_DestroyTrack(g_music); g_music = NULL; }
    if (g_mixer) { MIX_DestroyMixer(g_mixer); g_mixer = NULL; }
    MIX_Quit();
}

int Mix_QuerySpec(int *frequency, Uint16 *format, int *channels)
{
    // The engine only uses these loosely; report sane defaults.
    if (frequency) *frequency = 44100;
    if (format)    *format = AUDIO_S16;
    if (channels)  *channels = 2;
    return 1;
}

int Mix_AllocateChannels(int numchans)
{
    if (!g_mixer || numchans < 0) return 0;
    for (MIX_Track *t : g_channels) if (t) MIX_DestroyTrack(t);
    g_channels.assign(numchans, NULL);
    for (int i = 0; i < numchans; i++) g_channels[i] = MIX_CreateTrack(g_mixer);
    return numchans;
}

Mix_Chunk *Mix_LoadWAV(const char *file)
{
    if (!g_mixer) return NULL;
    return MIX_LoadAudio(g_mixer, file, true);   // predecode short SFX
}

Mix_Music *Mix_LoadMUS(const char *file)
{
    if (!g_mixer) return NULL;
    return MIX_LoadAudio(g_mixer, file, false);  // stream music
}

static bool play_track(MIX_Track *t, MIX_Audio *audio, int loops)
{
    if (!t || !audio) return false;
    if (!MIX_SetTrackAudio(t, audio)) return false;
    SDL_PropertiesID props = 0;
    if (loops != 0)   // SDL2: 0 = once, -1 = forever, N = repeat N times
    {
        props = SDL_CreateProperties();
        SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, loops);
    }
    bool ok = MIX_PlayTrack(t, props);
    if (props) SDL_DestroyProperties(props);
    return ok;
}

int Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops)
{
    if (!g_mixer) return -1;
    if (channel < 0)   // find a free channel
    {
        for (size_t i = 0; i < g_channels.size(); i++)
            if (g_channels[i] && !MIX_TrackPlaying(g_channels[i])) { channel = (int)i; break; }
        if (channel < 0) return -1;
    }
    if (channel >= (int)g_channels.size() || !g_channels[channel]) return -1;
    return play_track(g_channels[channel], chunk, loops) ? channel : -1;
}

int Mix_HaltChannel(int channel)
{
    if (channel < 0) { for (MIX_Track *t : g_channels) if (t) MIX_StopTrack(t, 0); return 0; }
    if (channel < (int)g_channels.size() && g_channels[channel]) MIX_StopTrack(g_channels[channel], 0);
    return 0;
}

int Mix_Playing(int channel)
{
    if (channel < 0)
    {
        int n = 0;
        for (MIX_Track *t : g_channels) if (t && MIX_TrackPlaying(t)) n++;
        return n;
    }
    if (channel < (int)g_channels.size() && g_channels[channel])
        return MIX_TrackPlaying(g_channels[channel]) ? 1 : 0;
    return 0;
}

int Mix_Volume(int channel, int volume)
{
    float gain = (volume < 0) ? 1.0f : (volume / (float)MIX_MAX_VOLUME);
    if (channel < 0) { for (MIX_Track *t : g_channels) if (t) MIX_SetTrackGain(t, gain); }
    else if (channel < (int)g_channels.size() && g_channels[channel])
        MIX_SetTrackGain(g_channels[channel], gain);
    return volume;
}

int Mix_PlayMusic(Mix_Music *music, int loops)
{
    return play_track(g_music, music, loops) ? 0 : -1;
}

int Mix_VolumeMusic(int volume)
{
    if (g_music) MIX_SetTrackGain(g_music, volume < 0 ? 1.0f : volume / (float)MIX_MAX_VOLUME);
    return volume;
}

int Mix_SetMusicPosition(double position)
{
    if (!g_music) return -1;
    Sint64 frames = MIX_TrackMSToFrames(g_music, (Sint64)(position * 1000.0));
    return MIX_SetTrackPlaybackPosition(g_music, frames) ? 0 : -1;
}
