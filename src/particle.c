#include "particle.h"

#include "bbc.h"

#include <math.h>

/* A seed's worth of scrambling, so a burst's flicker and tumble are the same
 * every run without drawing on SDL's stream (see `claim`). */
static unsigned particle_hash(unsigned value)
{
  value ^= value >> 16;
  value *= 0x7feb352du;
  value ^= value >> 15;
  value *= 0x846ca68bu;
  return value ^ (value >> 16);
}

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
    p->seed = particle_hash(++ps->serial * 2654435761u + (unsigned)kind);
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
 * Every particle is drawn the way a BBC game drew one: whole fat pixels in
 * the eight colours, and no alpha anywhere. What a modern renderer does with
 * transparency is done here with the two things the machine had instead. A
 * thing that cools or thins *steps down a colour ladder* — white, yellow, red
 * for heat; white, cyan, blue for light and dust; white, blue for smoke — and
 * a thing that is going out *blinks*, on its own clock, until it is gone.
 *
 * Smoke and dust are a material rather than a colour: a grey the eye makes out
 * of white and black on a checkerboard, and the checkerboard is anchored to
 * the world through the camera so a cloud drifting across the room does not
 * boil as it goes.
 */

/* A particle's own blink: off on one tick in `every`, `rate` ticks a second,
   salted by its seed so a burst does not flicker in step. */
static bool winks_out(const Particle *p, float rate, unsigned every)
{
  float age = p->lifespan - p->life;
  return ((p->seed + (unsigned)(age * rate)) % every) == 0u;
}

/* Whether the world cell under screen point (x, y) is on the checkerboard. */
static bool world_check(float x, float y, float oy, float cam_x)
{
  int gx = (int)floorf((x + cam_x) / (float)BBC_PX_W);
  int gy = (int)floorf((y - oy) / (float)BBC_PX_H);
  return ((gx + gy) & 1) == 0;
}

/*
 * A puff of a material: a disc on the glass, every cell of it lit in `c` or,
 * for a material with no colour of its own, only the cells on the world's
 * checkerboard.
 */
static void puff(SDL_Renderer *r, BbcColour c, bool checked, float cx,
                 float cy, float radius, float oy, float cam_x)
{
  if (!checked)
  {
    bbc_disc(r, c, cx, cy, radius);
    return;
  }
  if (!(radius > 0.0f) || radius > 512.0f)
    return;
  float top = bbc_snap_y(cy - radius);
  float left = bbc_snap_x(cx - radius);
  for (float y = top; y <= cy + radius; y += (float)BBC_PX_H)
    for (float x = left; x <= cx + radius; x += (float)BBC_PX_W)
    {
      float dx = x + (float)BBC_PX_W * 0.5f - cx;
      float dy = y + (float)BBC_PX_H * 0.5f - cy;
      if (dx * dx + dy * dy > radius * radius)
        continue;
      if (world_check(x, y, oy, cam_x))
        bbc_plot(r, c, x, y);
    }
}

/* Heat, cooling: white, then the flame's yellow, then its red. */
static BbcColour heat(float u, float white_until, float yellow_until)
{
  if (u < white_until)
    return BBC_WHITE;
  if (u < yellow_until)
    return BBC_YELLOW;
  return BBC_RED;
}

/* Past the floor it was thrown over a fragment has landed, and goes out. */
static bool landed(const Particle *p)
{
  return p->y > p->rest_y + 2.0f;
}

/*
 * The one fire: a ball that swells as it burns and cools from the heart of
 * the flash — white, yellow, red — and then turns to the smoke it is made of,
 * grey on the checkerboard, going blue and then out.
 */
static void draw_fire(SDL_Renderer *r, const Particle *p, float u, float oy,
                      float cam_x)
{
  /* Three quarters of the puff's size rather than half: a soft puff that
     blended into its neighbours could be small, and a hard-edged one has to
     overlap them to make one ball rather than a scatter of discs. */
  float radius = p->size * 0.75f * (0.70f + 1.10f * u);
  float cx = p->x - cam_x;
  float cy = p->y + oy;
  if (u < 0.45f)
  {
    puff(r, heat(u, 0.08f, 0.24f), false, cx, cy, radius, oy, cam_x);
    /* Its crown a step hotter, because the hottest gas is the part that is
       rising. */
    if (u >= 0.08f && radius >= 4.0f)
      bbc_plot(r, heat(u - 0.10f, 0.08f, 0.24f), cx, cy - radius * 0.5f);
    return;
  }
  if (u < 0.75f)
    puff(r, BBC_WHITE, true, cx, cy, radius, oy, cam_x);
  else if (!winks_out(p, 20.0f, 3u))
    puff(r, BBC_BLUE, true, cx, cy, radius, oy, cam_x);
}

static void draw_smoke(SDL_Renderer *r, const Particle *p, float u, float oy,
                       float cam_x)
{
  float radius = p->size * 0.5f * (0.80f + 1.40f * u);
  float cx = p->x - cam_x;
  float cy = p->y + oy;
  /* Grey while it is thick, blue as it thins, and gone a cell at a time. */
  if (u < 0.45f)
    puff(r, BBC_WHITE, true, cx, cy, radius, oy, cam_x);
  else if (u < 0.85f || !winks_out(p, 18.0f, 2u))
    puff(r, BBC_BLUE, true, cx, cy, radius, oy, cam_x);
}

/*
 * A hot fragment, with a pixel of trail where it was a moment ago — at this
 * size a spark is mostly its trail — cooling yellow to red, and guttering at
 * the end rather than switching off.
 */
static void draw_ember(SDL_Renderer *r, const Particle *p, float u, float oy,
                       float cam_x)
{
  if (landed(p) || (u > 0.65f && winks_out(p, 24.0f, 3u)))
    return;
  float x = p->x - cam_x;
  float y = p->y + oy;
  float speed = sqrtf(p->vx * p->vx + p->vy * p->vy);
  if (speed > 60.0f)
    bbc_plot(r, BBC_RED, x - p->vx / speed * (float)BBC_PX_W,
             y - p->vy / speed * (float)BBC_PX_H);
  bbc_plot(r, u < 0.5f ? BBC_YELLOW : BBC_RED, x, y);
}

/*
 * A chunk of whatever was next to the charge, tumbling: two cells one way up
 * and one the other, red-hot along its edge at first and then the white and
 * blue of broken plaster.
 */
static void draw_debris(SDL_Renderer *r, const Particle *p, float u, float oy,
                        float cam_x)
{
  if (landed(p) || (u > 0.75f && winks_out(p, 16.0f, 2u)))
    return;
  float age = p->lifespan - p->life;
  bool turned = ((p->seed + (unsigned)(age * 16.0f)) & 1u) != 0u;
  bool big = p->size >= 3.0f;
  float x = p->x - cam_x;
  float y = p->y + oy;
  BbcColour body = u < 0.35f ? BBC_RED : BBC_BLUE;
  BbcColour edge = u < 0.35f ? BBC_YELLOW : BBC_WHITE;
  bbc_plot(r, edge, x, y);
  if (big)
  {
    if (turned)
      bbc_plot(r, body, x + (float)BBC_PX_W, y);
    else
      bbc_plot(r, body, x, y + (float)BBC_PX_H);
  }
}

/* Blood: a red droplet, a second cell of it behind a fast one, going out in
   a blink once it has nearly landed. */
static void draw_blood(SDL_Renderer *r, const Particle *p, float u, float oy,
                       float cam_x)
{
  if (landed(p) || (u > 0.6f && winks_out(p, 20.0f, 2u)))
    return;
  float x = p->x - cam_x;
  float y = p->y + oy;
  bbc_plot(r, BBC_RED, x, y);
  float speed = sqrtf(p->vx * p->vx + p->vy * p->vy);
  if (speed > 40.0f && p->size >= 3.0f)
    bbc_plot(r, BBC_RED, x - p->vx / speed * (float)BBC_PX_W,
             y - p->vy / speed * (float)BBC_PX_H);
}

/* Floor dust kicked out sideways: a pale puff that thins down the cold
   ladder — white, cyan, blue — as it hangs, and is gone before it lands. */
static void draw_dust(SDL_Renderer *r, const Particle *p, float u, float oy,
                      float cam_x)
{
  BbcColour c = u < 0.30f ? BBC_WHITE : (u < 0.60f ? BBC_CYAN : BBC_BLUE);
  if (u > 0.85f && winks_out(p, 18.0f, 2u))
    return;
  float radius = p->size * 0.5f * (1.0f + u * 0.9f);
  float cx = p->x - cam_x;
  float cy = p->y + oy;
  if (radius < 3.0f)
    bbc_plot(r, c, cx, cy);
  else
    puff(r, c, true, cx, cy, radius, oy, cam_x);
}

/*
 * A blast's first instant, before anything in it has moved: for a frame or
 * two the whole bang is one disc of yellow light — a BBC explosion flashing —
 * and then the white ball the fire comes out of, gone in half the flash, and
 * the pressure front racing out as a ring and cooling as it goes.
 */
static void draw_flash_light(SDL_Renderer *r, const Particle *p, float u,
                             float oy, float cam_x)
{
  if (u < 0.18f)
    bbc_disc(r, BBC_YELLOW, p->x - cam_x, p->y + oy, 12.0f + 26.0f * p->size);
}

static void draw_flash_core(SDL_Renderer *r, const Particle *p, float u,
                            float oy, float cam_x)
{
  float s = p->size;
  float cx = p->x - cam_x;
  float cy = p->y + oy;
  float age = p->lifespan - p->life;
  if (u < 0.6f)
  {
    float k = 1.0f - u / 0.6f;
    float radius = (5.0f + 9.0f * s) * k;
    /* The heart of it flashes white and yellow on its own fast clock. */
    bool hot = ((unsigned)(age * 30.0f) & 1u) == 0u;
    bbc_disc(r, hot ? BBC_YELLOW : BBC_RED, cx, cy, radius);
    bbc_disc(r, hot ? BBC_WHITE : BBC_YELLOW, cx, cy, radius * 0.6f);
  }
  if (u < 0.7f)
  {
    float w = u / 0.7f;
    float out = 1.0f - (1.0f - w) * (1.0f - w) * (1.0f - w);
    bbc_ring(r, heat(w, 0.25f, 0.6f), cx, cy, (8.0f + 40.0f * s) * out);
  }
}

/*
 * The flash charge: cold where a blast is hot, and wider, because the light
 * is what it is for. For its first beat the room around it goes white — the
 * one picture of a charge a BBC can give — then a white heart and a ring
 * going out through cyan to blue.
 */
static void draw_stun_light(SDL_Renderer *r, const Particle *p, float u,
                            float oy, float cam_x)
{
  if (u < 0.22f)
    bbc_disc(r, BBC_WHITE, p->x - cam_x, p->y + oy, 96.0f * (0.8f + u));
  else if (u < 0.32f)
    bbc_disc(r, BBC_CYAN, p->x - cam_x, p->y + oy, 40.0f);
}

static void draw_stun_core(SDL_Renderer *r, const Particle *p, float u,
                           float oy, float cam_x)
{
  float cx = p->x - cam_x;
  float cy = p->y + oy;
  if (u < 0.45f)
  {
    float k = 1.0f - u / 0.45f;
    bbc_disc(r, BBC_WHITE, cx, cy, 10.0f * k + 2.0f);
  }
  if (u < 0.8f)
  {
    float w = u / 0.8f;
    float out = 1.0f - (1.0f - w) * (1.0f - w) * (1.0f - w);
    BbcColour c = w < 0.35f ? BBC_WHITE : (w < 0.7f ? BBC_CYAN : BBC_BLUE);
    bbc_ring(r, c, cx, cy, 10.0f + 70.0f * out);
  }
}

/* A spark of burning magnesium: white, cooling to cyan, flickering out. */
static void draw_glint(SDL_Renderer *r, const Particle *p, float u, float oy,
                       float cam_x)
{
  if (landed(p) || (u > 0.5f && winks_out(p, 30.0f, 2u)))
    return;
  float x = p->x - cam_x;
  float y = p->y + oy;
  float speed = sqrtf(p->vx * p->vx + p->vy * p->vy);
  if (speed > 60.0f && u < 0.5f)
    bbc_plot(r, BBC_CYAN, x - p->vx / speed * (float)BBC_PX_W,
             y - p->vy / speed * (float)BBC_PX_H);
  bbc_plot(r, u < 0.4f ? BBC_WHITE : BBC_CYAN, x, y);
}

void particle_system_render(ParticleSystem *ps, SDL_Renderer *r, float oy, float cam_x)
{
  /*
   * Back to front, a pass per material: the flash's light, then what hangs
   * in the air, the chunks, the fire, the fast bright fragments, and the
   * flash's own core and ring on top of all of it. Drawn in slot order, a
   * puff of smoke spawned after a fireball would paint over it.
   */
  for (int pass = 0; pass < 6; ++pass)
  {
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
          draw_flash_light(r, p, u, oy, cam_x);
        else if (pass == 5)
          draw_flash_core(r, p, u, oy, cam_x);
        break;
      case PARTICLE_STUN:
        if (pass == 0)
          draw_stun_light(r, p, u, oy, cam_x);
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
          draw_dust(r, p, u, oy, cam_x);
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
      case PARTICLE_SPARK:
      default:
        if (pass == 4)
          draw_blood(r, p, u, oy, cam_x);
        break;
      }
    }
  }
}

void particle_system_clear(ParticleSystem *ps)
{
  for (int i = 0; i < PS_MAX_PARTICLES; ++i)
    ps->particles[i].active = false;
}
