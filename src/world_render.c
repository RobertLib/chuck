/*
 * The sector as a place: the room behind everything, the masonry, every
 * fitting, prop, pickup and hazard in it, the cast walking through it, and the
 * climbs' walls. The HUD, the sheets and the cards drawn over it are
 * [game_render.c](game_render.c)'s; the wall material and the backdrop are
 * [level_art.c](level_art.c)'s, chosen by the level's theme.
 *
 * Split out of game_render.c so the world and the interface over it can be
 * worked on separately: nothing here draws a word of interface, and the one
 * thing the interface needs from here is `render_world` itself.
 *
 * **Drawn as a BBC Micro in MODE 2 would draw it, through bbc.h and nothing
 * else.** A tile is eight BBC pixels across and sixteen down, so every fitting
 * below is laid out on that grid — `cell` places a block by its BBC column and
 * row inside the tile — and most of them are sprites drawn on squared paper,
 * one character to a pixel. What the old renderer did with light it does here
 * with colours that mean something, and the meanings are the ones
 * [docs/art-and-audio.md](../docs/art-and-audio.md) rations: green is the way
 * out, red is what hurts, yellow is what is worth picking up and what you
 * climb, cyan is the building's own technology, and blue is everything behind.
 * Nothing glows, nothing casts a shadow and nothing fades, because the machine
 * could do none of them; what it could do is flash, and the things that have to
 * shout here do.
 */

#include "world_render.h"

#include <SDL3/SDL.h>
#include <math.h>

#include "bbc.h"
#include "bbc_cast.h"
#include "gameplay_ai.h"
#include "gameplay_world.h"
#include "level_art.h"
#include "level_paint.h"
#include "particle.h"
#include "render_figures.h"

/* ---- The grid ----------------------------------------------------------- */

static unsigned tile_hash(int x, int y)
{
  unsigned h = (unsigned)x * 0x8da6b343u;
  h ^= (unsigned)y * 0xd8163841u;
  h ^= h >> 13;
  h *= 0xcb1ab31fu;
  return h ^ (h >> 16);
}

/* A variation keyed to a thing rather than to where it is on the screen: a
 * crate's slot, a pickup's index. */
static unsigned mix_hash(unsigned h)
{
  h ^= h >> 16;
  h *= 0x7feb352du;
  h ^= h >> 15;
  h *= 0x846ca68bu;
  return h ^ (h >> 16);
}

/*
 * `w` by `h` BBC pixels whose top-left is BBC column `col` and row `row` of the
 * tile at (x, y).
 *
 * The one placement call this file makes, because a fitting designed in BBC
 * pixels and placed in logical ones is a fitting whose numbers are four times
 * and twice the size of what they mean. Negative and out-of-tile cells are
 * allowed — a lamp hangs a row above its door, a stall's frame stands either
 * side of its tile.
 */
static void cell(SDL_Renderer *r, BbcColour c, float x, float y, int col,
                 int row, int w, int h)
{
  bbc_block(r, c, x + (float)(col * BBC_PX_W), y + (float)(row * BBC_PX_H), w,
            h);
}

static const LevelThemePaint *paint_of(const Level *lvl)
{
  return level_theme_paint(lvl->map.theme);
}

/*
 * A disc laid in a dither rather than a colour — the cable reel's timber cheek,
 * which is brown, and brown on this machine is red and yellow on a
 * checkerboard. The same span arithmetic `bbc_disc` uses, so it is the same
 * round on the glass.
 */
static void dither_disc(SDL_Renderer *r, BbcColour base, BbcColour ink,
                        BbcDither pattern, float cx, float cy, float radius)
{
  for (float py = bbc_snap_y(cy - radius); py <= cy + radius;
       py += (float)BBC_PX_H)
  {
    float dy = py + (float)BBC_PX_H * 0.5f - cy;
    if (fabsf(dy) > radius)
      continue;
    float half = sqrtf(radius * radius - dy * dy);
    float x0 = ceilf((cx - half - (float)BBC_PX_W * 0.5f) / (float)BBC_PX_W) *
               (float)BBC_PX_W;
    float x1 = floorf((cx + half - (float)BBC_PX_W * 0.5f) / (float)BBC_PX_W) *
                   (float)BBC_PX_W +
               (float)BBC_PX_W;
    if (x1 > x0)
      bbc_dither(r, base, ink, pattern, x0, py, x1 - x0, (float)BBC_PX_H);
  }
}


/* ---- The restroom's own room ------------------------------------------- */

/* A head and shoulders, filled in one colour by `bbc_sprite_solid`: Chuck in a
 * washroom mirror, or somebody working late behind a lit window on the climb.
 * Either is somebody in the scene, not a second figure to read in a fight. */
BBC_SPRITE(SPR_REFLECTION,
    ".1.",
    "111",
    ".1.",
    "111",
    "111");

/*
 * Mirror over a basin: a frame in the room's accent, black glass with one
 * diagonal of light across it, and a blue silhouette that follows the player
 * along the counter. Only a silhouette, on purpose — it makes the room feel
 * occupied without adding a second figure the player has to read while
 * fighting.
 */
static void draw_restroom_mirror(SDL_Renderer *r, BbcColour frame, float x,
                                 float y, float reflection_offset,
                                 bool reflected)
{
  cell(r, frame, x, y, 0, 0, 7, 13);
  cell(r, BBC_BLACK, x, y, 1, 1, 5, 11);
  if (reflected && reflection_offset > -9.0f && reflection_offset < 9.0f)
  {
    int shift = reflection_offset > 3.0f ? 1 : (reflection_offset < -3.0f ? -1 : 0);
    bbc_sprite_solid(r, &SPR_REFLECTION, x + (float)((2 + shift) * BBC_PX_W),
                     y + 14.0f, 1, BBC_BLUE);
  }
  /* The glint: three cells stepping up and to the right, which is all a BBC
   * mirror ever had to say it was glass. */
  cell(r, BBC_WHITE, x, y, 4, 2, 1, 1);
  cell(r, BBC_WHITE, x, y, 3, 4, 1, 1);
  cell(r, BBC_WHITE, x, y, 2, 6, 1, 1);
}

/*
 * Every level's backdrop comes from its theme (see level_art.c). The restroom
 * is the one exception: its interior is derived from the map's own wall
 * bounding box, so it is built here where the room geometry is to hand.
 */
static void render_restroom_backdrop(Game *game, int win_w, int win_h,
                                     float cam_x, float oy)
{
  SDL_Renderer *r = game->platform.renderer;
  const Level *level = &game->gameplay.level;
  const LevelThemePaint *paint = paint_of(level);
  const float fh = (float)win_h - HUD_HEIGHT;
  float t = game->presentation.render_clock;

  /* The interior is derived from the map's own masonry, so the tiling, mirrors
   * and lighting follow whatever shape the sublevel is drawn in instead of
   * assuming one fixed room height. */
  int wall_top = level->map.height;
  int wall_bottom = -1;
  int wall_left = level->map.width;
  int wall_right = -1;
  for (int row = 0; row < level->map.height; ++row)
  {
    for (int col = 0; col < level->map.width; ++col)
    {
      if (level->map.tiles[row][col] != TILE_WALL)
        continue;
      if (row < wall_top)
        wall_top = row;
      if (row > wall_bottom)
        wall_bottom = row;
      if (col < wall_left)
        wall_left = col;
      if (col > wall_right)
        wall_right = col;
    }
  }
  if (wall_bottom <= wall_top || wall_right <= wall_left)
    return;

  /* Outside the room is the service void: black, with the building's steelwork
   * drawn in the room's own blue and moving at a fraction of the camera, which
   * is all the depth a BBC screen ever had. */
  for (int support = 0; support < 6; ++support)
  {
    float sx = 58.0f + (float)support * 184.0f - fmodf(cam_x * 0.12f, 184.0f);
    bbc_fill(r, paint->backdrop, sx, HUD_HEIGHT, 4.0f, fh);
  }
  bbc_fill(r, paint->backdrop, 0.0f, HUD_HEIGHT + 82.0f, (float)win_w, 2.0f);

  float room_left = (float)(wall_left + 1) * (float)TILE_SIZE - cam_x;
  float room_right = (float)wall_right * (float)TILE_SIZE - cam_x;
  float room_width = room_right - room_left;
  float room_top = (float)(wall_top + 1) * (float)TILE_SIZE + oy;
  float room_bottom = (float)wall_bottom * (float)TILE_SIZE + oy;
  float room_height = room_bottom - room_top;
  bbc_fill(r, BBC_BLACK, room_left, room_top, room_width, room_height);

  /* The dado rail is measured up from the floor rather than down from the
   * ceiling, so it stays at waist height in a room of any storey count. The
   * wall under it is tiled, and the tiling is the room's blue lines — a grid
   * a ceramic tile wide, which is what says washroom before a single fitting
   * is read. Above the rail it is plain, so the mirrors have a wall to hang
   * on. */
  float rail_y = room_bottom - 60.0f;
  for (float ty = rail_y + 30.0f; ty < room_bottom; ty += 30.0f)
    bbc_fill(r, paint->backdrop, room_left, ty, room_width, 2.0f);
  for (int col = wall_left + 1; col < wall_right; col += 2)
    bbc_fill(r, paint->backdrop, (float)col * (float)TILE_SIZE - cam_x,
             rail_y + 4.0f, 4.0f, room_bottom - rail_y - 4.0f);
  bbc_fill(r, paint->accent, room_left, rail_y, room_width, 2.0f);
  bbc_fill(r, paint->backdrop, room_left, rail_y + 2.0f, room_width, 2.0f);

  /* A plumbing run and its brackets keep the interior from reading as an
   * office backdrop while leaving the fittings dominant. */
  bbc_fill(r, paint->backdrop, room_left, room_top + 20.0f, room_width, 6.0f);
  bbc_fill(r, paint->accent, room_left, room_top + 20.0f, room_width, 2.0f);
  for (float bracket = room_left + 40.0f; bracket < room_right;
       bracket += 96.0f)
    bbc_fill(r, paint->backdrop, bracket, room_top + 14.0f, 4.0f, 6.0f);

  int fixture_count = (int)(room_width / 300.0f) + 1;
  if (fixture_count > 4)
    fixture_count = 4;
  for (int fixture = 0; fixture < fixture_count; ++fixture)
  {
    float centre = room_left + room_width * ((float)fixture + 0.5f) /
                                   (float)fixture_count;
    /* One tube in the room is on its last legs. It goes out rather than dims,
     * because there is no dim: rare enough to be atmosphere, not a strobe. */
    bool out = fixture == 1 && fmodf(t * 1.9f, 5.0f) < 0.11f &&
               !game->settings.reduced_motion;
    bbc_fill(r, paint->backdrop, centre - 52.0f, room_top + 4.0f, 104.0f, 6.0f);
    bbc_fill(r, out ? BBC_BLUE : paint->lamp, centre - 48.0f, room_top + 6.0f,
             96.0f, 2.0f);
  }

  /* Mirrors hang over the basins the map placed. */
  for (int i = 0; i < level->map.decoration_count; ++i)
  {
    const Decoration *decoration = &level->map.decorations[i];
    if (decoration->type != DECOR_RESTROOM_BASIN)
      continue;
    float mirror_x = (float)decoration->col * (float)TILE_SIZE - cam_x;
    float mirror_y = (float)(decoration->row - 1) * (float)TILE_SIZE + oy + 2.0f;
    float basin_centre = ((float)decoration->col + 0.5f) * (float)TILE_SIZE;
    float player_centre = game->gameplay.player.x + PLAYER_W * 0.5f;
    bool in_front = fabsf(game->gameplay.player.y -
                          ((float)decoration->row * (float)TILE_SIZE)) <
                    TILE_SIZE;
    draw_restroom_mirror(r, paint->accent, mirror_x, mirror_y,
                         (player_centre - basin_centre) * 0.45f, in_front);
  }

  /* The sign over the door back to the level, in inverse video: green, the
   * colour of the way out, with the two letters cut out of it. */
  if (level->map.has_sublevel_return)
  {
    float sign_x = (float)level->map.sublevel_return_col * (float)TILE_SIZE -
                   cam_x + 8.0f;
    float sign_y = (float)(level->map.sublevel_return_row - 1) *
                       (float)TILE_SIZE +
                   oy + 16.0f;
    bbc_text_inverse(r, sign_x, sign_y, 1.0f, BBC_GREEN, BBC_BLACK, "WC");
  }
}

/*
 * The room behind everything. `cam_x` and `cam_y` are the camera the tiles are
 * drawn with, shake included and already on the grid: an interior's back wall
 * is anchored to the rooms it is behind, and a wall that did not shake with the
 * slabs would slide against them for the length of every blast.
 */
static void render_background(Game *game, int win_w, int win_h, float cam_x,
                              float cam_y)
{
  SDL_Renderer *r = game->platform.renderer;
  const Level *lvl = &game->gameplay.level;
  float oy = HUD_HEIGHT - cam_y;

  if (lvl->map.theme == LEVEL_THEME_RESTROOM)
  {
    render_restroom_backdrop(game, win_w, win_h, cam_x, oy);
    return;
  }

  /*
   * The alarm, the way a BBC did it: one palette change.
   *
   * The room's own lines are drawn in the theme's backdrop colour, which is
   * blue on every interior, so re-pointing blue at red for the length of the
   * backdrop turns every window frame, shelf edge and girder in the room red at
   * once — the building's lights going over to the emergency circuit, for the
   * cost of one entry in the palette. It alternates with the room's own blue at
   * the machine's flash rate, so it holds red under the reduced-motion switch.
   * The masonry and the cast are drawn after it is put back, so what the player
   * reads a floor plan from is never touched.
   */
  bool emergency = lvl->map.mode != LEVEL_MODE_FACADE &&
                   gameplay_alarm_active(&game->gameplay) && bbc_flash_on();
  BbcColour was = bbc_map(BBC_BLUE);
  if (emergency)
    bbc_palette_set(BBC_BLUE, BBC_RED);

  LevelArtScene scene = {r, lvl, cam_x, cam_y, win_w, win_h,
                         game->presentation.render_clock,
                         game->campaign.current_level,
                         game->settings.reduced_motion};
  level_art_backdrop(&scene);

  if (emergency)
    bbc_palette_set(BBC_BLUE, was);
  /* A parallax layer is free to move the pattern origin; everything after the
   * backdrop is in the world and has to be dithered from the world's corner —
   * the same formula level_art.c anchors its own world layers by, the screen
   * position of world (0, 0) under the snapped camera. */
  bbc_set_pattern_origin(-cam_x, oy);
}

/* Topmost masonry row: the room's own ceiling, which must not be mistaken
 * for a slab floating inside the room. */
static int restroom_ceiling_row(const Level *lvl)
{
  for (int row = 0; row < lvl->map.height; ++row)
    for (int col = 0; col < lvl->map.width; ++col)
      if (lvl->map.tiles[row][col] == TILE_WALL)
        return row;
  return 0;
}

/*
 * The restroom's masonry: glazed tile in the paint's own colours, grouted on a
 * sixteen-pixel module — four BBC pixels by eight — so a run of it reads as
 * ceramic rather than as a slab, and the floor's lip in the paint's trim so the
 * surface a foot lands on is the same kind of line it is upstairs.
 */
static void draw_restroom_wall_tile(SDL_Renderer *r, const Level *lvl,
                                    int col, int row, float x, float y,
                                    int ceiling_row)
{
  const LevelThemePaint *paint = paint_of(lvl);
  unsigned h = tile_hash(col, row);
  bool floor_top = !level_is_solid(lvl, col, row - 1);
  bool open_below = !level_is_solid(lvl, col, row + 1);

  bbc_fill(r, paint->wall, x, y, TILE_SIZE, TILE_SIZE);
  cell(r, paint->wall_ink, x, y, 0, 0, 1, 16);
  cell(r, paint->wall_ink, x, y, 4, 0, 1, 16);
  cell(r, paint->wall_ink, x, y, 0, 0, 8, 1);
  cell(r, paint->wall_ink, x, y, 0, 8, 8, 1);
  /* A chipped glaze on one tile in thirteen, so the wall is not one tile
   * repeated. */
  if ((h % 13u) == 0u)
    cell(r, paint->wall_ink, x, y, 2, 3, 1, 2);

  if (floor_top)
  {
    cell(r, paint->trim_ink, x, y, 0, 0, 8, 1);
    cell(r, paint->trim, x, y, 0, 1, 8, 2);
  }
  if (open_below)
    cell(r, paint->trim, x, y, 0, 15, 8, 1);

  /* A slab inside the room with open air both above and below is a service
   * catwalk rather than the room's own floor: it gets a handrail and hangers. */
  if (floor_top && row > ceiling_row && open_below)
  {
    cell(r, paint->trim, x, y, 0, -7, 8, 1);
    cell(r, BBC_BLUE, x, y, 0, -4, 8, 1);
    cell(r, BBC_BLUE, x, y, 1, -6, 1, 6);
    cell(r, BBC_BLUE, x, y, 6, -6, 1, 6);
    cell(r, BBC_BLUE, x, y, 3, 16, 2, 2);
  }
}

/* ---- The fittings every floor is built with ---------------------------- */

/*
 * A ladder: two rails and a rung every eight pixels, in yellow on every floor
 * of the building. A ladder is the one fitting a player has to find on every
 * screen without looking for it, so it is the one that does not take the
 * theme's paint — yellow against the black of the room, inset a pixel from
 * each side of its tile so a run of masonry beside it never touches it.
 */
static void draw_ladder_tile(SDL_Renderer *r, float x, float y, int row)
{
  cell(r, BBC_YELLOW, x, y, 1, 0, 1, 16);
  cell(r, BBC_YELLOW, x, y, 6, 0, 1, 16);
  for (int rung = 2; rung < 16; rung += 4)
    cell(r, BBC_YELLOW, x, y, 2, rung, 4, 1);
  /* A wall bracket every other course, in the room's blue, so the run is
   * fixed to something. */
  if ((row & 1) == 0)
  {
    cell(r, BBC_BLUE, x, y, 0, 7, 1, 2);
    cell(r, BBC_BLUE, x, y, 7, 7, 1, 2);
  }
}

/* A lift shaft is a hole in the floor plan: black, because nothing of the room
 * behind is in it, with two guide rails down it in the room's blue because they
 * are the far side of the hole; the car that rides them is the bright thing. A
 * landing mark on one tile in four says which storey is which. */
static void draw_shaft_tile(SDL_Renderer *r, float x, float y, int col, int row)
{
  unsigned h = tile_hash(col, row);
  bbc_fill(r, BBC_BLACK, x, y, TILE_SIZE, TILE_SIZE);
  cell(r, BBC_BLUE, x, y, 1, 0, 1, 16);
  cell(r, BBC_BLUE, x, y, 6, 0, 1, 16);
  if ((h & 3u) == 0u)
    cell(r, BBC_CYAN, x, y, 3, 7, 2, 1);
}

/*
 * A duct mouth: trunking let into the wall, louvred, with nothing readable
 * behind the louvres.
 *
 * The dark is the mechanic rather than a mood. A shaft is safe — no guard sees
 * into it, no round comes through it — and what it costs is knowing where you
 * are coming out, so the picture must not quietly give that back by showing the
 * room at the far end. It is drawn the same way under every theme, because a
 * duct is the one thing in the building that is the same on all seventeen
 * floors, and it is the DUCTS sector's own material everywhere else.
 *
 * **And it is drawn in two halves, which no other tile in the game is.** A duct
 * is the one tile something is ever *inside*, so it has a back and a front with
 * a crawler between them: `draw_vent_plenum` is the shaft and
 * `draw_vent_grille` is what is screwed over it. In one piece, as it was, the
 * whole tile went down in the structural pass and the figure went down near the
 * end of the frame — so a man in a shaft was painted over the louvres he was
 * supposed to be behind, and the tile whose entire cost is that those louvres
 * are opaque both ways read as a man crawling along in front of them.
 * `render_duct_fronts` lays the front half back over him. The grille draws
 * nothing in its gaps, so what shows between the blades is whatever is behind
 * them: black in an empty shaft, a slot of Chuck in an occupied one.
 */

static void draw_vent_plenum(SDL_Renderer *r, float x, float y)
{
  bbc_fill(r, BBC_BLACK, x, y, TILE_SIZE, TILE_SIZE);
}

static void draw_vent_grille(SDL_Renderer *r, float x, float y, int col,
                             int row)
{
  unsigned h = tile_hash(col, row);

  /* Four louvres, each a white blade with its own blue underside. White rather
   * than the building's cyan, because the one floor that is built of trunking
   * is built of cyan sheet, and a grille that matched the masonry round it
   * would be a wall with stripes on. */
  for (int slat = 3; slat < 15; slat += 3)
  {
    cell(r, BBC_WHITE, x, y, 1, slat, 6, 1);
    cell(r, BBC_BLUE, x, y, 1, slat + 1, 6, 1);
  }
  /* The frame it is all screwed into, and the yellow arris along the top: the
   * walkway line every surface the player can be on top of carries. */
  cell(r, BBC_BLUE, x, y, 0, 0, 1, 16);
  cell(r, BBC_BLUE, x, y, 7, 0, 1, 16);
  cell(r, BBC_BLUE, x, y, 0, 15, 8, 1);
  cell(r, BBC_WHITE, x, y, 0, 1, 8, 1);
  cell(r, BBC_YELLOW, x, y, 0, 0, 8, 1);
  /* One fixing off the tile hash, so a run of trunking is not one tile
   * repeated. */
  if ((h & 1u) == 0u)
    cell(r, BBC_WHITE, x, y, 1, 1, 1, 1);
  else
    cell(r, BBC_WHITE, x, y, 6, 14, 1, 1);
}

static void draw_vent_tile(SDL_Renderer *r, float x, float y, int col, int row)
{
  draw_vent_plenum(r, x, y);
  draw_vent_grille(r, x, y, col, row);
}

typedef enum
{
  DECK_LIFT,
  DECK_MOVING,
  DECK_FALLING
} DeckKind;

/*
 * A deck the player rides: four rows of steel, a bright lip, an open grating
 * and an underside. Which deck it is reads off the lip, because that is the
 * line a foot lands on — cyan for a lift car (the building's machinery), white
 * for a plate on a track, and yellow-and-black for the panel that drops, which
 * is the building's own hazard marking. Once a falling panel has taken Chuck's
 * weight the marking goes red and the grating splits: the panel saying it is
 * going before it goes.
 */
static void draw_platform(SDL_Renderer *r, float x, float y, DeckKind kind,
                          bool cracked)
{
  if (kind == DECK_FALLING)
  {
    for (int c = 0; c < 8; ++c)
      cell(r, ((c >> 1) & 1) ? BBC_BLACK : (cracked ? BBC_RED : BBC_YELLOW), x,
           y, c, 0, 1, 1);
  }
  else
  {
    cell(r, kind == DECK_LIFT ? BBC_CYAN : BBC_WHITE, x, y, 0, 0, 8, 1);
  }
  for (int c = 0; c < 8; ++c)
    cell(r, (c & 1) ? BBC_BLACK : BBC_BLUE, x, y, c, 1, 1, 2);
  cell(r, BBC_WHITE, x, y, 0, 1, 1, 2);
  cell(r, BBC_WHITE, x, y, 7, 1, 1, 2);
  cell(r, BBC_BLUE, x, y, 0, 3, 8, 1);
  if (cracked)
  {
    cell(r, BBC_BLACK, x, y, 2, 0, 1, 3);
    cell(r, BBC_BLACK, x, y, 5, 1, 1, 3);
    cell(r, BBC_RED, x, y, 3, 3, 1, 1);
  }
}

/*
 * What a lift deck hangs from.
 *
 * A car has a frame and a frame has ropes: two stiles up from the deck to a
 * crosshead a tile above it — behind the rider, so he stands inside the car
 * rather than in front of a ladder — and a pair of hoist ropes from the
 * crosshead to a sheave at the head of the shaft. The head is found from the
 * map rather than from the deck, so the ropes shorten as the car climbs and are
 * gone when it parks at the top, which is what says the deck is being pulled
 * rather than floated. The car is the deck's cyan; the ropes are white, the
 * brightest thing in the shaft, because they are what move.
 */
static void draw_lift_rigging(SDL_Renderer *r, const Level *lvl,
                              const Elevator *el, float cam_x, float oy)
{
  float x = (float)el->col * (float)TILE_SIZE - cam_x;
  float deck = el->y + oy;
  int row = (int)floorf((el->y - 1.0f) / (float)TILE_SIZE);
  if (level_tile(lvl, el->col, row) != TILE_ELEVATOR_SHAFT)
    return;
  while (row > 0 && level_tile(lvl, el->col, row - 1) == TILE_ELEVATOR_SHAFT)
    --row;
  float head = (float)row * (float)TILE_SIZE + oy;
  float cross = deck - 30.0f;
  if (cross < head + 4.0f)
    cross = head + 4.0f;

  if (cross - head > 4.0f)
  {
    bbc_fill(r, BBC_WHITE, x + 8.0f, head + 4.0f, 4.0f, cross - head - 4.0f);
    bbc_fill(r, BBC_WHITE, x + 20.0f, head + 4.0f, 4.0f, cross - head - 4.0f);
  }
  /* The sheave at the head of the shaft, under the slab. */
  bbc_fill(r, BBC_WHITE, x + 8.0f, head, 16.0f, 2.0f);
  bbc_fill(r, BBC_BLUE, x + 8.0f, head + 2.0f, 16.0f, 2.0f);

  if (deck - cross > 6.0f)
  {
    bbc_fill(r, BBC_CYAN, x, cross, 4.0f, deck - cross);
    bbc_fill(r, BBC_CYAN, x + 28.0f, cross, 4.0f, deck - cross);
  }
  bbc_fill(r, BBC_CYAN, x, cross, 32.0f, 4.0f);
  bbc_fill(r, BBC_WHITE, x + 12.0f, cross, 8.0f, 2.0f);
}

/*
 * A sliding service door, one of a numbered pair: cyan frame, blue leaf, a
 * viewing slit, the reader's green lamp, and a plate holding the pair's digit at
 * the font's own size — the whole point of numbering the doors is that the
 * player can read the number and know where the other one comes out.
 */
BBC_SPRITE(SPR_DOOR,
    "CCCCCCCC",
    "CBBBBBBC",
    "CBKKKKBC",
    "CBKCCKBC",
    "CBKKKKBC",
    "CBBBBBBC",
    "CBBKKBBC",
    "CBBKKBGC",
    "CBBKKBBC",
    "CBBKKBBC",
    "CBBBBBBC",
    "CBBBBBBC",
    "CBCCCCBC",
    "CBBBBBBC",
    "CBCCCCBC",
    "WWWWWWWW");

static void draw_door(SDL_Renderer *r, float x, float y, int index)
{
  /* The lamp over the frame, so a door at the end of a dark corridor is found
   * before it is read. */
  cell(r, BBC_BLUE, x, y, 2, -2, 4, 1);
  cell(r, BBC_YELLOW, x, y, 3, -1, 2, 1);
  bbc_sprite(r, &SPR_DOOR, x, y, 1, NULL);
  char label[3];
  SDL_snprintf(label, sizeof(label), "%d", index / 2 + 1);
  bbc_text(r, x + 12.0f, y + 12.0f, 1.0f, BBC_WHITE, label);
}

/* The washroom door: the paired doors' colours swapped round — a blue frame, a
 * cyan leaf — so it is never mistaken for one of them, with its plate in
 * inverse video and the plate sized to the two letters at the font's own scale
 * rather than the letters shrunk onto the plate. */
BBC_SPRITE(SPR_WC_DOOR,
    "BBBBBBBB",
    "BCCCCCCB",
    "BCCCCCCB",
    "BCCCCCCB",
    "BCCCCCCB",
    "BCCCCCCB",
    "BCCCCCCB",
    "BCCCCCCB",
    "BCCCCCCB",
    "BCWWCCCB",
    "BCWWCYCB",
    "BCCCCYCB",
    "BCCCCCCB",
    "BCCCCCCB",
    "BCCCCCCB",
    "WWWWWWWW");

static void draw_restroom_door(SDL_Renderer *r, float x, float y)
{
  bbc_sprite(r, &SPR_WC_DOOR, x, y, 1, NULL);
  bbc_text_inverse(r, x + 8.0f, y + 6.0f, 1.0f, BBC_WHITE, BBC_BLACK, "WC");
}

/*
 * The stair door, and the one thing about it a player has to read from across
 * the floor: whether it opens.
 *
 * The frame *is* the signal, slot 1 below. Red is locked, green is open, and the
 * door of a sector that leaves by its window is boarded and blue, because on
 * those floors it is not a way out at all and must not look like one waiting
 * for a card. The reader says the same thing in two cells at the only size the
 * font has — two characters is what a card reader has ever shown anybody — and
 * the open door's reader flashes, because a door that has just opened is the
 * one change on the floor worth shouting about.
 */
BBC_SPRITE(SPR_EXIT_DOOR,
    "11111111",
    "1WWWWWW1",
    "1B1111B1",
    "1BBBBBB1",
    "1BKKKKB1",
    "1BKKKKB1",
    "1BKKKKB1",
    "1BKKKKB1",
    "1BKKKKB1",
    "1BBBBBB1",
    "1BBBBB11",
    "1BWWWBB1",
    "1BBBBBB1",
    "1BBBBBB1",
    "1BBBBBB1",
    "11111111");

static void draw_exit(SDL_Renderer *r, const Game *game, float x, float y)
{
  bool blocked = game->gameplay.level.map.has_window;
  bool unlocked = game->gameplay.level.runtime.exit_unlocked && !blocked;
  BbcColour signal = blocked ? BBC_BLUE : (unlocked ? BBC_GREEN : BBC_RED);
  BbcColour slots[BBC_SPRITE_SLOTS] = {signal, signal, signal, signal};
  bbc_sprite(r, &SPR_EXIT_DOOR, x, y, 1, slots);
  bbc_text(r, x + 8.0f, y + 10.0f, 1.0f,
           unlocked ? bbc_flash_pair(BBC_GREEN, BBC_WHITE) : signal,
           blocked ? "--" : (unlocked ? "GO" : "NO"));
  if (blocked)
  {
    /* Boards across it: brown, which is red and yellow on a checkerboard. */
    bbc_dither(r, BBC_RED, BBC_YELLOW, BBC_DITHER_CHECK, x, y + 4.0f, 32.0f,
               4.0f);
    bbc_dither(r, BBC_RED, BBC_YELLOW, BBC_DITHER_CHECK, x, y + 22.0f, 32.0f,
               4.0f);
    cell(r, BBC_WHITE, x, y, 1, 2, 1, 1);
    cell(r, BBC_WHITE, x, y, 6, 11, 1, 1);
  }
}

/*
 * The one window in an interior that is a way out, so the one that shows the
 * outside: night through the glass with a few lit windows in the tower across
 * the street, keyed to this window's own place in the wall so they never boil,
 * a green frame because it is the exit, and the arrow that says which way.
 */
BBC_SPRITE(SPR_INTERIOR_WINDOW,
    "11111111",
    "1KKKKKK1",
    "1KKKKKK1",
    "1KKKKKK1",
    "1KKKKKK1",
    "1KKKKKK1",
    "1BBBBBB1",
    "1KKKKKK1",
    "1KKKKKK1",
    "1KKKKKK1",
    "1KKKKKK1",
    "1KKKKKK1",
    "11111111",
    "WWWWWWWW",
    "BBBBBBBB");

static void draw_open_window(SDL_Renderer *r, float x, float y, int col,
                             int row)
{
  static const BbcColour FRAME[BBC_SPRITE_SLOTS] = {BBC_GREEN, BBC_GREEN,
                                                    BBC_GREEN, BBC_GREEN};
  bbc_sprite(r, &SPR_INTERIOR_WINDOW, x, y, 1, FRAME);
  unsigned view = tile_hash(col, row) ^ 0x2c1b3c6du;
  for (int lamp = 0; lamp < 4; ++lamp)
  {
    unsigned lh = mix_hash(view + (unsigned)lamp * 0x9e3779b9u);
    int lc = 1 + (int)(lh % 6u);
    int lr = (int)((lh >> 8) % 2u) ? 1 + (int)((lh >> 12) % 5u)
                                    : 7 + (int)((lh >> 12) % 5u);
    cell(r, ((lh >> 16) & 1u) ? BBC_YELLOW : BBC_WHITE, x, y, lc, lr, 1, 1);
  }
  bbc_text(r, x + 12.0f, y + 15.0f, 1.0f, BBC_GREEN, ">");
}

/* ---- The climb --------------------------------------------------------- */

/*
 * A window in the tower's face: a surround and a sill in the face's own blue,
 * black glass with a transom across it, and — on one window in three, on its
 * own clock — the lamp colour behind the glass while somebody in that office
 * has the light on. The surround is blue rather than the cornice's paint on
 * purpose: there are dozens of these on a wall and the ledges between them are
 * what a climb is routed around, so a sill must never look like something a
 * foot could stand on.
 */
static void draw_facade_closed_window(SDL_Renderer *r, float x, float y,
                                      unsigned variant, float world_t,
                                      const LevelThemePaint *paint)
{
  bool lit = false;
  if (variant % 3u == 0u)
  {
    float period = 6.5f + (float)(variant % 5u) * 0.9f;
    float phase_offset = (float)(variant % 23u) * 1.17f;
    float phase = fmodf(world_t + phase_offset, period);
    lit = phase < period * 0.52f + 0.25f;
  }
  BbcColour frame = paint->backdrop;

  cell(r, frame, x, y, 0, 0, 8, 1);
  cell(r, frame, x, y, 0, 22, 8, 1);
  cell(r, frame, x, y, 0, 1, 1, 21);
  cell(r, frame, x, y, 7, 1, 1, 21);
  cell(r, lit ? paint->lamp : BBC_BLACK, x, y, 1, 1, 6, 21);
  cell(r, frame, x, y, 1, 11, 6, 1);
  /* A drawn blind on one window in seven: the lower half of a lit pane, or a
   * blue sheet behind a dark one. And in one lit office in five somebody is
   * still at work, a blue head and shoulders against the lamp — the only
   * people on the climb who are not trying to stop it. */
  if (variant % 7u == 0u)
    cell(r, lit ? BBC_RED : frame, x, y, 4, 12, 3, 10);
  else if (lit && variant % 5u == 0u)
    bbc_sprite_solid(r, &SPR_REFLECTION, x + 8.0f, y + 32.0f, 1, frame);
  cell(r, frame, x, y, -1, 23, 10, 1);
}

/* An open window on the climb: the sash run up behind the head of the frame
 * and nothing in the opening. The one a climb ends at is framed in green and
 * points up, which is the same pair of signals an interior's way out gives. */
static void draw_facade_open_window(SDL_Renderer *r, float x, float y,
                                    bool destination,
                                    const LevelThemePaint *paint)
{
  BbcColour frame = destination ? BBC_GREEN : paint->backdrop;
  cell(r, frame, x, y, 0, 0, 8, 1);
  cell(r, frame, x, y, 0, 22, 8, 1);
  cell(r, frame, x, y, 0, 1, 1, 21);
  cell(r, frame, x, y, 7, 1, 1, 21);
  cell(r, BBC_BLACK, x, y, 1, 1, 6, 21);
  bbc_dither(r, BBC_BLACK, paint->backdrop, BBC_DITHER_ROWS, x + 4.0f,
             y + 2.0f, 24.0f, 8.0f);
  cell(r, frame, x, y, -1, 23, 10, 1);
  if (destination)
    bbc_text(r, x + 12.0f, y + 20.0f, 1.0f,
             bbc_flash_pair(BBC_GREEN, BBC_WHITE), "^");
}

/*
 * Masonry on the facade: the projecting stone cornice that caps each floor,
 * the thing the climb is actually routed around and the thing bricks shatter
 * against. Its top face is the paint's trim, the brightest line on the wall,
 * because it is the line a hand reaches for; under it the stone face, a drip
 * mould, and a soffit falling into shade so the climbable gaps between runs
 * stay obvious. Some tiles carry a plant unit on top; that is only paint, since
 * a lone solid tile between two cornices would seal the gap the player needs.
 */
static void draw_facade_ledge(SDL_Renderer *r, const Level *level, int col,
                              int row, float x, float y, float world_t,
                              bool steady)
{
  const LevelThemePaint *paint = paint_of(level);
  bool left = level_is_solid(level, col - 1, row);
  bool right = level_is_solid(level, col + 1, row);
  bool open_above = !level_is_solid(level, col, row - 1);
  unsigned variant = tile_hash(col, row);

  if (variant % 11u == 0u && open_above)
  {
    /* Condenser unit standing on the cornice: a louvred case with its fan
     * behind it, two frames of a blade turning, kept low so it never reads as
     * something to climb. */
    cell(r, BBC_BLUE, x, y, 1, -7, 5, 7);
    cell(r, paint->wall_ink, x, y, 1, -7, 5, 1);
    cell(r, BBC_CYAN, x, y, 2, -5, 1, 4);
    bool turn = !steady && (((int)(world_t * 6.0f) + (int)(variant % 7u)) & 1);
    if (turn)
      cell(r, BBC_WHITE, x, y, 3, -4, 2, 1);
    else
      cell(r, BBC_WHITE, x, y, 4, -5, 1, 3);
  }
  else if (variant % 13u == 0u && open_above)
  {
    /* Vent stack breaking up a long cornice. */
    cell(r, BBC_BLUE, x, y, 3, -6, 2, 6);
    cell(r, paint->wall_ink, x, y, 2, -7, 4, 1);
  }

  cell(r, paint->trim, x, y, 0, 0, 8, 1);
  cell(r, paint->wall, x, y, 0, 1, 8, 5);
  /* The cornice is cut from blocks like the wall it sits on, two to a tile,
   * each joint a row higher or lower off the stone's own hash so a long run is
   * not extruded. */
  for (int block = 0; block < 2; ++block)
  {
    unsigned bh = tile_hash(col * 2 + block, row + 601);
    cell(r, paint->wall_ink, x, y, block * 4, (bh & 1u) ? 1 : 2, 1, 4);
  }
  cell(r, paint->wall_ink, x, y, 0, 6, 8, 1);
  bbc_dither(r, BBC_BLACK, paint->wall_ink, BBC_DITHER_CHECK, x, y + 14.0f,
             32.0f, 4.0f);
  bbc_dither(r, BBC_BLACK, paint->wall_ink, BBC_DITHER_SPARSE, x, y + 18.0f,
             32.0f, 14.0f);
  if (!left)
    cell(r, paint->trim, x, y, 0, 0, 1, 6);
  if (!right)
    cell(r, paint->trim, x, y, 7, 0, 1, 6);
}

/* A gull on a sill, facing out over the street it will come in from. */
BBC_SPRITE(SPR_PERCHED_GULL,
    "...WY",
    "WWWW.",
    ".WW..",
    "..Y..");

static void draw_facade_hazard_source(SDL_Renderer *r,
                                      const FacadeHazardSpawn *spawn,
                                      float cam_x, float oy, float world_t,
                                      float windup,
                                      const LevelThemePaint *paint)
{
  float x = spawn->x - TILE_SIZE * 0.5f - cam_x;
  float y = spawn->y - TILE_SIZE * 0.5f + oy;
  if (spawn->type == FACADE_HAZARD_BIRD)
  {
    draw_facade_closed_window(r, x, y,
                              tile_hash((int)(spawn->x / TILE_SIZE),
                                        (int)(spawn->y / TILE_SIZE)),
                              world_t, paint);
    /* A perched bird telegraphs where birds come from; it bobs a whole row at
     * a time, because a figure moves a BBC pixel or not at all. */
    float bob = sinf(world_t * 3.0f + spawn->y * 0.02f) > 0.0f ? -2.0f : 0.0f;
    bbc_sprite(r, &SPR_PERCHED_GULL, x + 8.0f, y + 38.0f + bob, 1, NULL);
    return;
  }

  draw_facade_open_window(r, x, y, false, paint);
  /* Hands and a held brick make the danger readable before the first throw. */
  cell(r, BBC_WHITE, x, y, 2, 17, 1, 1);
  cell(r, BBC_WHITE, x, y, 5, 17, 1, 1);
  cell(r, BBC_RED, x, y, 3, 15, 2, 2);

  if (windup <= 0.0f)
    return;

  /* The wind-up is the contract with the player: the thrower leans out and
   * raises the object for a beat before anything is in the air. He leans a
   * whole row at a time and the alarm over him flashes, because this is the
   * one warning the climb gives. */
  int lean = (int)(3.0f * (1.0f - windup));
  cell(r, BBC_RED, x, y, 2, 10 - lean, 4, 8);
  cell(r, BBC_WHITE, x, y, 3, 6 - lean, 2, 4);
  cell(r, BBC_BLACK, x, y, 4, 7 - lean, 1, 1);
  cell(r, BBC_WHITE, x, y, 6, 6 - lean, 1, 3);
  cell(r, BBC_YELLOW, x, y, 6, 3 - lean, 2, 3);
  bbc_text(r, x + 12.0f, y - 10.0f, 1.0f, bbc_flash(BBC_RED), "!");
}

/* ---- What watches and what is worked ----------------------------------- */

/*
 * A terminal cabinet, and its four states told apart at a glance.
 *
 * The console a sector can be hacked from is cyan, the building's technology
 * switched on; every other one is a dead blue box with a red standby dot. On
 * the live one the screen scrolls a log over an upload bar, and the bar is the
 * state — empty until the player starts, filling with the hack as it runs, full
 * once it is done — with a flashing beacon on the cabinet until then. The strip
 * under the screen says it again in two cells at scale 1.0, the only size the
 * font is sharp at: ON while it waits, OK when it is done, and NO in red when
 * it is done and the route it was meant to open is not there.
 */
static void draw_terminal(SDL_Renderer *r, float x, float y, bool active,
                          bool hacked, bool route_blocked, bool hacking,
                          float progress, float world_t)
{
  BbcColour casing = active ? BBC_CYAN : BBC_BLUE;
  cell(r, casing, x, y, 1, 0, 6, 15);
  cell(r, active ? BBC_WHITE : BBC_CYAN, x, y, 1, 0, 6, 1);
  cell(r, BBC_BLUE, x, y, 1, 15, 6, 1);
  cell(r, BBC_BLACK, x, y, 2, 1, 4, 6);
  cell(r, BBC_BLACK, x, y, 2, 8, 4, 5);

  if (active)
  {
    /* The log: two lines of differing lengths, stepping up the screen. */
    int scroll = (int)(world_t * 3.0f);
    for (int line = 0; line < 2; ++line)
    {
      unsigned lh = mix_hash((unsigned)(scroll + line) * 0x45d9f3bu);
      cell(r, BBC_GREEN, x, y, 2, 1 + line * 2, 1 + (int)(lh % 4u), 1);
    }
    int fill = hacked ? 4 : 0;
    if (!hacked && hacking)
    {
      fill = (int)(progress / TERMINAL_HACK_TIME * 4.0f + 0.5f);
      if (fill > 4)
        fill = 4;
    }
    cell(r, BBC_BLUE, x, y, 2, 5, 4, 1);
    if (fill > 0)
      cell(r, BBC_GREEN, x, y, 2, 5, fill, 1);
    if (!hacked && fill < 4 && bbc_flash_on())
      cell(r, BBC_WHITE, x, y, 2 + fill, 5, 1, 1);
    /* The beacon on the cabinet, until the job here is done. */
    cell(r, hacked ? BBC_GREEN : bbc_flash_pair(BBC_GREEN, BBC_WHITE), x, y, 3,
         -2, 2, 1);
  }
  else
  {
    cell(r, BBC_BLUE, x, y, 2, 1, 4, 6);
    cell(r, BBC_RED, x, y, 3, 4, 1, 1);
  }

  BbcColour word = !active ? BBC_BLUE
                           : (hacked && route_blocked ? BBC_RED : BBC_GREEN);
  bbc_text(r, x + 8.0f, y + 18.0f, 1.0f, word,
           active ? (hacked ? (route_blocked ? "NO" : "OK") : "ON") : "--");
  /* Two keys on the plinth. */
  cell(r, BBC_YELLOW, x, y, 2, 13, 1, 1);
  cell(r, BBC_RED, x, y, 5, 13, 1, 1);
}

/*
 * A ceiling camera's beam, drawn as exactly what it sees.
 *
 * **The beam is the mechanic, so the beam is the drawing.** Everything else
 * that watches Chuck in this building is a figure whose facing the player reads
 * off the sprite; a camera is a fitting the size of a fist, and if the cone it
 * sweeps is not on screen then the one thing the player has to time is
 * invisible. There is no light to draw it with, so it is drawn the way a BBC
 * game drew a searchlight: a dotted wedge, its two edges solid and its body a
 * sparse lattice of dots in the beam's colour.
 *
 * And it is the camera's own geometry rather than a picture of it: every cell
 * is put through the test `camera_sees_player` makes — below the lens, inside
 * `CAMERA_RANGE`, inside `CAMERA_CONE_HALF_ANGLE` of the sweep, and with
 * `gameplay_sight_line_clear` from the lens to it — so the wedge stops at the
 * slab it cannot see through and leaves a shadow behind the crate it cannot see
 * past, which is exactly where a player can stand. The dither is keyed to the
 * world, so it is the wedge's edge that moves as it sweeps and not the dots
 * inside it.
 */
/* The body of the wedge: dots on a staggered lattice whose rows are twice as
 * close as its columns, because a BBC pixel is twice as wide as it is tall and a
 * lattice laid out square in cells is a set of vertical dashes on the glass.
 * `dense` is a quarter of the cells, for a beam that has found something; the
 * other is an eighth. */
static bool beam_dot(int gx, int gy, bool dense)
{
  if (dense)
    return (gy & 1) == 0 && ((gx + (gy >> 1)) & 1) == 0;
  return ((gy & 3) == 0 && (gx & 3) == 0) || ((gy & 3) == 2 && (gx & 3) == 2);
}

static void draw_camera_beam(SDL_Renderer *r, const GameplayState *state,
                             const SecurityCamera *camera, float angle,
                             BbcColour ink, bool dense, float cam_x, float oy,
                             int win_w, int win_h)
{
  const Level *lvl = &state->level;
  float wx = ((float)camera->col + 0.5f) * (float)TILE_SIZE;
  float wy = ((float)camera->row + 0.5f) * (float)TILE_SIZE;
  float bx = sinf(angle);
  float by = cosf(angle);
  float cos_half = cosf(CAMERA_CONE_HALF_ANGLE);
  int c0 = (int)floorf((wx - CAMERA_RANGE) / (float)BBC_PX_W);
  int c1 = (int)floorf((wx + CAMERA_RANGE) / (float)BBC_PX_W);
  int r0 = (int)floorf(wy / (float)BBC_PX_H);
  int r1 = (int)floorf((wy + CAMERA_RANGE) / (float)BBC_PX_H);

  for (int gy = r0; gy <= r1; ++gy)
  {
    float py = (float)gy * (float)BBC_PX_H + (float)BBC_PX_H * 0.5f;
    float dy = py - wy;
    float sy = (float)gy * (float)BBC_PX_H + oy;
    if (dy <= 2.0f || sy < HUD_HEIGHT || sy >= (float)win_h)
      continue;
    for (int gx = c0; gx <= c1; ++gx)
    {
      float px = (float)gx * (float)BBC_PX_W + (float)BBC_PX_W * 0.5f;
      float dx = px - wx;
      float sx = (float)gx * (float)BBC_PX_W - cam_x;
      if (sx < 0.0f || sx >= (float)win_w)
        continue;
      float distance = sqrtf(dx * dx + dy * dy);
      if (distance > CAMERA_RANGE)
        continue;
      float dot = (dx * bx + dy * by) / distance;
      if (dot < cos_half)
        continue;
      float off = acosf(dot > 1.0f ? 1.0f : dot);
      bool edge = distance * (CAMERA_CONE_HALF_ANGLE - off) < 3.0f ||
                  distance > CAMERA_RANGE - 3.0f;
      if (!edge && !beam_dot(gx, gy, dense))
        continue;
      if (level_is_solid(lvl, (int)floorf(px / (float)TILE_SIZE),
                         (int)floorf(py / (float)TILE_SIZE)) ||
          !gameplay_sight_line_clear(state, wx, wy, px, py))
        continue;
      bbc_plot(r, ink, sx, sy);
    }
  }
}

/*
 * A ceiling camera, its beam, and what the beam is currently doing.
 *
 * Three states, and each has to be told apart at a glance while running. Idle
 * is cyan — security, the building's own eye — and sparse. `alerted` is the
 * beat between the lens finding Chuck and the alarm going up, and it is red and
 * denser, because that beat is the whole warning the player gets. `suspicious`
 * is the fade after it has lost him — yellow, so "it nearly had you" and "it
 * has you" can never be confused.
 *
 * A dead camera keeps its housing and loses its beam and its lamp, which is what
 * makes shooting one legible from across the room. The tell-tale lamp flashes
 * only while it is alerted, through `bbc_flash`, so it holds still under the
 * reduced-motion switch; the beam itself always sweeps, because the sweep is
 * gameplay and a fixed cone nobody could cross would be a different game.
 */
static void draw_security_camera(SDL_Renderer *r, const GameplayState *state,
                                 const SecurityCamera *camera, float x,
                                 float y, float angle, bool working,
                                 bool alerted, bool suspicious, float cam_x,
                                 float oy, int win_w, int win_h)
{
  float cx = x + TILE_SIZE * 0.5f;

  if (working)
    draw_camera_beam(r, state, camera, angle,
                     alerted ? BBC_RED : (suspicious ? BBC_YELLOW : BBC_CYAN),
                     alerted, cam_x, oy, win_w, win_h);

  /* The housing: a bracket off the slab and a body that leans a cell the way
   * the beam points, so the fitting reads as aimed even with the beam switched
   * off by a round. */
  float lean = sinf(angle) * 5.0f;
  float hx = cx - 8.0f + lean;
  bbc_fill(r, BBC_BLUE, cx - 4.0f, y, 4.0f, 4.0f);
  bbc_fill(r, working ? BBC_WHITE : BBC_BLUE, hx, y + 4.0f, 16.0f, 2.0f);
  bbc_fill(r, working ? BBC_CYAN : BBC_BLUE, hx, y + 6.0f, 16.0f, 4.0f);
  bbc_fill(r, BBC_BLUE, hx, y + 10.0f, 16.0f, 2.0f);
  /* The lens under the end it is pointing with, and the tell-tale on the
   * body. */
  float lens_x = cx - 2.0f + lean * 1.4f;
  bbc_fill(r, BBC_BLACK, lens_x, y + 10.0f, 4.0f, 4.0f);
  if (working)
  {
    bbc_plot(r, BBC_WHITE, lens_x, y + 12.0f);
    BbcColour lamp = alerted ? bbc_flash(BBC_RED)
                             : (suspicious ? BBC_YELLOW : BBC_GREEN);
    bbc_plot(r, lamp, hx + (lean >= 0.0f ? 0.0f : 12.0f), y + 6.0f);
  }
}

/*
 * A wall-mounted alarm call point: a red box, the white glass of its button, a
 * lamp over it and a conduit down the wall.
 *
 * The lamp is the state. Dark while the building is quiet, yellow while a guard
 * has his hand on it — and the glass with it, the button pressed — and flashing red and cyan the way every alarm on the
 * machine flashed once it is up — through `bbc_flash`, which holds it red under
 * the reduced-motion switch. The point that raised it also throws out four
 * sparks on the lit half of the flash, so the source can be found on a floor
 * with several.
 */
BBC_SPRITE(SPR_ALARM_SWITCH,
    ".11.",
    "RRRR",
    "RRRR",
    "R22R",
    "R22R",
    "R22R",
    "RRRR",
    "RRRR",
    ".BB.",
    ".BB.",
    ".BB.",
    ".BB.");

static void draw_alarm_switch(SDL_Renderer *r, float x, float y,
                              bool alarm_active, bool source, bool being_used)
{
  BbcColour lamp = alarm_active ? bbc_flash(BBC_RED)
                                : (being_used ? BBC_YELLOW : BBC_BLACK);
  BbcColour slots[BBC_SPRITE_SLOTS] = {
      lamp, alarm_active || being_used ? BBC_YELLOW : BBC_WHITE, BBC_BLACK,
      BBC_BLACK};
  bbc_sprite(r, &SPR_ALARM_SWITCH, x + 8.0f, y + 6.0f, 1, slots);
  if (alarm_active && source && bbc_flash_on())
  {
    cell(r, BBC_RED, x, y, 1, 2, 1, 1);
    cell(r, BBC_RED, x, y, 6, 2, 1, 1);
    cell(r, BBC_RED, x, y, 1, 7, 1, 1);
    cell(r, BBC_RED, x, y, 6, 7, 1, 1);
  }
}

/* ---- Furniture ---------------------------------------------------------- */

/*
 * The props are sprites, one tile each and standing on the tile below, so they
 * read as objects on a floor rather than as texture on a wall. They are drawn
 * mostly in the room's blue with one colour of their own, which keeps them
 * quieter than the cast that walks past them: the eye reads bright on black as
 * near and blue on black as far, and furniture is the middle distance.
 */

BBC_SPRITE(SPR_OFFICE_CHAIR,
    "........",
    "........",
    "..CC....",
    ".BCB....",
    ".BCB....",
    ".BCB....",
    ".BCB....",
    ".BBB....",
    ".BCCCCC.",
    ".BBBBBB.",
    "....W...",
    "....W...",
    "....W...",
    "..BBBBB.",
    ".BB...BB",
    ".W.....W");

/* A workstation: monitor, a brown desktop — red and yellow on a checkerboard —
 * a drawer pedestal and a mug. */
BBC_SPRITE(SPR_OFFICE_DESK,
    "........",
    "..BBBBB.",
    "..BCCCB.",
    "..BCWCB.",
    "..BCCCB.",
    "..BBBBB.",
    "....B...",
    "...BBBWW",
    "YRYRYRYR",
    "RYRYRYRY",
    "B....BBB",
    "B....BWB",
    "B....BBB",
    "B....BWB",
    "B....BBB",
    "B....B.B");

BBC_SPRITE(SPR_FILING_CABINET,
    "........",
    ".BBBBBB.",
    ".BCCCCB.",
    ".BBBBBB.",
    ".BCCCCB.",
    ".BCWWCB.",
    ".BBBBBB.",
    ".BCCCCB.",
    ".BCWWCB.",
    ".BBBBBB.",
    ".BCCCCB.",
    ".BCWWCB.",
    ".BBBBBB.",
    ".BBBBBB.",
    ".BBBBBB.",
    ".B....B.");

/* A copier with a sheet left in the tray and its green ready lamp. */
BBC_SPRITE(SPR_COPIER,
    "........",
    "........",
    "........",
    "..WWWW..",
    "..BCCB..",
    ".BBBBBB.",
    ".BKKKKB.",
    ".BWWWWB.",
    ".BBBBGB.",
    ".BBBBBB.",
    ".BCCCCB.",
    ".BBBBBB.",
    ".BCCCCB.",
    ".BBBBBB.",
    ".BBBBBB.",
    ".B....B.");

/* A small rack: four units, each with a status lamp in a slot so the lamps can
 * blink on clocks of their own. */
BBC_SPRITE(SPR_OFFICE_RACK,
    ".BBBBBB.",
    ".BCCCCB.",
    ".BKKKKB.",
    ".BKCK1B.",
    ".BBBBBB.",
    ".BKKKKB.",
    ".BKCK2B.",
    ".BBBBBB.",
    ".BKKKKB.",
    ".BKCK3B.",
    ".BBBBBB.",
    ".BKKKKB.",
    ".BKCK4B.",
    ".BBBBBB.",
    ".BBBBBB.",
    ".B....B.");

static void draw_office_equipment(SDL_Renderer *r, float x, float y,
                                  unsigned variant, float world_t)
{
  if (variant == 0u)
  {
    bbc_sprite(r, &SPR_FILING_CABINET, x, y, 1, NULL);
  }
  else if (variant == 1u)
  {
    bbc_sprite(r, &SPR_COPIER, x, y, 1, NULL);
  }
  else
  {
    BbcColour leds[BBC_SPRITE_SLOTS];
    for (int unit = 0; unit < 4; ++unit)
    {
      bool lit = ((int)(world_t * (2.0f + (float)unit * 0.31f)) + unit) % 3 != 0;
      leds[unit] = lit ? BBC_GREEN : BBC_BLACK;
    }
    bbc_sprite(r, &SPR_OFFICE_RACK, x, y, 1, leds);
  }
}

/*
 * Front-of-house fittings.
 *
 * The public floor of the building is furnished from this set rather than from
 * the office one: a lobby dressed in workstations and server racks reads as an
 * office floor no matter what the walls are made of. Everything here is stone,
 * brass, upholstery and planting, and every piece is a side profile inside one
 * tile so a run of them still reads at a glance.
 */

/* Reception desk: a polished cyan top, a front of blue stone veined in cyan,
 * and a brass reveal over the plinth. The tile runs into its neighbours, so it
 * has no ends of its own; the black joint at its left edge is what cuts a run
 * of them into slabs.
 *
 * Nothing on it is white and its top is not yellow, and that is the rule
 * rather than a taste: the lobby's masonry is white stone with a yellow lip,
 * and a counter that wore the same two colours would be a slab of floor a
 * player could not stand on. */
BBC_SPRITE(SPR_LOBBY_COUNTER,
    "........",
    "........",
    "CCCCCCCC",
    "BBBBBBBB",
    "KBBBBBBB",
    "KBBBBBBB",
    "KBBBBBBB",
    "KBBBBBBB",
    "KBBBBBBB",
    "KBBBBBBB",
    "KBBBBBBB",
    "KBBBBBBB",
    "YYYYYYYY",
    "KBBBBBBB",
    "KBBBBBBB",
    "BBBBBBBB");

static void draw_lobby_counter(SDL_Renderer *r, float x, float y,
                               unsigned variant)
{
  bbc_sprite(r, &SPR_LOBBY_COUNTER, x, y, 1, NULL);
  /* Veining off the tile, so the run is stone rather than one extruded box. */
  cell(r, BBC_CYAN, x, y, 2 + (int)(variant % 3u), 5, 1, 1);
  cell(r, BBC_CYAN, x, y, 3 + (int)((variant >> 3) % 3u), 6, 1, 1);
  if ((variant & 1u) == 0u)
    cell(r, BBC_CYAN, x, y, 5, 9, 2, 1);

  /* One tile of the run carries the visitor terminal, the rest a card tray. */
  if ((variant % 3u) == 0u)
  {
    cell(r, BBC_BLUE, x, y, 2, -4, 4, 5);
    cell(r, BBC_CYAN, x, y, 3, -3, 2, 3);
    cell(r, BBC_WHITE, x, y, 3, -3, 1, 1);
    cell(r, BBC_BLUE, x, y, 3, 1, 2, 1);
  }
  else
  {
    cell(r, BBC_WHITE, x, y, 4, 1, 3, 1);
  }
}

/* Waiting-area bench, seen from the side: a buttoned red back, a deep seat
 * with its cushion seam, a rolled arm at the far end, and brass legs. */
BBC_SPRITE(SPR_LOBBY_SOFA,
    "........",
    "........",
    "........",
    ".RR.....",
    "RRRR....",
    "RWRR....",
    "RRRR....",
    "RRWR..RR",
    "RRRR.RRR",
    "RRRRRRRR",
    "RRRRKRRR",
    "RRRRRRRR",
    ".BBBBBB.",
    ".Y....Y.",
    ".Y....Y.",
    ".Y....Y.");

/* A palm in a planter. Two drawings of the fronds, chosen per planter, so a
 * row of them is not stamped. */
BBC_SPRITE(SPR_LOBBY_PLANTER_A,
    "...G....",
    "G..G..G.",
    ".G.G.G..",
    "GGGGGGG.",
    "G..G..G.",
    "..GGG...",
    ".G.G.G..",
    "G..G..G.",
    "...G....",
    "...G....",
    ".YYYYY..",
    ".BBBBB..",
    ".BWBBB..",
    ".BBBBB..",
    ".BBBBB..",
    "..BBB...");

BBC_SPRITE(SPR_LOBBY_PLANTER_B,
    "........",
    "..G..G..",
    "G.G.G..G",
    ".GGGG.G.",
    "..GGGG..",
    ".G.GG.G.",
    "G..G..G.",
    "...G....",
    "...G....",
    "...G....",
    ".YYYYY..",
    ".BBBBB..",
    ".BBBWB..",
    ".BBBBB..",
    ".BBBBB..",
    "..BBB...");

static void draw_lobby_planter(SDL_Renderer *r, float x, float y,
                               unsigned variant)
{
  bbc_sprite(r, (variant & 1u) ? &SPR_LOBBY_PLANTER_B : &SPR_LOBBY_PLANTER_A,
             x, y, 1, NULL);
}

/*
 * Optical security gate: two waist-high pedestals with a glass leaf between
 * them, a reader on the near one and an indicator underfoot. It is the fitting
 * that explains why the stair door upstairs wants a card, and its lamp cycles
 * green and yellow — slot 1 — as visitors pass that the player never sees.
 */
BBC_SPRITE(SPR_LOBBY_TURNSTILE,
    "........",
    "........",
    "........",
    ".Y....Y.",
    "CCB..CCB",
    "CBBW.CBB",
    "CBBC.C1B",
    "CBBC.CBB",
    "CBBC.CBB",
    "CBBC.CBB",
    "CBBC.CBB",
    "CBBC.CBB",
    "CBBC.CBB",
    "CBBC.CBB",
    "CBB..CBB",
    "CBB11CBB");

static void draw_lobby_turnstile(SDL_Renderer *r, float x, float y,
                                 float world_t)
{
  bool clear = fmodf(world_t * 0.8f, 2.0f) < 1.4f;
  BbcColour signal = clear ? BBC_GREEN : BBC_YELLOW;
  BbcColour slots[BBC_SPRITE_SLOTS] = {signal, signal, signal, signal};
  bbc_sprite(r, &SPR_LOBBY_TURNSTILE, x, y, 1, slots);
}

/*
 * The two props that belong to tonight rather than to the building.
 *
 * Everything else in the level dressing was here yesterday and will be here
 * tomorrow. These two are the night itself left lying about: the case the
 * crew signed in on a maintenance docket in March, and the clock the whole
 * job is running against. Neither is collectable, neither is solid, and
 * neither tells the player anything they need in order to finish the sector —
 * they are there so that the story the manual states outright is also
 * something the player walks past.
 */

/* Standing closed against the wall, handle up: this is the one that has not
 * been opened yet. The same shell, corners, latches and red docket as the
 * manual's illustration, because the player is meant to recognise it. */
BBC_SPRITE(SPR_FLIGHT_CASE_SHUT,
    "........",
    "........",
    "........",
    "...BBB..",
    "..WBBBW.",
    "..BBBBB.",
    "..BRRRB.",
    "..BBBBB.",
    "..BBBBB.",
    "..WWWWW.",
    "..BBBBB.",
    "..BWKWB.",
    "..BKWKB.",
    "..BBBBB.",
    "..WBBBW.",
    "..WBBBW.");

/* Open on the floor with the lid propped back, and the long cutout in the foam
 * empty. What was in it is what the player has been picking up. */
BBC_SPRITE(SPR_FLIGHT_CASE_OPEN,
    "........",
    "........",
    "........",
    "........",
    "WW......",
    "BB......",
    "RB......",
    "BB......",
    "WB......",
    "WB......",
    "BB......",
    "BBWWWWWW",
    "BBBKKKKB",
    "BBBBKKBB",
    "BBBBBKBB",
    "WWWWWWWW");

static void draw_flight_case(SDL_Renderer *r, float x, float y,
                             unsigned variant)
{
  bbc_sprite(r, (variant & 1u) ? &SPR_FLIGHT_CASE_OPEN : &SPR_FLIGHT_CASE_SHUT,
             x, y, 1, NULL);
}

/*
 * The clock the whole night is running against.
 *
 * At 01:00 the overnight settlement leaves the roof by helicopter — the
 * sub-vault is emptied during the climb, and 01:00 is the last minute any of
 * it is still in the building. Everything tonight is timed off that deadline
 * rather than off the vault door. The dial reads the sector Chuck is
 * standing in, so the minute hand climbs the right-hand side of the face
 * across the campaign and is nearly back at the top by the roof — the player
 * is never told the time and never has to know it, but anyone who looks up in
 * two different sectors has been told how little of the night is left.
 *
 * The second hand is the part that matters: a dial with two static hands is a
 * painted clock, and a painted clock says nothing is happening. It is the one
 * red on the dial and it steps a BBC pixel at a time.
 */
BBC_SPRITE(SPR_WALL_CLOCK,
    "..BBB..",
    ".BWBWB.",
    "BWWWWWB",
    "BWWWWWB",
    "BWWWWWB",
    "BWWWWWB",
    "BWWWWWB",
    "BBWWWBB",
    "BWWWWWB",
    "BWWWWWB",
    "BWWWWWB",
    "BWWWWWB",
    ".BWBWB.",
    "..BBB..");

static void draw_wall_clock(SDL_Renderer *r, float x, float y,
                            int level_index, float world_t)
{
  float minute = NIGHT_CLOCK_FIRST_MINUTE +
                 (float)level_index * NIGHT_CLOCK_MINUTES_PER_SECTOR;
  if (minute > 58.0f)
    minute = 58.0f;

  /* A round face seven BBC pixels across and fourteen down, which is round on
   * the glass, screwed to the underside of the slab; the quarter marks are the
   * rim stepping in at twelve, three, six and nine. */
  cell(r, BBC_BLUE, x, y, 3, 0, 1, 1);
  bbc_sprite(r, &SPR_WALL_CLOCK, x, y + 2.0f, 1, NULL);
  float cx = x + 14.0f;
  float cy = y + 16.0f;

  /* Midnight has just gone, so the hour hand barely leaves the top; the minute
   * hand is the one carrying the reading. */
  float minute_angle = minute * (6.2831853f / 60.0f);
  float hour_angle = minute * (6.2831853f / 720.0f);
  float second_angle = floorf(fmodf(world_t, 60.0f)) * (6.2831853f / 60.0f);
  bbc_line(r, BBC_BLACK, cx, cy, cx + sinf(hour_angle) * 5.0f,
           cy - cosf(hour_angle) * 5.0f);
  bbc_line(r, BBC_BLACK, cx, cy, cx + sinf(minute_angle) * 9.0f,
           cy - cosf(minute_angle) * 10.0f);
  bbc_line(r, BBC_RED, cx, cy, cx + sinf(second_angle) * 8.0f,
           cy - cosf(second_angle) * 9.0f);
  bbc_plot(r, BBC_BLACK, cx, cy);
}

/* ---- The restroom's fittings ------------------------------------------- */

/* Side profile: cistern at the wall, projecting seat and a pedestal. White
 * ceramic, the brightest thing in the room after the cast, with its water and
 * its flush lever in the room's cyan. */
BBC_SPRITE(SPR_RESTROOM_TOILET,
    "........",
    "........",
    ".WWWW...",
    ".WCWW...",
    ".WWWW...",
    ".WWWW...",
    ".WWWW...",
    ".WWWW...",
    ".WWWWWWW",
    ".WWCCCCW",
    "...WWWWW",
    "...WWWW.",
    "....WW..",
    "....WW..",
    "....WW..",
    "...WWWW.");

/* Wall-mounted, with the tap over it and the trap under the bowl: an open bowl
 * of the room's water and a drain pipe, so it never reads as a monitor on a
 * stand. */
BBC_SPRITE(SPR_RESTROOM_BASIN,
    "........",
    "........",
    "........",
    "........",
    "...WW...",
    "....W...",
    "....W...",
    "WWWWWWWW",
    "WCCCCCCW",
    ".WWWWWW.",
    "..WWWW..",
    "...BB...",
    "...BB...",
    "...BBB..",
    "........",
    "........");

/* Wall-hung bowl with an exposed flush pipe. Shorter than the basin and hung
 * higher, so a row of them reads differently at a glance. */
BBC_SPRITE(SPR_RESTROOM_URINAL,
    "...W....",
    "...W....",
    "...W....",
    ".WWWWWW.",
    ".WWWWWW.",
    ".WKKKKW.",
    ".WKKKKW.",
    ".WKKKKW.",
    ".WKKKKW.",
    ".WCCCCW.",
    "..WWWW..",
    "...WW...",
    "...BB...",
    "........",
    "........",
    "........");

/* Narrow stall divider with a raised foot and a latch plate. */
BBC_SPRITE(SPR_RESTROOM_PARTITION,
    "......CB",
    "......CB",
    "......CB",
    "......CB",
    "......CB",
    "......CB",
    "......CB",
    "......WB",
    "......CB",
    "......CB",
    "......CB",
    "......CB",
    "......CB",
    "......CB",
    "......B.",
    "......B.");

/* A cubicle's frame, sized around the toilet: a header across the top and a
 * post either side, standing a cell outside the tile. */
static void draw_restroom_stall_frame(SDL_Renderer *r, float x, float y)
{
  cell(r, BBC_CYAN, x, y, -1, -3, 11, 1);
  cell(r, BBC_BLUE, x, y, -1, -2, 11, 1);
  cell(r, BBC_BLUE, x, y, -1, -1, 1, 17);
  cell(r, BBC_CYAN, x, y, 9, -1, 1, 17);
}

static void draw_restroom_stall_open(SDL_Renderer *r, float x, float y)
{
  /* The back wall of the cubicle, tiled in the room's blue, and the paper on
   * its holder. */
  cell(r, BBC_BLUE, x, y, 0, 3, 9, 1);
  cell(r, BBC_BLUE, x, y, 0, 9, 9, 1);
  cell(r, BBC_WHITE, x, y, 7, 1, 1, 3);
  bbc_sprite(r, &SPR_RESTROOM_TOILET, x, y, 1, NULL);
  draw_restroom_stall_frame(r, x, y);
  /* The door folded toward the viewer: a panel standing out past the post,
   * its head stepped where it swings, which is what makes "open" obvious. */
  cell(r, BBC_CYAN, x, y, 10, 1, 1, 13);
  cell(r, BBC_CYAN, x, y, 11, 2, 1, 11);
  cell(r, BBC_BLUE, x, y, 10, 14, 2, 1);
  cell(r, BBC_YELLOW, x, y, 11, 7, 1, 1);
}

static void draw_restroom_stall_closed(SDL_Renderer *r, float x, float y)
{
  draw_restroom_stall_frame(r, x, y);
  /* Full panel with a deliberate floor gap, hinges, latch and the red of an
   * occupied indicator. */
  cell(r, BBC_CYAN, x, y, 0, 0, 9, 14);
  cell(r, BBC_BLUE, x, y, 1, 1, 7, 1);
  cell(r, BBC_BLUE, x, y, 1, 12, 7, 1);
  cell(r, BBC_WHITE, x, y, 0, 2, 1, 2);
  cell(r, BBC_WHITE, x, y, 0, 9, 1, 2);
  cell(r, BBC_RED, x, y, 5, 4, 2, 1);
  cell(r, BBC_YELLOW, x, y, 6, 7, 1, 1);
}

/* ---- The plant set ------------------------------------------------------ */

/*
 * Four props for the rooms the office and foyer sets had nothing to say about.
 * See `DecorationType` in [level.h](level.h) for why they exist. All four are
 * a tile wide and stand on the tile below, and each is varied off `tile_hash`
 * so a run of them is not a run of the same object.
 */

/* A pallet — brown boards on red bearers — carrying two drums with the hazard
 * band every drum in a plant room carries. */
BBC_SPRITE(SPR_PALLET_DRUMS,
    "........",
    "........",
    "........",
    "........",
    "........",
    "WWW.WWW.",
    "BBB.BBB.",
    "BCB.BCB.",
    "YYY.YYY.",
    "BBB.BBB.",
    "BCB.BCB.",
    "BBB.BBB.",
    "WWW.WWW.",
    "RYRYRYRY",
    "YRYRYRYR",
    "R..RR..R");

static void draw_plant_pallet(SDL_Renderer *r, float x, float y,
                              unsigned variant)
{
  if ((variant & 1u) == 0u)
  {
    bbc_sprite(r, &SPR_PALLET_DRUMS, x, y, 1, NULL);
    return;
  }
  /* Half the pallets carry sacks, three of them, of heights off the tile. */
  for (int i = 0; i < 3; ++i)
  {
    int h = 4 + (int)((variant >> (unsigned)(i + 1)) & 3u);
    cell(r, (i & 1) ? BBC_YELLOW : BBC_WHITE, x, y, i * 3, 13 - h, 2, h);
    cell(r, BBC_BLUE, x, y, i * 3, 12, 2, 1);
  }
  cell(r, BBC_RED, x, y, 0, 13, 8, 2);
  bbc_dither(r, BBC_RED, BBC_YELLOW, BBC_DITHER_CHECK, x, y + 26.0f, 32.0f,
             4.0f);
  cell(r, BBC_RED, x, y, 0, 15, 1, 1);
  cell(r, BBC_RED, x, y, 3, 15, 2, 1);
  cell(r, BBC_RED, x, y, 7, 15, 1, 1);
}

/* A cable reel stood on its edge, which is how a reel is left: the near cheek
 * seen face on, brown timber, a groove ring a little inside the rim, the hub it
 * turns on, and the tail of cable pinned along the floor. */
static void draw_plant_cable_reel(SDL_Renderer *r, float x, float y,
                                  unsigned variant)
{
  float dir = (variant & 2u) ? 1.0f : -1.0f;
  float tail_x = dir > 0.0f ? x + 16.0f : x + 4.0f;
  bbc_fill(r, BBC_BLUE, tail_x, y + 28.0f, 12.0f, 2.0f);

  float cx = x + 16.0f;
  float cy = y + 20.0f;
  dither_disc(r, BBC_RED, BBC_YELLOW, BBC_DITHER_CHECK, cx, cy, 11.5f);
  bbc_ring(r, BBC_RED, cx, cy, 8.0f);
  bbc_disc(r, BBC_YELLOW, cx, cy, 4.5f);
  bbc_plot(r, BBC_BLACK, cx, cy);
}

/* A pair of risers off the floor with a run across them, flanges where they
 * meet, and a red hand wheel on the valve, turned a little differently on
 * every one of them. */
static void draw_plant_pipe_rail(SDL_Renderer *r, float x, float y,
                                 unsigned variant)
{
  /* Steel is white on its lit side and cyan on the other: the run across the
   * top, the two risers off the floor, and a blue collar where each meets it. */
  cell(r, BBC_WHITE, x, y, 1, 5, 6, 1);
  cell(r, BBC_CYAN, x, y, 1, 6, 6, 1);
  cell(r, BBC_CYAN, x, y, 1, 7, 1, 9);
  cell(r, BBC_CYAN, x, y, 6, 7, 1, 9);
  cell(r, BBC_BLUE, x, y, 1, 4, 1, 4);
  cell(r, BBC_BLUE, x, y, 6, 4, 1, 4);
  cell(r, BBC_BLUE, x, y, 3, 7, 2, 2);

  float wx = x + 16.0f;
  float wy = y + 23.0f;
  bbc_ring(r, BBC_RED, wx, wy, 6.0f);
  if (variant & 1u)
  {
    bbc_line(r, BBC_RED, wx - 4.0f, wy, wx + 4.0f, wy);
    bbc_line(r, BBC_RED, wx, wy - 5.0f, wx, wy + 5.0f);
  }
  else
  {
    bbc_line(r, BBC_RED, wx - 4.0f, wy - 4.0f, wx + 4.0f, wy + 4.0f);
    bbc_line(r, BBC_RED, wx - 4.0f, wy + 4.0f, wx + 4.0f, wy - 4.0f);
  }
  bbc_plot(r, BBC_YELLOW, wx, wy);
}

/* A short post with two reflective bands, bolted down. */
BBC_SPRITE(SPR_PLANT_BOLLARD,
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "...WW...",
    "...WW...",
    "...YY...",
    "...YY...",
    "...WW...",
    "...YY...",
    "...WW...",
    "...WW...",
    "..BBBB..");

/*
 * A switch with no `default:`, deliberately. This used to be an if/else chain
 * ending in a bare `else` that drew the turnstile, which meant a decoration
 * added to `DecorType` tomorrow was drawn as a turnstile rather than reported:
 * the map would look wrong and nothing would say why. Every other table in the
 * game is held to the rule that a value the drawing side does not know about
 * must not compile — the legend against the editor's palette, a `SettingRow`
 * against the field it names — and this is that rule for the props. `-Wswitch`
 * is what enforces it, so the case list has to stay exhaustive and the
 * fall-through must not come back.
 */
/* Whether the map put a prop of `type` in the tile `dx` across from this one:
 * a chair turns to the desk it is drawn up to. */
static bool decoration_beside(const Level *lvl, const Decoration *decoration,
                              int dx, DecorationType type)
{
  for (int i = 0; i < lvl->map.decoration_count; ++i)
  {
    const Decoration *other = &lvl->map.decorations[i];
    if (other->type == type && other->row == decoration->row &&
        other->col == decoration->col + dx)
      return true;
  }
  return false;
}

static void draw_decoration(SDL_Renderer *r, const Level *lvl,
                            const Decoration *decoration, float cam_x,
                            float oy, float world_t, int level_index)
{
  float x = (float)decoration->col * (float)TILE_SIZE - cam_x;
  float y = (float)decoration->row * (float)TILE_SIZE + oy;
  unsigned variant = tile_hash(decoration->col, decoration->row);
  switch (decoration->type)
  {
  case DECOR_FLIGHT_CASE:
    draw_flight_case(r, x, y, variant);
    break;
  case DECOR_WALL_CLOCK:
    draw_wall_clock(r, x, y, level_index, world_t);
    break;
  case DECOR_OFFICE_CHAIR:
    bbc_sprite(r, &SPR_OFFICE_CHAIR, x, y,
               decoration_beside(lvl, decoration, 1, DECOR_OFFICE_DESK) ? 1
                                                                        : -1,
               NULL);
    break;
  case DECOR_OFFICE_DESK:
    bbc_sprite(r, &SPR_OFFICE_DESK, x, y, 1, NULL);
    break;
  case DECOR_OFFICE_EQUIPMENT:
    draw_office_equipment(r, x, y, variant % 3u, world_t);
    break;
  case DECOR_RESTROOM_TOILET:
    bbc_sprite(r, &SPR_RESTROOM_TOILET, x, y, 1, NULL);
    break;
  case DECOR_RESTROOM_BASIN:
    bbc_sprite(r, &SPR_RESTROOM_BASIN, x, y, 1, NULL);
    break;
  case DECOR_RESTROOM_URINAL:
    bbc_sprite(r, &SPR_RESTROOM_URINAL, x, y, 1, NULL);
    break;
  case DECOR_RESTROOM_PARTITION:
    bbc_sprite(r, &SPR_RESTROOM_PARTITION, x, y, 1, NULL);
    break;
  case DECOR_RESTROOM_STALL_OPEN:
    draw_restroom_stall_open(r, x, y);
    break;
  case DECOR_RESTROOM_STALL_CLOSED:
    draw_restroom_stall_closed(r, x, y);
    break;
  case DECOR_LOBBY_COUNTER:
    draw_lobby_counter(r, x, y, variant);
    break;
  case DECOR_LOBBY_SOFA:
    bbc_sprite(r, &SPR_LOBBY_SOFA, x, y, 1, NULL);
    break;
  case DECOR_LOBBY_PLANTER:
    draw_lobby_planter(r, x, y, variant);
    break;
  case DECOR_LOBBY_TURNSTILE:
    draw_lobby_turnstile(r, x, y, world_t);
    break;
  case DECOR_PLANT_PALLET:
    draw_plant_pallet(r, x, y, variant);
    break;
  case DECOR_PLANT_CABLE_REEL:
    draw_plant_cable_reel(r, x, y, variant);
    break;
  case DECOR_PLANT_PIPE_RAIL:
    draw_plant_pipe_rail(r, x, y, variant);
    break;
  case DECOR_PLANT_BOLLARD:
    bbc_sprite(r, &SPR_PLANT_BOLLARD, x, y, 1, NULL);
    break;
  }
}

/* ---- Pickups ------------------------------------------------------------ */

/*
 * The glint that says "take me": one white pixel at the pickup's corner,
 * lit for a moment every couple of seconds on a phase of its own — so a shelf
 * of them never flashes in unison — and a four-pointed star at the height of
 * it. Switched rather than faded, because there is nothing between lit and
 * not.
 */
static void draw_pickup_glint(SDL_Renderer *r, float x, float y, float world_t,
                              int salt)
{
  float phase = world_t * 0.42f +
                (float)(mix_hash((unsigned)salt + 17u) % 997u) * 0.001f;
  phase -= floorf(phase);
  if (phase > 0.16f)
    return;
  bbc_plot(r, BBC_WHITE, x, y);
  if (phase > 0.04f && phase < 0.12f)
  {
    bbc_plot(r, BBC_WHITE, x - 4.0f, y);
    bbc_plot(r, BBC_WHITE, x + 4.0f, y);
    bbc_plot(r, BBC_WHITE, x, y - 2.0f);
    bbc_plot(r, BBC_WHITE, x, y + 2.0f);
  }
}

typedef enum
{
  CARD_PLAIN,  /* an ordinary card lying on the floor */
  CARD_SHOWN,  /* the one the key-card intro is pointing at */
  CARD_WAITING /* one it is not pointing at yet */
} CardShowing;

/*
 * One pickup, drawn from the same icon the HUD and the manual use, centred on
 * the item and bobbing a whole BBC row at a time.
 *
 * The three throwables and the docket sheet are drawn by
 * [render_figures.c](render_figures.c), because the throwing pose is what
 * decides how a grenade has to look, and they keep the coordinates they always
 * had. A card the key-card intro is not pointing at is a blue silhouette — the
 * only way there is to put a thing in the background — and the one it is
 * pointing at stands in a flashing frame.
 */
static void draw_pickup(SDL_Renderer *r, const Item *it, int index,
                        float cam_x, float oy, float world_t,
                        CardShowing showing)
{
  float bob = sinf(world_t * 3.14159265f + (float)index * 0.7f) * 3.0f;
  float sx = it->x - cam_x;
  float sy = it->y + oy + bob;
  float x = sx - 7.0f;
  float y = sy - 9.0f;
  switch (it->type)
  {
  case ITEM_CARD:
    if (showing == CARD_WAITING)
    {
      bbc_sprite_solid(r, &SPR_ICON_CARD, sx - 8.0f, sy - 6.0f, 1, BBC_BLUE);
      return;
    }
    if (showing == CARD_SHOWN)
      bbc_frame(r, bbc_flash_pair(BBC_WHITE, BBC_CYAN), sx - 12.0f, sy - 10.0f,
                24.0f, 20.0f);
    bbc_sprite(r, &SPR_ICON_CARD, sx - 8.0f, sy - 6.0f, 1, NULL);
    break;
  case ITEM_GUN:
    bbc_sprite(r, &SPR_ICON_PISTOL, sx - 8.0f, sy - 3.0f, 1, NULL);
    break;
  case ITEM_GRENADE:
    draw_grenade(r, x + 3.0f, y + 4.0f, 0.0f);
    break;
  case ITEM_MEDKIT:
    bbc_sprite(r, &SPR_ICON_MEDKIT, sx - 10.0f, sy - 5.0f, 1, NULL);
    break;
  case ITEM_BAZOOKA:
    bbc_sprite(r, &SPR_ICON_BAZOOKA, sx - 16.0f, sy - 4.0f, 1, NULL);
    break;
  case ITEM_EVIDENCE:
    draw_evidence_pickup(r, x, y);
    break;
  case ITEM_FLASHBANG:
    draw_flashbang(r, x + 3.0f, y + 4.0f, 0.0f);
    break;
  }
  draw_pickup_glint(r, sx + 8.0f, sy - 8.0f, world_t, index);
}

/* A dropped magazine: the HUD's cartridge laid on the floor, white tips over a
 * brass body, so the pickup is read as ammunition before it is walked over. */
BBC_SPRITE(SPR_AMMO_DROP,
    "W.W",
    "YYY",
    "YYY",
    "YYY");

/* ---- Hazards ------------------------------------------------------------ */

/*
 * A bed of spikes: four steel points to a tile on a hazard-striped plate.
 *
 * Each point is a white cell at its tip and a white-and-red pair under it, lit
 * down its left face and red down its right, because red is what hurts and the
 * strip has to be read as a hazard before it is read as a shape. The plate is
 * the building's own warning marking, yellow and black.
 *
 * `y` is the top of the bed's own hazard box, not of its tile, and the strip is
 * exactly that box tall: three rows of tip, three of blade, two of plate. It
 * used to be handed the tile and draw its tips sixteen pixels down while the box
 * started at the tile's top, so the player was hurt by the air over the blades
 * and a hop under the route model's clearance had a take-off window of one
 * simulation step. The assertion is the picture and the box agreeing about the
 * one number they share; see SPIKE_H.
 */
_Static_assert(SPIKE_H == 16, "the spike strip is drawn sixteen rows tall");
BBC_SPRITE(SPR_SPIKE_STRIP,
    ".W...W..",
    ".W...W..",
    ".WR..WR.",
    "WWR.WWR.",
    "WWRRWWRR",
    "WWRRWWRR",
    "YYKKYYKK",
    "KYYKKYYK");

/*
 * A ceiling fan: the plate it hangs from, its rod, the motor with the hazard
 * band every moving guard in a plant room carries, and the rotor.
 *
 * The rotor is two frames, the way a BBC game turned anything: the blades seen
 * edge-on at their full reach, then end-on as a stub, swapped ten times a
 * second. A dotted line under them always marks how far they reach, because
 * that band — exactly `CEILING_FAN_BLADE_LENGTH` either side — is the one a
 * jump must not cross, and a frame that happens to show the stub must not make
 * it look narrower. Under the reduced-motion switch the rotor holds its long
 * frame.
 */
static void draw_ceiling_fan(SDL_Renderer *r, const CeilingFan *fan,
                             float cam_x, float oy, float world_t, int index,
                             bool steady)
{
  float cx = fan->x - cam_x;
  float cy = fan->y + oy;
  float plate_y = fan->mount_y + oy;
  float rod_top = plate_y + 4.0f;
  float rod_height = cy - rod_top;
  if (rod_height < 2.0f)
    rod_height = 2.0f;

  bbc_fill(r, BBC_BLUE, cx - 8.0f, plate_y, 16.0f, 4.0f);
  bbc_fill(r, BBC_WHITE, cx - 4.0f, rod_top, 4.0f, rod_height);

  for (float dx = -CEILING_FAN_BLADE_LENGTH; dx < CEILING_FAN_BLADE_LENGTH;
       dx += 8.0f)
    bbc_plot(r, BBC_BLUE, cx + dx, cy + 2.0f);

  bbc_fill(r, BBC_BLUE, cx - 8.0f, cy - 6.0f, 16.0f, 8.0f);
  bbc_fill(r, BBC_WHITE, cx - 8.0f, cy - 6.0f, 16.0f, 2.0f);
  for (int check = 0; check < 4; ++check)
    bbc_plot(r, (check & 1) ? BBC_BLACK : BBC_YELLOW,
             cx - 8.0f + (float)check * 4.0f, cy - 2.0f);

  bool stub = !steady && (((int)(world_t * 10.0f) + index) & 1);
  if (stub)
    bbc_fill(r, BBC_WHITE, cx - 8.0f, cy + 2.0f, 16.0f, 2.0f);
  else
    bbc_fill(r, BBC_WHITE, cx - CEILING_FAN_BLADE_LENGTH, cy + 2.0f,
             CEILING_FAN_BLADE_LENGTH * 2.0f, 2.0f);
  bbc_fill(r, BBC_RED, cx - 4.0f, cy + 4.0f, 8.0f, 2.0f);
}

/*
 * A crate: a yellow batten frame round red boards with black joints between
 * them, a diagonal brace, and the stencilled plate with the cross on it.
 *
 * Solid boards rather than a dithered brown, and that is a decision: a crate is
 * pushed, and a dither laid from the world's corner stays put while the box
 * slides over it, so the timber would crawl. What varies from one crate to the
 * next is keyed to its slot, not to where it has been pushed, so a crate keeps
 * its own scuffs as it slides across the floor.
 */
static void draw_crate(SDL_Renderer *r, const Crate *crate, int index,
                       float cam_x, float oy)
{
  enum
  {
    COLS = CRATE_W / BBC_PX_W,
    ROWS = CRATE_H / BBC_PX_H
  };
  float x = bbc_snap_x(crate->x - cam_x);
  float y = bbc_snap_y(crate->y + oy);
  unsigned h = mix_hash((unsigned)index * 0x9e3779b9u + 0x51u);

  cell(r, BBC_RED, x, y, 1, 1, COLS - 2, ROWS - 2);
  cell(r, BBC_BLACK, x, y, 1, ROWS / 3, COLS - 2, 1);
  cell(r, BBC_BLACK, x, y, 1, 2 * ROWS / 3, COLS - 2, 1);
  for (int row = 1; row < ROWS - 1; ++row)
  {
    int col = 1 + (ROWS - 2 - row) * (COLS - 3) / (ROWS - 3);
    cell(r, BBC_YELLOW, x, y, col, row, 1, 1);
  }
  cell(r, BBC_YELLOW, x, y, 0, 0, COLS, 1);
  cell(r, BBC_YELLOW, x, y, 0, ROWS - 1, COLS, 1);
  cell(r, BBC_YELLOW, x, y, 0, 1, 1, ROWS - 2);
  cell(r, BBC_YELLOW, x, y, COLS - 1, 1, 1, ROWS - 2);

  /* The stencilled plate. */
  cell(r, BBC_WHITE, x, y, 2, 5, 3, 4);
  bbc_text(r, x + 10.0f, y + 10.0f, 1.0f, BBC_BLACK, "X");

  /* Wear: a knock in the frame, off the crate's own hash. */
  if (h & 1u)
    cell(r, BBC_WHITE, x, y, 1 + (int)((h >> 3) % 4u), 0, 1, 1);
  if (h & 2u)
    cell(r, BBC_RED, x, y, 0, 3 + (int)((h >> 7) % 7u), 1, 1);
}

/*
 * A mine: a blue dome on a white skirt, with the lamp in its pressure plate.
 *
 * The lamp is the state. Red while it waits; once a foot has armed it, it
 * flashes white and red at twelve times a second, the fastest thing on the
 * screen and the one most worth holding still when asked — so under the
 * reduced-motion switch it is held *lit*: the light is what says the fuse is
 * running, and MINE_TRIGGER_DELAY is under half a second, so nothing is lost by
 * not counting it out.
 */
BBC_SPRITE(SPR_MINE,
    ".11.",
    "BBBB",
    "BBBB",
    "WWWW",
    "Y..Y");

static void draw_mine(SDL_Renderer *r, const Mine *mine, float cam_x, float oy,
                      bool steady)
{
  bool lit = mine->triggered &&
             (steady || ((int)(mine->timer * 24.0f) & 1) == 0);
  BbcColour lamp = lit ? BBC_WHITE : BBC_RED;
  BbcColour slots[BBC_SPRITE_SLOTS] = {lamp, lamp, lamp, lamp};
  bbc_sprite(r, &SPR_MINE, mine->x - cam_x, mine->y + oy, 1, slots);
}

/* A narrow pressure cylinder: a brass valve, a red shoulder for what is in it,
 * a yellow warning label, and the white of the lamp down its lit side. The
 * silhouette stays below a standing shot, which is the canister's own
 * mechanic. */
BBC_SPRITE(SPR_GAS_CANISTER,
    ".Y.",
    ".W.",
    "RRR",
    "WBB",
    "WYB",
    "WYB",
    "WBB",
    "BBB");

/* ---- What is in the air ------------------------------------------------- */

/* A rocket on its side, nose right: green tube, yellow warhead, fins at the
 * tail. Four BBC pixels by three is exactly `ROCKET_W` by `ROCKET_H`, which is
 * the ink the HUD's carried row is measured against. */
BBC_SPRITE(SPR_ROCKET_FLIGHT,
    "GG..",
    "GGGY",
    "GG..");

_Static_assert(ROCKET_W == 4 * BBC_PX_W && ROCKET_H == 3 * BBC_PX_H,
               "the rocket sprite is the rocket's box");

static void draw_rocket_sprite(SDL_Renderer *r, float x, float y, int dir, bool flame)
{
  bbc_sprite(r, &SPR_ROCKET_FLIGHT, x, y, dir, NULL);
  if (flame)
  {
    /* The exhaust: two cells behind the tail, which swap over as it travels so
     * the fire flickers without a clock. */
    bool flip = ((int)floorf(x / 8.0f) & 1) != 0;
    float near_x = dir > 0 ? x - 4.0f : x + (float)ROCKET_W;
    float far_x = dir > 0 ? x - 8.0f : x + (float)ROCKET_W + 4.0f;
    bbc_plot(r, flip ? BBC_RED : BBC_YELLOW, near_x, y + 2.0f);
    bbc_plot(r, flip ? BBC_YELLOW : BBC_RED, far_x, y + 2.0f);
  }
}

/* The same rocket travelling up or down the screen: its box rotates with it,
 * so it is `ROCKET_H` wide and `ROCKET_W` tall, nose toward where it is going. */
static void draw_vertical_rocket_sprite(SDL_Renderer *r, float x, float y,
                                        int dir)
{
  int nose = dir > 0 ? 7 : 0;
  int tail = dir > 0 ? 0 : 6;
  cell(r, BBC_GREEN, x, y, 0, 0, 1, 8);
  cell(r, BBC_GREEN, x, y, 1, tail, 1, 2);
  cell(r, BBC_YELLOW, x, y, 0, nose, 1, 1);
  float flame_y = dir > 0 ? y - 2.0f : y + (float)ROCKET_W;
  bool flip = ((int)floorf(y / 8.0f) & 1) != 0;
  bbc_plot(r, flip ? BBC_RED : BBC_YELLOW, x, flame_y);
  bbc_plot(r, flip ? BBC_YELLOW : BBC_RED, x, dir > 0 ? flame_y - 2.0f
                                                       : flame_y + 2.0f);
}

/*
 * A round in flight: a block of its colour with a bright leading cell, and one
 * dotted cell of trail behind it, which is what tells a fast pixel from a
 * stationary one. Chuck's are yellow with a white point; theirs are red with a
 * yellow one, because red is the enemy's fire.
 */
static void draw_round(SDL_Renderer *r, const Bullet *b, float cam_x, float oy,
                       BbcColour body, BbcColour point)
{
  float x = b->x - cam_x;
  float y = b->y + oy;
  if (fabsf(b->vy) > fabsf(b->vx))
  {
    bool down = b->vy > 0.0f;
    bbc_fill(r, body, x, y, (float)BULLET_H, (float)BULLET_W);
    bbc_fill(r, point, x, down ? y + (float)BULLET_W - 2.0f : y,
             (float)BULLET_H, 2.0f);
    bbc_plot(r, body, x, down ? y - 6.0f : y + (float)BULLET_W + 4.0f);
  }
  else
  {
    bool right = b->vx > 0.0f;
    bbc_fill(r, body, x, y, (float)BULLET_W, (float)BULLET_H);
    bbc_fill(r, point, right ? x + (float)BULLET_W - 4.0f : x, y, 4.0f,
             (float)BULLET_H);
    bbc_plot(r, body, right ? x - 8.0f : x + (float)BULLET_W + 4.0f, y);
  }
}

/* ---- The climb, as a whole --------------------------------------------- */

static bool facade_fixture_at(const Level *level, int col, int row)
{
  int start_col = (int)floorf((level->map.start_x + PLAYER_W * 0.5f) /
                              TILE_SIZE);
  int start_row = (int)floorf((level->map.start_y + PLAYER_H * 0.5f) /
                              TILE_SIZE);
  if ((col == start_col && row == start_row) ||
      (level->map.has_window && col == level->map.window_col &&
       row == level->map.window_row))
  {
    return true;
  }

  for (int i = 0; i < level->map.facade_hazard_spawn_count; ++i)
  {
    const FacadeHazardSpawn *spawn = &level->map.facade_hazard_spawns[i];
    int spawn_col = (int)floorf(spawn->x / TILE_SIZE);
    int spawn_row = (int)floorf(spawn->y / TILE_SIZE);
    if (col == spawn_col && row == spawn_row)
      return true;
  }
  return false;
}

/*
 * Wind is invisible, so it is drawn twice: as streaks that start during the
 * warning beat and lengthen once the gust lands — blue while it is coming,
 * white when it is here — and as a bracket around Chuck whenever a ledge upwind
 * of him is actually taking the push. Chevrons down the windward edge say which
 * way the wall is pushing, which the old wash of light used to.
 */
static void render_facade_wind(Game *game, int win_w, int win_h, float world_t,
                               float cam_x, float oy)
{
  SDL_Renderer *r = game->platform.renderer;
  const GameplayState *state = &game->gameplay;
  if (state->facade_wind_phase == FACADE_WIND_CALM)
    return;

  bool gusting = state->facade_wind_phase == FACADE_WIND_GUSTING;
  float intensity =
      gusting ? 1.0f
              : 1.0f - state->facade_wind_timer / FACADE_WIND_WARN_TIME;
  if (intensity < 0.0f)
    intensity = 0.0f;
  float dir = (float)state->facade_wind_dir;
  float span = (float)win_w + 160.0f;
  float field = (float)(win_h - HUD_HEIGHT);
  int streaks = gusting ? 28 : 6 + (int)(intensity * 14.0f);

  for (int i = 0; i < streaks; ++i)
  {
    unsigned h = tile_hash(i * 37 + 11, 909);
    float speed = 300.0f + (float)(h % 260u);
    float y = HUD_HEIGHT + fmodf((float)((h >> 7) % 4096u), field);
    float x = fmodf((float)(h % 617u) + world_t * speed * dir, span);
    if (x < 0.0f)
      x += span;
    x -= 80.0f;
    float length = 16.0f + (float)(h % 44u) * (0.5f + intensity * 0.5f);
    bbc_fill(r, gusting ? BBC_WHITE : BBC_BLUE, x, y, length, 2.0f);
  }

  if (gusting)
  {
    const char *chevron = dir > 0.0f ? ">" : "<";
    float edge = dir > 0.0f ? 8.0f : (float)win_w - 16.0f;
    for (float y = HUD_HEIGHT + 48.0f; y < (float)win_h - 16.0f; y += 96.0f)
      bbc_text(r, edge, y, 1.0f, bbc_flash_pair(BBC_CYAN, BBC_BLUE), chevron);
  }

  if (gusting && state->facade_wind_sheltered && !state->player.dying &&
      bbc_flash_on())
  {
    float px = state->player.x - cam_x;
    float py = state->player.y + oy;
    bbc_fill(r, BBC_GREEN, px - 4.0f, py - 4.0f, 8.0f, 2.0f);
    bbc_fill(r, BBC_GREEN, px + PLAYER_W - 4.0f, py - 4.0f, 8.0f, 2.0f);
    bbc_fill(r, BBC_GREEN, px - 4.0f, py + PLAYER_H + 2.0f, 8.0f, 2.0f);
    bbc_fill(r, BBC_GREEN, px + PLAYER_W - 4.0f, py + PLAYER_H + 2.0f, 8.0f,
             2.0f);
  }
}

/*
 * Has the reveal walked as far as this tile yet?
 *
 * The interior's structural loop asks `tiles_visible` directly and skips the
 * tile until it has; the facade drew *nothing* until `reveal.done` and then
 * drew all of it in one frame. What that cost is the whole beat: a climb is
 * entered through a window, so its reveal is held open for
 * `SECTOR_TALLY_HOLD_TIME` to carry the line written over it, and the picture
 * under that line was a flat backdrop for the full 3.8 seconds and a finished
 * wall on the frame after. Five of the seventeen sectors, every time they are
 * entered.
 *
 * A named helper rather than the interior's inline test because a facade is
 * built out of fixtures placed on the tile grid rather than out of the grid
 * itself — a window straddles the architectural spacing, a ledge is a `#`, a
 * hazard mount is neither — so the question is asked from four places and each
 * of them has to bound its own coordinates.
 */
static bool facade_tile_revealed(const Level *level, int col, int row)
{
  if (col < 0 || row < 0 || col >= level->map.width ||
      row >= level->map.height)
  {
    return level->reveal.done;
  }
  return level->reveal.tiles_visible[row][col];
}

static void render_facade_world(Game *game, int win_w, int win_h, float cam_x,
                                float cam_y)
{
  SDL_Renderer *r = game->platform.renderer;
  const Level *level = &game->gameplay.level;
  const LevelThemePaint *paint = paint_of(level);
  float oy = HUD_HEIGHT - cam_y;
  float world_t = game->presentation.render_clock;
  bool steady = game->settings.reduced_motion;

  render_background(game, win_w, win_h, cam_x, cam_y);

  /* Closed windows and gameplay windows share the same architectural grid.
   * A special fixture replaces its ordinary window instead of covering it. */
  for (int row = 3; row < level->map.height; row += 3)
  {
    float y = (float)row * (float)TILE_SIZE + oy;
    if (y < HUD_HEIGHT - 56.0f || y > (float)win_h + 8.0f)
      continue;
    for (int col = 4; col < level->map.width - 4; col += 4)
    {
      if (facade_fixture_at(level, col, row))
        continue;
      if (!facade_tile_revealed(level, col, row))
        continue;
      float x = (float)col * (float)TILE_SIZE - cam_x;
      draw_facade_closed_window(r, x, y, tile_hash(col, row), world_t, paint);
    }
  }

  float start_center = level->map.start_x + PLAYER_W * 0.5f;
  int start_col = (int)floorf(start_center / TILE_SIZE);
  int start_row = (int)floorf((level->map.start_y + PLAYER_H * 0.5f) /
                              TILE_SIZE);
  if (facade_tile_revealed(level, start_col, start_row))
    draw_facade_open_window(r, (float)start_col * (float)TILE_SIZE - cam_x,
                            (float)start_row * (float)TILE_SIZE + oy, false,
                            paint);
  if (level->map.has_window &&
      facade_tile_revealed(level, level->map.window_col,
                           level->map.window_row))
  {
    draw_facade_open_window(
        r, (float)level->map.window_col * (float)TILE_SIZE - cam_x,
        (float)level->map.window_row * (float)TILE_SIZE + oy, true, paint);
  }

  /* A hazard mount is drawn with the structure rather than with the men and
   * the birds it sends out, because `facade_fixture_at` has already taken its
   * ordinary window out of the grid above: left for later it would be a hole
   * in the wall for the length of the reveal rather than a fixture not yet
   * arrived. Its windup is nought until the sector starts, so what a revealing
   * wall draws is the mount at rest. */
  for (int i = 0; i < level->map.facade_hazard_spawn_count; ++i)
  {
    const FacadeHazardSpawn *spawn = &level->map.facade_hazard_spawns[i];
    if (!facade_tile_revealed(level, (int)floorf(spawn->x / TILE_SIZE),
                              (int)floorf(spawn->y / TILE_SIZE)))
    {
      continue;
    }
    float windup = game->gameplay.facade_hazard_windup_timers[i] /
                   THROWN_OBJECT_WINDUP;
    draw_facade_hazard_source(r, spawn, cam_x, oy, world_t, windup, paint);
  }

  /* Ledges and plant sit in front of the windows: they are the geometry the
   * climb is actually routed around. */
  int first_row = (int)floorf(cam_y / TILE_SIZE) - 1;
  int last_row = first_row + (win_h / TILE_SIZE) + 3;
  if (first_row < 0)
    first_row = 0;
  if (last_row >= level->map.height)
    last_row = level->map.height - 1;
  for (int row = first_row; row <= last_row; ++row)
  {
    for (int col = 0; col < level->map.width; ++col)
    {
      if (level->map.tiles[row][col] != TILE_WALL)
        continue;
      if (!facade_tile_revealed(level, col, row))
        continue;
      draw_facade_ledge(r, level, col, row,
                        (float)col * (float)TILE_SIZE - cam_x,
                        (float)row * (float)TILE_SIZE + oy, world_t, steady);
    }
  }

  /* Everything below this line is the sector running, and the interior's own
   * structural loop returns in exactly the same place: during a reveal nobody
   * is drawn yet. */
  if (!level->reveal.done)
    return;

  /* Pickups left out on the wall for climbers willing to detour. */
  for (int i = 0; i < level->runtime.item_count; ++i)
  {
    const Item *item = &level->runtime.items[i];
    if (!item->collected)
      draw_pickup(r, item, i, cam_x, oy, world_t, CARD_PLAIN);
  }

  for (int i = 0; i < MAX_THROWN_OBJECTS; ++i)
    if (game->gameplay.thrown_objects[i].active)
      draw_thrown_object(r, &game->gameplay.thrown_objects[i], cam_x, oy);
  for (int i = 0; i < MAX_BIRDS; ++i)
    if (game->gameplay.birds[i].active)
      draw_bird(r, &game->gameplay.birds[i], cam_x, oy);

  particle_system_render(&game->presentation.particles, r, oy, cam_x);
  const Player *p = &game->gameplay.player;
  bool blink = !p->dying && game->gameplay.invuln_timer > 0.0f &&
               ((int)(game->gameplay.invuln_timer * 12.0f) % 2 == 0);
  if (!blink)
    draw_player(r, p, level, cam_x, oy, false, 0.0f,
                game->presentation.player_land_squash);

  render_facade_wind(game, win_w, win_h, world_t, cam_x, oy);
}

/* ---- The interior, as a whole ------------------------------------------ */

/*
 * The emergency circuit, drawn over the floor while the alarm is up.
 *
 * There is no red wash, because there is no wash: what the building does is
 * flash. A strip along the top and the foot of the view in red and cyan — the
 * pair every alarm on the machine blinked in — and beacon dashes that swap
 * places each half of the flash. All of it through `bbc_flash`, so under the
 * reduced-motion switch the strips hold red and the dashes hold still. The
 * room's own lines have already gone red with the palette in
 * `render_background`, and the ceiling lamps with it below.
 */
static void render_alarm_lighting(SDL_Renderer *r, int win_w, int win_h)
{
  BbcColour strip = bbc_flash(BBC_RED);
  bbc_fill(r, strip, 0.0f, HUD_HEIGHT, (float)win_w, 2.0f);
  bbc_fill(r, strip, 0.0f, (float)win_h - 2.0f, (float)win_w, 2.0f);
  float offset = bbc_flash_on() ? 0.0f : 24.0f;
  for (int x = -12; x < win_w + 24; x += 48)
  {
    bbc_fill(r, BBC_RED, (float)x + offset, HUD_HEIGHT + 4.0f, 20.0f, 2.0f);
    bbc_fill(r, BBC_RED, (float)x + 24.0f - offset, (float)win_h - 6.0f, 20.0f,
             2.0f);
  }
}

/*
 * How densely a theme's ceilings carry lamps, in tiles: the clean rooms are lit
 * end to end, the plant floors at intervals, the plenum and the roof barely at
 * all. A switch over the themes rather than a field of the paint, because it is
 * this file's question and nothing else asks it.
 */
static unsigned lamp_spacing(LevelTheme theme)
{
  switch (theme)
  {
  case LEVEL_THEME_LOBBY:
  case LEVEL_THEME_CANTEEN:
  case LEVEL_THEME_LAB:
  case LEVEL_THEME_PENTHOUSE:
  case LEVEL_THEME_RESTROOM:
    return 4u;
  case LEVEL_THEME_PLANT:
  case LEVEL_THEME_OFFICE:
  case LEVEL_THEME_ARCHIVE:
  case LEVEL_THEME_SECURITY:
    return 7u;
  case LEVEL_THEME_SERVER:
  case LEVEL_THEME_DUCTS:
  case LEVEL_THEME_ROOF:
  case LEVEL_THEME_VAULT:
  case LEVEL_THEME_FACADE_NIGHT:
  case LEVEL_THEME_FACADE_STORM:
  case LEVEL_THEME_FACADE_MOON:
  case LEVEL_THEME_FACADE_HIGH:
  case LEVEL_THEME_FACADE_SLEET:
  case LEVEL_THEME_COUNT:
    break;
  }
  return 13u;
}

/*
 * The ceiling lamps, found from the tile grid: a fitting under a long ceiling
 * on one tile in so many, so the fittings always match the architecture.
 *
 * A lamp is a blue housing and a strip of the theme's lamp colour, and that is
 * the whole of it — there is no cone under it and no pool on the floor, because
 * the machine had no light to throw. One in nine is on its last legs and goes
 * out for a moment now and then. While the alarm is up every lamp in the sector
 * swings between its own colour and red with the flash, so the corridor Chuck
 * is standing in tells him before the guards arriving in it do.
 */
static void draw_ceiling_lamps(SDL_Renderer *r, const Game *game,
                               const Level *lvl, float cam_x, float oy,
                               int win_w, int win_h, float world_t)
{
  const LevelThemePaint *paint = paint_of(lvl);
  if (!paint->lit)
    return;
  unsigned spacing = lamp_spacing(lvl->map.theme);
  bool alarm = gameplay_alarm_active(&game->gameplay);
  BbcColour lamp = alarm ? bbc_flash_pair(BBC_RED, paint->lamp) : paint->lamp;
  int first_col = (int)(cam_x / (float)TILE_SIZE) - 1;
  if (first_col < 1)
    first_col = 1;
  int last_col = first_col + win_w / TILE_SIZE + 3;
  if (last_col > lvl->map.width - 1)
    last_col = lvl->map.width - 1;
  for (int row = 1; row < lvl->map.height; ++row)
  {
    float y = (float)row * (float)TILE_SIZE + oy;
    if (y + TILE_SIZE < HUD_HEIGHT || y > (float)win_h)
      continue;
    for (int col = first_col; col < last_col; ++col)
    {
      if (lvl->map.tiles[row][col] != TILE_EMPTY ||
          !level_is_solid(lvl, col, row - 1) ||
          !level_is_solid(lvl, col - 1, row - 1) ||
          !level_is_solid(lvl, col + 1, row - 1))
        continue;
      unsigned h = tile_hash(col, row);
      if ((h % spacing) != 0u)
        continue;
      bool out = !game->settings.reduced_motion && ((h >> 5) % 9u) == 0u &&
                 fmodf(world_t * 1.9f + (float)col, 5.0f) < 0.08f;
      float x = (float)col * (float)TILE_SIZE - cam_x;
      cell(r, BBC_BLUE, x, y, 1, 0, 6, 1);
      cell(r, out ? BBC_BLUE : lamp, x, y, 2, 1, 4, 1);
    }
  }
}

/*
 * The front face of every duct the player is inside, laid back over him.
 *
 * The figure layers are drawn near the end of the frame and the tile layers at
 * the start of it, which is right for every other tile in the game: nothing is
 * ever *inside* masonry. Trunking is the exception the whole mechanic rests on,
 * so without this pass a crawl through a shaft drew Chuck over the louvres —
 * outside the wall he had just gone into, on the one tile whose documented cost
 * is that it is opaque both ways. Behind the grille he is what he should be: a
 * slot of shirt at a time between the blades, and the half of him not in yet
 * still out in the room.
 *
 * Only the tiles the man is in, rather than every duct on screen, and that is
 * the point of doing it here rather than in the tile loop: a blast beside the
 * trunking is drawn in front of it and has to stay in front of it. The wall
 * comes back over the frame where the man is and nowhere else.
 *
 * A tile either side of him as well, because the pose reaches further than the
 * box it is drawn from: a knife thrust and the launcher's muzzle both land about
 * a dozen pixels past his shoulder, which is well into the next length of
 * trunking. Asked only once his own box has been found to hold a duct at all, so
 * a man walking past the outside of one never repaints it.
 */
static void render_duct_fronts(SDL_Renderer *r, const Level *lvl,
                               const Player *p, float cam_x, float oy,
                               int win_w, int win_h)
{
  float height = p->crawling ? (float)PLAYER_CRAWL_H : (float)PLAYER_H;
  int first_col = (int)floorf(p->x / (float)TILE_SIZE);
  int last_col = (int)floorf((p->x + (float)PLAYER_W - 1.0f) / (float)TILE_SIZE);
  int first_row = (int)floorf(p->y / (float)TILE_SIZE);
  int last_row = (int)floorf((p->y + height - 1.0f) / (float)TILE_SIZE);
  bool inside = false;

  for (int row = first_row; row <= last_row && !inside; ++row)
    for (int col = first_col; col <= last_col && !inside; ++col)
      inside = level_tile(lvl, col, row) == TILE_VENT;
  if (!inside)
    return;

  for (int row = first_row; row <= last_row; ++row)
    for (int col = first_col - 1; col <= last_col + 1; ++col)
    {
      if (level_tile(lvl, col, row) != TILE_VENT)
        continue;
      /* Revealed, because the structural pass draws nothing that is not, and a
         front face over a tile with no back to it would be trunking hanging in
         the dark ahead of the reveal. */
      if (!lvl->reveal.tiles_visible[row][col])
        continue;
      float x = (float)col * (float)TILE_SIZE - cam_x;
      float y = (float)row * (float)TILE_SIZE + oy;
      if (x + TILE_SIZE < 0.0f || x > (float)win_w ||
          y + TILE_SIZE < HUD_HEIGHT || y > (float)win_h)
        continue;
      /* The grille only — the plenum is not laid over him a second time,
         because the plenum is the far wall of the shaft behind him and the
         gaps between the blades are where he shows. */
      draw_vent_grille(r, x, y, col, row);
    }
}

void render_world(Game *game)
{
  SDL_Renderer *r = game->platform.renderer;
  const Level *lvl = &game->gameplay.level;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);

  /*
   * The camera, on the grid, before anything is drawn.
   *
   * Everything below draws through bbc.h, which snaps every coordinate it is
   * handed — so a camera left between grid lines would round each thing it
   * placed on its own, and a figure, the floor under it and the dither on the
   * floor would step at different moments as the view scrolled. Snapped once
   * here and handed to every pass, the figures and the particles included, the
   * whole world moves a BBC pixel at a time and moves together. The pattern
   * origin is world (0, 0), so a dithered floor is anchored to the floor and not
   * to the glass.
   */
  const float cam_x = bbc_snap_x(game->presentation.cam_x -
                                 game->presentation.camera_shake_x);
  const float cam_y = bbc_snap_y(game->presentation.cam_y -
                                 game->presentation.camera_shake_y);
  const float oy = HUD_HEIGHT - cam_y;
  const float world_t = game->presentation.render_clock;
  const bool steady = game->settings.reduced_motion;
  bbc_set_pattern_origin(-cam_x, oy);

  if (lvl->map.mode == LEVEL_MODE_FACADE)
  {
    render_facade_world(game, win_w, win_h, cam_x, cam_y);
    bbc_set_pattern_origin(0.0f, 0.0f);
    return;
  }

  render_background(game, win_w, win_h, cam_x, cam_y);

  /* Structural tile layer. */
  int ceiling_row = lvl->map.theme == LEVEL_THEME_RESTROOM ? restroom_ceiling_row(lvl) : 0;
  for (int row = 0; row < lvl->map.height; ++row)
  {
    for (int col = 0; col < lvl->map.width; ++col)
    {
      float x = (float)col * (float)TILE_SIZE - cam_x;
      float y = (float)row * (float)TILE_SIZE + oy;
      if (x + TILE_SIZE < 0.0f || x > (float)win_w ||
          y + TILE_SIZE < HUD_HEIGHT || y > (float)win_h ||
          !lvl->reveal.tiles_visible[row][col])
        continue;
      TileType tile = lvl->map.tiles[row][col];
      if (tile == TILE_WEAK_WALL)
      {
        /* Drawn by level_art in every theme, restroom included: the patch and
         * the crack are what tell the player the wall can be opened, and a
         * sector that quietly drew it as ordinary masonry would be hiding the
         * one route the player has to be told about. */
        if (level_wall_broken(lvl, col, row))
          level_art_broken_wall_tile(r, lvl, col, row, x, y);
        else
          level_art_wall_tile(r, lvl, col, row, x, y);
        /* level_art anchors its own dithers from the tile it is handed, so
         * the editor can draw a tile with no camera at all; put the world's
         * origin back before anything else of ours is dithered. */
        bbc_set_pattern_origin(-cam_x, oy);
      }
      else if (tile == TILE_WALL)
      {
        if (lvl->map.theme == LEVEL_THEME_RESTROOM)
          draw_restroom_wall_tile(r, lvl, col, row, x, y, ceiling_row);
        else
        {
          level_art_wall_tile(r, lvl, col, row, x, y);
          bbc_set_pattern_origin(-cam_x, oy);
        }
      }
      else if (tile == TILE_LADDER)
        draw_ladder_tile(r, x, y, row);
      else if (tile == TILE_ELEVATOR_SHAFT)
        draw_shaft_tile(r, x, y, col, row);
      else if (tile == TILE_VENT)
        draw_vent_tile(r, x, y, col, row);
    }
  }

  if (!lvl->reveal.done)
  {
    bbc_set_pattern_origin(0.0f, 0.0f);
    return;
  }

  draw_ceiling_lamps(r, game, lvl, cam_x, oy, win_w, win_h, world_t);

  for (int i = 0; i < lvl->runtime.elevator_count; ++i)
  {
    const Elevator *el = &lvl->runtime.elevators[i];
    draw_lift_rigging(r, lvl, el, cam_x, oy);
    draw_platform(r, (float)el->col * (float)TILE_SIZE - cam_x, el->y + oy,
                  DECK_LIFT, false);
  }
  for (int i = 0; i < lvl->runtime.fall_platform_count; ++i)
  {
    const FallPlatform *fp = &lvl->runtime.fall_platforms[i];
    if (!fp->removed)
      draw_platform(r, (float)fp->col * (float)TILE_SIZE - cam_x, fp->y + oy,
                    DECK_FALLING, fp->triggered);
  }
  for (int i = 0; i < lvl->runtime.moving_platform_count; ++i)
  {
    const MovingPlatform *mp = &lvl->runtime.moving_platforms[i];
    draw_platform(r, mp->x - cam_x, (float)mp->row * (float)TILE_SIZE + oy,
                  DECK_MOVING, false);
  }

  /* Furniture stays behind every interactive object and actor. */
  for (int i = 0; i < lvl->map.decoration_count; ++i)
  {
    const Decoration *decoration = &lvl->map.decorations[i];
    float x = (float)decoration->col * (float)TILE_SIZE - cam_x;
    if (x + 2.0f * TILE_SIZE < 0.0f || x - TILE_SIZE > (float)win_w)
      continue;
    draw_decoration(r, lvl, decoration, cam_x, oy, world_t,
                    game->campaign.current_level);
  }

  /* Doors and other wall-mounted fixtures belong behind anyone walking along
   * the corridor floor. */
  for (int d = 0; d < lvl->map.door_count; ++d)
  {
    float x = (float)lvl->map.doors[d].col * (float)TILE_SIZE - cam_x;
    float y = (float)lvl->map.doors[d].row * (float)TILE_SIZE + oy;
    draw_door(r, x, y, d);
  }
  if (lvl->map.has_sublevel_entrance)
  {
    float x = (float)lvl->map.sublevel_entrance_col * (float)TILE_SIZE - cam_x;
    float y = (float)lvl->map.sublevel_entrance_row * (float)TILE_SIZE + oy;
    draw_restroom_door(r, x, y);
  }
  if (lvl->map.has_sublevel_return)
  {
    float x = (float)lvl->map.sublevel_return_col * (float)TILE_SIZE - cam_x;
    float y = (float)lvl->map.sublevel_return_row * (float)TILE_SIZE + oy;
    draw_restroom_door(r, x, y);
  }
  if (lvl->map.has_exit)
  {
    float x = (float)lvl->map.exit_col * (float)TILE_SIZE - cam_x;
    float y = (float)lvl->map.exit_row * (float)TILE_SIZE + oy;
    draw_exit(r, game, x, y);
  }
  if (lvl->map.has_window)
  {
    float x = (float)lvl->map.window_col * (float)TILE_SIZE - cam_x;
    float y = (float)lvl->map.window_row * (float)TILE_SIZE + oy;
    draw_open_window(r, x, y, lvl->map.window_col, lvl->map.window_row);
  }
  for (int i = 0; i < lvl->map.terminal_count; ++i)
  {
    float x = (float)lvl->map.terminals[i].col * (float)TILE_SIZE - cam_x;
    float y = (float)lvl->map.terminals[i].row * (float)TILE_SIZE + oy;
    bool active = i == lvl->runtime.active_terminal_index;
    draw_terminal(r, x, y, active, active && lvl->runtime.terminal_hacked,
                  lvl->map.has_window,
                  active && game->gameplay.terminal_hacking,
                  game->gameplay.terminal_hack_progress, world_t);
  }
  for (int i = 0; i < lvl->map.alarm_switch_count; ++i)
  {
    float x = (float)lvl->map.alarm_switches[i].col * (float)TILE_SIZE - cam_x;
    float y = (float)lvl->map.alarm_switches[i].row * (float)TILE_SIZE + oy;
    bool being_used = false;
    for (int guard = 0; guard < game->gameplay.enemy_count; ++guard)
    {
      const Enemy *enemy = &game->gameplay.enemies[guard];
      if (!enemy->dead && enemy->raising_alarm &&
          enemy->alarm_switch_index == i && enemy->alarm_use_timer > 0.0f)
      {
        being_used = true;
        break;
      }
    }
    draw_alarm_switch(r, x, y, gameplay_alarm_active(&game->gameplay),
                      game->gameplay.active_alarm_switch == i, being_used);
  }
  for (int i = 0; i < lvl->map.camera_count; ++i)
  {
    const CameraState *cam = &game->gameplay.cameras[i];
    float x = (float)lvl->map.cameras[i].col * (float)TILE_SIZE - cam_x;
    float y = (float)lvl->map.cameras[i].row * (float)TILE_SIZE + oy;
    /* Alerted means the lens has Chuck *now* and the alarm is coming;
       suspicious is the fade after it lost him. The two must never look alike,
       because one of them is a warning the player can still act on. */
    draw_security_camera(r, &game->gameplay, &lvl->map.cameras[i], x, y,
                         gameplay_camera_angle(cam->sweep), cam->working,
                         cam->notice > 0.0f,
                         cam->notice <= 0.0f && cam->suspicion > 0.0f, cam_x,
                         oy, win_w, win_h);
  }

  /* Ambient staff remain subdued, but correctly stand in front of the back
   * wall and its fixtures. Floor props and gameplay actors render later. */
  for (int i = 0; i < game->gameplay.janitor_count; ++i)
    draw_janitor(r, &game->gameplay.janitors[i], lvl, cam_x, oy);
  /* Civilians share that layer: they belong to the room the player is walking
   * into, so they pass behind its counters and planting rather than over it. */
  for (int i = 0; i < game->gameplay.civilian_count; ++i)
    draw_civilian(r, &game->gameplay.civilians[i], lvl, cam_x, oy);
  /* The front desk belongs on that layer for the same reason, and for one
   * more: the post is behind the counter, so the counter has to be drawn over
   * it or the staff side and the visitor side look the same. */
  for (int i = 0; i < game->gameplay.receptionist_count; ++i)
    draw_receptionist(r, &game->gameplay.receptionists[i], lvl, cam_x, oy);

  /* Redraw ladders as a middle layer so ambient janitors pass behind their
   * rails and rungs. Interactive actors are rendered later and remain in
   * front, preserving the usual climbing readability. */
  for (int row = 0; row < lvl->map.height; ++row)
  {
    for (int col = 0; col < lvl->map.width; ++col)
    {
      if (lvl->map.tiles[row][col] != TILE_LADDER)
        continue;
      float x = (float)col * (float)TILE_SIZE - cam_x;
      if (x + TILE_SIZE < 0.0f || x > (float)win_w)
        continue;
      float y = (float)row * (float)TILE_SIZE + oy;
      draw_ladder_tile(r, x, y, row);
    }
  }

  /* Pickups, each on its own bob and its own glint. */
  int card_pos = 0;
  for (int i = 0; i < lvl->runtime.item_count; ++i)
  {
    const Item *it = &lvl->runtime.items[i];
    if (it->collected)
      continue;
    CardShowing showing = CARD_PLAIN;
    if (it->type == ITEM_CARD)
    {
      if (game->state == STATE_SHOW_KEYCARD)
        showing = card_pos == game->presentation.card_anim_current
                      ? CARD_SHOWN
                      : CARD_WAITING;
      card_pos++;
    }
    draw_pickup(r, it, i, cam_x, oy, world_t, showing);
  }

  /* Dropped magazines: the HUD's cartridge pictogram laid on the floor. */
  for (int i = 0; i < MAX_AMMO_DROPS; ++i)
  {
    const AmmoDrop *drop = &game->gameplay.ammo_drops[i];
    if (!drop->active)
      continue;
    float x = drop->x - cam_x;
    float y = drop->y + oy;
    if (x + AMMO_DROP_W < 0.0f || x > (float)win_w)
      continue;
    bbc_sprite(r, &SPR_AMMO_DROP, x, y, 1, NULL);
    draw_pickup_glint(r, x + (float)AMMO_DROP_W, y - 2.0f, world_t, i + 101);
  }

  for (int i = 0; i < lvl->map.spike_count; ++i)
    bbc_sprite(r, &SPR_SPIKE_STRIP, lvl->map.spike_spawns[i].x - cam_x,
               lvl->map.spike_spawns[i].y + oy, 1, NULL);

  for (int i = 0; i < lvl->map.ceiling_fan_count; ++i)
    draw_ceiling_fan(r, &lvl->map.ceiling_fans[i], cam_x, oy, world_t, i,
                     steady);

  for (int i = 0; i < lvl->runtime.crate_count; ++i)
    if (lvl->runtime.crates[i].active)
      draw_crate(r, &lvl->runtime.crates[i], i, cam_x, oy);

  for (int i = 0; i < lvl->runtime.gas_canister_count; ++i)
  {
    const GasCanister *canister = &lvl->runtime.gas_canisters[i];
    if (canister->active)
      bbc_sprite(r, &SPR_GAS_CANISTER, canister->x - cam_x, canister->y + oy,
                 1, NULL);
  }

  for (int i = 0; i < game->gameplay.mine_count; ++i)
    if (game->gameplay.mines[i].active)
      draw_mine(r, &game->gameplay.mines[i], cam_x, oy, steady);

  /* The fallen go down first, so anyone still on their feet passes in front of
     them rather than behind. */
  for (int i = 0; i < game->gameplay.dog_count; ++i)
    if (game->gameplay.dogs[i].dead)
      draw_downed_dog(r, &game->gameplay.dogs[i], lvl, cam_x, oy);

  for (int i = 0; i < game->gameplay.enemy_count; ++i)
    if (game->gameplay.enemies[i].dead)
      draw_downed_enemy(r, &game->gameplay.enemies[i], lvl, cam_x, oy);

  for (int i = 0; i < game->gameplay.dog_count; ++i)
    if (!game->gameplay.dogs[i].dead)
      draw_dog(r, &game->gameplay.dogs[i], lvl, cam_x, oy);

  for (int i = 0; i < game->gameplay.enemy_count; ++i)
    if (!game->gameplay.enemies[i].dead)
      draw_enemy(r, &game->gameplay.enemies[i], lvl, cam_x, oy);

  for (int i = 0; i < game->gameplay.grenade_count; ++i)
  {
    const Grenade *g = &game->gameplay.grenades[i];
    if (g->active)
      draw_grenade(r, g->x - cam_x, g->y + oy, g->timer);
  }

  for (int i = 0; i < MAX_FLASHBANGS; ++i)
  {
    const Grenade *f = &game->gameplay.flashbangs[i];
    if (f->active)
      draw_flashbang(r, f->x - cam_x, f->y + oy, f->timer);
  }

  for (int i = 0; i < MAX_DECOYS; ++i)
  {
    const Decoy *d = &game->gameplay.decoys[i];
    if (d->active)
      draw_decoy(r, d->x - cam_x, d->y + oy);
  }

  /* Rockets in flight, with a trail of smoke a cell at a time behind them. */
  for (int i = 0; i < MAX_ROCKETS; ++i)
  {
    const Rocket *rocket = &game->gameplay.rockets[i];
    if (!rocket->active)
      continue;
    float x = rocket->x - cam_x;
    float y = rocket->y + oy;
    if (fabsf(rocket->vy) > fabsf(rocket->vx))
    {
      int dir = rocket->vy >= 0.0f ? 1 : -1;
      bbc_plot(r, BBC_WHITE, x, dir > 0 ? y - 10.0f : y + (float)ROCKET_W + 8.0f);
      bbc_plot(r, BBC_BLUE, x, dir > 0 ? y - 16.0f : y + (float)ROCKET_W + 14.0f);
      draw_vertical_rocket_sprite(r, x, y, dir);
    }
    else
    {
      int dir = rocket->vx >= 0.0f ? 1 : -1;
      bbc_plot(r, BBC_WHITE, dir > 0 ? x - 14.0f : x + (float)ROCKET_W + 10.0f,
               y + 2.0f);
      bbc_plot(r, BBC_BLUE, dir > 0 ? x - 22.0f : x + (float)ROCKET_W + 18.0f,
               y + 2.0f);
      draw_rocket_sprite(r, x, y, dir, true);
    }
  }
  for (int i = 0; i < MAX_BULLETS; ++i)
    if (game->gameplay.bullets[i].active)
      draw_round(r, &game->gameplay.bullets[i], cam_x, oy, BBC_YELLOW,
                 BBC_WHITE);
  for (int i = 0; i < MAX_ENEMY_BULLETS; ++i)
    if (game->gameplay.enemy_bullets[i].active)
      draw_round(r, &game->gameplay.enemy_bullets[i], cam_x, oy, BBC_RED,
                 BBC_YELLOW);

  particle_system_render(&game->presentation.particles, r, oy, cam_x);

  /* Chuck last of the cast, and the louvres of any duct he is in over him. A
   * man who is dying is still drawn — the figure renderer lays him down as he
   * fell — and the mercy blink is for a man who is going to get up. */
  const Player *p = &game->gameplay.player;
  bool blink = !p->dying && game->gameplay.invuln_timer > 0.0f &&
               ((int)(game->gameplay.invuln_timer * 12.0f) % 2 == 0);
  if (!blink)
    draw_player(r, p, lvl, cam_x, oy, game->gameplay.terminal_hacking,
                game->gameplay.terminal_hack_progress,
                game->presentation.player_land_squash);
  render_duct_fronts(r, lvl, p, cam_x, oy, win_w, win_h);
  if (gameplay_alarm_active(&game->gameplay))
    render_alarm_lighting(r, win_w, win_h);

  /* Hand the frame back with the pattern on the glass again: the HUD is drawn
   * on the screen, not in the world. */
  bbc_set_pattern_origin(0.0f, 0.0f);
}
