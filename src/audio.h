#ifndef CHUCK_AUDIO_H
#define CHUCK_AUDIO_H

#include "common.h"
#include "sound_id.h"

#define AUDIO_VOICE_COUNT 16

typedef enum
{
    MUSIC_INTRO = 0,
    MUSIC_LEVEL_ONE,
    MUSIC_LEVEL_TWO,
    MUSIC_TRACK_COUNT
} MusicTrack;

typedef struct
{
    float *samples;
    int frame_count;
    float gain;
    Uint32 min_gap_ms;
} CachedSound;

typedef struct
{
    SDL_AudioDeviceID device;
    SDL_AudioStream *voices[AUDIO_VOICE_COUNT];
    SDL_AudioStream *music_stream;
    Uint64 voice_end_ns[AUDIO_VOICE_COUNT];
    CachedSound sounds[SFX_COUNT];
    /*
     * Music uses the same cached PCM representation as effects, but is
     * streamed and looped independently so it never consumes an SFX voice.
     */
    CachedSound music_tracks[MUSIC_TRACK_COUNT];
    Uint64 last_play_ns[SFX_COUNT];
    int next_voice;
    int current_music;
    bool subsystem_initialized;
    bool ready;
    bool muted;
} AudioSystem;

/* Audio failure is non-fatal: the game can continue silently. */
bool audio_init(AudioSystem *audio);
void audio_shutdown(AudioSystem *audio);

void audio_play(AudioSystem *audio, SoundEffect effect);
void audio_play_at(AudioSystem *audio, SoundEffect effect,
                   float source_x, float source_y,
                   float listener_x, float listener_y);
void audio_play_music(AudioSystem *audio, MusicTrack track);
void audio_stop_music(AudioSystem *audio);
void audio_stop_effects(AudioSystem *audio);
void audio_update_music(AudioSystem *audio);
void audio_toggle_mute(AudioSystem *audio);

#endif /* CHUCK_AUDIO_H */
