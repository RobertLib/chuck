#ifndef CHUCK_GAME_EVENT_H
#define CHUCK_GAME_EVENT_H

#include "sound_id.h"

#include <stdbool.h>

#define MAX_GAME_EVENTS 128

typedef enum
{
    GAME_EVENT_SOUND,
    GAME_EVENT_WORLD_SOUND,
    GAME_EVENT_PARTICLES,
    GAME_EVENT_EXPLOSION,
    /* Masonry dust rather than sparks: what comes off a surface, not out of
     * something. Blood-red fragments arcing away from a broken wall would read
     * as the wrong material however many of them there were. */
    GAME_EVENT_DUST,
    GAME_EVENT_CAMERA_SHAKE,
    /* Somebody on the crew said something out loud. The simulation reports
     * that it happened, who and where; the words are the shell's, exactly the
     * way a waveform behind a SoundEffect is. */
    GAME_EVENT_CHATTER
} GameEventType;

/* What prompted the line, which is the whole of what the simulation knows
 * about it. The shell keeps one table per kind. */
typedef enum
{
    CHATTER_RADIO = 0, /* a man on his own, into a handset */
    CHATTER_TALK,      /* two of them standing together */
    CHATTER_ALARM,     /* the beat a wall switch goes over */
    CHATTER_WALL,      /* shouted out of a window on the facade */
    /* Not the crew: somebody who works here, running for the street. The one
     * kind with no callsign on it — the people in the lobby are not on
     * anybody's docket, and putting a name on them would file them as staff
     * the player is supposed to keep track of. */
    CHATTER_PANIC,
    CHATTER_KIND_COUNT
} ChatterKind;

typedef struct
{
    GameEventType type;
    union
    {
        struct
        {
            SoundEffect effect;
            float x;
            float y;
        } sound;
        struct
        {
            float x;
            float y;
            int count;
            int direction;
        } particles;
        struct
        {
            float x;
            float y;
            int count;
        } explosion;
        struct
        {
            float x;
            float y;
            int count;
            float spread; /* how wide the surface it came off was, in pixels */
        } dust;
        struct
        {
            float strength;
            float duration;
        } shake;
        struct
        {
            float x;
            float y;
            /* Enemy slot, so the shell can put a name on the line. On the
             * facade there is no enemy array and this is the window index
             * instead — the men out there are the same crew either way. */
            int speaker;
            ChatterKind kind;
            /* An opaque draw off the level RNG. Which line it selects is a
             * decision the shell makes and the simulation must not have an
             * opinion about, or every new line of flavour text would be a
             * change to a deterministic gameplay module. */
            int roll;
        } chatter;
    } data;
} GameEvent;

typedef struct
{
    GameEvent items[MAX_GAME_EVENTS];
    int count;
    bool overflowed;
} GameEventBuffer;

void game_events_clear(GameEventBuffer *events);
bool game_events_sound(GameEventBuffer *events, SoundEffect effect);
bool game_events_world_sound(GameEventBuffer *events, SoundEffect effect,
                             float x, float y);
bool game_events_particles(GameEventBuffer *events, float x, float y,
                           int count, int direction);
bool game_events_explosion(GameEventBuffer *events, float x, float y,
                           int count);
bool game_events_dust(GameEventBuffer *events, float x, float y, int count,
                      float spread);
bool game_events_camera_shake(GameEventBuffer *events, float strength,
                              float duration);
bool game_events_chatter(GameEventBuffer *events, float x, float y,
                         int speaker, ChatterKind kind, int roll);

#endif /* CHUCK_GAME_EVENT_H */
