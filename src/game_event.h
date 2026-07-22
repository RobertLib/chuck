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
    GAME_EVENT_CAMERA_SHAKE
} GameEventType;

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
            float strength;
            float duration;
        } shake;
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
bool game_events_camera_shake(GameEventBuffer *events, float strength,
                              float duration);

#endif /* CHUCK_GAME_EVENT_H */
