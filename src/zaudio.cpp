#include "zaudio.h"
#include <vector>

// One mixer device; SFX play on a pool of tracks (emulating SDL2_mixer
// "channels"); music gets its own dedicated track. The engine's volumes are
// 0..ZAUDIO_MAX_VOLUME ints; MIX_ gains are 0.0..1.0 floats.
static MIX_Mixer              *g_mixer = NULL;
static std::vector<MIX_Track*> g_channels;
static MIX_Track              *g_music = NULL;

bool ZAudio_Init(void)
{
    return MIX_Init();
}

bool ZAudio_Open(int /*frequency*/, Uint16 /*format*/, int /*channels*/, int /*chunksize*/)
{
    if (!MIX_Init()) return false;
    g_mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (!g_mixer) return false;
    g_music = MIX_CreateTrack(g_mixer);
    return true;
}

void ZAudio_Close(void)
{
    for (MIX_Track *t : g_channels) if (t) MIX_DestroyTrack(t);
    g_channels.clear();
    if (g_music) { MIX_DestroyTrack(g_music); g_music = NULL; }
    if (g_mixer) { MIX_DestroyMixer(g_mixer); g_mixer = NULL; }
    MIX_Quit();
}

bool ZAudio_QuerySpec(int *frequency, Uint16 *format, int *channels)
{
    // The engine only uses these loosely; report sane defaults.
    if (frequency) *frequency = 44100;
    if (format)    *format = SDL_AUDIO_S16LE;
    if (channels)  *channels = 2;
    return true;
}

int ZAudio_AllocateChannels(int numchans)
{
    if (!g_mixer || numchans < 0) return 0;
    for (MIX_Track *t : g_channels) if (t) MIX_DestroyTrack(t);
    g_channels.assign(numchans, NULL);
    for (int i = 0; i < numchans; i++) g_channels[i] = MIX_CreateTrack(g_mixer);
    return numchans;
}

ZAudio_Sound *ZAudio_LoadSound(const char *file)
{
    if (!g_mixer) return NULL;
    return MIX_LoadAudio(g_mixer, file, true);   // predecode short SFX
}

ZAudio_Music *ZAudio_LoadMusic(const char *file)
{
    if (!g_mixer) return NULL;
    return MIX_LoadAudio(g_mixer, file, false);  // stream music
}

static bool play_track(MIX_Track *t, MIX_Audio *audio, int loops)
{
    if (!t || !audio) return false;
    if (!MIX_SetTrackAudio(t, audio)) return false;
    SDL_PropertiesID props = 0;
    if (loops != 0)   // legacy: 0 = once, -1 = forever, N = repeat N times
    {
        props = SDL_CreateProperties();
        SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, loops);
    }
    bool ok = MIX_PlayTrack(t, props);
    if (props) SDL_DestroyProperties(props);
    return ok;
}

int ZAudio_PlayChannel(int channel, ZAudio_Sound *sound, int loops)
{
    if (!g_mixer) return -1;
    if (channel < 0)   // find a free channel
    {
        for (size_t i = 0; i < g_channels.size(); i++)
            if (g_channels[i] && !MIX_TrackPlaying(g_channels[i])) { channel = (int)i; break; }
        if (channel < 0) return -1;
    }
    if (channel >= (int)g_channels.size() || !g_channels[channel]) return -1;
    return play_track(g_channels[channel], sound, loops) ? channel : -1;
}

void ZAudio_HaltChannel(int channel)
{
    if (channel < 0) { for (MIX_Track *t : g_channels) if (t) MIX_StopTrack(t, 0); return; }
    if (channel < (int)g_channels.size() && g_channels[channel]) MIX_StopTrack(g_channels[channel], 0);
}

bool ZAudio_ChannelPlaying(int channel)
{
    if (channel < 0)
    {
        for (MIX_Track *t : g_channels) if (t && MIX_TrackPlaying(t)) return true;
        return false;
    }
    if (channel < (int)g_channels.size() && g_channels[channel])
        return MIX_TrackPlaying(g_channels[channel]);
    return false;
}

void ZAudio_SetChannelVolume(int channel, int volume)
{
    float gain = (volume < 0) ? 1.0f : (volume / (float)ZAUDIO_MAX_VOLUME);
    if (channel < 0) { for (MIX_Track *t : g_channels) if (t) MIX_SetTrackGain(t, gain); }
    else if (channel < (int)g_channels.size() && g_channels[channel])
        MIX_SetTrackGain(g_channels[channel], gain);
}

bool ZAudio_PlayMusic(ZAudio_Music *music, int loops)
{
    return play_track(g_music, music, loops);
}

void ZAudio_SetMusicVolume(int volume)
{
    if (g_music) MIX_SetTrackGain(g_music, volume < 0 ? 1.0f : volume / (float)ZAUDIO_MAX_VOLUME);
}

bool ZAudio_SetMusicPosition(double position)
{
    if (!g_music) return false;
    Sint64 frames = MIX_TrackMSToFrames(g_music, (Sint64)(position * 1000.0));
    return MIX_SetTrackPlaybackPosition(g_music, frames);
}
