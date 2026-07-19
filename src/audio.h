#ifndef CHUCK_AUDIO_H
#define CHUCK_AUDIO_H

#include "common.h"

#define AUDIO_VOICE_COUNT 16

typedef enum
{
    MUSIC_INTRO = 0,
    MUSIC_LEVEL_ONE,
    MUSIC_LEVEL_TWO,
    MUSIC_TRACK_COUNT
} MusicTrack;

/*
 * Every effect is synthesised once during audio_init() and kept as a PCM
 * buffer. Playing an effect only queues the cached buffer on a free voice.
 */
typedef enum
{
    SFX_MENU_START = 0,
    SFX_MENU_BACK,
    SFX_OPENING_RAIN,
    SFX_OPENING_SUV_ENGINE,
    SFX_OPENING_CAR_ENGINE,
    SFX_OPENING_BRAKE,
    SFX_OPENING_CAR_DOOR,
    SFX_REVEAL_TICK,
    SFX_CARD_SCAN,
    SFX_CARD_TARGET,
    SFX_JUMP,
    SFX_LAND,
    SFX_STEP_A,
    SFX_STEP_B,
    SFX_LADDER,
    SFX_DOOR,
    SFX_ELEVATOR,
    SFX_PLATFORM_CRACK,
    SFX_PLAYER_SHOT,
    SFX_EMPTY_CLICK,
    SFX_ENEMY_ALERT,
    SFX_ENEMY_SHOT,
    SFX_BULLET_IMPACT,
    SFX_ENEMY_HIT,
    SFX_ENEMY_DOWN,
    SFX_GUARD_TALK,
    SFX_DOG_BARK,
    SFX_DOG_BARK_ALT,
    SFX_DOG_GROWL,
    SFX_DOG_BITE,
    SFX_DOG_YELP,
    SFX_GRENADE_THROW,
    SFX_GRENADE_BOUNCE,
    SFX_EXPLOSION,
    SFX_MINE_ARM,
    SFX_CRATE_BREAK,
    SFX_PICKUP_AMMO,
    SFX_PICKUP_GRENADE,
    SFX_PICKUP_HEALTH,
    SFX_CARD_WRONG,
    SFX_EXIT_UNLOCKED,
    SFX_PLAYER_HIT,
    SFX_RESPAWN,
    SFX_LEVEL_CLEAR,
    SFX_GAME_OVER,
    SFX_WIN,
    SFX_COUNT
} SoundEffect;

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
