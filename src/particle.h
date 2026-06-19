/* Reusable particle system (simple particles for blood, sparks, etc.) */
#ifndef CHUCK_PARTICLE_H
#define CHUCK_PARTICLE_H

#include "common.h"

/*
 * Room for a rocket and the blood it spills at once.
 *
 * It was 64, and the blasts ask for more than that on their own — a rocket
 * requests 88, a gas canister 72 — so the first explosion on a floor took every
 * slot, the next hit that frame drew no blood at all, and the burst itself was
 * cut off wherever the array ran out. Nothing here is simulation: the system
 * lives in `PresentationState` and no gameplay module or test reads it.
 */
#define PS_MAX_PARTICLES 320

/*
 * What a particle is made of.
 *
 * Sparks and blood are thrown hard and fall like matter. Dust is the opposite:
 * it is kicked sideways off a surface, barely notices gravity, and fades rather
 * than lands. One system can carry both as long as it knows which it is
 * holding — a puff of floor dust drawn in blood red and arcing like a bullet
 * fragment is worse than no puff at all.
 *
 * A blast is five materials rather than one, because what sells an explosion
 * is that its parts behave differently: the flash is over before anything else
 * has moved, the fireball swells and stops, the embers streak out and cool,
 * the debris tumbles and drops, and the smoke arrives last and outlives the
 * rest. Painted as one kind — as they were — they are a handful of orange
 * squares falling through the floor together.
 */
typedef enum
{
  PARTICLE_SPARK = 0, /* blood and hit spray */
  PARTICLE_DUST,
  PARTICLE_FLASH,  /* a blast's first instant: its light, core and ring */
  PARTICLE_FIRE,   /* a fireball puff: white hot, flame, soot, smoke */
  PARTICLE_SMOKE,  /* what the blast leaves hanging in the room */
  PARTICLE_EMBER,  /* hot fragments that streak and cool */
  PARTICLE_DEBRIS, /* dark chunks that tumble and drop */
  PARTICLE_STUN,   /* a flash charge: white light and a ring, no fire */
  PARTICLE_GLINT   /* the burning magnesium it throws: white, cooling, blinking */
} ParticleKind;

typedef struct
{
  float x, y;
  float vx, vy;
  float life;     /* remaining life in seconds */
  float lifespan; /* what `life` started at, so a fade knows how far along it is */
  float size;     /* render size in pixels */
  /* Seconds it waits, unseen and unmoving, before its life starts — how the
   * smoke arrives after the fire that made it. */
  float delay;
  /* Where a falling fragment is taken to have reached the floor it was thrown
   * over. The system has no map, so this is an estimate from where the burst
   * was spawned; a fragment fades out across it rather than stopping on it,
   * which reads as landing where the estimate is right and as burning out
   * where it is not — never as a chip hanging on a floor that is not there. */
  float rest_y;
  /* A per-particle salt for flicker and tumble, handed out in spawn order so
   * the same run always draws the same burst. */
  unsigned seed;
  ParticleKind kind;
  bool active;
} Particle;

typedef struct
{
  Particle particles[PS_MAX_PARTICLES];
  unsigned serial; /* spawn counter the seeds are drawn from */
} ParticleSystem;

void particle_system_init(ParticleSystem *ps);
void particle_system_emit(ParticleSystem *ps, float x, float y, int count, int facing);
void particle_system_explosion(ParticleSystem *ps, float x, float y, int count);
/* A flash charge: the same instant of light a blast opens with, much whiter
 * and much wider, and then nothing but a few falling sparks and a pale haze —
 * no fireball, no soot, no debris, because nothing burned. */
void particle_system_flash(ParticleSystem *ps, float x, float y);
/* A puff off a surface. `spread` is how wide the contact was, in pixels. */
void particle_system_dust(ParticleSystem *ps, float x, float y, int count,
                          float spread);
void particle_system_update(ParticleSystem *ps, float dt);
void particle_system_render(ParticleSystem *ps, SDL_Renderer *r, float oy, float cam_x);
void particle_system_clear(ParticleSystem *ps);

#endif /* CHUCK_PARTICLE_H */
