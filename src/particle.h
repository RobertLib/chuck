/* Reusable particle system (simple particles for blood, sparks, etc.) */
#ifndef CHUCK_PARTICLE_H
#define CHUCK_PARTICLE_H

#include "common.h"

#define PS_MAX_PARTICLES 64

/*
 * What a particle is made of.
 *
 * Sparks and blood are thrown hard and fall like matter. Dust is the opposite:
 * it is kicked sideways off a surface, barely notices gravity, and fades rather
 * than lands. One system can carry both as long as it knows which it is
 * holding — a puff of floor dust drawn in blood red and arcing like a bullet
 * fragment is worse than no puff at all.
 */
typedef enum
{
  PARTICLE_SPARK = 0,
  PARTICLE_DUST
} ParticleKind;

typedef struct
{
  float x, y;
  float vx, vy;
  float life;     /* remaining life in seconds */
  float lifespan; /* what `life` started at, so a fade knows how far along it is */
  float size;     /* render size in pixels */
  ParticleKind kind;
  bool active;
} Particle;

typedef struct
{
  Particle particles[PS_MAX_PARTICLES];
} ParticleSystem;

void particle_system_init(ParticleSystem *ps);
void particle_system_emit(ParticleSystem *ps, float x, float y, int count, int facing);
void particle_system_explosion(ParticleSystem *ps, float x, float y, int count);
/* A puff off a surface. `spread` is how wide the contact was, in pixels. */
void particle_system_dust(ParticleSystem *ps, float x, float y, int count,
                          float spread);
void particle_system_update(ParticleSystem *ps, float dt);
void particle_system_render(ParticleSystem *ps, SDL_Renderer *r, float oy, float cam_x);
void particle_system_clear(ParticleSystem *ps);

#endif /* CHUCK_PARTICLE_H */
