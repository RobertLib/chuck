#ifndef CHUCK_AUDIO_H
#define CHUCK_AUDIO_H

#include "common.h"
#include "sound_id.h"

#define AUDIO_VOICE_COUNT 16

/*
 * One score per level theme, so a sector sounds like the floor it is on.
 * `level_theme_music` in level_art.c owns the theme-to-track mapping; because
 * it is one to one, two consecutive sectors can never share a score — the
 * themes already never repeat back to back.
 */
typedef enum
{
    MUSIC_INTRO = 0, /* title screen, cutscenes and the game-over card */
    MUSIC_PURSUIT,   /* the prologue drive */
    MUSIC_LOBBY,
    MUSIC_OFFICE,
    MUSIC_SERVER,
    MUSIC_PLANT,
    MUSIC_CANTEEN,
    MUSIC_LAB,
    MUSIC_ARCHIVE,
    MUSIC_SECURITY,
    MUSIC_DUCTS,
    MUSIC_PENTHOUSE,
    MUSIC_ROOF,
    MUSIC_RESTROOM,
    MUSIC_FACADE_NIGHT,
    MUSIC_FACADE_STORM,
    MUSIC_FACADE_MOON,
    MUSIC_FACADE_HIGH,
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
     * Eighteen forty-second loops resident at once would be most of the
     * process's memory, so a track is built when it is first asked for and
     * only the title theme, the current one and the one before it are kept.
     */
    CachedSound music_tracks[MUSIC_TRACK_COUNT];
    Uint64 last_play_ns[SFX_COUNT];
    int next_voice;
    int current_music;
    int previous_music;
    /*
     * The player's two levels, 0 to 1, sitting on top of the mix rather than
     * inside it: every effect and every score is still built and balanced at
     * the gain it was written with, and these only scale what leaves. They are
     * separate buses because a score under a conversation and a shot that
     * still has to be heard are two different decisions, and one knob cannot
     * make both of them.
     */
    float music_volume;
    float sfx_volume;
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

/* The two levels the options sheet owns, each 0 to 1. Applied immediately, so
 * a slider is heard while it is being moved rather than at the next track. */
void audio_set_volumes(AudioSystem *audio, float music, float sfx);

/* The one key that silences everything at once, on top of both levels. It is
 * deliberately not saved: a kill switch is a thing you reach for during a
 * phone call, and a game that starts silent with nothing on screen explaining
 * why is a game the player thinks is broken. */
void audio_toggle_mute(AudioSystem *audio);

/* Whether that switch is currently down.
 *
 * The options sheet is the one screen in the game that shows the mix, and a
 * kill switch sitting on top of the mix is part of what the mix currently is:
 * a sheet reading MUSIC 100 over a silent game is wrong however the silence
 * was arrived at. Keeping M off that sheet stopped it being *created* there
 * and did nothing about a player who muted first and opened the sheet second,
 * which is one keystroke and the more likely order. So the sheet asks. */
bool audio_is_muted(const AudioSystem *audio);

#endif /* CHUCK_AUDIO_H */
