#include "particle.h"

#include <math.h>

void particle_system_init(ParticleSystem *ps)
{
  for (int i = 0; i < PS_MAX_PARTICLES; ++i)
    ps->particles[i].active = false;
}

static float frand_range(float a, float b)
{
  return a + (b - a) * ((float)SDL_rand(10001) * 0.00009999f);
}

void particle_system_emit(ParticleSystem *ps, float x, float y, int count, int facing)
{
  if (count <= 0)
    return;

  /* Emit particles in a wide spread around the player so splatter covers
   * both sides; apply a small forward bias based on facing to keep feel. */
  for (int i = 0; i < PS_MAX_PARTICLES && count > 0; ++i)
  {
    if (!ps->particles[i].active)
    {
      Particle *p = &ps->particles[i];
      p->active = true;
      p->x = x + frand_range(-4.0f, 4.0f);
      p->y = y + frand_range(-4.0f, 4.0f);
      /* Full-circle angle, but bias downward and slightly toward facing */
      float ang = frand_range(-3.14159265f, 3.14159265f);
      float sp = frand_range(20.0f, 80.0f);
      p->vx = cosf(ang) * sp + (facing * frand_range(-10.0f, 20.0f));
      p->vy = sinf(ang) * sp * 0.6f - frand_range(10.0f, 30.0f);
      p->life = frand_range(0.35f, 0.9f);
      p->size = frand_range(2.0f, 4.0f);
      --count;
    }
  }
}

void particle_system_update(ParticleSystem *ps, float dt)
{
  for (int i = 0; i < PS_MAX_PARTICLES; ++i)
  {
    Particle *p = &ps->particles[i];
    if (!p->active)
      continue;
    p->vy += GRAVITY * dt * 0.6f;
    p->x += p->vx * dt;
    p->y += p->vy * dt;
    p->life -= dt;
    if (p->life <= 0.0f)
      p->active = false;
  }
}

void particle_system_render(ParticleSystem *ps, SDL_Renderer *r, float oy)
{
  for (int i = 0; i < PS_MAX_PARTICLES; ++i)
  {
    Particle *p = &ps->particles[i];
    if (!p->active)
      continue;
    SDL_SetRenderDrawColor(r, 180, 20, 20, 255);
    SDL_FRect rect = {p->x - p->size * 0.5f, p->y + oy - p->size * 0.5f, p->size, p->size};
    SDL_RenderFillRect(r, &rect);
  }
}

void particle_system_clear(ParticleSystem *ps)
{
  for (int i = 0; i < PS_MAX_PARTICLES; ++i)
    ps->particles[i].active = false;
}
