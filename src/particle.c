#include "particle.h"

#include "fx.h"

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
      p->kind = PARTICLE_SPARK;
      p->x = x + frand_range(-4.0f, 4.0f);
      p->y = y + frand_range(-4.0f, 4.0f);
      /* Full-circle angle, but bias downward and slightly toward facing */
      float ang = frand_range(-3.14159265f, 3.14159265f);
      float sp = frand_range(20.0f, 80.0f);
      p->vx = cosf(ang) * sp + (facing * frand_range(-10.0f, 20.0f));
      p->vy = sinf(ang) * sp * 0.6f - frand_range(10.0f, 30.0f);
      p->life = frand_range(0.35f, 0.9f);
      p->lifespan = p->life;
      p->size = frand_range(2.0f, 4.0f);
      --count;
    }
  }
}

void particle_system_explosion(ParticleSystem *ps, float x, float y, int count)
{
  if (count <= 0)
    return;

  for (int i = 0; i < PS_MAX_PARTICLES && count > 0; ++i)
  {
    if (!ps->particles[i].active)
    {
      Particle *p = &ps->particles[i];
      p->active = true;
      p->kind = PARTICLE_SPARK;
      p->x = x + frand_range(-6.0f, 6.0f);
      p->y = y + frand_range(-6.0f, 6.0f);
      float ang = frand_range(-3.14159265f, 3.14159265f);
      float sp = frand_range(60.0f, 220.0f);
      p->vx = cosf(ang) * sp;
      p->vy = sinf(ang) * sp * 0.6f - frand_range(20.0f, 60.0f);
      p->life = frand_range(0.35f, 1.1f);
      p->lifespan = p->life;
      p->size = frand_range(3.0f, 7.0f);
      --count;
    }
  }
}

/*
 * Dust off a floor.
 *
 * It leaves sideways rather than upward, because what throws it is a boot
 * pushing air out from under itself, and it starts along the whole width of the
 * contact rather than at one point — a puff from a single pixel reads as a
 * spark however it is coloured.
 */
void particle_system_dust(ParticleSystem *ps, float x, float y, int count,
                          float spread)
{
  if (count <= 0)
    return;

  float half = spread * 0.5f;
  for (int i = 0; i < PS_MAX_PARTICLES && count > 0; ++i)
  {
    if (!ps->particles[i].active)
    {
      Particle *p = &ps->particles[i];
      float side = frand_range(-half, half);
      p->active = true;
      p->kind = PARTICLE_DUST;
      p->x = x + side;
      p->y = y + frand_range(-1.0f, 1.0f);
      /* Outward from the middle of the contact, faster the further out it
       * starts, with just enough rise to clear the floor. */
      p->vx = (side >= 0.0f ? 1.0f : -1.0f) * frand_range(14.0f, 46.0f);
      p->vy = -frand_range(6.0f, 22.0f);
      p->life = frand_range(0.30f, 0.55f);
      p->lifespan = p->life;
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
    if (p->kind == PARTICLE_DUST)
    {
      /* Dust hangs: almost no weight, and the air takes the speed out of it. */
      p->vy += GRAVITY * dt * 0.08f;
      p->vx -= p->vx * fminf(1.0f, dt * 3.4f);
    }
    else
    {
      p->vy += GRAVITY * dt * 0.6f;
    }
    p->x += p->vx * dt;
    p->y += p->vy * dt;
    p->life -= dt;
    if (p->life <= 0.0f)
      p->active = false;
  }
}

void particle_system_render(ParticleSystem *ps, SDL_Renderer *r, float oy, float cam_x)
{
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  for (int i = 0; i < PS_MAX_PARTICLES; ++i)
  {
    Particle *p = &ps->particles[i];
    if (!p->active)
      continue;
    float left = p->lifespan > 0.0f ? p->life / p->lifespan : 0.0f;
    float size = p->size;
    if (p->kind == PARTICLE_DUST)
    {
      /* Pale, thinning, and growing as it disperses. The colour is the room's
       * own ambient slate rather than a brown, so the same puff belongs on a
       * lobby floor and on a plenum walkway. It has to carry against a lit
       * stone floor as well as against a dark deck, which is why it goes on at
       * half opacity rather than as a whisper. */
      size = p->size * (1.0f + (1.0f - left) * 0.9f);
      SDL_SetRenderDrawColor(r, FX_PALE.r, FX_PALE.g, FX_PALE.b,
                             (Uint8)(left * 140.0f));
    }
    else
    {
      /* A spark is born bright and dies dark, and both ends of that ramp come
       * from the palette: fire-sized fragments start at the lamp's amber, the
       * small ones at blood red, and every one of them cools toward the same
       * deep red as it falls. Painted at a flat colour and hard-cut at death,
       * these were the most-seen off-palette pixels in the game — they draw on
       * every hit and every blast. The alpha holds while the spark still
       * carries energy and lets go over the last of its life, so it dies out
       * instead of vanishing between two frames. */
      SDL_Color hot = p->size > 4.5f ? FX_AMBER : FX_RED;
      SDL_Color c = fx_mix(FX_RED_DK, hot, left);
      SDL_SetRenderDrawColor(r, c.r, c.g, c.b,
                             (Uint8)(255.0f * fminf(1.0f, left * 2.5f)));
    }
    SDL_FRect rect = {p->x - cam_x - size * 0.5f, p->y + oy - size * 0.5f,
                      size, size};
    SDL_RenderFillRect(r, &rect);
  }
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void particle_system_clear(ParticleSystem *ps)
{
  for (int i = 0; i < PS_MAX_PARTICLES; ++i)
    ps->particles[i].active = false;
}
