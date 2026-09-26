#include "particle.h"

#include "fx.h"

#include <math.h>

void particle_system_init(ParticleSystem *ps)
{
  for (int i = 0; i < PS_MAX_PARTICLES; ++i)
    ps->particles[i].active = false;
  ps->serial = 0u;
}

static float frand_range(float a, float b)
{
  return a + (b - a) * ((float)SDL_rand(10001) * 0.00009999f);
}

static float clamp01(float v)
{
  return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

/*
 * The next free slot, stamped with everything every kind shares.
 *
 * The seed comes off a spawn counter rather than off SDL's stream: the stream
 * is shared with the camera shake, and a burst that drew extra numbers from it
 * for its flicker would move every shake after it.
 */
static Particle *claim(ParticleSystem *ps, int *cursor, ParticleKind kind)
{
  for (; *cursor < PS_MAX_PARTICLES; ++*cursor)
  {
    Particle *p = &ps->particles[*cursor];
    if (p->active)
      continue;
    p->active = true;
    p->kind = kind;
    p->delay = 0.0f;
    p->rest_y = 1.0e9f;
    p->seed = fx_hash(++ps->serial * 2654435761u + (unsigned)kind);
    ++*cursor;
    return p;
  }
  return NULL;
}

void particle_system_emit(ParticleSystem *ps, float x, float y, int count, int facing)
{
  if (count <= 0)
    return;

  /* Emit particles in a wide spread around the player so splatter covers
   * both sides; apply a small forward bias based on facing to keep feel. */
  int cursor = 0;
  for (; count > 0; --count)
  {
    Particle *p = claim(ps, &cursor, PARTICLE_SPARK);
    if (p == NULL)
      return;
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
    /* Spray from the middle of a body comes down about where its feet were. */
    p->rest_y = y + 9.0f;
  }
}

/*
 * A blast, as the five things it is made of.
 *
 * `count` is what the gameplay asked for and it already says how big the bang
 * is — a rocket asks 88, a canister 72, a grenade 64, a mine 48, a camera 12 —
 * so it sets the scale of everything here rather than just how many squares
 * go up. Each requested particle is one of the four moving materials, dealt
 * round in a fixed order so a small burst still gets some of each, and the
 * flash is one more on top.
 *
 * Every particle takes exactly the seven numbers from SDL's stream that the old
 * single-kind burst took, and what they are spent on differs by material; the
 * rest of what varies — the smoke's delay, a puff's lobe, an ember's flicker —
 * comes off the particle's own seed.
 */
void particle_system_explosion(ParticleSystem *ps, float x, float y, int count)
{
  if (count <= 0)
    return;

  float s = fminf(1.4f, fmaxf(0.16f, (float)count / 64.0f));
  float reach = 0.45f + 0.55f * s;
  int cursor = 0;

  Particle *flash = claim(ps, &cursor, PARTICLE_FLASH);
  if (flash != NULL)
  {
    flash->x = x;
    flash->y = y;
    flash->vx = 0.0f;
    flash->vy = 0.0f;
    flash->life = 0.20f + 0.10f * s;
    flash->lifespan = flash->life;
    flash->size = s;
  }

  /* Unsized, and the deal is taken modulo its own length, so a material added
   * to the round cannot leave a zero-filled row dealing sparks. */
  static const ParticleKind DEAL[] = {
      PARTICLE_FIRE, PARTICLE_EMBER, PARTICLE_FIRE, PARTICLE_SMOKE,
      PARTICLE_FIRE, PARTICLE_EMBER, PARTICLE_FIRE, PARTICLE_DEBRIS,
      PARTICLE_EMBER, PARTICLE_SMOKE};

  for (int k = 0; k < count; ++k)
  {
    Particle *p = claim(ps, &cursor, DEAL[k % (int)SDL_arraysize(DEAL)]);
    if (p == NULL)
      return;
    float jx = frand_range(-1.0f, 1.0f);
    float jy = frand_range(-1.0f, 1.0f);
    float ang = frand_range(-3.14159265f, 3.14159265f);
    float sp01 = frand_range(0.0f, 1.0f);
    float lift01 = frand_range(0.0f, 1.0f);
    float life01 = frand_range(0.0f, 1.0f);
    float size01 = frand_range(0.0f, 1.0f);
    float c = cosf(ang);
    float sn = sinf(ang);

    switch (p->kind)
    {
    case PARTICLE_FIRE:
    {
      /* Thrown hard and stopped by the air inside the blast radius, so the
       * ball swells to its size and holds there while it burns. */
      float speed = (30.0f + 150.0f * sp01) * reach;
      p->x = x + jx * (3.0f + 7.0f * s);
      p->y = y + jy * (2.0f + 5.0f * s);
      p->vx = c * speed;
      /* Most charges go off on a floor, and a floor is what a fireball
       * cannot swell into: the half thrown downward is flattened against it
       * and spreads along it instead. */
      p->vy = sn * speed * (sn > 0.0f ? 0.3f : 0.75f) -
              (10.0f + 25.0f * lift01) * s;
      /* The puffs thrown hardest burn out first, so the rim of the ball goes
       * over to red and soot while its middle is still yellow — the whole
       * fireball has a hot heart rather than every puff having one. */
      p->life = 0.30f + 0.40f * (1.0f - sp01) + 0.25f * life01 + 0.15f * s;
      p->size = (4.0f + 5.0f * size01) * (0.55f + 0.45f * s);
      break;
    }
    case PARTICLE_SMOKE:
    {
      float speed = (8.0f + 40.0f * sp01) * (0.5f + 0.5f * s);
      p->x = x + jx * (4.0f + 10.0f * s);
      p->y = y + jy * (3.0f + 6.0f * s) - 2.0f * s;
      p->vx = c * speed;
      p->vy = sn * speed * 0.5f - (12.0f + 18.0f * lift01);
      p->life = 1.0f + 0.9f * life01 + 0.3f * s;
      p->size = (6.0f + 6.0f * size01) * (0.55f + 0.45f * s);
      p->delay = 0.05f + (float)(p->seed % 21u) * 0.01f;
      break;
    }
    case PARTICLE_EMBER:
    {
      float speed = (80.0f + 240.0f * sp01) * reach;
      p->x = x + jx * 3.0f * s;
      p->y = y + jy * 3.0f * s;
      p->vx = c * speed;
      p->vy = sn * speed - (20.0f + 60.0f * lift01);
      p->life = 0.35f + 0.55f * life01;
      p->size = size01 > 0.6f ? 2.0f : 1.0f;
      /* What is thrown out of a charge sitting on a floor comes back down
       * onto that floor a few pixels under the middle of the bang. */
      p->rest_y = y + 4.0f;
      break;
    }
    default: /* PARTICLE_DEBRIS */
    {
      float speed = (70.0f + 190.0f * sp01) * reach;
      p->x = x + jx * 4.0f * s;
      p->y = y + jy * 3.0f * s;
      p->vx = c * speed;
      p->vy = sn * speed * 0.7f - (60.0f + 80.0f * lift01) * s;
      p->life = 0.55f + 0.5f * life01;
      p->size = 2.0f + 1.5f * size01;
      p->rest_y = y + 4.0f;
      break;
    }
    }
    p->lifespan = p->life;
  }
}

/*
 * A flash charge going off.
 *
 * It used to go through `particle_system_explosion` with a count of forty, so
 * the device the game sells as the one thing you can set off in the room you
 * are standing in drew a fireball, embers and a cloud of soot — the picture of
 * a grenade, over a stun that kills nobody. What a flash charge is, is light:
 * one white instant a good deal wider than a blast's, a cold ring, the handful
 * of magnesium sparks it throws, and a pale haze that hangs where it was.
 */
void particle_system_flash(ParticleSystem *ps, float x, float y)
{
  int cursor = 0;
  Particle *stun = claim(ps, &cursor, PARTICLE_STUN);
  if (stun != NULL)
  {
    stun->x = x;
    stun->y = y;
    stun->vx = 0.0f;
    stun->vy = 0.0f;
    stun->life = 0.42f;
    stun->lifespan = stun->life;
    stun->size = 1.0f;
  }

  for (int k = 0; k < 16; ++k)
  {
    Particle *p = claim(ps, &cursor, PARTICLE_GLINT);
    if (p == NULL)
      return;
    float ang = frand_range(-3.14159265f, 3.14159265f);
    float speed = frand_range(50.0f, 190.0f);
    p->x = x + frand_range(-2.0f, 2.0f);
    p->y = y + frand_range(-2.0f, 2.0f);
    p->vx = cosf(ang) * speed;
    p->vy = sinf(ang) * speed * 0.7f - frand_range(20.0f, 60.0f);
    p->life = frand_range(0.30f, 0.75f);
    p->lifespan = p->life;
    p->size = 1.0f;
    p->rest_y = y + 4.0f;
  }

  for (int k = 0; k < 6; ++k)
  {
    Particle *p = claim(ps, &cursor, PARTICLE_DUST);
    if (p == NULL)
      return;
    p->x = x + frand_range(-8.0f, 8.0f);
    p->y = y + frand_range(-4.0f, 2.0f);
    p->vx = frand_range(-18.0f, 18.0f);
    p->vy = -frand_range(4.0f, 14.0f);
    p->life = frand_range(0.7f, 1.2f);
    p->lifespan = p->life;
    p->size = frand_range(5.0f, 8.0f);
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
  int cursor = 0;
  for (; count > 0; --count)
  {
    Particle *p = claim(ps, &cursor, PARTICLE_DUST);
    if (p == NULL)
      return;
    float side = frand_range(-half, half);
    p->x = x + side;
    p->y = y + frand_range(-1.0f, 1.0f);
    /* Outward from the middle of the contact, faster the further out it
     * starts, with just enough rise to clear the floor. */
    p->vx = (side >= 0.0f ? 1.0f : -1.0f) * frand_range(14.0f, 46.0f);
    p->vy = -frand_range(6.0f, 22.0f);
    p->life = frand_range(0.30f, 0.55f);
    p->lifespan = p->life;
    p->size = frand_range(2.0f, 4.0f);
  }
}

/* The air taking a share of the speed out every second, framerate-safe. */
static void drag(Particle *p, float rate, float dt)
{
  float k = fminf(1.0f, dt * rate);
  p->vx -= p->vx * k;
  p->vy -= p->vy * k;
}

void particle_system_update(ParticleSystem *ps, float dt)
{
  for (int i = 0; i < PS_MAX_PARTICLES; ++i)
  {
    Particle *p = &ps->particles[i];
    if (!p->active)
      continue;
    if (p->delay > 0.0f)
    {
      p->delay -= dt;
      continue;
    }
    float age = p->lifespan > 0.0f ? 1.0f - p->life / p->lifespan : 1.0f;
    switch (p->kind)
    {
    case PARTICLE_DUST:
      /* Dust hangs: almost no weight, and the air takes the speed out of it. */
      p->vy += GRAVITY * dt * 0.08f;
      p->vx -= p->vx * fminf(1.0f, dt * 3.4f);
      break;
    case PARTICLE_FLASH:
    case PARTICLE_STUN:
      break;
    case PARTICLE_GLINT:
      drag(p, 1.6f, dt);
      p->vy += GRAVITY * dt * 0.30f;
      break;
    case PARTICLE_FIRE:
      /* Hot gas: the air stops it, and it starts to climb as it cools. */
      drag(p, 5.5f, dt);
      p->vy -= (20.0f + 70.0f * age) * dt;
      break;
    case PARTICLE_SMOKE:
      drag(p, 2.2f, dt);
      p->vy -= 16.0f * dt;
      break;
    case PARTICLE_EMBER:
      drag(p, 1.2f, dt);
      p->vy += GRAVITY * dt * 0.55f;
      break;
    case PARTICLE_DEBRIS:
      drag(p, 0.6f, dt);
      p->vy += GRAVITY * dt * 0.9f;
      break;
    default:
      p->vy += GRAVITY * dt * 0.6f;
      break;
    }
    p->x += p->vx * dt;
    p->y += p->vy * dt;
    p->life -= dt;
    /* Past the floor it was thrown over and faded out on the way: nothing
     * left to draw, and a slot a later hit can have. */
    if (p->life <= 0.0f || p->y > p->rest_y + 10.0f)
      p->active = false;
  }
}

/* ---- Drawing ---------------------------------------------------------- */

/*
 * How much of a particle is left above the floor line it was thrown over,
 * 1 to 0 across ten pixels, so a falling chip goes out as it lands.
 */
static float rest_fade(const Particle *p)
{
  return clamp01(1.0f - (p->y - p->rest_y) * 0.1f);
}

static void blend_rect(SDL_Renderer *r, SDL_Color c, float alpha,
                       float x, float y, float w, float h)
{
  if (alpha <= 0.0f || w <= 0.0f || h <= 0.0f)
    return;
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, (Uint8)(fminf(alpha, 1.0f) * 255.0f));
  SDL_FRect rect = {x, y, w, h};
  SDL_RenderFillRect(r, &rect);
}

/*
 * A puff: a square with its corners off, which is the smallest shape that
 * stops reading as a pixel and starts reading as a volume. `x`, `y` is the
 * top-left, already snapped.
 */
static void puff_shape(SDL_Renderer *r, SDL_Color c, float alpha,
                       float x, float y, float size)
{
  if (size < 3.0f)
  {
    blend_rect(r, c, alpha, x, y, size, size);
    return;
  }
  /* Past seven pixels one pixel off each corner leaves a rounded tile, so the
   * big ones lose two: an octagon, which at this size is a circle. */
  float cut = size >= 7.0f ? 2.0f : 1.0f;
  blend_rect(r, c, alpha, x, y + cut, size, size - cut * 2.0f);
  blend_rect(r, c, alpha, x + cut, y, size - cut * 2.0f, 1.0f);
  blend_rect(r, c, alpha, x + cut, y + size - 1.0f, size - cut * 2.0f, 1.0f);
  if (cut > 1.0f)
  {
    blend_rect(r, c, alpha, x + 1.0f, y + 1.0f, size - 2.0f, 1.0f);
    blend_rect(r, c, alpha, x + 1.0f, y + size - 2.0f, size - 2.0f, 1.0f);
  }
}

/*
 * A body of smoke lit the way the room is: from the lamps overhead, so the
 * crown of every puff is a step paler than its belly. `under` is a colour the
 * fire below it is still throwing up onto its underside, and `under_a` how much.
 */
static void smoke_puff(SDL_Renderer *r, const Particle *p, SDL_Color c,
                       float alpha, float size, SDL_Color under, float under_a,
                       float oy, float cam_x)
{
  float s = roundf(size);
  float x = floorf(p->x - cam_x - s * 0.5f);
  float y = floorf(p->y + oy - s * 0.5f);
  /* A second, smaller lobe off to one side and up, picked by the seed, so
   * a cloud of these is a cloud rather than a stack of rounded tiles. */
  float lobe = roundf(s * 0.62f);
  float lx = x + ((p->seed & 1u) ? s - lobe * 0.55f : -lobe * 0.45f);
  float ly = y - lobe * 0.35f;
  puff_shape(r, c, alpha * 0.85f, floorf(lx), floorf(ly), lobe);
  puff_shape(r, c, alpha, x, y, s);
  if (s >= 4.0f)
  {
    float band = floorf(s * 0.34f);
    blend_rect(r, fx_mix(c, FX_PALE, 0.22f), alpha * 0.8f, x + 1.0f, y,
               s - 2.0f, band);
    blend_rect(r, fx_mix(c, FX_INK, 0.35f), alpha * 0.7f, x + 1.0f,
               y + s - band, s - 2.0f, band);
    if (under_a > 0.0f)
      blend_rect(r, under, alpha * under_a, x + 1.0f, y + s - band, s - 2.0f,
                 band);
  }
}

/* The soot a fireball turns to before it is smoke: a burnt brown rather than
 * the flame taken down to black, which stays a strong red all the way and
 * leaves the whole cloud looking like it is still on fire. */
static SDL_Color soot(void)
{
  return fx_mix(FX_WOOD_DK, FX_INK, 0.30f);
}

/* Smoke is the room's slate warmed by what it came out of — the palette's
 * greys are all blue, and soot mixed straight into them goes violet. */
static SDL_Color smoke_grey(void)
{
  return fx_mix(FX_STEEL, FX_WOOD_DK, 0.35f);
}

/*
 * The one fire, as a temperature ramp over a puff's life: white at the heart
 * of the flash, the flame's own yellow, then its red, then soot, then smoke.
 * Every stop is a palette colour or a mix of two.
 */
static SDL_Color fire_colour(float u)
{
  SDL_Color orange = fx_mix(FX_FLAME_HOT, FX_FLAME, 0.55f);
  if (u < 0.08f)
    return fx_mix(FX_CREAM, FX_FLAME_HOT, u / 0.08f);
  if (u < 0.24f)
    return fx_mix(FX_FLAME_HOT, orange, (u - 0.08f) / 0.16f);
  if (u < 0.36f)
    return fx_mix(orange, FX_FLAME, (u - 0.24f) / 0.12f);
  if (u < 0.50f)
    return fx_mix(FX_FLAME, soot(), (u - 0.36f) / 0.14f);
  return fx_mix(soot(), smoke_grey(), (u - 0.50f) / 0.50f);
}

static void draw_fire(SDL_Renderer *r, const Particle *p, float u,
                      float oy, float cam_x)
{
  /* Born compressed and swelling as it burns: a fireball grows. */
  float size = p->size * (0.70f + 1.10f * u);
  SDL_Color c = fire_colour(u);
  float alpha = u < 0.45f ? 1.0f : powf(1.0f - (u - 0.45f) / 0.55f, 1.2f) * 0.8f;
  if (u >= 0.45f)
  {
    /* Burnt out: what is left is smoke and draws as smoke. The flame is
     * still in its belly for a moment after it goes dark on top. */
    float glow = clamp01(1.0f - (u - 0.45f) / 0.20f);
    smoke_puff(r, p, c, alpha, size, FX_FLAME, glow * 0.55f, oy, cam_x);
    return;
  }
  float s = roundf(size);
  float x = floorf(p->x - cam_x - s * 0.5f);
  float y = floorf(p->y + oy - s * 0.5f);
  puff_shape(r, c, alpha, x, y, s);
  /* Its crown a temperature step hotter, because the hottest gas is the part
   * that is rising, and its underside already going over to soot. */
  if (s >= 4.0f)
  {
    float band = floorf(s * 0.34f);
    blend_rect(r, fire_colour(fmaxf(0.0f, u - 0.10f)), alpha, x + 1.0f, y,
               s - 2.0f, band);
    blend_rect(r, fx_mix(c, soot(), 0.45f), alpha * 0.7f, x + 1.0f,
               y + s - 1.0f, s - 2.0f, 1.0f);
  }
}

static void draw_smoke(SDL_Renderer *r, const Particle *p, float u,
                       float oy, float cam_x)
{
  float size = p->size * (0.80f + 1.40f * u);
  /* In over the first stretch rather than popping on, and out slowly. */
  float alpha = u < 0.12f ? u / 0.12f : powf(1.0f - (u - 0.12f) / 0.88f, 1.3f);
  alpha *= 0.62f;
  SDL_Color c = u < 0.25f ? fx_mix(soot(), smoke_grey(), u / 0.25f)
                          : fx_mix(smoke_grey(), FX_MID, (u - 0.25f) / 0.75f);
  float glow = clamp01(1.0f - u / 0.30f);
  smoke_puff(r, p, c, alpha, size, FX_FLAME, glow * 0.30f, oy, cam_x);
}

/*
 * A hot fragment, with the streak of where it was a moment ago behind it —
 * a spark at this size is mostly its trail — cooling from the flame's yellow
 * through its red, and guttering at the end rather than switching off.
 */
static void draw_ember(SDL_Renderer *r, const Particle *p, float u,
                       float oy, float cam_x)
{
  float age = p->lifespan - p->life;
  if (u > 0.65f && ((p->seed + (unsigned)(age * 24.0f)) & 3u) == 0u)
    return;
  SDL_Color head = u < 0.5f ? fx_mix(FX_FLAME_HOT, FX_FLAME, u / 0.5f)
                            : fx_mix(FX_FLAME, FX_RED_DK, (u - 0.5f) / 0.5f);
  float alpha = (u < 0.6f ? 1.0f : 1.0f - (u - 0.6f) / 0.4f) * rest_fade(p);
  float x = floorf(p->x - cam_x);
  float y = floorf(p->y + oy);
  float speed = sqrtf(p->vx * p->vx + p->vy * p->vy);
  if (speed > 1.0f)
  {
    int len = (int)fminf(6.0f, speed * 0.022f);
    float dx = -p->vx / speed;
    float dy = -p->vy / speed;
    SDL_Color tail = fx_mix(head, FX_FLAME, 0.5f);
    for (int j = 1; j <= len; ++j)
      blend_rect(r, tail, alpha * (1.0f - (float)j / (float)(len + 1)) * 0.8f,
                 floorf(p->x - cam_x + dx * (float)j),
                 floorf(p->y + oy + dy * (float)j), 1.0f, 1.0f);
  }
  blend_rect(r, head, alpha, x, y, p->size, p->size);
}

/*
 * A chunk of whatever was next to the charge: dark, lit along whichever edge
 * is on top as it turns over, and glowing along the other while it is still
 * hot from the blast.
 */
static void draw_debris(SDL_Renderer *r, const Particle *p, float u,
                        float oy, float cam_x)
{
  float age = p->lifespan - p->life;
  bool turned = ((p->seed + (unsigned)(age * 16.0f)) & 1u) != 0u;
  float s = roundf(p->size);
  float w = turned ? s : fmaxf(1.0f, roundf(s * 0.6f));
  float h = turned ? fmaxf(1.0f, roundf(s * 0.6f)) : s;
  float x = floorf(p->x - cam_x - w * 0.5f);
  float y = floorf(p->y + oy - h * 0.5f);
  float alpha = (u < 0.75f ? 1.0f : 1.0f - (u - 0.75f) / 0.25f) * rest_fade(p);
  SDL_Color body = fx_mix(FX_INK, FX_STEEL_DK, 0.55f);
  blend_rect(r, body, alpha, x, y, w, h);
  blend_rect(r, FX_STEEL, alpha, x, y, w, 1.0f);
  if (u < 0.35f && h > 1.0f)
    blend_rect(r, fx_mix(FX_FLAME, body, u / 0.35f), alpha, x, y + h - 1.0f,
               w, 1.0f);
}

/*
 * Blood: a droplet with a short wet streak behind it, darkening as it falls.
 * One in four is thrown out as a fine mist around the hit for the first
 * moment, which is what makes a spray read as a spray at twelve pixels.
 */
static void draw_blood(SDL_Renderer *r, const Particle *p, float u,
                       float left, float oy, float cam_x)
{
  SDL_Color dry = fx_mix(FX_RED_DK, FX_INK, 0.35f);
  SDL_Color c = fx_mix(FX_RED, dry, u);
  float alpha = fminf(1.0f, left * 2.5f) * rest_fade(p);
  float sz = fmaxf(1.0f, roundf(p->size * 0.8f));
  float x = floorf(p->x - cam_x - sz * 0.5f);
  float y = floorf(p->y + oy - sz * 0.5f);
  if ((p->seed & 3u) == 0u && u < 0.35f)
  {
    float mist = sz * 3.0f;
    puff_shape(r, FX_RED_DK, alpha * 0.28f * (1.0f - u / 0.35f),
               floorf(x + sz * 0.5f - mist * 0.5f),
               floorf(y + sz * 0.5f - mist * 0.5f), mist);
  }
  float speed = sqrtf(p->vx * p->vx + p->vy * p->vy);
  if (speed > 1.0f)
  {
    int len = (int)fminf(4.0f, speed * 0.03f);
    float dx = -p->vx / speed;
    float dy = -p->vy / speed;
    for (int j = 1; j <= len; ++j)
      blend_rect(r, fx_mix(c, dry, 0.4f), alpha * 0.6f,
                 floorf(x + sz * 0.5f + dx * (float)(j + 1)),
                 floorf(y + sz * 0.5f + dy * (float)(j + 1)), 1.0f, 1.0f);
  }
  blend_rect(r, c, alpha, x, y, sz, sz);
  if (sz >= 3.0f)
    blend_rect(r, fx_mix(c, FX_CREAM, 0.35f), alpha, x, y, 1.0f, 1.0f);
}

/* Pale, thinning, and growing as it disperses. The colour is the room's own
 * ambient slate rather than a brown, so the same puff belongs on a lobby floor
 * and on a plenum walkway. It has to carry against a lit stone floor as well
 * as against a dark deck, which is why its core goes on at about half opacity
 * rather than as a whisper; the skirt round it is what makes it a puff. */
static void draw_dust(SDL_Renderer *r, const Particle *p, float u, float left,
                      float oy, float cam_x)
{
  float size = roundf(p->size * (1.0f + u * 0.9f)) + 1.0f;
  float x = floorf(p->x - cam_x - size * 0.5f);
  float y = floorf(p->y + oy - size * 0.5f);
  SDL_Color c = fx_mix(FX_PALE, FX_STEEL_LT, u * 0.6f);
  puff_shape(r, c, left * 0.34f, x, y, size);
  float core = fmaxf(1.0f, roundf(size * 0.5f));
  blend_rect(r, c, left * 0.26f, floorf(x + (size - core) * 0.5f),
             floorf(y + (size - core) * 0.5f), core, core);
}

/* A filled disc in horizontal spans, snapped to the pixel grid. */
static void disc(SDL_Renderer *r, SDL_Color c, float alpha, float cx, float cy,
                 float radius)
{
  int rr = (int)roundf(radius);
  for (int dy = -rr; dy <= rr; ++dy)
  {
    float half = floorf(sqrtf((float)(rr * rr - dy * dy)) + 0.35f);
    blend_rect(r, c, alpha, cx - half, cy + (float)dy, half * 2.0f + 1.0f, 1.0f);
  }
}

/* The front of the pressure wave: a thin ring racing out and thinning. */
static void ring(SDL_Renderer *r, SDL_Color c, float alpha, float cx, float cy,
                 float radius, float width)
{
  enum
  {
    RING_SEGMENTS = 32
  };
  if (alpha <= 0.0f || radius < 1.0f)
    return;
  SDL_Vertex v[(RING_SEGMENTS + 1) * 2];
  int idx[RING_SEGMENTS * 6];
  SDL_FColor col = fx_fcolor(c, fminf(alpha, 1.0f));
  float inner = fmaxf(0.0f, radius - width);
  for (int i = 0; i <= RING_SEGMENTS; ++i)
  {
    float a = (float)i / (float)RING_SEGMENTS * 6.2831853f;
    float ca = cosf(a);
    float sa = sinf(a);
    v[i * 2].position = (SDL_FPoint){cx + ca * radius, cy + sa * radius};
    v[i * 2 + 1].position = (SDL_FPoint){cx + ca * inner, cy + sa * inner};
    v[i * 2].color = col;
    v[i * 2 + 1].color = col;
    v[i * 2].tex_coord = (SDL_FPoint){0.0f, 0.0f};
    v[i * 2 + 1].tex_coord = (SDL_FPoint){0.0f, 0.0f};
  }
  for (int i = 0; i < RING_SEGMENTS; ++i)
  {
    int o = i * 2;
    idx[i * 6 + 0] = o;
    idx[i * 6 + 1] = o + 1;
    idx[i * 6 + 2] = o + 2;
    idx[i * 6 + 3] = o + 1;
    idx[i * 6 + 4] = o + 3;
    idx[i * 6 + 5] = o + 2;
  }
  SDL_RenderGeometry(r, NULL, v, (RING_SEGMENTS + 1) * 2, idx,
                     RING_SEGMENTS * 6);
}

/*
 * The light a blast throws on the room, laid down before anything else in the
 * burst so the fire and the smoke are seen *in* it. A muzzle flash lights the
 * room it goes off in; a charge going off had been the brightest thing in the
 * game lighting nothing at all.
 */
static void draw_flash_light(SDL_Renderer *r, const Particle *p, float u,
                             float oy, float cam_x)
{
  float s = p->size;
  float cx = p->x - cam_x;
  float cy = p->y + oy;
  float fade = (1.0f - u) * (1.0f - u);
  fx_glow(r, cx, cy, (36.0f + 46.0f * s) * (0.85f + 0.3f * u), FX_FLAME_HOT,
          (Uint8)(150.0f * fade));
  fx_glow(r, cx, cy, 14.0f + 16.0f * s, FX_CREAM,
          (Uint8)(170.0f * fade * (1.0f - u)));
}

static void draw_flash_core(SDL_Renderer *r, const Particle *p, float u,
                            float oy, float cam_x)
{
  float s = p->size;
  float cx = floorf(p->x - cam_x);
  float cy = floorf(p->y + oy);
  /* The ball of light the fire comes out of, gone in the first half of the
   * flash — a white core that lingered would read as a lamp. */
  if (u < 0.6f)
  {
    float k = powf(1.0f - u / 0.6f, 0.7f);
    float radius = (5.0f + 9.0f * s) * k;
    disc(r, FX_FLAME_HOT, 0.9f * k, cx, cy, radius);
    disc(r, FX_CREAM, k, cx, cy, radius * 0.6f);
  }
  /* The pressure front: fast out of the middle and slowing, thinning as it
   * goes, reaching about as far as the blast actually does. */
  if (u < 0.7f)
  {
    float w = u / 0.7f;
    float out = 1.0f - powf(1.0f - w, 3.0f);
    float radius = (8.0f + 40.0f * s) * out;
    float width = s > 0.9f && w < 0.4f ? 3.0f : 2.0f;
    ring(r, fx_mix(FX_CREAM, FX_PALE, w), 0.5f * (1.0f - w) * (1.0f - w), cx,
         cy, radius, width);
  }
}

/*
 * The flash charge's light: cold where a blast's is hot, and wider, because it
 * is what the charge is for. Laid down first like a blast's, so everything in
 * the room is seen lit by it.
 */
static void draw_stun_light(SDL_Renderer *r, const Particle *p, float u,
                            float oy, float cam_x)
{
  float cx = p->x - cam_x;
  float cy = p->y + oy;
  float fade = (1.0f - u) * (1.0f - u);
  fx_glow(r, cx, cy, 120.0f * (0.8f + 0.3f * u), FX_PALE,
          (Uint8)(160.0f * fade));
  fx_glow(r, cx, cy, 44.0f, FX_CREAM, (Uint8)(230.0f * fade));
}

static void draw_stun_core(SDL_Renderer *r, const Particle *p, float u,
                           float oy, float cam_x)
{
  float cx = floorf(p->x - cam_x);
  float cy = floorf(p->y + oy);
  if (u < 0.45f)
  {
    float k = powf(1.0f - u / 0.45f, 0.6f);
    disc(r, FX_CREAM, k, cx, cy, 10.0f * k + 2.0f);
  }
  if (u < 0.8f)
  {
    float w = u / 0.8f;
    float out = 1.0f - powf(1.0f - w, 3.0f);
    ring(r, fx_mix(FX_CREAM, FX_LAMP, w), 0.55f * (1.0f - w) * (1.0f - w),
         cx, cy, 10.0f + 70.0f * out, 2.0f);
  }
}

/* A spark of burning magnesium: white, cooling toward the lamp's cold blue,
 * and flickering as it goes out rather than switching off. */
static void draw_glint(SDL_Renderer *r, const Particle *p, float u,
                       float oy, float cam_x)
{
  float age = p->lifespan - p->life;
  if (u > 0.5f && ((p->seed + (unsigned)(age * 30.0f)) & 1u) == 0u)
    return;
  SDL_Color head = fx_mix(FX_CREAM, FX_LAMP, u);
  float alpha = (u < 0.6f ? 1.0f : 1.0f - (u - 0.6f) / 0.4f) * rest_fade(p);
  float speed = sqrtf(p->vx * p->vx + p->vy * p->vy);
  if (speed > 1.0f)
  {
    float dx = -p->vx / speed;
    float dy = -p->vy / speed;
    int len = (int)fminf(4.0f, speed * 0.02f);
    for (int j = 1; j <= len; ++j)
      blend_rect(r, head, alpha * (1.0f - (float)j / (float)(len + 1)) * 0.6f,
                 floorf(p->x - cam_x + dx * (float)j),
                 floorf(p->y + oy + dy * (float)j), 1.0f, 1.0f);
  }
  blend_rect(r, head, alpha, floorf(p->x - cam_x), floorf(p->y + oy), 1.0f,
             1.0f);
}

void particle_system_render(ParticleSystem *ps, SDL_Renderer *r, float oy, float cam_x)
{
  /*
   * Back to front, a pass per material: the flash's light on the room, then
   * what hangs in the air, the chunks, the fire, the fast bright fragments,
   * and the flash's own core and ring on top of all of it. Drawn in slot order
   * as they were, a puff of smoke spawned after a fireball painted over it.
   */
  for (int pass = 0; pass < 6; ++pass)
  {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < PS_MAX_PARTICLES; ++i)
    {
      Particle *p = &ps->particles[i];
      if (!p->active || p->delay > 0.0f)
        continue;
      float left = p->lifespan > 0.0f ? p->life / p->lifespan : 0.0f;
      float u = 1.0f - left;
      switch (p->kind)
      {
      case PARTICLE_FLASH:
        if (pass == 0)
        {
          draw_flash_light(r, p, u, oy, cam_x);
          SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        }
        else if (pass == 5)
          draw_flash_core(r, p, u, oy, cam_x);
        break;
      case PARTICLE_STUN:
        if (pass == 0)
        {
          draw_stun_light(r, p, u, oy, cam_x);
          SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        }
        else if (pass == 5)
          draw_stun_core(r, p, u, oy, cam_x);
        break;
      case PARTICLE_GLINT:
        if (pass == 4)
          draw_glint(r, p, u, oy, cam_x);
        break;
      case PARTICLE_SMOKE:
        if (pass == 1)
          draw_smoke(r, p, u, oy, cam_x);
        break;
      case PARTICLE_DUST:
        if (pass == 1)
          draw_dust(r, p, u, left, oy, cam_x);
        break;
      case PARTICLE_DEBRIS:
        if (pass == 2)
          draw_debris(r, p, u, oy, cam_x);
        break;
      case PARTICLE_FIRE:
        if (pass == 3)
          draw_fire(r, p, u, oy, cam_x);
        break;
      case PARTICLE_EMBER:
        if (pass == 4)
          draw_ember(r, p, u, oy, cam_x);
        break;
      default:
        if (pass == 4)
          draw_blood(r, p, u, left, oy, cam_x);
        break;
      }
    }
  }
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void particle_system_clear(ParticleSystem *ps)
{
  for (int i = 0; i < PS_MAX_PARTICLES; ++i)
    ps->particles[i].active = false;
}
