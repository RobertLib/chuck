/* Reusable particle system (simple particles for blood, sparks, etc.) */
#ifndef CHUCK_PARTICLE_H
#define CHUCK_PARTICLE_H

#include "common.h"

#define PS_MAX_PARTICLES 64

typedef struct
{
  float x, y;
  float vx, vy;
  float life; /* remaining life in seconds */
  float size; /* render size in pixels */
  bool active;
} Particle;

typedef struct
{
  Particle particles[PS_MAX_PARTICLES];
} ParticleSystem;

void particle_system_init(ParticleSystem *ps);
void particle_system_emit(ParticleSystem *ps, float x, float y, int count, int facing);
void particle_system_explosion(ParticleSystem *ps, float x, float y, int count);
void particle_system_update(ParticleSystem *ps, float dt);
void particle_system_render(ParticleSystem *ps, SDL_Renderer *r, float oy, float cam_x);
void particle_system_clear(ParticleSystem *ps);

#endif /* CHUCK_PARTICLE_H */
