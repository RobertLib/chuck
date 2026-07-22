#include "game_event.h"

#include <stddef.h>

static GameEvent *next_event(GameEventBuffer *events, GameEventType type)
{
    if (events->count >= MAX_GAME_EVENTS)
    {
        events->overflowed = true;
        return NULL;
    }
    GameEvent *event = &events->items[events->count++];
    event->type = type;
    return event;
}

void game_events_clear(GameEventBuffer *events)
{
    events->count = 0;
    events->overflowed = false;
}

bool game_events_sound(GameEventBuffer *events, SoundEffect effect)
{
    GameEvent *event = next_event(events, GAME_EVENT_SOUND);
    if (event == NULL)
        return false;
    event->data.sound.effect = effect;
    event->data.sound.x = 0.0f;
    event->data.sound.y = 0.0f;
    return true;
}

bool game_events_world_sound(GameEventBuffer *events, SoundEffect effect,
                             float x, float y)
{
    GameEvent *event = next_event(events, GAME_EVENT_WORLD_SOUND);
    if (event == NULL)
        return false;
    event->data.sound.effect = effect;
    event->data.sound.x = x;
    event->data.sound.y = y;
    return true;
}

bool game_events_particles(GameEventBuffer *events, float x, float y,
                           int count, int direction)
{
    GameEvent *event = next_event(events, GAME_EVENT_PARTICLES);
    if (event == NULL)
        return false;
    event->data.particles.x = x;
    event->data.particles.y = y;
    event->data.particles.count = count;
    event->data.particles.direction = direction;
    return true;
}

bool game_events_explosion(GameEventBuffer *events, float x, float y,
                           int count)
{
    GameEvent *event = next_event(events, GAME_EVENT_EXPLOSION);
    if (event == NULL)
        return false;
    event->data.explosion.x = x;
    event->data.explosion.y = y;
    event->data.explosion.count = count;
    return true;
}

bool game_events_camera_shake(GameEventBuffer *events, float strength,
                              float duration)
{
    GameEvent *event = next_event(events, GAME_EVENT_CAMERA_SHAKE);
    if (event == NULL)
        return false;
    event->data.shake.strength = strength;
    event->data.shake.duration = duration;
    return true;
}
