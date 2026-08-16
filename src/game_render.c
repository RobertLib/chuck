#include "game.h"

#include <SDL3/SDL.h>
#include <math.h>

#include "chase_render.h"
#include "crew.h"
#include "fx.h"
#include "gameplay_interaction.h"
#include "gameplay_world.h"
#include "level_art.h"
#include "render_figures.h"
#include "render_sprite.h"

#ifdef CHUCK_DEBUG
#include "embedded_levels.h"
#endif

/*
 * Chuck intentionally ships without art assets.  Everything in this file is
 * assembled at runtime from small, hard-edged shapes.  The palette and the
 * lighting primitives live in fx.h and are shared with the intro and the
 * cutscenes, so the whole game reads as one production.  This file adds the
 * things only gameplay needs: ambient occlusion under floors, warm ceiling
 * lights, every actor and prop, and a finishing scanline/vignette pass.
 *
 * The wall material and the parallax backdrop are the one part that changes
 * from sector to sector; both come from the level's theme in level_art.c, so
 * a level's look is authored in its map file rather than hard-coded here.
 */


/* Overlay subtitles: cream cooled a step so a verdict's second line
 * never outshines its first. */
static const SDL_Color COL_SUBTITLE = {210, 220, 215, 255};

/* The crew strip's accents are the palette's own semantics — cyan is the
 * handset, red is the alarm, amber is a voice on the wall — with one exception,
 * and this is it. Two men talking in a room is the one kind of line where
 * *nothing is happening*, so it must not carry an accent that means something:
 * every colour fx.h offers for this band does. What is wanted is the absence of
 * one — the strip's own steel, a shade off the plate it stands on, so the words
 * read and the colour says nothing at all. */
static const SDL_Color COL_CHATTER_IDLE = {118, 134, 138, 255};

void game_get_view_size(Game *game, int *out_w, int *out_h)
{
  int lw = 0, lh = 0;
  SDL_RendererLogicalPresentation mode;
  if (SDL_GetRenderLogicalPresentation(game->platform.renderer, &lw, &lh, &mode) && lw > 0 && lh > 0)
  {
    *out_w = lw;
    *out_h = lh;
    return;
  }
  SDL_GetWindowSize(game->platform.window, out_w, out_h);
}


static void color_quad(SDL_Renderer *r, SDL_Color c,
                       float x0, float y0, float x1, float y1,
                       float x2, float y2, float x3, float y3)
{
  SDL_FColor fc = {(float)c.r / 255.0f, (float)c.g / 255.0f,
                   (float)c.b / 255.0f, (float)c.a / 255.0f};
  SDL_Vertex vertices[4] = {
      {{x0, y0}, fc, {0.0f, 0.0f}},
      {{x1, y1}, fc, {0.0f, 0.0f}},
      {{x2, y2}, fc, {0.0f, 0.0f}},
      {{x3, y3}, fc, {0.0f, 0.0f}},
  };
  static const int indices[6] = {0, 1, 2, 0, 2, 3};
  SDL_RenderGeometry(r, NULL, vertices, 4, indices, 6);
}


static void draw_text_centered(Game *game, float center_y, float scale,
                               Uint8 cr, Uint8 cg, Uint8 cb, const char *text)
{
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);
  (void)win_h;
  float text_w = draw_text_width(text, scale);
  float text_h = (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
  draw_text(game->platform.renderer, ((float)win_w - text_w) * 0.5f,
            center_y - text_h * 0.5f, scale, cr, cg, cb, text);
}

static unsigned tile_hash(int x, int y)
{
  unsigned h = (unsigned)x * 0x8da6b343u;
  h ^= (unsigned)y * 0xd8163841u;
  h ^= h >> 13;
  h *= 0xcb1ab31fu;
  return h ^ (h >> 16);
}

static void draw_soft_glow(SDL_Renderer *r, float x, float y, float w, float h,
                           SDL_Color color)
{
  /* True radial falloff sells emissive surfaces far better than rings. */
  float radius = (w > h ? w : h) * 0.5f + 12.0f;
  fx_glow(r, x + w * 0.5f, y + h * 0.5f, radius, color, 72);
}

/*
 * Mirror over a basin. Chuck's reflection is deliberately only a silhouette:
 * it makes the room feel occupied without adding a second figure the player
 * has to read while fighting.
 */
static void draw_restroom_mirror(SDL_Renderer *r, float x, float y,
                                 float reflection_offset, bool reflected)
{
  color_rect(r, (SDL_Color){10, 17, 21, 255}, x - 2.0f, y - 2.0f,
             30.0f, 30.0f);
  color_rect(r, (SDL_Color){139, 160, 156, 255}, x - 1.0f, y - 1.0f,
             28.0f, 28.0f);
  fx_vgrad(r, x, y, 26.0f, 26.0f,
           (SDL_Color){96, 130, 136, 255}, 255,
           (SDL_Color){46, 74, 84, 255}, 255);

  if (reflected && reflection_offset > -9.0f && reflection_offset < 9.0f)
  {
    float rx = x + 13.0f + reflection_offset;
    color_rect(r, (SDL_Color){38, 60, 72, 255}, rx - 5.0f, y + 12.0f,
               11.0f, 14.0f);
    color_rect(r, (SDL_Color){52, 76, 86, 255}, rx - 4.0f, y + 6.0f,
               8.0f, 7.0f);
  }

  /* A stepped streak of light is what makes flat fill read as glass. */
  for (int step = 0; step < 6; ++step)
  {
    fx_rect_a(r, (SDL_Color){196, 224, 218, 255}, 46,
              x + 4.0f + (float)step * 1.6f,
              y + 22.0f - (float)step * 4.0f, 5.0f, 4.0f);
  }
  color_rect(r, (SDL_Color){193, 208, 200, 255}, x - 1.0f, y - 1.0f,
             28.0f, 2.0f);
}

/*
 * Every level's backdrop comes from its theme (see level_art.c). The restroom
 * is the one exception: its interior is derived from the map's own wall
 * bounding box, so it is built here where the room geometry is to hand.
 */
static void render_background(Game *game, int win_w, int win_h)
{
  SDL_Renderer *r = game->platform.renderer;
  const float oy = HUD_HEIGHT;
  const float fh = (float)win_h - oy;
  float t = (float)SDL_GetTicksNS() * 1.0e-9f;

  if (game->gameplay.level.map.theme == LEVEL_THEME_RESTROOM)
  {
    const Level *level = &game->gameplay.level;
    float cam_x = game->presentation.cam_x;
    /* The interior is derived from the map's own masonry, so the tiling,
     * mirrors and lighting follow whatever shape the sublevel is drawn in
     * instead of assuming one fixed room height. */
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

    /* Outside the room is a deep service void, not another tiled wall. */
    color_rect(r, (SDL_Color){6, 10, 16, 255}, 0.0f, oy,
               (float)win_w, fh);
    fx_vgrad(r, 0.0f, oy, (float)win_w, fh,
             (SDL_Color){7, 13, 22, 255}, 255,
             (SDL_Color){19, 31, 38, 255}, 255);
    color_rect(r, (SDL_Color){11, 19, 28, 255},
               0.0f, oy + 80.0f, (float)win_w, 18.0f);
    color_rect(r, (SDL_Color){35, 53, 60, 255},
               0.0f, oy + 83.0f, (float)win_w, 3.0f);
    color_rect(r, (SDL_Color){8, 15, 23, 255},
               0.0f, (float)win_h - 92.0f, (float)win_w, 92.0f);
    for (int support = 0; support < 5; ++support)
    {
      float sx = 58.0f + support * 184.0f -
                 fmodf(cam_x * 0.12f, 184.0f);
      color_rect(r, (SDL_Color){13, 23, 31, 255},
                 sx, oy, 14.0f, fh);
      color_rect(r, (SDL_Color){29, 43, 49, 255},
                 sx + 2.0f, oy, 2.0f, fh);
    }

    /* The bright ceramic surface is clipped to the actual interior bounded
     * by the map's structural wall ring. */
    float room_left = (wall_left + 1) * (float)TILE_SIZE - cam_x;
    float room_right = wall_right * (float)TILE_SIZE - cam_x;
    float room_width = room_right - room_left;
    float room_top = (wall_top + 1) * (float)TILE_SIZE + oy;
    float room_bottom = wall_bottom * (float)TILE_SIZE + oy;
    float room_height = room_bottom - room_top;
    color_rect(r, (SDL_Color){3, 8, 12, 220},
               room_left - 8.0f, room_top - 8.0f,
               room_width + 16.0f, room_bottom - room_top + 16.0f);
    color_rect(r, (SDL_Color){139, 163, 157, 255},
               room_left, room_top, room_width, room_bottom - room_top);
    fx_vgrad(r, room_left, room_top, room_width, room_bottom - room_top,
             (SDL_Color){203, 211, 196, 255}, 255,
             (SDL_Color){126, 157, 151, 255}, 255);
    for (float y = room_top; y < room_bottom; y += 24.0f)
      color_rect(r, (SDL_Color){83, 112, 112, 255},
                 room_left, y, room_width, 1.0f);
    for (int col = wall_left + 1; col < wall_right; ++col)
    {
      float x = col * (float)TILE_SIZE - cam_x;
      color_rect(r, (SDL_Color){91, 120, 119, 255},
                 x, room_top, 1.0f, room_height);
    }

    /* The dado rail is measured up from the floor rather than down from the
     * ceiling, so it stays at waist height in a room of any storey count. */
    float rail_y = room_bottom - 60.0f;
    color_rect(r, (SDL_Color){49, 92, 91, 255},
               room_left, rail_y, room_width, 8.0f);
    color_rect(r, (SDL_Color){185, 205, 191, 255},
               room_left, rail_y, room_width, 2.0f);

    /* A plumbing run and ceiling fixtures keep the interior from reading as
     * an office backdrop while leaving the fittings dominant. */
    color_rect(r, (SDL_Color){29, 54, 58, 255},
               room_left, room_top + 20.0f, room_width, 7.0f);
    color_rect(r, (SDL_Color){112, 146, 141, 255},
               room_left, room_top + 21.0f, room_width, 2.0f);
    for (float bracket = room_left + 40.0f; bracket < room_right;
         bracket += 96.0f)
    {
      color_rect(r, (SDL_Color){22, 44, 48, 255},
                 bracket, room_top + 14.0f, 4.0f, 8.0f);
    }

    int fixture_count = (int)(room_width / 300.0f) + 1;
    if (fixture_count > 4)
      fixture_count = 4;
    for (int fixture = 0; fixture < fixture_count; ++fixture)
    {
      float centre = room_left + room_width *
                                     ((float)fixture + 0.5f) /
                                     (float)fixture_count;
      /* One tube in the room is on its last legs; the flicker is rare enough
       * to be atmosphere rather than a strobe. */
      float flicker = fixture == 1 && fmodf(t * 1.9f, 5.0f) < 0.11f
                          ? 0.35f
                          : 1.0f;
      fx_glow(r, centre, room_top + 8.0f, 62.0f,
              (SDL_Color){190, 240, 226, 255}, (Uint8)(42.0f * flicker));
      color_rect(r, (SDL_Color){25, 42, 46, 255},
                 centre - 54.0f, room_top + 4.0f, 108.0f, 8.0f);
      color_rect(r, fx_dim((SDL_Color){202, 235, 222, 255}, flicker),
                 centre - 49.0f, room_top + 6.0f, 98.0f, 3.0f);
    }

    /* Mirrors hang over the basins the map placed. */
    for (int i = 0; i < level->map.decoration_count; ++i)
    {
      const Decoration *decoration = &level->map.decorations[i];
      if (decoration->type != DECOR_RESTROOM_BASIN)
        continue;
      float mirror_x = decoration->col * (float)TILE_SIZE - cam_x + 3.0f;
      float mirror_y = (decoration->row - 1) * (float)TILE_SIZE + oy + 3.0f;
      float basin_centre = (decoration->col + 0.5f) * (float)TILE_SIZE;
      float player_centre = game->gameplay.player.x + PLAYER_W * 0.5f;
      bool in_front =
          fabsf(game->gameplay.player.y -
                (decoration->row * (float)TILE_SIZE)) < TILE_SIZE;
      draw_restroom_mirror(r, mirror_x, mirror_y,
                           (player_centre - basin_centre) * 0.45f,
                           in_front);
    }

    /* Illuminated sign over the door back to the level. */
    if (level->map.has_sublevel_return)
    {
      float sign_x = level->map.sublevel_return_col * (float)TILE_SIZE -
                     cam_x + 2.0f;
      float sign_y = (level->map.sublevel_return_row - 1) *
                         (float)TILE_SIZE +
                     oy + 8.0f;
      fx_glow(r, sign_x + 14.0f, sign_y + 8.0f, 26.0f,
              (SDL_Color){110, 230, 170, 255}, 46);
      color_rect(r, (SDL_Color){12, 22, 24, 255}, sign_x, sign_y,
                 28.0f, 16.0f);
      color_rect(r, (SDL_Color){36, 92, 74, 255}, sign_x + 1.0f,
                 sign_y + 1.0f, 26.0f, 14.0f);
      draw_text(r, sign_x + 6.0f, sign_y + 4.0f, 1.0f, 156, 240, 196, "WC");
    }
    return;
  }

  LevelArtScene scene = {r, &game->gameplay.level,
                         game->presentation.cam_x, game->presentation.cam_y,
                         win_w, win_h, t, game->campaign.current_level,
                         game->settings.reduced_motion};
  level_art_backdrop(&scene);
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

static void draw_restroom_wall_tile(SDL_Renderer *r, const Level *lvl,
                                    int col, int row, float x, float y,
                                    int ceiling_row)
{
  unsigned h = tile_hash(col, row);
  bool floor_top = !level_is_solid(lvl, col, row - 1);
  SDL_Color tile = ((col + row) & 1)
                       ? (SDL_Color){80, 105, 106, 255}
                       : (SDL_Color){91, 116, 115, 255};

  color_rect(r, (SDL_Color){24, 42, 46, 255},
             x, y, TILE_SIZE, TILE_SIZE);
  color_rect(r, tile, x + 1.0f, y + 1.0f,
             TILE_SIZE - 2.0f, TILE_SIZE - 2.0f);
  color_rect(r, (SDL_Color){124, 148, 143, 255},
             x + 2.0f, y + 2.0f, TILE_SIZE - 4.0f, 1.0f);

  /* The room's ceramic is laid in a smaller module than the tile grid, and the
   * grout is what says so. Four bays across a tile is the same eight-pixel
   * module the wall material upstairs uses, so the restroom reads as part of
   * the same building rather than as a checkerboard. */
  for (int grout = 1; grout < 4; ++grout)
  {
    float gx = x + (float)grout * 8.0f;
    color_rect(r, (SDL_Color){58, 82, 84, 255}, gx, y + 1.0f, 1.0f,
               TILE_SIZE - 2.0f);
  }
  color_rect(r, (SDL_Color){58, 82, 84, 255}, x + 1.0f, y + 16.0f,
             TILE_SIZE - 2.0f, 1.0f);
  if ((h % 13u) == 0u)
    fx_rect_a(r, (SDL_Color){202, 235, 222, 255}, 60, x + 3.0f, y + 3.0f,
              4.0f, 4.0f);

  if (floor_top)
  {
    SDL_Color floor = (h & 1u)
                          ? (SDL_Color){43, 64, 68, 255}
                          : (SDL_Color){54, 76, 78, 255};
    color_rect(r, floor, x + 1.0f, y + 4.0f,
               TILE_SIZE - 2.0f, TILE_SIZE - 5.0f);
    color_rect(r, (SDL_Color){190, 213, 202, 255},
               x, y, TILE_SIZE, 3.0f);
    color_rect(r, (SDL_Color){62, 94, 94, 255},
               x, y + 3.0f, TILE_SIZE, 2.0f);
    /* Small mosaic on the floor, and the light lying along the wet edge. */
    for (int square = 0; square < 6; ++square)
      color_rect(r, fx_mix(floor, (SDL_Color){23, 38, 42, 255}, 0.5f),
                 x + 2.0f + (float)square * 5.0f, y + 5.0f, 1.0f, 3.0f);
    fx_vgrad(r, x, y + 5.0f, TILE_SIZE, 9.0f,
             (SDL_Color){190, 213, 202, 255}, 26,
             (SDL_Color){190, 213, 202, 255}, 0);
  }
  else
  {
    /* A wall standing in a lit room falls away from its own top edge. */
    fx_vgrad(r, x, y, TILE_SIZE, 12.0f,
             (SDL_Color){202, 235, 222, 255}, 24,
             (SDL_Color){202, 235, 222, 255}, 0);
  }
  if (!level_is_solid(lvl, col, row + 1))
  {
    color_rect(r, (SDL_Color){17, 31, 35, 255},
               x, y + TILE_SIZE - 3.0f, TILE_SIZE, 3.0f);
    fx_vgrad(r, x, y + TILE_SIZE - 14.0f, TILE_SIZE, 12.0f,
             FX_INK, 0, FX_INK, 70);
  }
  if (!level_is_solid(lvl, col - 1, row))
    fx_hgrad(r, x, y, 10.0f, TILE_SIZE,
             (SDL_Color){202, 235, 222, 255}, 26,
             (SDL_Color){202, 235, 222, 255}, 0);
  if (!level_is_solid(lvl, col + 1, row))
    fx_hgrad(r, x + TILE_SIZE - 10.0f, y, 10.0f, TILE_SIZE,
             FX_INK, 0, FX_INK, 60);

  /* A slab inside the room with open air both above and below is a service
   * catwalk rather than the room's own floor: it gets a handrail and hangers. */
  if (floor_top && row > ceiling_row && !level_is_solid(lvl, col, row + 1))
  {
    color_rect(r, (SDL_Color){22, 40, 44, 255},
               x, y + TILE_SIZE - 6.0f, TILE_SIZE, 3.0f);
    color_rect(r, (SDL_Color){35, 60, 62, 255},
               x + 13.0f, y + TILE_SIZE, 6.0f, 4.0f);
    color_rect(r, (SDL_Color){28, 48, 52, 255}, x + 4.0f, y - 13.0f,
               3.0f, 13.0f);
    color_rect(r, (SDL_Color){28, 48, 52, 255}, x + 25.0f, y - 13.0f,
               3.0f, 13.0f);
    color_rect(r, (SDL_Color){52, 84, 84, 255}, x, y - 13.0f,
               TILE_SIZE, 3.0f);
    color_rect(r, (SDL_Color){130, 160, 152, 255}, x, y - 13.0f,
               TILE_SIZE, 1.0f);
    color_rect(r, (SDL_Color){40, 68, 70, 255}, x, y - 7.0f,
               TILE_SIZE, 2.0f);
  }
}

static void draw_ladder_tile(SDL_Renderer *r, float x, float y, int row)
{
  /* The shadow the ladder throws on whatever it is bolted to. It used to be
   * written as an opaque colour carrying an alpha the renderer never blended,
   * so the ladder wore two black bars into every wall in the building; drawn
   * for real it sits on the wall instead of cutting a hole in it. */
  fx_rect_a(r, FX_INK, 96, x + 4.0f, y, 8.0f, TILE_SIZE);
  fx_rect_a(r, FX_INK, 96, x + 22.0f, y, 8.0f, TILE_SIZE);

  /* Steel side rails, with the light running down the outer face of each. */
  color_rect(r, (SDL_Color){47, 46, 38, 255}, x + 7.0f, y, 4.0f, TILE_SIZE);
  color_rect(r, (SDL_Color){47, 46, 38, 255}, x + 22.0f, y, 4.0f, TILE_SIZE);
  color_rect(r, (SDL_Color){196, 148, 62, 255}, x + 8.0f, y, 2.0f, TILE_SIZE);
  color_rect(r, (SDL_Color){196, 148, 62, 255}, x + 23.0f, y, 2.0f, TILE_SIZE);
  fx_rect_a(r, (SDL_Color){255, 226, 158, 255}, 70, x + 8.0f, y, 1.0f,
            TILE_SIZE);
  fx_rect_a(r, (SDL_Color){255, 226, 158, 255}, 70, x + 23.0f, y, 1.0f,
            TILE_SIZE);

  for (int rung = -4; rung < TILE_SIZE; rung += 8)
  {
    color_rect(r, (SDL_Color){52, 42, 28, 255}, x + 8.0f, y + (float)rung + 2.0f, 17.0f, 3.0f);
    color_rect(r, (SDL_Color){226, 180, 84, 255}, x + 9.0f, y + (float)rung + 1.0f, 15.0f, 2.0f);
    color_rect(r, (SDL_Color){255, 224, 150, 255}, x + 9.0f, y + (float)rung + 1.0f, 15.0f, 1.0f);
    /* The paint is worn off the middle of a rung that gets stood on. */
    fx_rect_a(r, (SDL_Color){150, 152, 148, 255}, 90, x + 13.0f,
              y + (float)rung + 1.0f, 7.0f, 1.0f);
  }

  /* A wall bracket every other course, so the run is fixed to something. */
  if ((row & 1) == 0)
  {
    color_rect(r, (SDL_Color){58, 54, 43, 255}, x + 3.0f, y + 13.0f, 6.0f, 4.0f);
    color_rect(r, (SDL_Color){58, 54, 43, 255}, x + 24.0f, y + 13.0f, 6.0f, 4.0f);
    color_rect(r, (SDL_Color){124, 112, 80, 255}, x + 3.0f, y + 13.0f, 6.0f, 1.0f);
    color_rect(r, (SDL_Color){124, 112, 80, 255}, x + 24.0f, y + 13.0f, 6.0f, 1.0f);
  }
}

static void draw_shaft_tile(SDL_Renderer *r, float x, float y, int col, int row)
{
  unsigned h = tile_hash(col, row);
  color_rect(r, (SDL_Color){8, 13, 20, 255}, x + 4.0f, y, TILE_SIZE - 8.0f, TILE_SIZE);
  /* A shaft is a hole in the floor plan, so it has to darken toward its own
   * middle: the guide rails are the only lit thing in it. */
  fx_hgrad(r, x + 4.0f, y, 10.0f, TILE_SIZE, FX_INK, 90, FX_INK, 0);
  fx_hgrad(r, x + TILE_SIZE - 14.0f, y, 10.0f, TILE_SIZE, FX_INK, 0, FX_INK, 90);
  color_rect(r, (SDL_Color){23, 31, 42, 255}, x + 5.0f, y, 3.0f, TILE_SIZE);
  color_rect(r, (SDL_Color){23, 31, 42, 255}, x + 24.0f, y, 3.0f, TILE_SIZE);
  color_rect(r, (SDL_Color){82, 94, 102, 255}, x + 8.0f, y, 2.0f, TILE_SIZE);
  color_rect(r, (SDL_Color){82, 94, 102, 255}, x + 22.0f, y, 2.0f, TILE_SIZE);
  fx_rect_a(r, (SDL_Color){150, 168, 178, 255}, 90, x + 8.0f, y, 1.0f,
            TILE_SIZE);
  fx_rect_a(r, (SDL_Color){150, 168, 178, 255}, 90, x + 22.0f, y, 1.0f,
            TILE_SIZE);
  color_rect(r, FX_INK, x + 15.0f, y, 2.0f, TILE_SIZE);
  if ((h & 3u) == 0u)
    color_rect(r, (SDL_Color){42, 125, 126, 255}, x + 12.0f, y + 13.0f, 8.0f, 3.0f);
}

static void draw_platform(SDL_Renderer *r, float x, float y, SDL_Color accent, bool unstable)
{
  /* The shadow it throws. A platform with nothing under it is a bar painted on
   * the backdrop; the shadow is what puts it in the room, and it is the only
   * cue the player has that the thing is hanging in mid-air. */
  fx_vgrad(r, x - 1.0f, y + 8.0f, TILE_SIZE + 2.0f, 13.0f, FX_INK, 92,
           FX_INK, 0);

  color_rect(r, FX_INK, x - 1.0f, y - 1.0f, TILE_SIZE + 2.0f, 9.0f);
  color_rect(r, (SDL_Color){50, 61, 66, 255}, x, y, TILE_SIZE, 7.0f);
  fx_vgrad(r, x, y, TILE_SIZE, 7.0f, (SDL_Color){112, 128, 134, 255}, 90,
           (SDL_Color){10, 14, 19, 255}, 130);

  /* Open steel grating: the pattern is what reads as a deck to stand on rather
   * than a solid slab, and it is drawn once so it never fights the accent. */
  for (int slot = 0; slot < 5; ++slot)
    color_rect(r, (SDL_Color){17, 23, 29, 255}, x + 3.0f + (float)slot * 6.0f,
               y + 3.0f, 3.0f, 3.0f);

  /* The accent line stays exactly where it was: which platform this is has to
   * read the same however the steel around it is finished. */
  color_rect(r, accent, x + 1.0f, y, TILE_SIZE - 2.0f, 2.0f);
  fx_glow(r, x + TILE_SIZE * 0.5f, y + 1.0f, 21.0f, accent, 32);

  /* Bolted end plates. */
  color_rect(r, (SDL_Color){132, 143, 139, 255}, x + 1.0f, y + 3.0f, 2.0f, 3.0f);
  color_rect(r, (SDL_Color){132, 143, 139, 255}, x + TILE_SIZE - 3.0f, y + 3.0f,
             2.0f, 3.0f);
  if (unstable)
  {
    /* Hazard marking rather than two dots: this is the platform that drops. */
    for (int chevron = 0; chevron < 3; ++chevron)
    {
      float cx = x + 7.0f + (float)chevron * 7.0f;
      color_rect(r, FX_AMBER, cx, y + 1.0f, 3.0f, 2.0f);
      color_rect(r, FX_RED, cx + 3.0f, y + 1.0f, 3.0f, 2.0f);
    }
  }
}

static void draw_door(SDL_Renderer *r, float x, float y, int index)
{
  /* A small lamp above the frame spills warm light onto the doorway. */
  fx_glow(r, x + 16.0f, y + 1.0f, 16.0f, (SDL_Color){248, 202, 118, 255}, 46);
  color_rect(r, (SDL_Color){15, 20, 30, 255}, x + 10.0f, y - 3.0f, 12.0f, 3.0f);
  color_rect(r, (SDL_Color){248, 202, 118, 255}, x + 12.0f, y - 1.0f, 8.0f, 1.0f);

  /* Sliding steel service door in a machined frame. */
  color_rect(r, FX_INK, x + 1.0f, y, 30.0f, 32.0f);
  color_rect(r, (SDL_Color){62, 75, 90, 255}, x + 2.0f, y + 1.0f, 28.0f, 31.0f);
  color_rect(r, (SDL_Color){88, 104, 120, 255}, x + 2.0f, y + 1.0f, 28.0f, 1.0f);
  color_rect(r, (SDL_Color){34, 43, 56, 255}, x + 5.0f, y + 4.0f, 22.0f, 28.0f);
  color_rect(r, (SDL_Color){44, 55, 70, 255}, x + 6.0f, y + 5.0f, 20.0f, 26.0f);

  /* Recessed viewing slit and brushed panel lines. The lines sit below the
   * number plate rather than through it. */
  color_rect(r, (SDL_Color){12, 18, 26, 255}, x + 9.0f, y + 7.0f, 14.0f, 5.0f);
  color_rect(r, (SDL_Color){30, 66, 76, 255}, x + 10.0f, y + 8.0f, 12.0f, 3.0f);
  for (int line = 0; line < 2; ++line)
    color_rect(r, (SDL_Color){33, 42, 54, 255},
               x + 8.0f, y + 25.0f + line * 4.0f, 16.0f, 1.0f);

  /* Access reader with a live status LED. */
  color_rect(r, (SDL_Color){14, 20, 28, 255}, x + 22.0f, y + 16.0f, 5.0f, 9.0f);
  color_rect(r, (SDL_Color){86, 226, 186, 255}, x + 23.0f, y + 17.0f, 3.0f, 2.0f);

  /* Which of the pair this is, on a plate big enough to hold the digit at the
   * font's own size — the whole point of numbering the doors is that the player
   * can read the number and know where the other one comes out. */
  color_rect(r, (SDL_Color){14, 20, 28, 255}, x + 11.0f, y + 13.0f, 10.0f, 10.0f);
  char label[3];
  SDL_snprintf(label, sizeof(label), "%d", index / 2 + 1);
  draw_text(r, x + 12.0f, y + 14.0f, 1.0f, 148, 176, 188, label);
}

static void draw_restroom_door(SDL_Renderer *r, float x, float y)
{
  fx_glow(r, x + 16.0f, y + 1.0f, 15.0f,
          (SDL_Color){116, 226, 209, 255}, 42);
  color_rect(r, FX_INK, x + 1.0f, y, 30.0f, 32.0f);
  color_rect(r, (SDL_Color){48, 73, 80, 255},
             x + 2.0f, y + 1.0f, 28.0f, 31.0f);
  color_rect(r, (SDL_Color){80, 113, 118, 255},
             x + 4.0f, y + 3.0f, 24.0f, 27.0f);
  color_rect(r, (SDL_Color){25, 43, 50, 255},
             x + 6.0f, y + 5.0f, 20.0f, 23.0f);
  /* The plate is sized to the two letters at the font's own scale rather than
   * the letters being shrunk onto the plate. */
  color_rect(r, (SDL_Color){185, 226, 218, 255},
             x + 7.0f, y + 6.0f, 18.0f, 10.0f);
  draw_text(r, x + 8.0f, y + 7.0f, 1.0f, 31, 72, 73, "WC");
  color_rect(r, (SDL_Color){116, 226, 209, 255},
             x + 22.0f, y + 21.0f, 3.0f, 5.0f);
}

static void draw_exit(SDL_Renderer *r, const Game *game, float x, float y)
{
  bool blocked = game->gameplay.level.map.has_window;
  bool unlocked = game->gameplay.level.runtime.exit_unlocked && !blocked;
  SDL_Color signal = blocked ? (SDL_Color){104, 110, 116, 255}
                     : unlocked ? (SDL_Color){64, 238, 145, 255}
                                : (SDL_Color){230, 75, 61, 255};
  if (unlocked)
    draw_soft_glow(r, x + 5.0f, y + 3.0f, 22.0f, 28.0f, signal);

  color_rect(r, FX_INK, x + 1.0f, y, 30.0f, 32.0f);
  color_rect(r, (SDL_Color){41, 54, 62, 255}, x + 3.0f, y + 2.0f, 26.0f, 30.0f);
  color_rect(r, (SDL_Color){66, 82, 88, 255}, x + 5.0f, y + 4.0f, 22.0f, 26.0f);
  color_rect(r, (SDL_Color){19, 29, 34, 255}, x + 7.0f, y + 12.0f, 18.0f, 10.0f);
  color_rect(r, signal, x + 6.0f, y + 6.0f, 20.0f, 3.0f);
  /*
   * The reader says one thing, in two cells, at the only size the 8x8 font has.
   *
   * It used to spell LOCK across a sixteen-pixel screen at 0.65 of a scale —
   * five-pixel glyphs, resampled, running off the plate and past the right edge
   * of the door itself. The rule the rest of the game follows is that a row
   * that does not fit at scale 1.0 loses words rather than scale, and this is a
   * card reader: two characters is what a card reader has ever shown anybody.
   */
  draw_text(r, x + 8.0f, y + 13.0f, 1.0f, signal.r, signal.g, signal.b,
            blocked ? "--" : (unlocked ? "GO" : "NO"));
  /* The status LED moved out from under the screen it was overlapping. */
  color_rect(r, (SDL_Color){11, 18, 22, 255}, x + 20.0f, y + 24.0f, 5.0f, 4.0f);
  color_rect(r, signal, x + 21.0f, y + 25.0f, 3.0f, 2.0f);
  if (blocked)
  {
    color_rect(r, (SDL_Color){91, 71, 55, 255}, x + 3.0f, y + 7.0f,
               26.0f, 4.0f);
    color_rect(r, (SDL_Color){91, 71, 55, 255}, x + 3.0f, y + 23.0f,
               26.0f, 4.0f);
    color_rect(r, (SDL_Color){151, 126, 82, 255}, x + 4.0f, y + 7.0f,
               24.0f, 1.0f);
  }
}

static void draw_open_window(SDL_Renderer *r, float x, float y)
{
  SDL_Color signal = {91, 238, 183, 255};
  fx_glow(r, x + 16.0f, y + 15.0f, 24.0f, signal, 75);

  color_rect(r, (SDL_Color){12, 16, 24, 255}, x + 1.0f, y,
             30.0f, 32.0f);
  color_rect(r, (SDL_Color){63, 72, 82, 255}, x + 3.0f, y + 2.0f,
             26.0f, 28.0f);
  color_rect(r, (SDL_Color){8, 17, 28, 255},
             x + 6.0f, y + 5.0f, 20.0f, 20.0f);
  color_rect(r, (SDL_Color){53, 69, 79, 255},
             x + 14.0f, y + 5.0f, 3.0f, 20.0f);
  color_rect(r, (SDL_Color){53, 69, 79, 255},
             x + 6.0f, y + 14.0f, 20.0f, 3.0f);
  color_rect(r, (SDL_Color){17, 20, 27, 255}, x + 2.0f, y + 27.0f,
             28.0f, 5.0f);
  color_rect(r, (SDL_Color){112, 120, 126, 255}, x, y + 27.0f,
             32.0f, 2.0f);

  draw_text(r, x + 10.0f, y + 9.0f, 1.0f,
            signal.r, signal.g, signal.b, ">");
}

static void draw_facade_closed_window(SDL_Renderer *r, float x, float y,
                                      unsigned variant, float world_t)
{
  float light = 0.0f;
  if (variant % 3u == 0u)
  {
    float period = 6.5f + (float)(variant % 5u) * 0.9f;
    float phase_offset = (float)(variant % 23u) * 1.17f;
    float phase = fmodf(world_t + phase_offset, period);
    float fade_in_end = 0.45f;
    float fade_out_start = period * 0.52f;
    float fade_out_end = fade_out_start + 0.55f;
    if (phase < fade_in_end)
      light = phase / fade_in_end;
    else if (phase < fade_out_start)
      light = 1.0f;
    else if (phase < fade_out_end)
      light = 1.0f - (phase - fade_out_start) /
                         (fade_out_end - fade_out_start);
  }

  SDL_Color glass = fx_mix((SDL_Color){12, 24, 35, 255},
                           (SDL_Color){125, 91, 49, 255}, light);
  SDL_Color reflection = fx_mix((SDL_Color){37, 66, 82, 255},
                                (SDL_Color){196, 144, 72, 255}, light);

  if (light > 0.02f)
  {
    fx_glow(r, x + 16.0f, y + 24.0f, 22.0f,
            (SDL_Color){220, 158, 76, 255}, (Uint8)(light * 42.0f));
  }

  color_rect(r, (SDL_Color){19, 20, 24, 255}, x - 2.0f, y - 2.0f,
             36.0f, 54.0f);
  color_rect(r, (SDL_Color){80, 77, 74, 255}, x, y,
             32.0f, 50.0f);
  color_rect(r, glass, x + 4.0f, y + 4.0f, 24.0f, 41.0f);
  color_rect(r, reflection, x + 6.0f, y + 6.0f, 3.0f, 36.0f);
  color_rect(r, (SDL_Color){50, 52, 55, 255}, x + 15.0f, y + 4.0f,
             3.0f, 41.0f);
  color_rect(r, (SDL_Color){50, 52, 55, 255}, x + 4.0f, y + 24.0f,
             24.0f, 3.0f);
  if (variant % 7u == 0u)
  {
    color_rect(r, (SDL_Color){72, 63, 57, 255}, x + 19.0f, y + 5.0f,
               8.0f, 38.0f);
  }
  color_rect(r, (SDL_Color){28, 29, 32, 255}, x - 3.0f, y + 49.0f,
             38.0f, 5.0f);
  color_rect(r, (SDL_Color){105, 99, 91, 255}, x - 3.0f, y + 49.0f,
             38.0f, 2.0f);
}

static void draw_facade_open_window(SDL_Renderer *r, float x, float y,
                                    bool destination)
{
  SDL_Color signal = {91, 238, 183, 255};
  if (destination)
    fx_glow(r, x + 16.0f, y + 24.0f, 27.0f, signal, 82);

  color_rect(r, (SDL_Color){15, 17, 22, 255}, x - 2.0f, y - 2.0f,
             36.0f, 54.0f);
  color_rect(r, (SDL_Color){82, 78, 73, 255}, x, y,
             32.0f, 50.0f);
  color_rect(r, (SDL_Color){5, 10, 17, 255}, x + 4.0f, y + 4.0f,
             24.0f, 41.0f);
  color_rect(r, (SDL_Color){46, 52, 59, 255}, x + 5.0f, y + 5.0f,
             4.0f, 39.0f);
  color_rect(r, (SDL_Color){97, 91, 82, 255}, x - 3.0f, y + 49.0f,
             38.0f, 5.0f);
  color_rect(r, (SDL_Color){139, 130, 116, 255}, x - 3.0f, y + 49.0f,
             38.0f, 2.0f);
  if (destination)
  {
    draw_text(r, x + 11.0f, y + 17.0f, 1.0f,
              signal.r, signal.g, signal.b, "^");
  }
}

/*
 * Masonry on the facade: the projecting stone cornice that caps each floor,
 * the thing the climb is actually routed around and the thing bricks shatter
 * against. Some tiles carry a plant unit on top; that is only paint, since a
 * lone solid tile between two cornices would seal the gap the player needs.
 */
static void draw_facade_ledge(SDL_Renderer *r, const Level *level,
                              int col, int row, float x, float y,
                              float world_t)
{
  bool left = level_is_solid(level, col - 1, row);
  bool right = level_is_solid(level, col + 1, row);
  unsigned variant = tile_hash(col, row);

  if (variant % 11u == 0u && !level_is_solid(level, col, row - 1))
  {
    /* Condenser unit standing on the cornice: a louvered case with a slow
     * fan behind it, kept low so it never reads as something to climb. */
    float ux = x + 6.0f;
    float uy = y - 13.0f;
    color_rect(r, (SDL_Color){11, 13, 17, 255}, ux - 1.0f, uy - 1.0f,
               22.0f, 16.0f);
    color_rect(r, (SDL_Color){64, 68, 73, 255}, ux, uy, 20.0f, 14.0f);
    fx_vgrad(r, ux, uy, 20.0f, 14.0f,
             (SDL_Color){84, 88, 92, 255}, 255,
             (SDL_Color){45, 49, 55, 255}, 255);
    color_rect(r, (SDL_Color){30, 33, 38, 255}, ux + 2.0f, uy + 2.0f,
               16.0f, 10.0f);
    for (int slat = 0; slat < 4; ++slat)
    {
      color_rect(r, (SDL_Color){74, 79, 84, 255},
                 ux + 3.0f + (float)slat * 4.0f, uy + 3.0f, 1.0f, 8.0f);
    }
    float spin = world_t * 2.6f + (float)(variant % 7u);
    float blade_x = cosf(spin) * 4.0f;
    float blade_y = sinf(spin) * 3.0f;
    color_rect(r, (SDL_Color){104, 110, 116, 255},
               ux + 10.0f - blade_x, uy + 7.0f - blade_y,
               fabsf(blade_x) * 2.0f + 1.0f, fabsf(blade_y) * 2.0f + 1.0f);
    color_rect(r, (SDL_Color){20, 22, 27, 255}, ux + 9.0f, uy + 6.0f,
               2.0f, 2.0f);
    color_rect(r, (SDL_Color){96, 100, 105, 255}, ux, uy, 20.0f, 1.0f);
  }
  else if (variant % 13u == 0u && !level_is_solid(level, col, row - 1))
  {
    /* Vent stack and a bracketed pipe run breaking up a long cornice. */
    color_rect(r, (SDL_Color){14, 16, 20, 255}, x + 10.0f, y - 12.0f,
               11.0f, 15.0f);
    color_rect(r, (SDL_Color){86, 82, 76, 255}, x + 11.0f, y - 11.0f,
               9.0f, 13.0f);
    color_rect(r, (SDL_Color){118, 112, 100, 255}, x + 9.0f, y - 14.0f,
               13.0f, 3.0f);
  }

  /* Stone cornice: a lit top face, a shadowed soffit, and a dark underside
   * so the climbable gaps between runs stay obvious. The stone takes its
   * colour from the climb's theme, so the same wall reads wet under the storm
   * and silver under the moon while the silhouette the player routes around
   * is unchanged. */
  const LevelThemeArt *art = level_art(level->map.theme);
  SDL_Color stone = art->trim;
  SDL_Color soffit = fx_mix(stone, art->wall_dark, 0.55f);

  /* The shadow the cornice throws down the wall. A projecting stone that casts
   * nothing is not projecting: this one line is what turns the whole run from
   * a grey bar drawn over the masonry into something standing off it. */
  fx_vgrad(r, x, y + 32.0f, 32.0f, 20.0f, FX_INK, 96, FX_INK, 0);

  color_rect(r, fx_mix(art->wall_dark, FX_INK, 0.4f), x, y + 1.0f, 32.0f, 31.0f);
  color_rect(r, stone, x, y + 2.0f, 32.0f, 9.0f);
  color_rect(r, art->trim_hi, x, y + 2.0f, 32.0f, 3.0f);
  color_rect(r, soffit, x, y + 11.0f, 32.0f, 13.0f);
  fx_vgrad(r, x, y + 11.0f, 32.0f, 13.0f,
           fx_mix(soffit, stone, 0.3f), 255,
           fx_mix(soffit, FX_INK, 0.35f), 255);

  /* The cornice is cut from blocks like the wall it sits on, so it takes the
   * same vertical joints and the same variation from one stone to the next. */
  for (int stone_block = 0; stone_block < 2; ++stone_block)
  {
    unsigned bh = tile_hash(col * 2 + stone_block, row + 601);
    float bx = x + (float)stone_block * 16.0f;
    fx_rect_a(r, (bh & 1u) ? art->trim_hi : FX_INK,
              (Uint8)(12u + (bh >> 5) % 22u), bx, y + 2.0f, 16.0f, 22.0f);
    fx_rect_a(r, FX_INK, 90, bx, y + 4.0f, 1.0f, 20.0f);
  }

  /* A drip mould under the nose: the groove that stops rain running back along
   * the soffit, and the line that reads as the cornice's own thickness. */
  fx_rect_a(r, FX_INK, 120, x, y + 20.0f, 32.0f, 2.0f);
  fx_rect_a(r, art->trim_hi, 40, x, y + 22.0f, 32.0f, 1.0f);

  color_rect(r, fx_mix(art->wall_dark, FX_INK, 0.3f), x, y + 24.0f, 32.0f, 8.0f);
  if ((variant & 3u) == 0u)
  {
    /* Occasional weathering keeps a long run from looking extruded. */
    color_rect(r, fx_mix(stone, art->wall_dark, 0.6f), x + 7.0f, y + 5.0f,
               9.0f, 2.0f);
  }
  SDL_Color edge = fx_mix(stone, art->trim_hi, 0.35f);
  if (!left)
    color_rect(r, edge, x, y + 2.0f, 2.0f, 22.0f);
  if (!right)
    color_rect(r, edge, x + 30.0f, y + 2.0f, 2.0f, 22.0f);
}

static void draw_facade_hazard_source(SDL_Renderer *r,
                                      const FacadeHazardSpawn *spawn,
                                      float cam_x, float oy, float world_t,
                                      float windup)
{
  float x = spawn->x - TILE_SIZE * 0.5f - cam_x;
  float y = spawn->y - TILE_SIZE * 0.5f + oy;
  if (spawn->type == FACADE_HAZARD_BIRD)
  {
    draw_facade_closed_window(r, x, y,
                              tile_hash((int)(spawn->x / TILE_SIZE),
                                        (int)(spawn->y / TILE_SIZE)),
                              world_t);
    /* A perched silhouette telegraphs the direction from which birds enter. */
    float bob = sinf(world_t * 3.0f + spawn->y * 0.02f);
    color_rect(r, (SDL_Color){12, 15, 22, 255}, x + 8.0f,
               y + 45.0f + bob, 16.0f, 5.0f);
    color_rect(r, (SDL_Color){18, 21, 29, 255}, x + 12.0f,
               y + 40.0f + bob, 8.0f, 7.0f);
    color_rect(r, (SDL_Color){172, 126, 54, 255}, x + 18.0f,
               y + 42.0f + bob, 3.0f, 2.0f);
    return;
  }

  draw_facade_open_window(r, x, y, false);
  /* Hands and a held shape make the danger readable before the first throw. */
  color_rect(r, (SDL_Color){108, 75, 61, 255}, x + 8.0f, y + 35.0f,
             5.0f, 3.0f);
  color_rect(r, (SDL_Color){108, 75, 61, 255}, x + 19.0f, y + 35.0f,
             5.0f, 3.0f);
  color_rect(r, (SDL_Color){91, 101, 108, 255}, x + 13.0f, y + 30.0f,
             6.0f, 6.0f);

  if (windup <= 0.0f)
    return;

  /* The wind-up is the contract with the player: the thrower leans out and
   * raises the object for a beat before anything is in the air. */
  float lean = 6.0f * (1.0f - windup);
  color_rect(r, (SDL_Color){44, 52, 62, 255}, x + 9.0f, y + 20.0f - lean,
             14.0f, 17.0f);
  color_rect(r, (SDL_Color){64, 74, 86, 255}, x + 10.0f, y + 21.0f - lean,
             12.0f, 5.0f);
  color_rect(r, (SDL_Color){186, 134, 96, 255}, x + 12.0f, y + 12.0f - lean,
             9.0f, 8.0f);
  color_rect(r, (SDL_Color){108, 75, 61, 255}, x + 21.0f, y + 14.0f - lean,
             5.0f, 4.0f);
  color_rect(r, (SDL_Color){150, 96, 66, 255}, x + 24.0f, y + 8.0f - lean,
             8.0f, 8.0f);
  float pulse = 0.55f + 0.45f * sinf(world_t * 18.0f);
  fx_glow(r, x + 16.0f, y + 6.0f, 18.0f, (SDL_Color){236, 96, 72, 255},
          (Uint8)(70.0f * pulse));
  draw_text(r, x + 13.0f, y - 2.0f, 1.0f, 244, 132, 96, "!");
}

static void draw_terminal(SDL_Renderer *r, float x, float y,
                          bool active, bool hacked, bool route_blocked,
                          float world_t)
{
  SDL_Color screen = active ? (SDL_Color){68, 245, 159, 255}
                            : (SDL_Color){62, 75, 79, 255};
  bool pulse = ((int)(world_t * 4.0f) & 1) == 0;

  if (active)
  {
    draw_soft_glow(r, x + 5.0f, y + 3.0f, 22.0f, 25.0f, screen);
    color_rect(r, pulse ? (SDL_Color){126, 255, 196, 255} : screen,
               x + 13.0f, y - 4.0f, 6.0f, 3.0f);
    color_rect(r, screen, x + 15.0f, y - 7.0f, 2.0f, 4.0f);
  }

  color_rect(r, FX_INK, x + 2.0f, y + 1.0f, 28.0f, 30.0f);
  color_rect(r, active ? (SDL_Color){38, 72, 69, 255}
                       : (SDL_Color){42, 48, 51, 255},
             x + 4.0f, y + 3.0f, 24.0f, 27.0f);
  color_rect(r, (SDL_Color){11, 19, 23, 255},
             x + 6.0f, y + 5.0f, 20.0f, 13.0f);
  color_rect(r, active ? screen : (SDL_Color){50, 61, 64, 255},
             x + 8.0f, y + 7.0f, 16.0f, 9.0f);

  if (active)
  {
    color_rect(r, (SDL_Color){10, 76, 60, 255},
               x + 10.0f, y + 9.0f, 12.0f, 1.0f);
    color_rect(r, (SDL_Color){193, 255, 218, 255},
               x + 10.0f, y + 12.0f, hacked ? 12.0f : 7.0f, 2.0f);
  }
  else
  {
    color_rect(r, (SDL_Color){29, 35, 38, 255},
               x + 10.0f, y + 9.0f, 12.0f, 1.0f);
    color_rect(r, (SDL_Color){88, 47, 44, 255},
               x + 15.0f, y + 12.0f, 2.0f, 2.0f);
  }

  /*
   * The status strip under the screen, in two cells at scale 1.0.
   *
   * Four-letter words at 0.55 of a scale are four-pixel glyphs: at that size
   * LIVE, OPEN, FAIL and OFF were not words the player could read, they were a
   * smear of lit pixels that happened to differ. Two cells is what fits at the
   * only size the font is sharp at, and the strip lost its three decorative
   * keys to make room — the readout is the thing worth the space, and the state
   * it names was never anywhere else on the prop.
   */
  color_rect(r, (SDL_Color){20, 27, 30, 255},
             x + 6.0f, y + 20.0f, 20.0f, 10.0f);
  color_rect(r, active ? (SDL_Color){38, 72, 69, 255}
                       : (SDL_Color){42, 48, 51, 255},
             x + 6.0f, y + 20.0f, 20.0f, 1.0f);
  draw_text(r, x + 8.0f, y + 21.0f, 1.0f, screen.r, screen.g, screen.b,
            active ? (hacked ? (route_blocked ? "NO" : "OK") : "ON") : "--");
}

/* `steady` is the player's reduced-motion switch: a raised alarm still lights
 * this call point and still throws its cone across the wall, it simply stops
 * blinking at 5Hz to do it. The lamp takes the mean of the two states it would
 * otherwise alternate between and the cone half its alpha, so the amount of
 * red on the wall is about what it always was. */
static void draw_alarm_switch(SDL_Renderer *r, float x, float y,
                              bool alarm_active, bool source,
                              bool being_used, float world_t, bool steady)
{
  const SDL_Color lamp_lit = {255, 76, 58, 255};
  const SDL_Color lamp_dim = {150, 29, 27, 255};
  bool flash = !steady && ((int)(world_t * 5.0f) & 1) == 0;
  SDL_Color signal = alarm_active
                         ? (steady ? fx_mix(lamp_dim, lamp_lit, 0.5f)
                                   : (flash ? lamp_lit : lamp_dim))
                         : (being_used ? FX_AMBER
                                       : (SDL_Color){108, 44, 40, 255});

  if (alarm_active || being_used)
    draw_soft_glow(r, x + 10.0f, y + 8.0f, 12.0f, 15.0f, signal);
  if (alarm_active && (flash || steady))
  {
    float lit = steady ? 0.5f : 1.0f;
    fx_glow(r, x + 16.0f, y + 7.0f, source ? 30.0f : 22.0f,
            (SDL_Color){255, 48, 38, 255},
            (Uint8)((source ? 105.0f : 65.0f) * lit));
    fx_light_cone(r, x + 16.0f, y + 7.0f, 3.0f, 24.0f, 54.0f,
                  (SDL_Color){255, 54, 42, 255},
                  (Uint8)((source ? 54.0f : 34.0f) * lit));
  }

  /* Compact wall-mounted call point: a shallow metal housing, one status
   * lamp and a thumb-sized recessed button. */
  color_rect(r, (SDL_Color){16, 20, 23, 170},
             x + 11.0f, y + 9.0f, 13.0f, 16.0f);
  color_rect(r, FX_INK, x + 10.0f, y + 7.0f, 12.0f, 16.0f);
  color_rect(r, (SDL_Color){68, 76, 77, 255},
             x + 11.0f, y + 8.0f, 10.0f, 14.0f);
  color_rect(r, (SDL_Color){211, 162, 45, 255},
             x + 11.0f, y + 8.0f, 10.0f, 2.0f);
  color_rect(r, signal, x + 15.0f, y + 10.0f, 3.0f, 2.0f);
  color_rect(r, (SDL_Color){23, 26, 29, 255},
             x + 13.0f, y + 13.0f, 6.0f, 7.0f);
  color_rect(r, signal, x + 14.0f, y + 14.0f, 4.0f, 5.0f);
  color_rect(r, (SDL_Color){255, 151, 102, 255},
             x + 15.0f, y + 15.0f, 2.0f, 1.0f);
  color_rect(r, (SDL_Color){31, 36, 38, 255},
             x + 15.0f, y + 23.0f, 2.0f, 7.0f);
}

static void draw_office_chair(SDL_Renderer *r, float x, float y)
{
  /* Low-backed swivel chair, kept cool and subdued so actors read over it. */
  color_rect(r, (SDL_Color){3, 5, 9, 120}, x + 4.0f, y + 29.0f, 25.0f, 2.0f);

  color_rect(r, FX_INK, x + 5.0f, y + 4.0f, 17.0f, 15.0f);
  color_rect(r, (SDL_Color){43, 56, 68, 255}, x + 7.0f, y + 6.0f, 13.0f, 11.0f);
  color_rect(r, (SDL_Color){67, 84, 96, 255}, x + 8.0f, y + 7.0f, 11.0f, 2.0f);
  color_rect(r, (SDL_Color){28, 37, 48, 255}, x + 8.0f, y + 15.0f, 11.0f, 2.0f);

  color_rect(r, FX_INK, x + 7.0f, y + 17.0f, 20.0f, 6.0f);
  color_rect(r, (SDL_Color){61, 77, 88, 255}, x + 9.0f, y + 18.0f, 16.0f, 3.0f);
  color_rect(r, (SDL_Color){93, 108, 116, 255}, x + 10.0f, y + 18.0f, 14.0f, 1.0f);

  color_rect(r, (SDL_Color){18, 25, 34, 255}, x + 15.0f, y + 22.0f, 4.0f, 7.0f);
  color_rect(r, (SDL_Color){87, 101, 108, 255}, x + 16.0f, y + 22.0f, 2.0f, 6.0f);
  color_rect(r, FX_INK, x + 8.0f, y + 27.0f, 18.0f, 3.0f);
  color_rect(r, (SDL_Color){76, 88, 94, 255}, x + 10.0f, y + 27.0f, 14.0f, 1.0f);
  color_rect(r, FX_INK, x + 7.0f, y + 29.0f, 5.0f, 3.0f);
  color_rect(r, FX_INK, x + 23.0f, y + 29.0f, 5.0f, 3.0f);
}

static void draw_office_desk(SDL_Renderer *r, float x, float y)
{
  /* Compact workstation: desk, drawer, monitor and keyboard in one tile. */
  color_rect(r, (SDL_Color){3, 5, 9, 120}, x + 1.0f, y + 30.0f, 31.0f, 2.0f);

  color_rect(r, FX_INK, x + 8.0f, y + 1.0f, 19.0f, 14.0f);
  color_rect(r, (SDL_Color){37, 51, 61, 255}, x + 10.0f, y + 3.0f, 15.0f, 10.0f);
  color_rect(r, (SDL_Color){41, 119, 124, 255}, x + 11.0f, y + 4.0f, 13.0f, 8.0f);
  color_rect(r, (SDL_Color){92, 196, 190, 255}, x + 12.0f, y + 5.0f, 7.0f, 1.0f);
  color_rect(r, (SDL_Color){21, 74, 80, 255}, x + 12.0f, y + 8.0f, 10.0f, 1.0f);
  color_rect(r, (SDL_Color){21, 74, 80, 255}, x + 12.0f, y + 10.0f, 6.0f, 1.0f);
  color_rect(r, (SDL_Color){91, 104, 108, 255}, x + 16.0f, y + 14.0f, 4.0f, 3.0f);

  color_rect(r, FX_INK, x, y + 16.0f, 32.0f, 6.0f);
  color_rect(r, (SDL_Color){91, 69, 49, 255}, x + 1.0f, y + 17.0f, 30.0f, 4.0f);
  color_rect(r, (SDL_Color){150, 111, 66, 255}, x + 2.0f, y + 17.0f, 28.0f, 1.0f);
  color_rect(r, (SDL_Color){25, 32, 38, 255}, x + 20.0f, y + 14.0f, 9.0f, 2.0f);
  color_rect(r, (SDL_Color){99, 109, 108, 255}, x + 21.0f, y + 14.0f, 7.0f, 1.0f);

  color_rect(r, FX_INK, x + 3.0f, y + 21.0f, 5.0f, 11.0f);
  color_rect(r, (SDL_Color){70, 54, 43, 255}, x + 4.0f, y + 21.0f, 3.0f, 10.0f);
  color_rect(r, FX_INK, x + 25.0f, y + 21.0f, 5.0f, 11.0f);
  color_rect(r, (SDL_Color){70, 54, 43, 255}, x + 26.0f, y + 21.0f, 3.0f, 10.0f);
  color_rect(r, (SDL_Color){45, 49, 49, 255}, x + 21.0f, y + 22.0f, 7.0f, 6.0f);
  color_rect(r, (SDL_Color){93, 101, 98, 255}, x + 23.0f, y + 24.0f, 3.0f, 1.0f);
}

static void draw_office_equipment(SDL_Renderer *r, float x, float y,
                                  unsigned variant, float world_t)
{
  color_rect(r, (SDL_Color){3, 5, 9, 120}, x + 3.0f, y + 30.0f, 27.0f, 2.0f);

  if (variant == 0u)
  {
    /* Filing cabinet with alternating label holders and recessed handles. */
    color_rect(r, FX_INK, x + 5.0f, y + 2.0f, 23.0f, 30.0f);
    color_rect(r, (SDL_Color){61, 72, 78, 255}, x + 7.0f, y + 4.0f, 19.0f, 27.0f);
    color_rect(r, (SDL_Color){100, 112, 113, 255}, x + 8.0f, y + 5.0f, 17.0f, 2.0f);
    for (int drawer = 0; drawer < 3; ++drawer)
    {
      float dy = y + 7.0f + drawer * 8.0f;
      color_rect(r, (SDL_Color){34, 43, 49, 255}, x + 8.0f, dy, 17.0f, 7.0f);
      color_rect(r, (SDL_Color){76, 88, 91, 255}, x + 9.0f, dy + 1.0f, 15.0f, 5.0f);
      color_rect(r, (SDL_Color){20, 27, 31, 255}, x + 13.0f, dy + 2.0f, 7.0f, 2.0f);
      color_rect(r, (SDL_Color){145, 151, 137, 255}, x + 15.0f, dy + 2.0f, 3.0f, 1.0f);
    }
  }
  else if (variant == 1u)
  {
    /* Floor-standing copier/printer with a sheet left in the output tray. */
    color_rect(r, FX_INK, x + 3.0f, y + 12.0f, 27.0f, 20.0f);
    color_rect(r, (SDL_Color){75, 83, 83, 255}, x + 5.0f, y + 14.0f, 23.0f, 17.0f);
    color_rect(r, FX_INK, x + 7.0f, y + 7.0f, 20.0f, 9.0f);
    color_rect(r, (SDL_Color){113, 119, 114, 255}, x + 9.0f, y + 8.0f, 16.0f, 6.0f);
    color_rect(r, (SDL_Color){37, 48, 52, 255}, x + 6.0f, y + 17.0f, 21.0f, 5.0f);
    color_rect(r, (SDL_Color){204, 207, 187, 255}, x + 10.0f, y + 19.0f, 13.0f, 5.0f);
    color_rect(r, (SDL_Color){150, 157, 145, 255}, x + 11.0f, y + 20.0f, 11.0f, 1.0f);
    color_rect(r, (SDL_Color){50, 189, 155, 255}, x + 24.0f, y + 15.0f, 2.0f, 2.0f);
    color_rect(r, (SDL_Color){35, 42, 43, 255}, x + 8.0f, y + 27.0f, 17.0f, 2.0f);
  }
  else
  {
    /* Small office server/network rack with asynchronous status lights. */
    color_rect(r, FX_INK, x + 5.0f, y + 1.0f, 23.0f, 31.0f);
    color_rect(r, (SDL_Color){39, 49, 58, 255}, x + 7.0f, y + 3.0f, 19.0f, 28.0f);
    color_rect(r, (SDL_Color){83, 96, 103, 255}, x + 8.0f, y + 4.0f, 17.0f, 2.0f);
    for (int unit = 0; unit < 4; ++unit)
    {
      float uy = y + 7.0f + unit * 6.0f;
      color_rect(r, (SDL_Color){15, 22, 29, 255}, x + 9.0f, uy, 15.0f, 4.0f);
      color_rect(r, (SDL_Color){49, 66, 73, 255}, x + 10.0f, uy + 1.0f, 8.0f, 1.0f);
      bool blink = ((int)(world_t * (2.0f + unit * 0.31f)) + unit) % 3 != 0;
      SDL_Color led = blink ? (SDL_Color){61, 226, 161, 255}
                            : (SDL_Color){35, 76, 65, 255};
      color_rect(r, led, x + 20.0f, uy + 1.0f, 2.0f, 2.0f);
    }
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

static void draw_lobby_counter(SDL_Renderer *r, float x, float y,
                               unsigned variant)
{
  /* Reception desk: a stone-clad front with a brass reveal, a raised
     transaction top, and the working surface implied behind it. */
  color_rect(r, (SDL_Color){3, 5, 9, 120}, x, y + 30.0f, 32.0f, 2.0f);

  color_rect(r, FX_INK, x, y + 8.0f, 32.0f, 24.0f);
  color_rect(r, (SDL_Color){124, 118, 108, 255}, x, y + 10.0f, 32.0f, 21.0f);
  /* Polished stone: lit near the top where the counter light falls on it, and
     falling away into shadow at the plinth. */
  fx_vgrad(r, x, y + 10.0f, 32.0f, 21.0f,
           (SDL_Color){152, 144, 131, 255}, 255,
           (SDL_Color){70, 67, 64, 255}, 255);
  /* Vertical stone joints; the veining is what stops the run reading as one
     extruded box across three tiles. */
  color_rect(r, (SDL_Color){88, 84, 78, 255}, x + 11.0f, y + 10.0f, 1.0f, 21.0f);
  color_rect(r, (SDL_Color){88, 84, 78, 255}, x + 23.0f, y + 10.0f, 1.0f, 21.0f);
  if ((variant & 1u) == 0u)
  {
    color_rect(r, (SDL_Color){170, 161, 146, 255},
               x + 4.0f, y + 15.0f, 6.0f, 1.0f);
  }

  /* Brass reveal above the plinth and the lit counter top. */
  color_rect(r, (SDL_Color){130, 104, 62, 255}, x, y + 27.0f, 32.0f, 2.0f);
  color_rect(r, FX_INK, x - 1.0f, y + 5.0f, 34.0f, 4.0f);
  color_rect(r, (SDL_Color){158, 132, 86, 255}, x - 1.0f, y + 6.0f, 34.0f, 2.0f);
  color_rect(r, (SDL_Color){228, 206, 158, 255}, x - 1.0f, y + 6.0f, 34.0f, 1.0f);

  /* One tile of the run carries the visitor terminal and a card tray. */
  if ((variant % 3u) == 0u)
  {
    color_rect(r, FX_INK, x + 8.0f, y - 6.0f, 15.0f, 12.0f);
    color_rect(r, (SDL_Color){37, 45, 52, 255}, x + 9.0f, y - 5.0f, 13.0f, 10.0f);
    color_rect(r, (SDL_Color){96, 176, 172, 255}, x + 11.0f, y - 3.0f, 9.0f, 6.0f);
    color_rect(r, (SDL_Color){196, 236, 230, 255},
               x + 12.0f, y - 2.0f, 5.0f, 1.0f);
  }
  else
  {
    color_rect(r, FX_INK, x + 18.0f, y + 1.0f, 11.0f, 5.0f);
    color_rect(r, (SDL_Color){206, 198, 178, 255},
               x + 19.0f, y + 2.0f, 9.0f, 3.0f);
    color_rect(r, (SDL_Color){158, 132, 86, 255},
               x + 20.0f, y + 3.0f, 5.0f, 1.0f);
  }
}

static void draw_lobby_sofa(SDL_Renderer *r, float x, float y)
{
  /* Waiting-area bench seat, seen from the side: low back, deep cushion and
     tapered legs. Kept dark and warm so actors read over it. */
  color_rect(r, (SDL_Color){3, 5, 9, 120}, x + 2.0f, y + 30.0f, 28.0f, 2.0f);

  color_rect(r, FX_INK, x + 2.0f, y + 12.0f, 28.0f, 10.0f);
  color_rect(r, (SDL_Color){74, 56, 52, 255}, x + 3.0f, y + 13.0f, 26.0f, 8.0f);
  color_rect(r, (SDL_Color){104, 78, 68, 255}, x + 4.0f, y + 13.0f, 24.0f, 2.0f);

  /* Back cushion set behind the seat, and a bolster at the near arm. */
  color_rect(r, FX_INK, x + 2.0f, y + 4.0f, 12.0f, 10.0f);
  color_rect(r, (SDL_Color){88, 65, 58, 255}, x + 3.0f, y + 5.0f, 10.0f, 9.0f);
  color_rect(r, (SDL_Color){118, 88, 74, 255}, x + 4.0f, y + 5.0f, 8.0f, 2.0f);
  color_rect(r, FX_INK, x + 24.0f, y + 8.0f, 7.0f, 6.0f);
  color_rect(r, (SDL_Color){96, 71, 62, 255}, x + 25.0f, y + 9.0f, 5.0f, 4.0f);

  color_rect(r, (SDL_Color){45, 36, 34, 255}, x + 3.0f, y + 21.0f, 26.0f, 2.0f);
  color_rect(r, FX_INK, x + 5.0f, y + 23.0f, 3.0f, 8.0f);
  color_rect(r, FX_INK, x + 25.0f, y + 23.0f, 3.0f, 8.0f);
  color_rect(r, (SDL_Color){130, 104, 62, 255}, x + 6.0f, y + 23.0f, 1.0f, 7.0f);
  color_rect(r, (SDL_Color){130, 104, 62, 255}, x + 26.0f, y + 23.0f, 1.0f, 7.0f);
}

static void draw_lobby_planter(SDL_Renderer *r, float x, float y,
                               unsigned variant, float world_t)
{
  /* A stone planter with a broad-leaved palm. Leaves are drawn as tapered
     quads rather than as lines: a fan of one-pixel strokes reads as grass,
     which is what the old backdrop planting looked like. */
  color_rect(r, (SDL_Color){3, 5, 9, 120}, x + 5.0f, y + 30.0f, 22.0f, 2.0f);

  const int leaves = 5;
  for (int leaf = 0; leaf < leaves; ++leaf)
  {
    float spread = ((float)leaf - (float)(leaves - 1) * 0.5f) / 2.0f;
    /* Each frond sways on its own phase, so the plant never looks stamped. */
    float sway = sinf(world_t * 0.7f + (float)leaf * 1.9f +
                      (float)(variant % 5u)) *
                 1.6f;
    float tip_x = x + 16.0f + spread * 13.0f + sway;
    float tip_y = y + 5.0f + fabsf(spread) * 7.0f;
    float base_x = x + 16.0f - spread * 2.0f;
    float mid_x = (base_x + tip_x) * 0.5f;
    float mid_y = (y + 20.0f + tip_y) * 0.5f - 3.0f;
    SDL_Color blade = leaf % 2 == 0 ? (SDL_Color){46, 86, 58, 255}
                                    : (SDL_Color){36, 68, 48, 255};
    color_quad(r, FX_INK, base_x - 2.0f, y + 21.0f, mid_x - 3.0f, mid_y + 3.0f,
               tip_x, tip_y - 1.0f, mid_x + 3.0f, mid_y + 4.0f);
    color_quad(r, blade, base_x - 1.0f, y + 20.0f, mid_x - 2.0f, mid_y + 3.0f,
               tip_x, tip_y + 1.0f, mid_x + 2.0f, mid_y + 4.0f);
    if (leaf % 2 == 0)
    {
      color_quad(r, (SDL_Color){66, 114, 74, 255},
                 base_x, y + 20.0f, mid_x - 1.0f, mid_y + 3.0f,
                 tip_x - 1.0f, tip_y + 2.0f, mid_x, mid_y + 4.0f);
    }
  }
  color_rect(r, (SDL_Color){38, 60, 44, 255}, x + 15.0f, y + 14.0f, 2.0f, 8.0f);

  /* Planter: stone box with a brass rim and dressed bark inside. */
  color_rect(r, FX_INK, x + 5.0f, y + 18.0f, 22.0f, 14.0f);
  color_rect(r, (SDL_Color){80, 76, 71, 255}, x + 6.0f, y + 19.0f, 20.0f, 12.0f);
  fx_vgrad(r, x + 6.0f, y + 19.0f, 20.0f, 12.0f,
           (SDL_Color){104, 99, 92, 255}, 255,
           (SDL_Color){52, 50, 48, 255}, 255);
  color_rect(r, (SDL_Color){158, 132, 86, 255}, x + 5.0f, y + 18.0f, 22.0f, 2.0f);
  color_rect(r, (SDL_Color){228, 206, 158, 255},
             x + 5.0f, y + 18.0f, 22.0f, 1.0f);
  color_rect(r, (SDL_Color){33, 26, 22, 255}, x + 8.0f, y + 20.0f, 16.0f, 2.0f);
}

static void draw_lobby_turnstile(SDL_Renderer *r, float x, float y,
                                 float world_t)
{
  /* Optical security gate: two waist-high pedestals with a glass leaf between
     them and a floor-level indicator that cycles green. It is the fitting that
     explains why the stair door upstairs wants a card. */
  color_rect(r, (SDL_Color){3, 5, 9, 120}, x + 2.0f, y + 30.0f, 28.0f, 2.0f);

  /* Waist-high is chest-high at this scale: the pedestals run most of the tile
     so a pair of them reads as a gate line rather than as two floor bins. */
  for (int side = 0; side < 2; ++side)
  {
    float px = x + (side == 0 ? 2.0f : 20.0f);
    color_rect(r, FX_INK, px, y + 7.0f, 12.0f, 25.0f);
    color_rect(r, (SDL_Color){70, 74, 79, 255}, px + 1.0f, y + 8.0f, 10.0f, 23.0f);
    fx_vgrad(r, px + 1.0f, y + 8.0f, 10.0f, 23.0f,
             (SDL_Color){118, 123, 128, 255}, 255,
             (SDL_Color){42, 45, 50, 255}, 255);
    /* Brushed brass cap, and a shadowed reveal under it. */
    color_rect(r, (SDL_Color){158, 132, 86, 255}, px, y + 7.0f, 12.0f, 3.0f);
    color_rect(r, (SDL_Color){228, 206, 158, 255}, px, y + 7.0f, 12.0f, 1.0f);
    color_rect(r, (SDL_Color){32, 35, 39, 255}, px + 1.0f, y + 10.0f, 10.0f, 1.0f);
  }

  /* The glass leaf standing between them, closed: a tinted pane with a bright
     leading edge and a brass rail along the top. */
  color_rect(r, (SDL_Color){104, 158, 156, 70},
             x + 13.0f, y + 12.0f, 7.0f, 18.0f);
  color_rect(r, (SDL_Color){178, 224, 216, 150},
             x + 13.0f, y + 12.0f, 7.0f, 1.0f);
  color_rect(r, (SDL_Color){178, 224, 216, 110},
             x + 13.0f, y + 12.0f, 1.0f, 18.0f);

  /* Reader plate on the near pedestal, and the pass indicator underfoot. */
  bool clear = fmodf(world_t * 0.8f, 2.0f) < 1.4f;
  SDL_Color signal = clear ? (SDL_Color){96, 226, 158, 255}
                           : (SDL_Color){206, 168, 78, 255};
  color_rect(r, FX_INK, x + 21.0f, y + 13.0f, 8.0f, 6.0f);
  color_rect(r, (SDL_Color){28, 33, 36, 255}, x + 22.0f, y + 14.0f, 6.0f, 4.0f);
  color_rect(r, signal, x + 23.0f, y + 15.0f, 4.0f, 2.0f);
  fx_glow(r, x + 25.0f, y + 16.0f, 12.0f, signal, 52);
  fx_glow(r, x + 16.0f, y + 30.0f, 14.0f, signal, 40);
  color_rect(r, signal, x + 13.0f, y + 30.0f, 7.0f, 1.0f);
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

static void draw_flight_case(SDL_Renderer *r, float x, float y,
                             unsigned variant)
{
  /* The same shell, latches and rust-red docket as the manual's illustration,
     at a fourteenth of the size: the player is meant to recognise it, so the
     case they find on a carpet tile has to be the case they were shown. */
  SDL_Color shell = {49, 55, 58, 255};
  SDL_Color shell_lt = {96, 106, 112, 255};
  SDL_Color foam = {33, 38, 37, 255};
  SDL_Color cut = {12, 15, 16, 255};

  color_rect(r, (SDL_Color){3, 5, 9, 120}, x + 2.0f, y + 30.0f, 28.0f, 2.0f);

  if ((variant & 1u) == 0u)
  {
    /* Standing closed against the wall, handle up. This is the one that has
       not been opened yet, and there is no reading it beyond that. */
    color_rect(r, FX_INK, x + 6.0f, y + 9.0f, 21.0f, 23.0f);
    fx_vgrad(r, x + 7.0f, y + 10.0f, 19.0f, 21.0f, shell_lt, 255,
             fx_mix(shell, FX_INK, 0.5f), 255);
    color_rect(r, (SDL_Color){124, 136, 142, 255}, x + 7.0f, y + 10.0f,
               19.0f, 1.0f);
    /* Corner protectors down the outer edges, and the two latches. */
    color_rect(r, fx_mix(shell, FX_STEEL_LT, 0.4f), x + 6.0f, y + 9.0f,
               3.0f, 6.0f);
    color_rect(r, fx_mix(shell, FX_STEEL_LT, 0.4f), x + 24.0f, y + 9.0f,
               3.0f, 6.0f);
    color_rect(r, fx_mix(shell, FX_STEEL_LT, 0.4f), x + 6.0f, y + 26.0f,
               3.0f, 6.0f);
    color_rect(r, fx_mix(shell, FX_STEEL_LT, 0.4f), x + 24.0f, y + 26.0f,
               3.0f, 6.0f);
    color_rect(r, FX_STEEL_DK, x + 6.0f, y + 18.0f, 21.0f, 4.0f);
    color_rect(r, FX_STEEL_LT, x + 6.0f, y + 18.0f, 21.0f, 1.0f);
    /* Sprung handle folded flat on the lid. */
    color_rect(r, FX_INK, x + 12.0f, y + 6.0f, 9.0f, 4.0f);
    color_rect(r, (SDL_Color){88, 96, 100, 255}, x + 13.0f, y + 7.0f,
               7.0f, 2.0f);
    /* The docket, still on it. */
    color_rect(r, fx_dim(FX_RUST, 0.85f), x + 10.0f, y + 12.0f, 13.0f, 3.0f);
    color_rect(r, (SDL_Color){150, 154, 148, 255}, x + 10.0f, y + 23.0f,
               13.0f, 5.0f);
    for (int bar = 0; bar < 6; ++bar)
      color_rect(r, (SDL_Color){24, 28, 30, 255},
                 x + 11.0f + (float)bar * 2.0f, y + 24.0f,
                 (bar & 1) ? 1.0f : 2.0f, 3.0f);
  }
  else
  {
    /* Open on the floor with the lid propped back, and the long cutout in the
       foam empty. What was in it is what the player has been picking up. */
    color_rect(r, FX_INK, x + 3.0f, y + 8.0f, 8.0f, 24.0f);
    fx_vgrad(r, x + 4.0f, y + 9.0f, 6.0f, 22.0f, shell_lt, 255,
             fx_mix(shell, FX_INK, 0.5f), 255);
    color_rect(r, (SDL_Color){31, 36, 39, 255}, x + 5.0f, y + 11.0f,
               4.0f, 18.0f);
    color_rect(r, fx_dim(FX_RUST, 0.85f), x + 5.0f, y + 13.0f, 4.0f, 2.0f);
    color_rect(r, (SDL_Color){140, 144, 138, 255}, x + 5.0f, y + 18.0f,
               4.0f, 6.0f);

    color_rect(r, FX_INK, x + 10.0f, y + 20.0f, 20.0f, 12.0f);
    color_rect(r, shell, x + 11.0f, y + 21.0f, 18.0f, 10.0f);
    color_rect(r, shell_lt, x + 11.0f, y + 21.0f, 18.0f, 1.0f);
    color_rect(r, foam, x + 12.0f, y + 22.0f, 16.0f, 8.0f);
    /* The empty bed: a barrel channel, a receiver box and a pistol-grip
       notch, so the hole in the foam is unmistakably the shape of a rifle. */
    color_rect(r, cut, x + 13.0f, y + 24.0f, 14.0f, 2.0f);
    color_rect(r, cut, x + 17.0f, y + 23.0f, 6.0f, 4.0f);
    color_rect(r, cut, x + 19.0f, y + 27.0f, 3.0f, 2.0f);
    color_rect(r, (SDL_Color){44, 50, 49, 255}, x + 12.0f, y + 22.0f,
               16.0f, 1.0f);
  }
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
 * painted clock, and a painted clock says nothing is happening.
 */
static void draw_wall_clock(SDL_Renderer *r, float x, float y,
                            int level_index, float world_t)
{
  float minute = NIGHT_CLOCK_FIRST_MINUTE +
                 (float)level_index * NIGHT_CLOCK_MINUTES_PER_SECTOR;
  if (minute > 58.0f)
    minute = 58.0f;

  float cx = x + 16.0f;
  float cy = y + 14.0f;
  SDL_Color rim = {58, 66, 72, 255};
  SDL_Color face = {198, 202, 190, 255};

  /* Screwed to the underside of the slab, not floating in front of it. */
  color_rect(r, FX_INK, cx - 3.0f, y, 6.0f, 5.0f);
  color_rect(r, (SDL_Color){86, 95, 100, 255}, cx - 2.0f, y, 4.0f, 4.0f);

  fx_mass(r, FX_INK, cx - 11.0f, cy - 11.0f, 22.0f, 22.0f, 7, 7);
  fx_mass(r, rim, cx - 10.0f, cy - 10.0f, 20.0f, 20.0f, 6, 6);
  fx_mass(r, fx_mix(rim, FX_STEEL_LT, 0.5f), cx - 10.0f, cy - 10.0f,
          20.0f, 3.0f, 6, 0);
  fx_mass(r, face, cx - 8.0f, cy - 8.0f, 16.0f, 16.0f, 5, 5);
  /* The face is glass, and glass takes the ceiling light across its top. */
  fx_vgrad(r, cx - 6.0f, cy - 7.0f, 12.0f, 7.0f, FX_CREAM, 70, FX_CREAM, 0);

  /* Quarter marks only: twelve of them at this size is a grey ring. */
  SDL_Color tick = {66, 72, 70, 255};
  color_rect(r, tick, cx - 1.0f, cy - 7.0f, 2.0f, 2.0f);
  color_rect(r, tick, cx - 1.0f, cy + 5.0f, 2.0f, 2.0f);
  color_rect(r, tick, cx - 7.0f, cy - 1.0f, 2.0f, 2.0f);
  color_rect(r, tick, cx + 5.0f, cy - 1.0f, 2.0f, 2.0f);

  /* Hands. Midnight has just gone, so the hour hand barely leaves the top;
     the minute hand is the one carrying the reading. */
  float minute_angle = minute * (6.2831853f / 60.0f);
  float hour_angle = minute * (6.2831853f / 720.0f);
  float second_angle = fmodf(world_t, 60.0f) * (6.2831853f / 60.0f);

  fx_set(r, (SDL_Color){38, 44, 44, 255});
  SDL_RenderLine(r, cx, cy, cx + sinf(hour_angle) * 4.0f,
                 cy - cosf(hour_angle) * 4.0f);
  SDL_RenderLine(r, cx, cy + 1.0f, cx + sinf(hour_angle) * 4.0f,
                 cy - cosf(hour_angle) * 4.0f + 1.0f);
  fx_set(r, (SDL_Color){26, 31, 32, 255});
  SDL_RenderLine(r, cx, cy, cx + sinf(minute_angle) * 7.0f,
                 cy - cosf(minute_angle) * 7.0f);
  /* The one red on the dial, and it is a moving one: FX_RUST rather than
     FX_RED, because a clock hand is not a danger signal. */
  fx_set(r, FX_RUST);
  SDL_RenderLine(r, cx, cy, cx + sinf(second_angle) * 6.0f,
                 cy - cosf(second_angle) * 6.0f);
  color_rect(r, (SDL_Color){26, 31, 32, 255}, cx - 1.0f, cy - 1.0f, 2.0f, 2.0f);
}

static void draw_restroom_toilet(SDL_Renderer *r, float x, float y)
{
  /* Side profile: cistern at the wall, projecting seat and a curved pedestal.
   * This silhouette stays readable at one-tile resolution. */
  color_rect(r, (SDL_Color){4, 10, 12, 120},
             x + 2.0f, y + 29.0f, 29.0f, 3.0f);
  color_rect(r, FX_INK, x + 2.0f, y + 4.0f, 13.0f, 20.0f);
  color_rect(r, (SDL_Color){201, 218, 211, 255},
             x + 4.0f, y + 6.0f, 9.0f, 16.0f);
  color_rect(r, (SDL_Color){239, 244, 231, 255},
             x + 3.0f, y + 4.0f, 11.0f, 3.0f);
  color_rect(r, (SDL_Color){55, 92, 91, 255},
             x + 11.0f, y + 9.0f, 3.0f, 4.0f);
  color_rect(r, FX_INK, x + 10.0f, y + 17.0f, 21.0f, 7.0f);
  color_rect(r, (SDL_Color){235, 240, 227, 255},
             x + 11.0f, y + 18.0f, 19.0f, 4.0f);
  color_rect(r, (SDL_Color){67, 122, 129, 255},
             x + 15.0f, y + 20.0f, 12.0f, 2.0f);
  color_rect(r, (SDL_Color){182, 207, 202, 255},
             x + 14.0f, y + 23.0f, 13.0f, 5.0f);
  color_rect(r, (SDL_Color){168, 195, 191, 255},
             x + 16.0f, y + 27.0f, 8.0f, 5.0f);
  color_rect(r, FX_INK, x + 15.0f, y + 30.0f, 11.0f, 2.0f);
}

static void draw_restroom_basin(SDL_Renderer *r, float x, float y)
{
  /* Wall-mounted side profile: the exposed faucet, open bowl and drain pipe
   * avoid the rectangular monitor-and-stand silhouette of the old sprite. */
  color_rect(r, FX_INK, x + 1.0f, y + 3.0f, 7.0f, 11.0f);
  color_rect(r, (SDL_Color){86, 154, 144, 255},
             x + 3.0f, y + 5.0f, 3.0f, 7.0f);
  color_rect(r, (SDL_Color){221, 232, 218, 255},
             x + 4.0f, y + 4.0f, 1.0f, 3.0f);

  /* Tall gooseneck tap with a downward spout. */
  color_rect(r, FX_INK, x + 8.0f, y + 6.0f, 4.0f, 12.0f);
  color_rect(r, (SDL_Color){190, 205, 197, 255},
             x + 9.0f, y + 7.0f, 2.0f, 11.0f);
  color_rect(r, FX_INK, x + 9.0f, y + 5.0f, 14.0f, 4.0f);
  color_rect(r, (SDL_Color){190, 205, 197, 255},
             x + 10.0f, y + 6.0f, 12.0f, 2.0f);
  color_rect(r, FX_INK, x + 20.0f, y + 7.0f, 4.0f, 9.0f);
  color_rect(r, (SDL_Color){190, 205, 197, 255},
             x + 21.0f, y + 8.0f, 2.0f, 7.0f);

  /* Wide open ceramic bowl with visible water, sloped underside and U-bend. */
  color_rect(r, FX_INK, x + 3.0f, y + 15.0f, 29.0f, 8.0f);
  color_rect(r, (SDL_Color){230, 238, 225, 255},
             x + 5.0f, y + 16.0f, 26.0f, 5.0f);
  color_rect(r, (SDL_Color){72, 135, 142, 255},
             x + 9.0f, y + 17.0f, 18.0f, 2.0f);
  color_rect(r, (SDL_Color){193, 211, 204, 255},
             x + 8.0f, y + 22.0f, 19.0f, 4.0f);
  color_rect(r, (SDL_Color){129, 151, 148, 255},
             x + 14.0f, y + 26.0f, 4.0f, 5.0f);
  color_rect(r, (SDL_Color){129, 151, 148, 255},
             x + 14.0f, y + 29.0f, 9.0f, 3.0f);
  color_rect(r, (SDL_Color){129, 151, 148, 255},
             x + 21.0f, y + 27.0f, 3.0f, 5.0f);
}

static void draw_restroom_urinal(SDL_Renderer *r, float x, float y)
{
  /* Wall-hung bowl with an exposed flush pipe. Shorter than the basin and
   * mounted higher, so a row of them reads differently at a glance. */
  color_rect(r, FX_INK, x + 6.0f, y + 1.0f, 5.0f, 9.0f);
  color_rect(r, (SDL_Color){168, 188, 182, 255},
             x + 7.0f, y + 2.0f, 3.0f, 8.0f);
  color_rect(r, FX_INK, x + 9.0f, y + 3.0f, 12.0f, 4.0f);
  color_rect(r, (SDL_Color){190, 205, 197, 255},
             x + 10.0f, y + 4.0f, 10.0f, 2.0f);

  color_rect(r, (SDL_Color){4, 10, 12, 110},
             x + 5.0f, y + 27.0f, 22.0f, 3.0f);
  color_rect(r, FX_INK, x + 4.0f, y + 8.0f, 24.0f, 19.0f);
  color_rect(r, (SDL_Color){233, 239, 226, 255},
             x + 6.0f, y + 9.0f, 20.0f, 16.0f);
  color_rect(r, (SDL_Color){199, 214, 207, 255},
             x + 8.0f, y + 11.0f, 16.0f, 9.0f);
  color_rect(r, (SDL_Color){76, 132, 138, 255},
             x + 11.0f, y + 18.0f, 10.0f, 3.0f);
  color_rect(r, (SDL_Color){146, 168, 163, 255},
             x + 13.0f, y + 24.0f, 6.0f, 5.0f);
  color_rect(r, (SDL_Color){249, 250, 243, 255},
             x + 7.0f, y + 10.0f, 2.0f, 12.0f);
}

static void draw_restroom_partition(SDL_Renderer *r, float x, float y)
{
  /* Narrow stall divider with a raised foot and a visible latch plate. */
  color_rect(r, (SDL_Color){4, 10, 12, 110},
             x + 21.0f, y + 30.0f, 11.0f, 2.0f);
  color_rect(r, FX_INK, x + 22.0f, y, 10.0f, 28.0f);
  color_rect(r, (SDL_Color){48, 86, 84, 255},
             x + 24.0f, y + 2.0f, 7.0f, 24.0f);
  color_rect(r, (SDL_Color){91, 132, 124, 255},
             x + 25.0f, y + 3.0f, 2.0f, 22.0f);
  color_rect(r, (SDL_Color){174, 190, 180, 255},
             x + 23.0f, y + 15.0f, 3.0f, 5.0f);
  color_rect(r, FX_INK, x + 25.0f, y + 27.0f, 5.0f, 5.0f);
}

static void draw_restroom_stall_frame(SDL_Renderer *r, float x, float y)
{
  /* One-tile-scale cubicle frame, sized around the toilet rather than around
   * a full room doorway. */
  color_rect(r, (SDL_Color){4, 10, 12, 120},
             x - 6.0f, y + 29.0f, 46.0f, 3.0f);
  color_rect(r, FX_INK, x - 6.0f, y - 7.0f, 46.0f, 5.0f);
  color_rect(r, (SDL_Color){43, 82, 79, 255},
             x - 3.0f, y - 5.0f, 40.0f, 2.0f);
  color_rect(r, FX_INK, x - 6.0f, y - 3.0f, 5.0f, 35.0f);
  color_rect(r, (SDL_Color){64, 106, 101, 255},
             x - 4.0f, y - 1.0f, 2.0f, 28.0f);
  color_rect(r, FX_INK, x + 35.0f, y - 3.0f, 5.0f, 35.0f);
  color_rect(r, (SDL_Color){88, 130, 121, 255},
             x + 36.0f, y - 1.0f, 2.0f, 28.0f);
  color_rect(r, FX_INK, x - 4.0f, y + 27.0f, 4.0f, 5.0f);
  color_rect(r, FX_INK, x + 36.0f, y + 27.0f, 4.0f, 5.0f);
}

static void draw_restroom_stall_open(SDL_Renderer *r, float x, float y)
{
  /* Recessed tiled back wall and plumbing inside the open cubicle. */
  color_rect(r, (SDL_Color){49, 76, 76, 255},
             x - 1.0f, y - 2.0f, 36.0f, 29.0f);
  for (int line = 0; line < 2; ++line)
    color_rect(r, (SDL_Color){72, 103, 100, 255},
               x - 1.0f, y + 7.0f + line * 13.0f, 36.0f, 1.0f);
  color_rect(r, (SDL_Color){116, 148, 138, 255},
             x + 1.0f, y, 3.0f, 6.0f);
  color_rect(r, (SDL_Color){160, 184, 169, 255},
             x + 2.0f, y, 1.0f, 6.0f);
  color_rect(r, FX_INK, x + 24.0f, y + 1.0f, 10.0f, 7.0f);
  color_rect(r, (SDL_Color){230, 232, 214, 255},
             x + 26.0f, y + 2.0f, 6.0f, 5.0f);
  color_rect(r, (SDL_Color){92, 116, 111, 255},
             x + 28.0f, y + 3.0f, 2.0f, 2.0f);
  color_rect(r, (SDL_Color){230, 232, 214, 255},
             x + 30.0f, y + 6.0f, 2.0f, 3.0f);

  draw_restroom_toilet(r, x + 1.0f, y);
  draw_restroom_stall_frame(r, x, y);

  /* Door folded toward the viewer: a trapezoid makes its open state obvious. */
  color_quad(r, FX_INK,
             x + 29.0f, y, x + 44.0f, y + 4.0f,
             x + 44.0f, y + 28.0f, x + 29.0f, y + 27.0f);
  color_quad(r, (SDL_Color){52, 103, 96, 255},
             x + 32.0f, y + 3.0f, x + 41.0f, y + 6.0f,
             x + 41.0f, y + 24.0f, x + 32.0f, y + 24.0f);
  color_quad(r, (SDL_Color){96, 148, 134, 255},
             x + 33.0f, y + 4.0f, x + 35.0f, y + 5.0f,
             x + 35.0f, y + 23.0f, x + 33.0f, y + 23.0f);
  color_rect(r, (SDL_Color){220, 188, 102, 255},
             x + 38.0f, y + 13.0f, 2.0f, 2.0f);
}

static void draw_restroom_stall_closed(SDL_Renderer *r, float x, float y)
{
  draw_restroom_stall_frame(r, x, y);

  /* Full opaque panel with a deliberate floor gap, hinges and occupied latch. */
  color_rect(r, FX_INK, x - 2.0f, y, 36.0f, 27.0f);
  color_rect(r, (SDL_Color){47, 94, 89, 255},
             x + 1.0f, y + 3.0f, 30.0f, 21.0f);
  color_rect(r, (SDL_Color){73, 122, 113, 255},
             x + 3.0f, y + 5.0f, 26.0f, 2.0f);
  color_rect(r, (SDL_Color){34, 70, 69, 255},
             x + 3.0f, y + 19.0f, 26.0f, 2.0f);
  color_rect(r, (SDL_Color){145, 162, 149, 255},
             x - 2.0f, y + 5.0f, 3.0f, 5.0f);
  color_rect(r, (SDL_Color){145, 162, 149, 255},
             x - 2.0f, y + 17.0f, 3.0f, 5.0f);
  color_rect(r, (SDL_Color){220, 188, 102, 255},
             x + 25.0f, y + 13.0f, 3.0f, 3.0f);
  color_rect(r, (SDL_Color){183, 60, 53, 255},
             x + 22.0f, y + 9.0f, 7.0f, 2.0f);
}

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
static void draw_decoration(SDL_Renderer *r, const Decoration *decoration,
                            float cam_x, float oy, float world_t,
                            int level_index)
{
  float x = decoration->col * (float)TILE_SIZE - cam_x;
  float y = decoration->row * (float)TILE_SIZE + oy;
  switch (decoration->type)
  {
  case DECOR_FLIGHT_CASE:
    draw_flight_case(r, x, y, tile_hash(decoration->col, decoration->row));
    break;
  case DECOR_WALL_CLOCK:
    draw_wall_clock(r, x, y, level_index, world_t);
    break;
  case DECOR_OFFICE_CHAIR:
    draw_office_chair(r, x, y);
    break;
  case DECOR_OFFICE_DESK:
    draw_office_desk(r, x, y);
    break;
  case DECOR_OFFICE_EQUIPMENT:
    draw_office_equipment(r, x, y,
                          tile_hash(decoration->col, decoration->row) % 3u,
                          world_t);
    break;
  case DECOR_RESTROOM_TOILET:
    draw_restroom_toilet(r, x, y);
    break;
  case DECOR_RESTROOM_BASIN:
    draw_restroom_basin(r, x, y);
    break;
  case DECOR_RESTROOM_URINAL:
    draw_restroom_urinal(r, x, y);
    break;
  case DECOR_RESTROOM_PARTITION:
    draw_restroom_partition(r, x, y);
    break;
  case DECOR_RESTROOM_STALL_OPEN:
    draw_restroom_stall_open(r, x, y);
    break;
  case DECOR_RESTROOM_STALL_CLOSED:
    draw_restroom_stall_closed(r, x, y);
    break;
  case DECOR_LOBBY_COUNTER:
    draw_lobby_counter(r, x, y, tile_hash(decoration->col, decoration->row));
    break;
  case DECOR_LOBBY_SOFA:
    draw_lobby_sofa(r, x, y);
    break;
  case DECOR_LOBBY_PLANTER:
    draw_lobby_planter(r, x, y,
                       tile_hash(decoration->col, decoration->row), world_t);
    break;
  case DECOR_LOBBY_TURNSTILE:
    draw_lobby_turnstile(r, x, y, world_t);
    break;
  }
}

static void draw_card(SDL_Renderer *r, float x, float y, Uint8 alpha, bool active)
{
  SDL_Color glow = active ? (SDL_Color){71, 255, 225, 255}
                          : (SDL_Color){46, 181, 190, 255};
  SDL_BlendMode old = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(r, &old);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  set_rgba(r, glow.r, glow.g, glow.b, (Uint8)(alpha / 5));
  fill_rect(r, x - 4.0f, y - 4.0f, 22.0f, 26.0f);
  set_rgba(r, 8, 24, 30, alpha);
  fill_rect(r, x - 1.0f, y - 1.0f, 16.0f, 20.0f);
  set_rgba(r, glow.r, glow.g, glow.b, alpha);
  fill_rect(r, x, y, 14.0f, 18.0f);
  set_rgba(r, 10, 81, 92, alpha);
  fill_rect(r, x + 2.0f, y + 3.0f, 10.0f, 12.0f);
  set_rgba(r, 211, 255, 245, alpha);
  fill_rect(r, x + 3.0f, y + 4.0f, 5.0f, 4.0f);
  fill_rect(r, x + 3.0f, y + 11.0f, 8.0f, 2.0f);
  set_rgba(r, 255, 225, 90, alpha);
  fill_rect(r, x + 9.0f, y + 4.0f, 2.0f, 4.0f);
  SDL_SetRenderDrawBlendMode(r, old);
}

static void draw_gun_pickup(SDL_Renderer *r, float x, float y)
{
  draw_soft_glow(r, x + 2.0f, y + 1.0f, 15.0f, 13.0f, FX_AMBER);
  color_rect(r, COL_OUTLINE, x + 2.0f, y, 17.0f, 7.0f);
  color_rect(r, (SDL_Color){142, 157, 158, 255}, x + 3.0f, y + 1.0f, 15.0f, 4.0f);
  color_rect(r, (SDL_Color){218, 227, 214, 255}, x + 5.0f, y + 1.0f, 12.0f, 1.0f);
  color_rect(r, COL_OUTLINE, x + 6.0f, y + 5.0f, 7.0f, 10.0f);
  color_rect(r, (SDL_Color){91, 76, 57, 255}, x + 8.0f, y + 6.0f, 4.0f, 8.0f);
}

static void draw_bazooka_pickup(SDL_Renderer *r, float x, float y)
{
  draw_soft_glow(r, x + 3.0f, y + 4.0f, 21.0f, 13.0f,
                 (SDL_Color){112, 205, 93, 255});
  color_rect(r, COL_OUTLINE, x - 1.0f, y + 5.0f, 23.0f, 8.0f);
  color_rect(r, (SDL_Color){55, 77, 48, 255}, x, y + 6.0f, 20.0f, 6.0f);
  color_rect(r, (SDL_Color){100, 129, 72, 255}, x + 2.0f, y + 7.0f, 15.0f, 2.0f);
  color_rect(r, (SDL_Color){155, 167, 120, 255}, x + 19.0f, y + 4.0f, 4.0f, 10.0f);
  color_rect(r, COL_OUTLINE, x + 6.0f, y + 12.0f, 6.0f, 5.0f);
  color_rect(r, (SDL_Color){83, 68, 47, 255}, x + 8.0f, y + 12.0f, 3.0f, 4.0f);
}


static void draw_medkit(SDL_Renderer *r, float x, float y)
{
  draw_soft_glow(r, x + 2.0f, y + 2.0f, 16.0f, 16.0f, FX_RED);
  color_rect(r, COL_OUTLINE, x + 1.0f, y + 1.0f, 18.0f, 18.0f);
  color_rect(r, (SDL_Color){197, 48, 48, 255}, x + 2.0f, y + 2.0f, 16.0f, 16.0f);
  color_rect(r, (SDL_Color){244, 82, 65, 255}, x + 4.0f, y + 3.0f, 12.0f, 2.0f);
  color_rect(r, (SDL_Color){244, 239, 210, 255}, x + 8.0f, y + 5.0f, 4.0f, 10.0f);
  color_rect(r, (SDL_Color){244, 239, 210, 255}, x + 5.0f, y + 8.0f, 10.0f, 4.0f);
}

static void draw_spike_strip(SDL_Renderer *r, float x, float y)
{
  color_rect(r, (SDL_Color){25, 28, 31, 255}, x, y + 27.0f, TILE_SIZE, 5.0f);
  for (int k = 0; k < TILE_SIZE; k += 8)
  {
    color_rect(r, COL_OUTLINE, x + k, y + 19.0f, 8.0f, 10.0f);
    color_rect(r, (SDL_Color){134, 147, 147, 255}, x + k + 1.0f, y + 23.0f, 6.0f, 5.0f);
    color_rect(r, (SDL_Color){172, 187, 181, 255}, x + k + 2.0f, y + 19.0f, 4.0f, 9.0f);
    color_rect(r, (SDL_Color){222, 224, 204, 255}, x + k + 3.0f, y + 17.0f, 2.0f, 10.0f);
  }
}

static void draw_fan_segment(SDL_Renderer *r, float x1, float y1,
                             float x2, float y2, int thickness,
                             SDL_Color color)
{
  float dx = x2 - x1;
  float dy = y2 - y1;
  float length = sqrtf(dx * dx + dy * dy);
  float nx = length > 0.001f ? -dy / length : 0.0f;
  float ny = length > 0.001f ? dx / length : 0.0f;
  float half = (float)(thickness - 1) * 0.5f;

  set_color(r, color);
  for (int i = 0; i < thickness; ++i)
  {
    float offset = (float)i - half;
    SDL_RenderLine(r, x1 + nx * offset, y1 + ny * offset,
                   x2 + nx * offset, y2 + ny * offset);
  }
}

static void draw_ceiling_fan_blades(SDL_Renderer *r, float cx, float cy,
                                    float angle, bool front)
{
  for (int blade = 0; blade < 4; ++blade)
  {
    float a = angle + (float)blade * 1.57079633f;
    float projection = cosf(a);
    float depth = sinf(a);
    if ((depth >= 0.0f) != front)
      continue;

    /* A horizontal rotor seen edge-on: depth changes the apparent blade
       length and only nudges it vertically for a hint of perspective. */
    float inner_x = cx + projection * 4.0f;
    float inner_y = cy + depth * 0.4f;
    float outer_x = cx + projection * CEILING_FAN_BLADE_LENGTH;
    float outer_y = cy + depth * 2.0f;
    SDL_Color blade_color = front
                                ? (SDL_Color){177, 191, 187, 255}
                                : (SDL_Color){82, 94, 95, 255};
    draw_fan_segment(r, inner_x, inner_y, outer_x, outer_y, 7, COL_OUTLINE);
    draw_fan_segment(r, inner_x, inner_y, outer_x, outer_y, 3, blade_color);
  }
}

static void draw_ceiling_fan(SDL_Renderer *r, const CeilingFan *fan,
                             float cam_x, float oy, float world_t,
                             int index)
{
  float cx = fan->x - cam_x;
  float cy = fan->y + oy;
  float angle = world_t * 9.0f + (float)index * 0.83f;

  /* Side-view mount: ceiling plate, drop rod, and motor housing. The rod runs
   * from whatever the map says the fan hangs off, so a fan in a hall reads as
   * hung on a long rod rather than as one floating in the middle of the air. */
  float plate_y = fan->mount_y + oy;
  float rod_top = plate_y + 3.0f;
  float rod_height = cy - rod_top;
  if (rod_height < 2.0f)
    rod_height = 2.0f;
  color_rect(r, COL_OUTLINE, cx - 8.0f, plate_y, 16.0f, 5.0f);
  color_rect(r, (SDL_Color){107, 120, 121, 255},
             cx - 6.0f, plate_y + 1.0f, 12.0f, 3.0f);
  color_rect(r, COL_OUTLINE, cx - 3.0f, rod_top, 6.0f, rod_height);
  color_rect(r, (SDL_Color){139, 151, 148, 255},
             cx - 1.0f, rod_top + 1.0f, 2.0f, rod_height - 1.0f);

  draw_ceiling_fan_blades(r, cx, cy + 2.0f, angle, false);

  color_rect(r, COL_OUTLINE, cx - 7.0f, cy - 5.0f, 14.0f, 9.0f);
  color_rect(r, (SDL_Color){72, 84, 85, 255},
             cx - 5.0f, cy - 3.0f, 10.0f, 5.0f);
  color_rect(r, (SDL_Color){139, 151, 148, 255},
             cx - 4.0f, cy - 2.0f, 8.0f, 2.0f);

  draw_ceiling_fan_blades(r, cx, cy + 2.0f, angle, true);

  color_rect(r, COL_OUTLINE, cx - 3.0f, cy, 6.0f, 6.0f);
  color_rect(r, (SDL_Color){235, 86, 65, 255},
             cx - 1.0f, cy + 2.0f, 2.0f, 2.0f);
}

static void draw_crate(SDL_Renderer *r, const Crate *crate, float cam_x, float oy)
{
  float x = crate->x - cam_x;
  float y = crate->y + oy;
  color_rect(r, FX_INK, x - 1.0f, y - 1.0f, CRATE_W + 2.0f, CRATE_H + 2.0f);
  color_rect(r, (SDL_Color){105, 67, 38, 255}, x, y, CRATE_W, CRATE_H);
  color_rect(r, (SDL_Color){161, 103, 53, 255}, x + 2.0f, y + 2.0f, CRATE_W - 4.0f, 4.0f);
  color_rect(r, (SDL_Color){73, 48, 32, 255}, x + 2.0f, y + CRATE_H - 6.0f, CRATE_W - 4.0f, 4.0f);
  color_rect(r, (SDL_Color){143, 90, 47, 255}, x + 2.0f, y + 2.0f, 4.0f, CRATE_H - 4.0f);
  color_rect(r, (SDL_Color){70, 47, 33, 255}, x + CRATE_W - 6.0f, y + 2.0f, 4.0f, CRATE_H - 4.0f);
  set_rgba(r, 68, 44, 30, 255);
  SDL_RenderLine(r, x + 6.0f, y + 6.0f, x + CRATE_W - 7.0f, y + CRATE_H - 7.0f);
  SDL_RenderLine(r, x + CRATE_W - 7.0f, y + 6.0f, x + 6.0f, y + CRATE_H - 7.0f);
  set_rgba(r, 188, 125, 67, 255);
  SDL_RenderLine(r, x + 7.0f, y + 6.0f, x + CRATE_W - 7.0f, y + CRATE_H - 8.0f);
  color_rect(r, (SDL_Color){178, 171, 133, 255}, x + 10.0f, y + 9.0f, 10.0f, 10.0f);
  draw_text(r, x + 11.0f, y + 10.0f, 1.0f, 74, 62, 45, "X");
}

/* An armed mine blinks at 24Hz, which is the fastest thing on screen and the
 * one most worth holding still when asked. It is held *lit* rather than dark:
 * the light is what says the fuse is running, and MINE_TRIGGER_DELAY is under
 * half a second, so nothing is lost by not counting it out. */
static void draw_mine(SDL_Renderer *r, const Mine *mine, float cam_x, float oy,
                      bool steady)
{
  float x = mine->x - cam_x;
  float y = mine->y + oy;
  bool flash = mine->triggered &&
               (steady || ((int)(mine->timer * 24.0f) & 1) == 0);
  if (flash)
    draw_soft_glow(r, x + 5.0f, y, 6.0f, 5.0f, FX_RED);
  color_rect(r, FX_INK, x - 2.0f, y + 4.0f, MINE_W + 4.0f, 6.0f);
  color_rect(r, (SDL_Color){45, 51, 48, 255}, x, y + 2.0f, MINE_W, 7.0f);
  color_rect(r, (SDL_Color){91, 100, 89, 255}, x + 2.0f, y + 1.0f, MINE_W - 4.0f, 3.0f);
  color_rect(r, flash ? (SDL_Color){255, 230, 167, 255} : FX_RED,
             x + 6.0f, y + 1.0f, 4.0f, 3.0f);
  color_rect(r, (SDL_Color){157, 146, 90, 255}, x + 1.0f, y + 7.0f, 3.0f, 2.0f);
  color_rect(r, (SDL_Color){157, 146, 90, 255}, x + 12.0f, y + 7.0f, 3.0f, 2.0f);
}

static void draw_gas_canister(SDL_Renderer *r,
                              const GasCanister *canister,
                              float cam_x, float oy)
{
  float x = canister->x - cam_x;
  float y = canister->y + oy;

  /* A narrow pressure cylinder with stepped shoulders and a brass valve.
     The silhouette stays below a standing shot while remaining readable
     against the dark industrial background. */
  color_rect(r, (SDL_Color){3, 6, 9, 120},
             x - 2.0f, y + 14.0f, GAS_CANISTER_W + 4.0f, 3.0f);

  /* Valve, collar and short neck. */
  color_rect(r, FX_INK, x + 4.0f, y, 5.0f, 3.0f);
  color_rect(r, (SDL_Color){201, 174, 93, 255},
             x + 5.0f, y, 3.0f, 2.0f);
  color_rect(r, FX_INK, x + 3.0f, y + 2.0f, 6.0f, 4.0f);
  color_rect(r, (SDL_Color){118, 126, 116, 255},
             x + 4.0f, y + 3.0f, 4.0f, 3.0f);

  /* Rounded shoulders and long cylindrical body. */
  color_rect(r, FX_INK, x + 2.0f, y + 5.0f, 8.0f, 2.0f);
  color_rect(r, FX_INK, x + 1.0f, y + 6.0f, 10.0f, 9.0f);
  color_rect(r, FX_INK, x + 2.0f, y + 15.0f, 8.0f, 1.0f);
  color_rect(r, (SDL_Color){224, 70, 53, 255},
             x + 3.0f, y + 6.0f, 6.0f, 1.0f);
  color_rect(r, (SDL_Color){92, 103, 103, 255},
             x + 2.0f, y + 7.0f, 8.0f, 7.0f);
  color_rect(r, (SDL_Color){51, 61, 63, 255},
             x + 2.0f, y + 14.0f, 8.0f, 1.0f);
  color_rect(r, (SDL_Color){153, 163, 158, 255},
             x + 3.0f, y + 9.0f, 2.0f, 5.0f);

  /* A red shoulder identifies the flammable contents without making the
     whole cylinder resemble a fire extinguisher. */
  color_rect(r, (SDL_Color){174, 48, 41, 255},
             x + 2.0f, y + 7.0f, 8.0f, 2.0f);
  color_rect(r, (SDL_Color){237, 80, 58, 255},
             x + 3.0f, y + 7.0f, 2.0f, 2.0f);

  /* Small warning label with a dark gas-flame mark. */
  color_rect(r, (SDL_Color){239, 188, 48, 255},
             x + 5.0f, y + 10.0f, 4.0f, 3.0f);
  color_rect(r, (SDL_Color){70, 39, 27, 255},
             x + 6.0f, y + 10.0f, 2.0f, 2.0f);
  color_rect(r, (SDL_Color){255, 225, 103, 255},
             x + 5.0f, y + 10.0f, 1.0f, 2.0f);
}


static void draw_rocket_sprite(SDL_Renderer *r, float x, float y, int dir,
                               bool flame)
{
  sprite_rect(r, x, y, ROCKET_W, dir, 0.0f, 1.0f,
              12.0f, 5.0f, COL_OUTLINE);
  sprite_rect(r, x, y, ROCKET_W, dir, 2.0f, 2.0f,
              10.0f, 3.0f, (SDL_Color){112, 132, 91, 255});
  sprite_rect(r, x, y, ROCKET_W, dir, 12.0f, 2.0f,
              4.0f, 3.0f, (SDL_Color){213, 187, 111, 255});
  sprite_rect(r, x, y, ROCKET_W, dir, 1.0f, 0.0f,
              4.0f, 2.0f, (SDL_Color){73, 91, 65, 255});
  sprite_rect(r, x, y, ROCKET_W, dir, 1.0f, 5.0f,
              4.0f, 1.0f, (SDL_Color){73, 91, 65, 255});
  if (flame)
  {
    /* The exhaust lights the air it burns in; fire that illuminates nothing
       reads as a decal stuck to the tail. */
    fx_glow(r, sprite_point_x(x, ROCKET_W, dir, -2.5f), y + 3.5f, 12.0f,
            FX_AMBER, 90);
    sprite_rect(r, x, y, ROCKET_W, dir, -5.0f, 1.0f,
                5.0f, 5.0f, FX_FLAME);
    sprite_rect(r, x, y, ROCKET_W, dir, -3.0f, 2.0f,
                4.0f, 3.0f, FX_FLAME_HOT);
  }
}

/* Rotate the horizontal rocket's local rectangles into screen space. The
   collision box rotates with it, so a vertical rocket is ROCKET_H pixels wide
   and ROCKET_W pixels tall. */
static void vertical_rocket_rect(SDL_Renderer *r, float bx, float by, int dir,
                                 float lx, float ly, float w, float h,
                                 SDL_Color color)
{
  float y = dir > 0 ? by + lx : by + ROCKET_W - lx - w;
  color_rect(r, color, bx + ly, y, h, w);
}

static void draw_vertical_rocket_sprite(SDL_Renderer *r, float x, float y,
                                        int dir, bool flame)
{
  vertical_rocket_rect(r, x, y, dir, 0.0f, 1.0f,
                       12.0f, 5.0f, COL_OUTLINE);
  vertical_rocket_rect(r, x, y, dir, 2.0f, 2.0f,
                       10.0f, 3.0f, (SDL_Color){112, 132, 91, 255});
  vertical_rocket_rect(r, x, y, dir, 12.0f, 2.0f,
                       4.0f, 3.0f, (SDL_Color){213, 187, 111, 255});
  vertical_rocket_rect(r, x, y, dir, 1.0f, 0.0f,
                       4.0f, 2.0f, (SDL_Color){73, 91, 65, 255});
  vertical_rocket_rect(r, x, y, dir, 1.0f, 5.0f,
                       4.0f, 1.0f, (SDL_Color){73, 91, 65, 255});
  if (flame)
  {
    float tail_y = dir > 0 ? y - 2.5f : y + ROCKET_W + 2.5f;
    fx_glow(r, x + 3.5f, tail_y, 12.0f, FX_AMBER, 90);
    vertical_rocket_rect(r, x, y, dir, -5.0f, 1.0f,
                         5.0f, 5.0f, FX_FLAME);
    vertical_rocket_rect(r, x, y, dir, -3.0f, 2.0f,
                         4.0f, 3.0f, FX_FLAME_HOT);
  }
}




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
 * warning beat and thicken once the gust lands, and as a bracket around Chuck
 * whenever a ledge upwind of him is actually taking the push.
 */
static void render_facade_wind(Game *game, int win_w, int win_h,
                               float world_t)
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

  for (int i = 0; i < 28; ++i)
  {
    unsigned h = tile_hash(i * 37 + 11, 909);
    float speed = 300.0f + (float)(h % 260u);
    float y = HUD_HEIGHT + fmodf((float)((h >> 7) % 4096u), field);
    float x = fmodf((float)(h % 617u) + world_t * speed * dir, span);
    if (x < 0.0f)
      x += span;
    x -= 80.0f;
    float length = 16.0f + (float)(h % 44u) * (0.5f + intensity * 0.5f);
    Uint8 alpha = (Uint8)(intensity * (gusting ? 96.0f : 46.0f));
    fx_rect_a(r, (SDL_Color){208, 228, 238, 255}, alpha, x, y, length, 1.0f);
  }

  /* A wash on the windward edge shows which way the wall is pushing. */
  Uint8 wash = (Uint8)(intensity * (gusting ? 46.0f : 20.0f));
  float wash_w = 70.0f;
  fx_rect_a(r, (SDL_Color){150, 190, 214, 255}, wash,
            dir > 0.0f ? 0.0f : (float)win_w - wash_w, HUD_HEIGHT,
            wash_w, field);

  if (gusting && state->facade_wind_sheltered && !state->player.dying)
  {
    float px = state->player.x - game->presentation.cam_x +
               game->presentation.camera_shake_x;
    float py = state->player.y + HUD_HEIGHT - game->presentation.cam_y +
               game->presentation.camera_shake_y;
    SDL_Color safe = FX_GREEN;
    float pulse = 0.6f + 0.4f * sinf(world_t * 9.0f);
    fx_rect_a(r, safe, (Uint8)(150.0f * pulse), px - 4.0f, py - 4.0f,
              8.0f, 2.0f);
    fx_rect_a(r, safe, (Uint8)(150.0f * pulse), px + PLAYER_W - 4.0f,
              py - 4.0f, 8.0f, 2.0f);
    fx_rect_a(r, safe, (Uint8)(150.0f * pulse), px - 4.0f,
              py + PLAYER_H + 2.0f, 8.0f, 2.0f);
    fx_rect_a(r, safe, (Uint8)(150.0f * pulse), px + PLAYER_W - 4.0f,
              py + PLAYER_H + 2.0f, 8.0f, 2.0f);
  }
}

static void render_facade_world(Game *game, int win_w, int win_h)
{
  SDL_Renderer *r = game->platform.renderer;
  const Level *level = &game->gameplay.level;
  float cam_x = game->presentation.cam_x -
                game->presentation.camera_shake_x;
  float oy = HUD_HEIGHT - game->presentation.cam_y +
             game->presentation.camera_shake_y;
  float world_t = (float)SDL_GetTicksNS() * 1.0e-9f;

  render_background(game, win_w, win_h);
  if (!level->reveal.done)
    return;

  /* Closed windows and gameplay windows share the same architectural grid.
   * A special fixture replaces its ordinary window instead of covering it. */
  for (int row = 3; row < level->map.height; row += 3)
  {
    float y = row * (float)TILE_SIZE + oy;
    if (y < HUD_HEIGHT - 56.0f || y > (float)win_h + 8.0f)
      continue;
    for (int col = 4; col < level->map.width - 4; col += 4)
    {
      if (facade_fixture_at(level, col, row))
        continue;
      float x = col * (float)TILE_SIZE - cam_x;
      draw_facade_closed_window(r, x, y, tile_hash(col, row), world_t);
    }
  }

  float start_center = level->map.start_x + PLAYER_W * 0.5f;
  int start_col = (int)floorf(start_center / TILE_SIZE);
  int start_row = (int)floorf((level->map.start_y + PLAYER_H * 0.5f) /
                              TILE_SIZE);
  draw_facade_open_window(r, start_col * (float)TILE_SIZE - cam_x,
                          start_row * (float)TILE_SIZE + oy, false);
  if (level->map.has_window)
  {
    draw_facade_open_window(
        r, level->map.window_col * (float)TILE_SIZE - cam_x,
        level->map.window_row * (float)TILE_SIZE + oy, true);
  }

  for (int i = 0; i < level->map.facade_hazard_spawn_count; ++i)
  {
    float windup = game->gameplay.facade_hazard_windup_timers[i] /
                   THROWN_OBJECT_WINDUP;
    draw_facade_hazard_source(r, &level->map.facade_hazard_spawns[i],
                              cam_x, oy, world_t, windup);
  }

  /* Ledges and plant sit in front of the windows: they are the geometry the
   * climb is actually routed around. */
  int first_row = (int)floorf(game->presentation.cam_y / TILE_SIZE) - 1;
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
      draw_facade_ledge(r, level, col, row,
                        col * (float)TILE_SIZE - cam_x,
                        row * (float)TILE_SIZE + oy, world_t);
    }
  }

  /* Pickups left out on the wall for climbers willing to detour. */
  for (int i = 0; i < level->runtime.item_count; ++i)
  {
    const Item *item = &level->runtime.items[i];
    if (item->collected)
      continue;
    float bob = sinf(world_t * 3.14159265f + (float)i * 0.7f) * 3.0f;
    float x = item->x - 7.0f - cam_x;
    float y = item->y - 9.0f + oy + bob;
    fx_glow(r, x + 8.0f, y + 8.0f, 20.0f, (SDL_Color){140, 210, 224, 255}, 40);
    if (item->type == ITEM_GUN)
      draw_gun_pickup(r, x, y + 3.0f);
    else if (item->type == ITEM_GRENADE)
      draw_grenade(r, x + 3.0f, y + 4.0f, 0.0f);
    else if (item->type == ITEM_MEDKIT)
      draw_medkit(r, x, y);
    else if (item->type == ITEM_BAZOOKA)
      draw_bazooka_pickup(r, x - 4.0f, y + 1.0f);
    else
      draw_card(r, x, y, 255, true);
  }

  for (int i = 0; i < MAX_THROWN_OBJECTS; ++i)
    if (game->gameplay.thrown_objects[i].active)
      draw_thrown_object(r, &game->gameplay.thrown_objects[i], cam_x, oy);
  for (int i = 0; i < MAX_BIRDS; ++i)
    if (game->gameplay.birds[i].active)
      draw_bird(r, &game->gameplay.birds[i], cam_x, oy);

  particle_system_render(&game->presentation.particles, r, oy, cam_x);
  if (!game->gameplay.player.dying)
  {
    bool blink = game->gameplay.invuln_timer > 0.0f &&
                 ((int)(game->gameplay.invuln_timer * 12.0f) % 2 == 0);
    if (!blink)
      draw_player(r, &game->gameplay.player, &game->gameplay.level,
                  cam_x, oy, false, 0.0f,
                  game->presentation.player_land_squash);
  }

  render_facade_wind(game, win_w, win_h, world_t);
}

static void render_alarm_lighting(SDL_Renderer *r, int win_w, int win_h,
                                  float world_t)
{
  float wave = 0.5f + 0.5f * sinf(world_t * 7.0f);
  Uint8 wash_alpha = (Uint8)(10.0f + wave * 17.0f);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  set_rgba(r, 210, 20, 16, wash_alpha);
  fill_rect(r, 0.0f, HUD_HEIGHT, (float)win_w,
            (float)win_h - HUD_HEIGHT);
  set_rgba(r, 255, 47, 35, (Uint8)(105.0f + wave * 95.0f));
  fill_rect(r, 0.0f, HUD_HEIGHT, (float)win_w, 3.0f);
  fill_rect(r, 0.0f, (float)win_h - 3.0f, (float)win_w, 3.0f);

  /* Alternating edge blocks suggest rotating emergency beacons without
   * covering the action in the middle of the viewport. */
  for (int x = -12; x < win_w + 24; x += 48)
  {
    float offset = wave > 0.5f ? 0.0f : 24.0f;
    set_rgba(r, 255, 92, 54, 125);
    fill_rect(r, (float)x + offset, HUD_HEIGHT + 4.0f, 18.0f, 2.0f);
    fill_rect(r, (float)x + 24.0f - offset,
              (float)win_h - 7.0f, 18.0f, 2.0f);
  }
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void render_world(Game *game)
{
  SDL_Renderer *r = game->platform.renderer;
  const Level *lvl = &game->gameplay.level;
  const float oy = HUD_HEIGHT - game->presentation.cam_y +
                   game->presentation.camera_shake_y;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);
  if (lvl->map.mode == LEVEL_MODE_FACADE)
  {
    render_facade_world(game, win_w, win_h);
    return;
  }
  const float cam_x = game->presentation.cam_x - game->presentation.camera_shake_x;
  float world_t = (float)SDL_GetTicksNS() * 1.0e-9f;

  render_background(game, win_w, win_h);

  /* Structural tile layer. */
  int ceiling_row = lvl->map.theme == LEVEL_THEME_RESTROOM ? restroom_ceiling_row(lvl) : 0;
  for (int row = 0; row < lvl->map.height; ++row)
  {
    for (int col = 0; col < lvl->map.width; ++col)
    {
      float x = col * (float)TILE_SIZE - cam_x;
      float y = row * (float)TILE_SIZE + oy;
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
      }
      else if (tile == TILE_WALL)
      {
        if (lvl->map.theme == LEVEL_THEME_RESTROOM)
          draw_restroom_wall_tile(r, lvl, col, row, x, y, ceiling_row);
        else
          level_art_wall_tile(r, lvl, col, row, x, y);
      }
      else if (tile == TILE_LADDER)
        draw_ladder_tile(r, x, y, row);
      else if (tile == TILE_ELEVATOR_SHAFT)
        draw_shaft_tile(r, x, y, col, row);
    }
  }

  if (!lvl->reveal.done)
    return;

  /* Lighting pass derived from the tile grid: soft contact shadows under
   * every overhead surface, plus recessed warm fixtures on long ceilings.
   * Because it reads the level itself, light always matches architecture. */
  if (lvl->map.mode != LEVEL_MODE_FACADE)
  {
    /* Fixture colour and how densely a ceiling is lit both come from the
     * theme: the clean rooms are lit end to end, the plenum barely at all. */
    const LevelThemeArt *art = level_art(lvl->map.theme);
    unsigned fixture_spacing =
        art->lamp_alpha >= 90u ? 4u : (art->lamp_alpha >= 60u ? 7u : 13u);
    /* An alarm the player can only read off a HUD strip is an alarm happening
     * to the interface. This is a building with its own wiring: while the
     * alarm is up every fixture in the sector swings between its own colour
     * and the emergency circuit, so the corridor Chuck is standing in tells
     * him before the guards arriving in it do. It is the fixtures and their
     * pools only — repainting the ambient bounce as well would flood the
     * whole frame red and take the level's material with it. */
    float alarm_wash = gameplay_alarm_active(&game->gameplay)
                           ? 0.30f + 0.45f * (0.5f + 0.5f * sinf(world_t * 5.4f))
                           : 0.0f;
    int first_col = (int)(cam_x / (float)TILE_SIZE) - 1;
    if (first_col < 0)
      first_col = 0;
    int last_col = first_col + win_w / TILE_SIZE + 3;
    if (last_col > lvl->map.width)
      last_col = lvl->map.width;
    for (int row = 0; row < lvl->map.height; ++row)
    {
      for (int col = first_col; col < last_col; ++col)
      {
        /* A hole blown through a wall is air, and air beside a wall is lit:
         * without this the opening would be the one unlit place in the sector,
         * which is exactly the shape of a missing tile. */
        TileType lit_tile = lvl->map.tiles[row][col];
        if (lit_tile != TILE_EMPTY && lit_tile != TILE_LADDER &&
            !(lit_tile == TILE_WEAK_WALL && level_wall_broken(lvl, col, row)))
          continue;
        float x = col * (float)TILE_SIZE - cam_x;
        float y = row * (float)TILE_SIZE + oy;

        /* Ambient occlusion against every face the air touches, not just the
         * ceiling. A room whose walls meet its floor along a hard line is a
         * diagram of a room; the gathering dark in the corners is what says
         * the two surfaces are at different distances. The gradients simply
         * overlap where two faces meet, so concave corners come out darker
         * than either wall for free. */
        bool above = level_is_solid(lvl, col, row - 1);
        if (above)
          fx_vgrad(r, x, y, TILE_SIZE, 13.0f, FX_INK, 92, FX_INK, 0);
        if (level_is_solid(lvl, col, row + 1))
        {
          /* A floor gives light back. Two lines do it: a thin hard contact
           * shadow exactly at the junction, so the floor and the air are not
           * the same surface, and a soft bounce fading upward off it, so the
           * room is lit from below as well as above. The bounce is scaled by
           * how brightly the sector is lit — the plenum has nothing to bounce
           * and must not glow. */
          fx_vgrad(r, x, y + TILE_SIZE - 17.0f, TILE_SIZE, 15.0f,
                   art->trim_hi, 0, art->trim_hi,
                   (Uint8)((float)art->lamp_alpha * 0.3f));
          fx_vgrad(r, x, y + TILE_SIZE - 4.0f, TILE_SIZE, 4.0f,
                   FX_INK, 0, FX_INK, 66);
        }
        if (level_is_solid(lvl, col - 1, row))
          fx_hgrad(r, x, y, 11.0f, TILE_SIZE, FX_INK, 58, FX_INK, 0);
        if (level_is_solid(lvl, col + 1, row))
          fx_hgrad(r, x + TILE_SIZE - 11.0f, y, 11.0f, TILE_SIZE,
                   FX_INK, 0, FX_INK, 58);
        if (!above)
          continue;

        unsigned h = tile_hash(col, row);
        bool long_ceiling = level_is_solid(lvl, col - 1, row - 1) &&
                            level_is_solid(lvl, col + 1, row - 1);
        if ((h % fixture_spacing) == 0u && long_ceiling &&
            lvl->map.tiles[row][col] == TILE_EMPTY)
        {
          float cx = x + TILE_SIZE * 0.5f;
          float flicker = ((h >> 5) % 9u) == 0u &&
                                  fmodf(world_t * 1.9f + (float)col, 5.0f) < 0.08f
                              ? 0.35f
                              : 1.0f;
          SDL_Color fitting = alarm_wash > 0.0f
                                  ? fx_mix(art->lamp, FX_RED, alarm_wash)
                                  : art->lamp;
          SDL_Color lit = fx_dim(fitting, flicker);
          color_rect(r, (SDL_Color){15, 20, 30, 255}, cx - 7.0f, y - 1.0f, 14.0f, 4.0f);
          color_rect(r, lit, cx - 5.0f, y + 1.0f, 10.0f, 2.0f);
          fx_glow(r, cx, y + 3.0f, 12.0f, lit,
                  (Uint8)((float)art->lamp_alpha * flicker));
          fx_light_cone(r, cx, y + 2.0f, 7.0f, 30.0f, 86.0f, fitting,
                        (Uint8)((float)art->lamp_alpha * 0.42f * flicker));

          /* The pool the fixture puts on the floor under it. A cone that fades
           * out in mid-air is a beam with nothing at the end of it; light has
           * to land on something, and the floor it lands on is the surface the
           * player is reading the level off. */
          int floor_row = row;
          while (floor_row < lvl->map.height &&
                 floor_row < row + 5 &&
                 !level_is_solid(lvl, col, floor_row + 1))
            ++floor_row;
          if (level_is_solid(lvl, col, floor_row + 1))
          {
            float fy = (float)(floor_row + 1) * (float)TILE_SIZE + oy;
            float fade = 1.0f - (float)(floor_row - row) * 0.16f;
            Uint8 pool =
                (Uint8)((float)art->lamp_alpha * 0.36f * flicker * fade);
            for (int lobe = -1; lobe <= 1; ++lobe)
              fx_glow(r, cx + (float)lobe * 19.0f, fy + 2.0f, 30.0f,
                      fitting, pool);
          }
        }
      }
    }
  }

  /* A soft cool pool of light keeps the hero readable in dark rooms. */
  if (!game->gameplay.player.dying)
    fx_glow(r, game->gameplay.player.x + PLAYER_W * 0.5f - cam_x,
            game->gameplay.player.y + PLAYER_H * 0.5f + oy,
            120.0f, (SDL_Color){134, 196, 214, 255}, 26);

  for (int i = 0; i < lvl->runtime.elevator_count; ++i)
  {
    const Elevator *el = &lvl->runtime.elevators[i];
    draw_platform(r, el->col * (float)TILE_SIZE - cam_x, el->y + oy,
                  (SDL_Color){78, 218, 208, 255}, false);
  }
  for (int i = 0; i < lvl->runtime.fall_platform_count; ++i)
  {
    const FallPlatform *fp = &lvl->runtime.fall_platforms[i];
    if (!fp->removed)
      draw_platform(r, fp->col * (float)TILE_SIZE - cam_x, fp->y + oy,
                    fp->triggered ? FX_RED : FX_AMBER, true);
  }
  for (int i = 0; i < lvl->runtime.moving_platform_count; ++i)
  {
    const MovingPlatform *mp = &lvl->runtime.moving_platforms[i];
    draw_platform(r, mp->x - cam_x, mp->row * (float)TILE_SIZE + oy,
                  (SDL_Color){84, 187, 216, 255}, false);
  }

  /* Furniture stays behind every interactive object and actor. */
  for (int i = 0; i < lvl->map.decoration_count; ++i)
  {
    const Decoration *decoration = &lvl->map.decorations[i];
    float x = decoration->col * (float)TILE_SIZE - cam_x;
    if (x + TILE_SIZE < 0.0f || x > (float)win_w)
      continue;
    draw_decoration(r, decoration, cam_x, oy, world_t,
                    game->campaign.current_level);
  }

  /* Doors and other wall-mounted fixtures belong behind anyone walking along
   * the corridor floor. */
  for (int d = 0; d < lvl->map.door_count; ++d)
  {
    float x = lvl->map.doors[d].col * (float)TILE_SIZE - cam_x;
    float y = lvl->map.doors[d].row * (float)TILE_SIZE + oy;
    draw_door(r, x, y, d);
  }
  if (lvl->map.has_sublevel_entrance)
  {
    float x = lvl->map.sublevel_entrance_col * (float)TILE_SIZE - cam_x;
    float y = lvl->map.sublevel_entrance_row * (float)TILE_SIZE + oy;
    draw_restroom_door(r, x, y);
  }
  if (lvl->map.has_sublevel_return)
  {
    float x = lvl->map.sublevel_return_col * (float)TILE_SIZE - cam_x;
    float y = lvl->map.sublevel_return_row * (float)TILE_SIZE + oy;
    draw_restroom_door(r, x, y);
  }
  if (lvl->map.has_exit)
  {
    float x = lvl->map.exit_col * (float)TILE_SIZE - cam_x;
    float y = lvl->map.exit_row * (float)TILE_SIZE + oy;
    draw_exit(r, game, x, y);
  }
  if (lvl->map.has_window)
  {
    float x = lvl->map.window_col * (float)TILE_SIZE - cam_x;
    float y = lvl->map.window_row * (float)TILE_SIZE + oy;
    draw_open_window(r, x, y);
  }
  for (int i = 0; i < lvl->map.terminal_count; ++i)
  {
    float x = lvl->map.terminals[i].col * (float)TILE_SIZE - cam_x;
    float y = lvl->map.terminals[i].row * (float)TILE_SIZE + oy;
    bool active = i == lvl->runtime.active_terminal_index;
    draw_terminal(r, x, y, active,
                  active && lvl->runtime.terminal_hacked,
                  lvl->map.has_window, world_t);
  }
  for (int i = 0; i < lvl->map.alarm_switch_count; ++i)
  {
    float x = lvl->map.alarm_switches[i].col * (float)TILE_SIZE - cam_x;
    float y = lvl->map.alarm_switches[i].row * (float)TILE_SIZE + oy;
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
                      game->gameplay.active_alarm_switch == i,
                      being_used, world_t, game->settings.reduced_motion);
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
      float x = col * (float)TILE_SIZE - cam_x;
      if (x + TILE_SIZE < 0.0f || x > (float)win_w)
        continue;
      float y = row * (float)TILE_SIZE + oy;
      draw_ladder_tile(r, x, y, row);
    }
  }

  /* Pickups bob independently and cast restrained color-coded glows. */
  int card_pos = 0;
  for (int i = 0; i < lvl->runtime.item_count; ++i)
  {
    const Item *it = &lvl->runtime.items[i];
    if (it->collected)
      continue;
    float bob = sinf(world_t * 3.14159265f + (float)i * 0.7f) * 3.0f;
    float x = it->x - 7.0f - cam_x;
    float y = it->y - 9.0f + oy + bob;
    if (it->type == ITEM_CARD)
    {
      Uint8 alpha = 255;
      bool active = true;
      if (game->state == STATE_SHOW_KEYCARD)
      {
        active = card_pos == game->presentation.card_anim_current;
        alpha = active ? 255 : 80;
      }
      draw_card(r, x, y, alpha, active);
      card_pos++;
    }
    else if (it->type == ITEM_GUN)
      draw_gun_pickup(r, x, y + 3.0f);
    else if (it->type == ITEM_GRENADE)
      draw_grenade(r, x + 3.0f, y + 4.0f, 0.0f);
    else if (it->type == ITEM_MEDKIT)
      draw_medkit(r, x, y);
    else if (it->type == ITEM_BAZOOKA)
      draw_bazooka_pickup(r, x - 4.0f, y + 1.0f);
  }

  /* Dropped magazines: the HUD's cartridge pictogram laid on the floor, so
   * the pickup is read as ammunition before it is walked over. */
  for (int i = 0; i < MAX_AMMO_DROPS; ++i)
  {
    const AmmoDrop *drop = &game->gameplay.ammo_drops[i];
    if (!drop->active)
      continue;
    float x = drop->x - cam_x;
    float y = drop->y + oy;
    if (x + AMMO_DROP_W < 0.0f || x > (float)win_w)
      continue;
    /* The dropped magazine is the same brass the HUD's pips count. */
    color_rect(r, FX_STEEL,
               x, y + AMMO_DROP_H - 2.0f, (float)AMMO_DROP_W, 2.0f);
    color_rect(r, fx_dim(FX_AMBER, 0.90f),
               x + 1.0f, y + 1.0f, (float)AMMO_DROP_W - 2.0f,
               (float)AMMO_DROP_H - 3.0f);
    color_rect(r, fx_ramp(FX_AMBER).lit,
               x + 1.0f, y, (float)AMMO_DROP_W - 2.0f, 1.0f);
    color_rect(r, FX_AMBER_DK,
               x + AMMO_DROP_W - 3.0f, y + 1.0f, 2.0f,
               (float)AMMO_DROP_H - 3.0f);
  }

  for (int i = 0; i < lvl->map.spike_count; ++i)
    draw_spike_strip(r, lvl->map.spike_spawns[i].x - cam_x,
                     lvl->map.spike_spawns[i].y + oy);

  for (int i = 0; i < lvl->map.ceiling_fan_count; ++i)
    draw_ceiling_fan(r, &lvl->map.ceiling_fans[i], cam_x, oy, world_t, i);

  for (int i = 0; i < lvl->runtime.crate_count; ++i)
    if (lvl->runtime.crates[i].active)
      draw_crate(r, &lvl->runtime.crates[i], cam_x, oy);

  for (int i = 0; i < lvl->runtime.gas_canister_count; ++i)
    if (lvl->runtime.gas_canisters[i].active)
      draw_gas_canister(r, &lvl->runtime.gas_canisters[i], cam_x, oy);

  for (int i = 0; i < game->gameplay.mine_count; ++i)
    if (game->gameplay.mines[i].active)
      draw_mine(r, &game->gameplay.mines[i], cam_x, oy,
                game->settings.reduced_motion);

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

  /* Fast projectiles get a one-pixel trail and hot core for impact/readability. */
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  for (int i = 0; i < MAX_ROCKETS; ++i)
  {
    const Rocket *rocket = &game->gameplay.rockets[i];
    if (!rocket->active)
      continue;
    float x = rocket->x - cam_x;
    float y = rocket->y + oy;
    bool vertical = fabsf(rocket->vy) > fabsf(rocket->vx);
    float width = vertical ? (float)ROCKET_H : (float)ROCKET_W;
    float height = vertical ? (float)ROCKET_W : (float)ROCKET_H;
    int dir = vertical ? (rocket->vy >= 0.0f ? 1 : -1)
                       : (rocket->vx >= 0.0f ? 1 : -1);
    fx_glow(r, x + width * 0.5f, y + height * 0.5f, 18.0f,
            (SDL_Color){255, 132, 45, 255}, 125);
    set_rgba(r, 122, 132, 124, 70);
    if (vertical)
    {
      fill_rect(r, x + 1.0f,
                y - (dir > 0 ? 15.0f : -15.0f), 4.0f, 16.0f);
      draw_vertical_rocket_sprite(r, x, y, dir, true);
    }
    else
    {
      fill_rect(r, x - (dir > 0 ? 15.0f : -15.0f), y + 1.0f,
                16.0f, 4.0f);
      draw_rocket_sprite(r, x, y, dir, true);
    }
  }
  for (int i = 0; i < MAX_BULLETS; ++i)
  {
    const Bullet *b = &game->gameplay.bullets[i];
    if (!b->active)
      continue;
    float x = b->x - cam_x;
    float y = b->y + oy;
    bool vertical = fabsf(b->vy) > fabsf(b->vx);
    float bullet_w = vertical ? (float)BULLET_H : (float)BULLET_W;
    float bullet_h = vertical ? (float)BULLET_W : (float)BULLET_H;
    fx_glow(r, x + bullet_w * 0.5f, y + bullet_h * 0.5f, 11.0f,
            (SDL_Color){255, 200, 90, 255}, 110);
    set_rgba(r, 255, 183, 38, 75);
    if (vertical)
      fill_rect(r, x + 1.0f,
                y - (b->vy > 0.0f ? 8.0f : -8.0f), 2.0f, 12.0f);
    else
      fill_rect(r, x - (b->vx > 0.0f ? 8.0f : -8.0f),
                y + 1.0f, 12.0f, 2.0f);
    color_rect(r, (SDL_Color){255, 243, 170, 255},
               x, y, bullet_w, bullet_h);
  }
  for (int i = 0; i < MAX_ENEMY_BULLETS; ++i)
  {
    const Bullet *b = &game->gameplay.enemy_bullets[i];
    if (!b->active)
      continue;
    float x = b->x - cam_x;
    float y = b->y + oy;
    bool vertical = fabsf(b->vy) > fabsf(b->vx);
    float bullet_w = vertical ? (float)BULLET_H : (float)BULLET_W;
    float bullet_h = vertical ? (float)BULLET_W : (float)BULLET_H;
    fx_glow(r, x + bullet_w * 0.5f, y + bullet_h * 0.5f, 11.0f,
            (SDL_Color){255, 92, 62, 255}, 110);
    set_rgba(r, 255, 52, 39, 80);
    if (vertical)
      fill_rect(r, x, y - (b->vy > 0.0f ? 7.0f : -7.0f), 4.0f, 11.0f);
    else
      fill_rect(r, x - (b->vx > 0.0f ? 7.0f : -7.0f), y, 11.0f, 4.0f);
    color_rect(r, (SDL_Color){255, 103, 54, 255},
               x, y, bullet_w, bullet_h);
    if (vertical)
      color_rect(r, (SDL_Color){255, 225, 128, 255},
                 x + 1.0f, y + 2.0f, 2.0f, 4.0f);
    else
      color_rect(r, (SDL_Color){255, 225, 128, 255},
                 x + 2.0f, y + 1.0f, 4.0f, 2.0f);
  }
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

  particle_system_render(&game->presentation.particles, r, oy, cam_x);

  if (!game->gameplay.player.dying)
  {
    bool blink = game->gameplay.invuln_timer > 0.0f &&
                 ((int)(game->gameplay.invuln_timer * 12.0f) % 2 == 0);
    if (!blink)
      draw_player(r, &game->gameplay.player, &game->gameplay.level,
                  cam_x, oy,
                  game->gameplay.terminal_hacking,
                  game->gameplay.terminal_hack_progress,
                  game->presentation.player_land_squash);
  }
  else
  {
    /* The body is drawn where the spray is, so it has to be a spark from the
       hit and not whichever slot happens to be first — a footfall's dust would
       otherwise take slot zero and carry the body off with it. */
    const Particle *spark = NULL;
    for (int i = 0; i < PS_MAX_PARTICLES && spark == NULL; ++i)
    {
      const Particle *candidate = &game->presentation.particles.particles[i];
      if (candidate->active && candidate->kind == PARTICLE_SPARK)
        spark = candidate;
    }
    if (spark != NULL && spark->life > 0.0f)
    {
      color_rect(r, (SDL_Color){68, 17, 19, 255},
                 spark->x - cam_x - 7.0f, spark->y + oy - 3.0f, 14.0f, 5.0f);
      color_rect(r, FX_RED, spark->x - cam_x - 4.0f, spark->y + oy - 4.0f, 8.0f, 2.0f);
    }
  }
  if (gameplay_alarm_active(&game->gameplay))
    render_alarm_lighting(r, win_w, win_h, world_t);
}

static void draw_hud_separator(SDL_Renderer *r, float x)
{
  color_rect(r, FX_INK, x, 8.0f, 1.0f, 24.0f);
  color_rect(r, fx_mix(FX_STEEL_DK, FX_STEEL_LT, 0.25f),
             x + 1.0f, 8.0f, 1.0f, 24.0f);
}

/* The console counts the same heart the manual teaches and the outro hands
   over — one glyph, one red, drawn in fx.h. */
static void draw_hud_heart(SDL_Renderer *r, float x, float y, bool filled)
{
  fx_heart(r, x, y, 1.5f, filled);
}

static const char *player_weapon_label(PlayerWeapon weapon)
{
  switch (weapon)
  {
  case PLAYER_WEAPON_PISTOL:
    return "PISTOL";
  case PLAYER_WEAPON_KNIFE:
    return "KNIFE";
  case PLAYER_WEAPON_GRENADE:
    return "GRENADE";
  case PLAYER_WEAPON_BAZOOKA:
    return "BAZOOKA";
  case PLAYER_WEAPON_COUNT:
    return "NONE";
  }
  return "NONE";
}

static void render_facade_hud(Game *game, int win_w)
{
  SDL_Renderer *r = game->platform.renderer;
  fx_vgrad(r, 0.0f, 0.0f, (float)win_w, 37.0f,
           (SDL_Color){31, 39, 52, 255}, 255,
           (SDL_Color){11, 17, 28, 255}, 255);
  color_rect(r, (SDL_Color){181, 132, 56, 255},
             0.0f, 38.0f, (float)win_w, 2.0f);
  draw_text(r, 12.0f, 8.0f, 1.0f, FX_CREAM.r, FX_CREAM.g, FX_CREAM.b,
            "FACADE");
  draw_text(r, 12.0f, 25.0f, 1.0f, FX_LABEL.r, FX_LABEL.g, FX_LABEL.b,
            "MOVE ON WALL");
  draw_hud_separator(r, 112.0f);

  draw_text(r, 124.0f, 8.0f, 1.0f, FX_LABEL.r, FX_LABEL.g, FX_LABEL.b,
            "VITAL");
  int max_hp = gameplay_player_max_hp(&game->gameplay);
  for (int i = 0; i < max_hp; ++i)
    draw_hud_heart(r, 124.0f + i * 14.0f, 19.0f,
                   i < game->gameplay.player.hp);
  char life_buf[8];
  SDL_snprintf(life_buf, sizeof(life_buf), "x%d", game->campaign.lives);
  /* The wall earns extra lives exactly as the sectors do, so it announces them
   * the same way: a score that pays out silently out here reads as the counter
   * having miscounted. */
  if (game->presentation.extra_life_timer > 0.0f &&
      fmodf(game->presentation.extra_life_timer, 0.3f) > 0.15f)
    draw_text(r, 124.0f + max_hp * 14.0f + 4.0f, 20.0f, 1.0f,
              120, 255, 190, "1UP");
  else
    draw_text(r, 124.0f + max_hp * 14.0f + 4.0f, 20.0f, 1.0f,
              246, 110, 96, life_buf);
  draw_hud_separator(r, 218.0f);

  /* Wind gauge. During the warning it shows which way the gust will push;
   * during the gust it also says whether Chuck is currently in its lee. */
  const GameplayState *state = &game->gameplay;
  bool gusting = state->facade_wind_phase == FACADE_WIND_GUSTING;
  bool warning = state->facade_wind_phase == FACADE_WIND_WARNING;
  draw_text(r, 230.0f, 8.0f, 1.0f, FX_LABEL.r, FX_LABEL.g, FX_LABEL.b,
            "WIND");
  SDL_Color wind_color = gusting ? (state->facade_wind_sheltered
                                        ? FX_GREEN
                                        : (SDL_Color){236, 132, 84, 255})
                                 : warning ? (SDL_Color){225, 198, 112, 255}
                                           : (SDL_Color){58, 74, 92, 255};
  const char *wind_text = gusting
                              ? (state->facade_wind_sheltered ? "LEE" : "GUST")
                              : warning ? "RISING" : "CALM";
  for (int arrow = 0; arrow < 4; ++arrow)
  {
    float lit = (gusting || warning) ? 1.0f : 0.35f;
    float ax = 230.0f + (float)arrow * 9.0f;
    float grow = (gusting ? 1.0f : 0.55f) * (2.0f + (float)arrow * 1.2f);
    SDL_Color body = fx_dim(wind_color, lit);
    color_rect(r, body,
               state->facade_wind_dir >= 0 ? ax : ax + 6.0f - grow * 0.4f,
               22.0f - grow * 0.5f, grow, 1.0f + grow * 0.5f);
  }
  draw_text(r, 272.0f, 19.0f, 1.0f, wind_color.r, wind_color.g,
            wind_color.b, wind_text);
  draw_hud_separator(r, 330.0f);

  float start_y = game->gameplay.level.map.start_y;
  float target_y = game->gameplay.level.map.window_row * (float)TILE_SIZE;
  float distance = start_y - target_y;
  float climbed = start_y - game->gameplay.player.y;
  float progress = distance > 0.0f ? climbed / distance : 1.0f;
  if (progress < 0.0f)
    progress = 0.0f;
  if (progress > 1.0f)
    progress = 1.0f;
  draw_text(r, 342.0f, 8.0f, 1.0f, FX_LABEL.r, FX_LABEL.g, FX_LABEL.b,
            "ALTITUDE");
  color_rect(r, FX_NIGHT,
             342.0f, 20.0f, 264.0f, 11.0f);
  color_rect(r, (SDL_Color){54, 128, 128, 255},
             344.0f, 22.0f, 260.0f * progress, 7.0f);
  color_rect(r, FX_CYAN,
             344.0f, 22.0f, 260.0f * progress, 2.0f);
  /* Where a lost life puts him back on the wall. The bar is the only thing
   * that can say it, and a climber deciding whether to take the next gust
   * head-on is asking exactly this question. */
  if (game->gameplay.facade_has_checkpoint && distance > 0.0f)
  {
    float banked = (start_y - game->gameplay.facade_checkpoint_y) / distance;
    if (banked < 0.0f)
      banked = 0.0f;
    if (banked > 1.0f)
      banked = 1.0f;
    color_rect(r, (SDL_Color){225, 198, 112, 255},
               343.0f + 260.0f * banked, 19.0f, 2.0f, 13.0f);
  }

  int tiles_remaining = (int)ceilf(fabsf(game->gameplay.player.y - target_y) /
                                   TILE_SIZE);
  char remaining[24];
  SDL_snprintf(remaining, sizeof(remaining), "WINDOW %02dM", tiles_remaining);
  draw_text(r, 618.0f, 8.0f, 1.0f, 225, 198, 112, remaining);

  /* What Chuck is carrying up the wall is what he will have inside — drawn in
   * the same glyphs the sector draws it in, because the player is asked to
   * recognise a cartridge and a grenade everywhere else in the game and a
   * climb spelling them `A6 G0 R0` is the one screen teaching a private
   * shorthand. Nothing here is usable on the wall, so it is set at label
   * weight rather than as a live readout: it is a note about the next sector,
   * not a thing to reach for.
   *
   * Two of the three fields are constants and that is by construction rather
   * than by accident, which is worth saying so nobody reads them as tracking
   * anything: the label is always PISTOL and every pip is always lit, because
   * `player_begin_sector` resets the weapon in hand and hands over a full clip
   * at the doorway, `update_facade_playing` clears `switch_weapon` for the
   * whole climb, and nothing up here can fire. They stay because the note is
   * still true — that *is* what the next sector opens with, and a new player
   * has no way to know it — but the grenade and the rocket below are the only
   * part of this that a climb can change. It is also why a climb lays out no
   * `G`: see `test_no_climb_lays_out_a_pickup_it_cannot_use`. */
  draw_text(r, 618.0f, 20.0f, 1.0f, FX_LABEL.r, FX_LABEL.g, FX_LABEL.b,
            player_weapon_label(game->gameplay.player.active_weapon));
  for (int i = 0; i < MAX_AMMO; ++i)
    fx_ammo_pip(r, 677.0f + i * 7.0f, 19.0f,
                i < game->gameplay.player.bullets);
  if (game->gameplay.player.grenades > 0)
    draw_grenade(r, 724.0f, 19.0f, 0.0f);
  if (game->gameplay.player.bazooka_rockets > 0)
    draw_rocket_sprite(r, 742.0f, 22.0f, 1, false);
}

static void render_hud(Game *game)
{
  SDL_Renderer *r = game->platform.renderer;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);
  (void)win_h;
  if (game->gameplay.level.map.mode == LEVEL_MODE_FACADE)
  {
    render_facade_hud(game, win_w);
    return;
  }
  const Uint8 label_r = FX_LABEL.r, label_g = FX_LABEL.g,
              label_b = FX_LABEL.b;

  /* Brushed console body, lit from above, closed by a cyan status line. */
  fx_vgrad(r, 0.0f, 0.0f, (float)win_w, 37.0f,
           (SDL_Color){30, 40, 56, 255}, 255,
           (SDL_Color){13, 19, 30, 255}, 255);
  color_rect(r, (SDL_Color){60, 76, 98, 255}, 0.0f, 0.0f, (float)win_w, 1.0f);
  color_rect(r, FX_INK, 0.0f, 37.0f, (float)win_w, 1.0f);
  color_rect(r, (SDL_Color){38, 112, 110, 255}, 0.0f, 38.0f, (float)win_w, 1.0f);
  color_rect(r, FX_INK, 0.0f, 39.0f, (float)win_w, 1.0f);

  /* Identity block. A block is only as wide as its widest row, and every row
   * here is set in the 8px debug font: five cells at scale 2 put the
   * wordmark's last inked column at x=89, so the divider has to stand clear
   * of it instead of painting over the last stroke of the K. The rule under
   * it is that wordmark's own underline and carries the same width. */
  color_rect(r, FX_RED, 0.0f, 0.0f, 3.0f, 37.0f);
  draw_text(r, 12.0f, 9.0f, 2.0f, FX_CREAM.r, FX_CREAM.g, FX_CREAM.b,
            "CHUCK");
  color_rect(r, (SDL_Color){170, 52, 46, 255}, 12.0f, 23.0f, 78.0f, 2.0f);
  draw_text(r, 12.0f, 27.0f, 1.0f, FX_LABEL.r, FX_LABEL.g, FX_LABEL.b,
            "NO BACKUP");
  draw_hud_separator(r, 96.0f);

  /* Hearts are the hits left in this life; the counter beside them is the
   * lives left in the run. Empty sockets keep the maximum readable. The block
   * is sized for the assist row of five plus the widest counter, which is the
   * three-cell "1UP" and not the "x9" it replaces. */
  draw_text(r, 107.0f, 8.0f, 1.0f, label_r, label_g, label_b, "VITAL");
  int max_hp = gameplay_player_max_hp(&game->gameplay);
  for (int i = 0; i < max_hp; ++i)
    draw_hud_heart(r, 107.0f + i * 14.0f, 19.0f,
                   i < game->gameplay.player.hp);
  char life_buf[8];
  SDL_snprintf(life_buf, sizeof(life_buf), "x%d", game->campaign.lives);
  if (game->presentation.extra_life_timer > 0.0f &&
      fmodf(game->presentation.extra_life_timer, 0.3f) > 0.15f)
    draw_text(r, 107.0f + max_hp * 14.0f + 4.0f, 20.0f, 1.0f,
              120, 255, 190, "1UP");
  else
    draw_text(r, 107.0f + max_hp * 14.0f + 4.0f, 20.0f, 1.0f,
              246, 110, 96, life_buf);
  draw_hud_separator(r, 206.0f);

  /* All carried ammunition remains visible; the label names the weapon that
   * the next attack will actually use. */
  draw_text(r, 217.0f, 8.0f, 1.0f, label_r, label_g, label_b,
            player_weapon_label(game->gameplay.player.active_weapon));
  for (int i = 0; i < MAX_AMMO; ++i)
    fx_ammo_pip(r, 218.0f + i * 7.0f, 19.0f,
                i < game->gameplay.player.bullets);
  if (game->gameplay.player.grenades > 0)
    draw_grenade(r, 263.0f, 19.0f, 0.0f);
  if (game->gameplay.player.bazooka_rockets > 0)
    draw_rocket_sprite(r, 276.0f, 22.0f, 1, false);
  draw_hud_separator(r, 292.0f);

  /* Access status chip with a live LED.
   *
   * It reports the *sector*, not the room the boots are standing in. Entering
   * the restroom swaps the whole simulation, so reading the active one put the
   * WC's own (permanently locked, because it has none) door on the strip: a
   * player who had already found the card watched ACCESS fall back to a
   * blinking red LOCKED for the length of a detour, and go green again on the
   * way out. SECTOR beside it already names the sector rather than the room,
   * and the two have to agree. */
  const GameplayState *sector =
      game->in_sublevel ? &game->inactive_gameplay : &game->gameplay;
  draw_text(r, 303.0f, 8.0f, 1.0f, label_r, label_g, label_b, "ACCESS");
  bool blocked = sector->level.map.has_window;
  bool unlocked = sector->level.runtime.exit_unlocked;
  float blink = 0.5f + 0.5f * sinf((float)SDL_GetTicksNS() * 1.0e-9f * 4.0f);
  if (blocked)
  {
    color_rect(r, (SDL_Color){36, 38, 42, 255}, 303.0f, 19.0f, 70.0f, 13.0f);
    color_rect(r, (SDL_Color){96, 102, 108, 255}, 303.0f, 19.0f, 70.0f, 1.0f);
    color_rect(r, (SDL_Color){166, 142, 91, 255}, 307.0f, 24.0f, 3.0f, 3.0f);
    draw_text(r, 314.0f, 22.0f, 1.0f, FX_PALE.r, FX_PALE.g, FX_PALE.b,
              "BLOCKED");
  }
  else if (unlocked)
  {
    color_rect(r, (SDL_Color){16, 52, 40, 255}, 303.0f, 19.0f, 70.0f, 13.0f);
    color_rect(r, (SDL_Color){40, 132, 96, 255}, 303.0f, 19.0f, 70.0f, 1.0f);
    color_rect(r, FX_GREEN, 307.0f, 24.0f, 3.0f, 3.0f);
    draw_text(r, 314.0f, 22.0f, 1.0f, FX_GREEN.r, FX_GREEN.g, FX_GREEN.b,
              "GRANTED");
  }
  else
  {
    color_rect(r, (SDL_Color){54, 24, 24, 255}, 303.0f, 19.0f, 70.0f, 13.0f);
    color_rect(r, (SDL_Color){124, 52, 46, 255}, 303.0f, 19.0f, 70.0f, 1.0f);
    color_rect(r, fx_dim((SDL_Color){246, 90, 70, 255}, 0.45f + blink * 0.55f),
               307.0f, 24.0f, 3.0f, 3.0f);
    draw_text(r, 314.0f, 22.0f, 1.0f, FX_RED.r, FX_RED.g, FX_RED.b,
              "LOCKED");
  }
  draw_hud_separator(r, 387.0f);

  char level_buf[32];
  SDL_snprintf(level_buf, sizeof(level_buf), "%02d", game->campaign.current_level + 1);
  draw_text(r, 398.0f, 8.0f, 1.0f, label_r, label_g, label_b, "SECTOR");
  draw_text(r, 398.0f, 19.0f, 2.0f, 226, 232, 220, level_buf);
  draw_hud_separator(r, 456.0f);

  /* Score keeps its leading zeros, but only the live digits glow. */
  char score_buf[16];
  SDL_snprintf(score_buf, sizeof(score_buf), "%07d", game->campaign.score);
  draw_text(r, 467.0f, 8.0f, 1.0f, label_r, label_g, label_b, "SCORE");
  int first_digit = 0;
  while (first_digit < 6 && score_buf[first_digit] == '0')
    ++first_digit;
  draw_text(r, 467.0f, 19.0f, 2.0f, 74, 88, 102, score_buf);
  draw_text(r, 467.0f + first_digit * 16.0f, 19.0f, 2.0f,
            FX_AMBER.r, FX_AMBER.g, FX_AMBER.b, score_buf + first_digit);

  /* The passive trail meter becomes an unmistakable security readout while
   * the building alarm is active.
   *
   * Off the *sector*, for the reason ACCESS is: the restroom is a room of the
   * building, not a different building, and its own simulation has no alarm in
   * it. Reading the active one meant a sector left ringing went quiet the
   * moment Chuck stepped through the WC door and started ringing again when he
   * came back out — a countdown that pauses when the player hides is the HUD
   * offering a safe room the sector never granted. The timer itself is frozen
   * with the rest of the sector while he is away, so the number he returns to
   * is the number he left. */
  float t = (float)SDL_GetTicksNS() * 1.0e-9f;
  if (gameplay_alarm_active(sector))
  {
    float pulse = 0.5f + 0.5f * sinf(t * 7.0f);
    SDL_Color alert = fx_dim(fx_ramp(FX_RED).lit,
                             0.55f + pulse * 0.45f);
    char alarm_buf[24];
    SDL_snprintf(alarm_buf, sizeof(alarm_buf), "ALERT %02d",
                 (int)ceilf(sector->terminal_alarm_timer));
    draw_text(r, 650.0f, 8.0f, 1.0f,
              alert.r, alert.g, alert.b, "SECURITY");
    color_rect(r, (SDL_Color){58, 16, 18, 255},
               648.0f, 17.0f, 134.0f, 16.0f);
    color_rect(r, alert, 648.0f, 17.0f, 134.0f, 2.0f);
    color_rect(r, alert, 653.0f, 22.0f, 5.0f, 5.0f);
    draw_text(r, 665.0f, 20.0f, 1.0f,
              alert.r, alert.g, alert.b, alarm_buf);
  }
  else
  {
    draw_text(r, 650.0f, 8.0f, 1.0f, label_r, label_g, label_b, "TRAIL");
    color_rect(r, FX_NIGHT, 648.0f, 17.0f, 134.0f, 16.0f);
    color_rect(r, (SDL_Color){30, 42, 58, 255}, 648.0f, 17.0f, 134.0f, 1.0f);
    for (int i = 0; i < 12; ++i)
    {
      float wave = 0.5f + 0.5f * sinf(t * 2.6f + (float)i * 0.9f);
      float height = 3.0f + wave * 9.0f;
      SDL_Color bar = i < 9 ? fx_mix((SDL_Color){24, 96, 96, 255}, FX_CYAN, wave)
                            : (SDL_Color){52, 68, 82, 255};
      color_rect(r, bar, 653.0f + i * 10.0f, 30.0f - height, 6.0f, height);
    }
  }
}

/*
 * What the crew just said, printed under the strip.
 *
 * The building has always had a night shift talking to itself — a pose, a
 * bubble of dots and a handset sound — and never once a word of it. This is
 * the words, and it is the only place in the game the plot is told while the
 * player is actually playing rather than watching. It is deliberately a strip
 * and not a speech bubble: at twenty-six pixels across, a guard is not wide
 * enough to hang a sentence off, and a bubble would have to track a man who is
 * about to walk out of frame. The plate stays put and names him instead.
 *
 * The accent is the palette's own semantics rather than five decorative
 * colours: cyan is the handset (technology), red is the alarm (danger), amber
 * is a voice shouting out of a window on the wall (warning), cream is somebody
 * who does not work for Meridian, and two men talking in a room carry no
 * accent at all, because nothing is happening.
 */
static void render_crew_chatter(Game *game, int win_w)
{
  if (game->presentation.chatter_timer <= 0.0f)
    return;
  /* The unlocked-exit banner lands in exactly this band and owns the frame
   * for its two seconds. Two plates arguing over the same rows is worse than
   * losing one overheard line. */
  if (game->presentation.exit_unlocked_timer > 0.0f)
    return;

  SDL_Renderer *r = game->platform.renderer;
  ChatterKind kind = game->presentation.chatter_kind;
  /* Everybody on the crew is named; the people running out of the lobby are
   * not on anybody's docket and are not given one. */
  const char *who = kind == CHATTER_PANIC
                        ? NULL
                        : crew_callsign(game->presentation.chatter_speaker);
  /* The name is settled before the words are, because it decides which words
   * this man is allowed to be the one saying — and so does the hour. The
   * sector is the campaign's, never the restroom's, for the same reason every
   * other field on this strip reads through the sector: a man in the WC two
   * floors under the roof is still two floors under the roof. */
  CrewSituation situation = {game->campaign.current_level + 1,
                             game->campaign.hostiles_down};
  const char *line =
      crew_line_in(kind, game->presentation.chatter_roll, who, &situation);
  if (line == NULL)
    return;

  SDL_Color accent = FX_CYAN;
  if (kind == CHATTER_ALARM)
    accent = FX_RED;
  else if (kind == CHATTER_WALL)
    accent = FX_AMBER;
  else if (kind == CHATTER_PANIC)
    accent = FX_CREAM;
  else if (kind == CHATTER_TALK)
    accent = COL_CHATTER_IDLE;

  /* One curve for the whole life of the line: up fast, hold, and out over the
   * last half second. Read off the timer rather than off a wall clock, so it
   * freezes with the sector when the game is paused. */
  float left = game->presentation.chatter_timer;
  float rising = (CHATTER_HOLD_TIME - left) / 0.16f;
  float falling = left / 0.5f;
  float fade = rising < falling ? rising : falling;
  if (fade > 1.0f)
    fade = 1.0f;
  if (fade < 0.0f)
    fade = 0.0f;

  char buffer[96];
  if (who != NULL)
    SDL_snprintf(buffer, sizeof(buffer), "%s: %s", who, line);
  else
    SDL_strlcpy(buffer, line, sizeof(buffer));

  const float x = 14.0f;
  const float y = (float)HUD_HEIGHT + 8.0f;
  const float h = 17.0f;
  float w = draw_text_width(buffer, 1.0f) + 22.0f;
  if (w > (float)win_w - x * 2.0f)
    w = (float)win_w - x * 2.0f;

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  set_rgba(r, 6, 10, 16, (Uint8)(206.0f * fade));
  fill_rect(r, x, y, w, h);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
  /* The tick down the left edge is the same mark the report between sectors
   * puts against its intel line: this is the same kind of thing, overheard
   * instead of read afterwards. */
  color_rect(r, fx_dim(accent, fade), x, y, 3.0f, h);

  SDL_Color body = fx_dim((SDL_Color){186, 196, 192, 255}, fade);
  if (who != NULL)
  {
    draw_text(r, x + 11.0f, y + 5.0f, 1.0f,
              (Uint8)(accent.r * fade), (Uint8)(accent.g * fade),
              (Uint8)(accent.b * fade), who);
    draw_text(r, x + 11.0f + draw_text_width(who, 1.0f), y + 5.0f, 1.0f,
              body.r, body.g, body.b, buffer + SDL_strlen(who));
  }
  else
  {
    draw_text(r, x + 11.0f, y + 5.0f, 1.0f, body.r, body.g, body.b, buffer);
  }
}

static void render_interaction_prompt(Game *game, int win_w, int win_h)
{
  bool terminal_available = game->gameplay.terminal_in_range &&
                            !game->gameplay.level.runtime.exit_unlocked;
  bool door_available = gameplay_player_door_index(&game->gameplay) >= 0;
  SublevelDoorAction sublevel_action =
      gameplay_player_sublevel_door_action(&game->gameplay);
  bool sublevel_door_available = sublevel_action != SUBLEVEL_DOOR_NONE;
  if (game->state != STATE_PLAYING ||
      (!terminal_available && !door_available && !sublevel_door_available))
  {
    return;
  }

  SDL_Renderer *r = game->platform.renderer;
  float progress = terminal_available
                       ? game->gameplay.terminal_hack_progress /
                             TERMINAL_HACK_TIME
                       : 0.0f;
  if (progress < 0.0f)
    progress = 0.0f;
  if (progress > 1.0f)
    progress = 1.0f;

  const PadHints *pad = game_pad_hints(game);
  char buf[64];
  const char *label;
  /*
   * The key this prompt names is whatever the player has USE bound to, not the
   * E it shipped with. These four are the only lines in the game that name a
   * rebindable key while it is being played, and a prompt naming a button the
   * state does not accept is the one thing this file's own rules refuse — the
   * mirror of the ungated E the door key used to answer without being named.
   *
   * An action with no key at all prints "-", which reads as "nothing does
   * this" rather than as a missing word. The sheet says the same thing about
   * the same row, so the two agree.
   */
  const char *use_key =
      keybind_key_name(game->settings.bindings.keys[BIND_USE][0]);
  if (use_key[0] == '\0')
    use_key = keybind_key_name(game->settings.bindings.keys[BIND_USE][1]);
  if (use_key[0] == '\0')
    use_key = "-";
  char key_form[64];
  if (terminal_available && game->gameplay.terminal_hacking)
  {
    int percent = (int)(progress * 100.0f);
    SDL_snprintf(buf, sizeof(buf), "BREACHING SECURITY... %d%%", percent);
    label = buf;
  }
  else if (terminal_available)
  {
    SDL_snprintf(key_form, sizeof(key_form),
                 "HOLD %s TO HACK ACTIVE TERMINAL", use_key);
    label = pad_hint(pad, buf, sizeof(buf), "HOLD $Y TO HACK ACTIVE TERMINAL",
                     key_form);
  }
  else if (sublevel_action == SUBLEVEL_DOOR_ENTER)
  {
    SDL_snprintf(key_form, sizeof(key_form), "PRESS %s TO ENTER WC", use_key);
    label = pad_hint(pad, buf, sizeof(buf), "PRESS $Y TO ENTER WC", key_form);
  }
  else if (sublevel_action == SUBLEVEL_DOOR_RETURN)
  {
    SDL_snprintf(key_form, sizeof(key_form), "PRESS %s TO LEAVE WC", use_key);
    label = pad_hint(pad, buf, sizeof(buf), "PRESS $Y TO LEAVE WC", key_form);
  }
  else
  {
    SDL_snprintf(key_form, sizeof(key_form), "PRESS %s TO ENTER DOOR",
                 use_key);
    label = pad_hint(pad, buf, sizeof(buf), "PRESS $Y TO ENTER DOOR",
                     key_form);
  }

  const float text_scale = 1.0f;
  const float panel_padding = 12.0f;
  float panel_w = draw_text_width(label, text_scale) + panel_padding * 2.0f;
  float panel_h = terminal_available ? 45.0f : 31.0f;
  float x = ((float)win_w - panel_w) * 0.5f;
  float y = (float)win_h - panel_h - 12.0f;

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  set_rgba(r, 5, 10, 15, 235);
  fill_rect(r, x, y, panel_w, panel_h);
  set_rgba(r, 24, 46, 44, 255);
  fill_rect(r, x, y, panel_w, 1.0f);
  fill_rect(r, x, y + panel_h - 1.0f, panel_w, 1.0f);
  fill_rect(r, x, y, 1.0f, panel_h);
  fill_rect(r, x + panel_w - 1.0f, y, 1.0f, panel_h);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

  /* Terminal-green corner ticks frame the prompt like a targeting HUD. */
  set_rgba(r, 86, 240, 170, 255);
  const float tick = 7.0f;
  fill_rect(r, x, y, tick, 2.0f);
  fill_rect(r, x, y, 2.0f, tick);
  fill_rect(r, x + panel_w - tick, y, tick, 2.0f);
  fill_rect(r, x + panel_w - 2.0f, y, 2.0f, tick);
  fill_rect(r, x, y + panel_h - 2.0f, tick, 2.0f);
  fill_rect(r, x, y + panel_h - tick, 2.0f, tick);
  fill_rect(r, x + panel_w - tick, y + panel_h - 2.0f, tick, 2.0f);
  fill_rect(r, x + panel_w - 2.0f, y + panel_h - tick, 2.0f, tick);

  draw_text(r, x + panel_padding, y + 9.0f, text_scale,
            FX_GREEN.r, FX_GREEN.g, FX_GREEN.b, label);

  if (terminal_available)
  {
    color_rect(r, (SDL_Color){16, 34, 32, 255},
               x + panel_padding, y + 31.0f,
               panel_w - panel_padding * 2.0f, 7.0f);
    if (progress > 0.0f)
    {
      float bar_w = (panel_w - panel_padding * 2.0f) * progress;
      color_rect(r, (SDL_Color){44, 168, 118, 255},
                 x + panel_padding, y + 31.0f, bar_w, 7.0f);
      color_rect(r, (SDL_Color){120, 255, 190, 255},
                 x + panel_padding, y + 31.0f, bar_w, 2.0f);
      color_rect(r, (SDL_Color){190, 255, 220, 255},
                 x + panel_padding + bar_w - 2.0f,
                 y + 31.0f, 2.0f, 7.0f);
      fx_glow(r, x + panel_padding + bar_w, y + 34.0f, 14.0f,
              (SDL_Color){86, 240, 170, 255}, 80);
    }
  }
}

static void draw_overlay_panel(Game *game, float y, SDL_Color accent,
                               const char *title, const char *subtitle)
{
  SDL_Renderer *r = game->platform.renderer;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);

  /* Dim the whole scene so the verdict owns the frame. */
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  set_rgba(r, 4, 6, 11, 150);
  fill_rect(r, 0.0f, 0.0f, (float)win_w, (float)win_h);
  set_rgba(r, 5, 9, 15, 235);
  fill_rect(r, 0.0f, y - 35.0f, (float)win_w, subtitle ? 93.0f : 70.0f);
  set_rgba(r, accent.r, accent.g, accent.b, 220);
  fill_rect(r, 0.0f, y - 35.0f, (float)win_w, 3.0f);
  fill_rect(r, 0.0f, y + (subtitle ? 55.0f : 32.0f), (float)win_w, 2.0f);
  set_rgba(r, accent.r, accent.g, accent.b, 60);
  fill_rect(r, 0.0f, y - 32.0f, (float)win_w, 1.0f);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

  fx_glow(r, (float)win_w * 0.5f, y + 8.0f, 190.0f, accent, 34);
  draw_text_centered(game, y + 3.0f, 4.0f, FX_INK.r, FX_INK.g, FX_INK.b,
                     title);
  draw_text_centered(game, y, 4.0f, accent.r, accent.g, accent.b, title);
  if (subtitle)
    draw_text_centered(game, y + 37.0f, 2.0f, COL_SUBTITLE.r, COL_SUBTITLE.g,
                       COL_SUBTITLE.b, subtitle);
}

/*
 * The end of a run, and the only screen a score is being looked at rather than
 * played for. Until the score outlived the process there was nothing to
 * compare this run to; now there is, so the card says both — what this run
 * made, and the best any run ever has. When they are the same number the run
 * that just ended is the best one, and the card says that outright instead of
 * printing the same figure twice.
 */
static void draw_game_over_panel(Game *game)
{
  draw_overlay_panel(game, 225.0f, FX_RED, "GAME OVER",
                     "RETURNING TO MAIN MENU");

  char tally[64];
  int best = game_best_score(game);
  if (game->campaign.score >= best && game->campaign.score > 0)
    SDL_snprintf(tally, sizeof(tally), "SCORE %d - YOUR BEST YET",
                 game->campaign.score);
  else
    SDL_snprintf(tally, sizeof(tally), "SCORE %d - BEST %d",
                 game->campaign.score, best);
  draw_text_centered(game, 305.0f, 2.0f, COL_SUBTITLE.r, COL_SUBTITLE.g,
                     COL_SUBTITLE.b, tally);
}

static void draw_continue_overlay(Game *game)
{
  char hint[40];
  draw_overlay_panel(game, 190.0f, FX_AMBER,
                     "CONTINUE?",
                     pad_hint(game_pad_hints(game), hint, sizeof(hint),
                              "PRESS $A OR $START", "PRESS ENTER OR SPACE"));

  int seconds = (int)ceilf(game->campaign.continue_timer);
  char countdown[16];
  char remaining[48];
  SDL_snprintf(countdown, sizeof(countdown), "%d", seconds);
  /* The retry is always on the table; what runs out is the score insurance. */
  if (game->campaign.continues_remaining > 0)
    SDL_snprintf(remaining, sizeof(remaining), "SCORE KEPT - %d LEFT",
                 game->campaign.continues_remaining);
  else
    SDL_snprintf(remaining, sizeof(remaining), "SCORE RESETS ON CONTINUE");
  draw_text_centered(game, 310.0f, 6.0f, FX_AMBER.r, FX_AMBER.g, FX_AMBER.b,
                     countdown);
  draw_text_centered(game, 365.0f, 2.0f, COL_SUBTITLE.r, COL_SUBTITLE.g,
                     COL_SUBTITLE.b, remaining);
}

/*
 * The one plate both sheets are drawn on: the pause menu and the options.
 * They are the same object seen twice, so they are lit the same way — a steel
 * face falling into shade, a lit top edge, a dark base and the cyan spine that
 * marks every interface surface in the game.
 */
static void draw_sheet_plate(SDL_Renderer *r, float x, float y, float w,
                             float h)
{
  fx_vgrad(r, x, y, w, h,
           (SDL_Color){30, 40, 56, 255}, 255,
           (SDL_Color){13, 19, 30, 255}, 255);
  color_rect(r, (SDL_Color){60, 76, 98, 255}, x, y, w, 1.0f);
  color_rect(r, FX_INK, x, y + h - 1.0f, w, 1.0f);
  color_rect(r, FX_CYAN, x, y, 3.0f, h);
}

/* The cursor's own row: a wash the width of the plate and the caret that says
 * which line the next press lands on. The caret's height is given rather than
 * derived from the band, because a row is a label with a sentence under it and
 * the caret belongs beside the label — centring it in the band puts it in the
 * gap between the two, pointing at neither. */
static void draw_sheet_cursor(SDL_Renderer *r, float x, float y, float w,
                              float h, float caret_y)
{
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  set_rgba(r, FX_CYAN.r, FX_CYAN.g, FX_CYAN.b, 26);
  fill_rect(r, x + 12.0f, y, w - 24.0f, h);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
  draw_text(r, x + 16.0f, caret_y, 1.0f,
            FX_CYAN.r, FX_CYAN.g, FX_CYAN.b, ">");
}

static void dim_whole_frame(Game *game, Uint8 alpha)
{
  SDL_Renderer *r = game->platform.renderer;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  set_rgba(r, 4, 6, 11, alpha);
  fill_rect(r, 0.0f, 0.0f, (float)win_w, (float)win_h);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

/*
 * The pause menu. Three items, because a paused run has exactly three things
 * the player can want from it, and the one that cannot be taken back is last
 * and set in the danger red — a list where ABANDON RUN looks like RESUME is a
 * list somebody eventually loses a run to.
 */
static void draw_pause_menu(Game *game)
{
  SDL_Renderer *r = game->platform.renderer;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);

  dim_whole_frame(game, 205);

  static const char *labels[PAUSE_ITEM_COUNT] = {
      "RESUME", "OPTIONS", "ABANDON RUN"};
  static const char *details[PAUSE_ITEM_COUNT] = {
      "BACK TO THE SECTOR",
      "SOUND, DISPLAY AND ASSIST",
      "GIVE UP THIS RUN AND RETURN TO THE TITLE"};

  const float row_h = 44.0f;
  const float panel_w = 420.0f;
  const float rows_top = 78.0f;
  float panel_h = rows_top + row_h * (float)PAUSE_ITEM_COUNT + 40.0f;
  float panel_x = ((float)win_w - panel_w) * 0.5f;
  float panel_y = ((float)win_h - panel_h) * 0.5f - 10.0f;

  draw_sheet_plate(r, panel_x, panel_y, panel_w, panel_h);
  fx_glow(r, panel_x + panel_w * 0.5f, panel_y + 26.0f, 200.0f, FX_CYAN, 26);

  draw_text(r, panel_x + 22.0f, panel_y + 20.0f, 2.0f,
            FX_CREAM.r, FX_CREAM.g, FX_CREAM.b, "PAUSED");
  draw_text(r, panel_x + 22.0f, panel_y + 46.0f, 1.0f,
            FX_LABEL.r, FX_LABEL.g, FX_LABEL.b,
            "THE BUILDING WAITS.");

  for (int i = 0; i < PAUSE_ITEM_COUNT; ++i)
  {
    float row_y = panel_y + rows_top + (float)i * row_h;
    bool selected = game->pause_cursor == i;
    if (selected)
      draw_sheet_cursor(r, panel_x, row_y - 6.0f, panel_w, 38.0f,
                        row_y + 4.0f);

    SDL_Color tint = i == PAUSE_ITEM_ABANDON ? FX_RED : FX_CREAM;
    draw_text(r, panel_x + 40.0f, row_y, 2.0f,
              tint.r, tint.g, tint.b, labels[i]);
    draw_text(r, panel_x + 40.0f, row_y + 20.0f, 1.0f,
              FX_LABEL.r, FX_LABEL.g, FX_LABEL.b, details[i]);
  }

  char hint[80];
  draw_text(r, panel_x + 22.0f, panel_y + panel_h - 26.0f, 1.0f,
            FX_LABEL.r, FX_LABEL.g, FX_LABEL.b,
            pad_hint(game_pad_hints(game), hint, sizeof(hint),
                     "LS/DPAD: SELECT   $A: CHOOSE   $B: RESUME",
                     "ARROWS: SELECT   ENTER: CHOOSE   ESC: RESUME"));
}

/* From the end of the bar to the sheet's right margin: the widest reading the
 * row can print ("100%", four cells of the 8x8 font) plus a gap that keeps it
 * off the bar. */
#define SETTING_VALUE_GUTTER 46.0f

/* The level a slider is set to, drawn as a bar rather than only as a number:
 * a number says what it is, a bar says how far along it is, and moving one is
 * a thing the player does by eye. */
static void draw_setting_slider(SDL_Renderer *r, float x, float y, float w,
                                int percent, bool selected)
{
  const float h = 9.0f;
  color_rect(r, (SDL_Color){16, 22, 32, 255}, x, y, w, h);
  color_rect(r, FX_INK, x, y, w, 1.0f);

  float filled = w * (float)percent / 100.0f;
  if (filled > 0.0f)
  {
    SDL_Color body = selected ? FX_CYAN : fx_dim(FX_CYAN, 0.66f);
    color_rect(r, fx_dim(body, 0.55f), x, y, filled, h);
    color_rect(r, body, x, y, filled, 2.0f);
    if (selected)
      fx_glow(r, x + filled, y + h * 0.5f, 16.0f, FX_CYAN, 70);
  }

  /* The ten notches the row actually moves in, so the bar reads as a control
   * with stops rather than as a continuous fill the player is nudging. */
  for (int notch = 1; notch < 100 / SETTING_VOLUME_STEP; ++notch)
  {
    float nx = x + w * (float)notch * (float)SETTING_VOLUME_STEP / 100.0f;
    color_rect(r, (SDL_Color){8, 12, 18, 255}, nx, y + 1.0f, 1.0f, h - 2.0f);
  }

  char value[8];
  SDL_snprintf(value, sizeof(value), "%d%%", percent);
  /* Right-aligned against the sheet's own margin, so the digits do not shuffle
   * sideways as the level walks between 90 and 100 — and clear of the bar by
   * more than the one pixel that "wide enough for 100%" turns out to leave. */
  float value_w = draw_text_width(value, 1.0f);
  draw_text(r, x + w + SETTING_VALUE_GUTTER - value_w, y, 1.0f,
            selected ? FX_CREAM.r : FX_PALE.r,
            selected ? FX_CREAM.g : FX_PALE.g,
            selected ? FX_CREAM.b : FX_PALE.b, value);
}

/* A switch: a lit chip when on, a dark socket when off. Unchanged from the
 * assist sheet this grew out of, because it is the same object. */
static void draw_setting_toggle(SDL_Renderer *r, float x, float y, bool on)
{
  if (on)
  {
    color_rect(r, (SDL_Color){16, 52, 40, 255}, x, y, 56.0f, 15.0f);
    color_rect(r, (SDL_Color){40, 132, 96, 255}, x, y, 56.0f, 1.0f);
    color_rect(r, FX_GREEN, x + 5.0f, y + 6.0f, 3.0f, 3.0f);
    draw_text(r, x + 15.0f, y + 4.0f, 1.0f,
              FX_GREEN.r, FX_GREEN.g, FX_GREEN.b, "ON");
  }
  else
  {
    color_rect(r, (SDL_Color){36, 38, 42, 255}, x, y, 56.0f, 15.0f);
    color_rect(r, (SDL_Color){96, 102, 108, 255}, x, y, 56.0f, 1.0f);
    color_rect(r, (SDL_Color){80, 86, 94, 255}, x + 5.0f, y + 6.0f, 3.0f, 3.0f);
    draw_text(r, x + 15.0f, y + 4.0f, 1.0f,
              FX_PALE.r, FX_PALE.g, FX_PALE.b, "OFF");
  }
}

/*
 * A binding row's two keycaps.
 *
 * Both are drawn whether or not they hold anything, because an action with one
 * key and an action with an empty second slot are different states and a row
 * that drew only what was filled would show them identically. An empty cap is
 * a dashed outline with a dash in it — visibly a place a key goes, rather than
 * a gap in the sheet.
 *
 * The caret is the slot the next capture will land in, and it is only drawn on
 * the row under the cursor: a caret on every row would be nine claims about
 * where a press is about to go.
 */
static void draw_setting_keys(Game *game, float right, float y,
                              const SettingRow *row, bool selected)
{
  SDL_Renderer *r = game->platform.renderer;
  BindAction action = settings_row_action(row->id);
  if (action == BIND_COUNT)
    return;

  /*
   * Sized off the longest name each half may spell, so the caps sit in the
   * same place on every row however short what is in them is. A column that
   * moved with its contents would make the sheet jitter as it is edited.
   *
   * The two halves get their own width because they are two different
   * alphabets: a key spells itself in up to `KEYBIND_NAME_MAX`, while a pad
   * button is at most `PADBIND_NAME_MAX` and most of them are two characters.
   * Giving the pad the keyboard's width would put four wide caps on a row that
   * has nine label characters to spare.
   */
  const float cap_w =
      (float)KEYBIND_NAME_MAX * SETTINGS_GLYPH_W + SETTINGS_CAP_PAD;
  const float pad_cap_w =
      (float)PADBIND_NAME_MAX * SETTINGS_GLYPH_W + SETTINGS_CAP_PAD;
  const float cap_h = 15.0f;
  const float gap = SETTINGS_CAP_GAP;
  const float group_gap = SETTINGS_CAP_GROUP_GAP;

  const PadHints *hints = game_pad_hints(game);
  /* The keyboard's run ends on the right margin; the pad's sits to its left,
   * so a row reads keys-then-buttons in the order the caret walks them. */
  float keys_left = right - (float)BIND_SLOTS * (cap_w + gap) + gap;
  float pad_left =
      keys_left - group_gap - (float)BIND_SLOTS * (pad_cap_w + gap) + gap;

  for (int slot = 0; slot < BIND_TOTAL_SLOTS; ++slot)
  {
    bool is_pad = slot >= BIND_PAD_SLOT;
    int within = is_pad ? slot - BIND_PAD_SLOT : slot;
    float w = is_pad ? pad_cap_w : cap_w;
    float x = (is_pad ? pad_left : keys_left) + (float)within * (w + gap);

    /* A pad name is a `pad_hint` template — `$A` and the rest — because what a
     * face button is called depends on what is plugged in. Expanded here, once,
     * through the same call every other prompt in the game makes. */
    char spelled[16];
    const char *name;
    if (is_pad)
    {
      const char *form =
          keybind_pad_name(game->settings.bindings.pad[action][within]);
      name = form[0] == '\0'
                 ? ""
                 : pad_hint(hints, spelled, sizeof(spelled), form, form);
    }
    else
    {
      name = keybind_key_name(game->settings.bindings.keys[action][within]);
    }
    bool empty = name[0] == '\0';
    bool caret = selected && game->settings_bind_slot == slot;
    bool arming = caret && game->settings_capturing;

    SDL_Color face = arming ? (SDL_Color){64, 46, 18, 255}
                            : (SDL_Color){36, 38, 42, 255};
    SDL_Color edge = arming   ? FX_AMBER
                     : caret  ? (SDL_Color){150, 158, 168, 255}
                              : (SDL_Color){96, 102, 108, 255};
    color_rect(r, face, x, y, w, cap_h);
    color_rect(r, edge, x, y, w, 1.0f);

    const char *shown = arming ? "?" : (empty ? "-" : name);
    SDL_Color ink = arming  ? FX_AMBER
                    : empty ? (SDL_Color){96, 102, 108, 255}
                            : FX_PALE;
    /* Centred in the cap rather than left-aligned: a one-character name and a
     * six-character one both have to read as the same kind of thing. */
    float text_w = draw_text_width(shown, 1.0f);
    draw_text(r, x + (w - text_w) * 0.5f, y + 4.0f, 1.0f, ink.r, ink.g,
              ink.b, shown);
  }
}

/*
 * The options sheet. Its rows come straight out of the table in
 * [settings.c](settings.c) and it draws whatever it finds there: adding a
 * setting is a line in that table and nothing here. The two kinds of control
 * are the whole layout language, which is the same bargain the manual's
 * `ManualLine` makes.
 */

static void draw_settings_sheet(Game *game)
{
  SDL_Renderer *r = game->platform.renderer;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);
  /* Part of the layout, not a decoration on top of it: the warning stands in
   * the audio heading's detail slot, so the plate is sized with it. */
  bool muted = audio_is_muted(&game->platform.audio);

  dim_whole_frame(game, 205);

  int row_count = 0;
  const SettingRow *rows = settings_rows(game->settings_page, &row_count);

  /* A heading is a rule with a name in it; one that carries a sentence as well
   * needs the room for it, or the sentence lands on the first row of its own
   * section. The height is asked per row rather than assumed, which is also
   * what lets the sheet grow a section without any of this being retuned. */
  const float heading_h = 26.0f;
  const float heading_detail_h = 40.0f;
  float value_h = 40.0f;
  /* A binding row carries no sentence under its label, so it needs no room for
   * one. Nine of them at the full row height is 90px the controls page has
   * nowhere to put. */
  float bind_h = 28.0f;
  const float rows_top = 66.0f;
  const float panel_w = SETTINGS_PANEL_W;

  float rows_h = 0.0f;
  for (int i = 0; i < row_count; ++i)
  {
    if (rows[i].kind == SETTING_ROW_BINDING)
      rows_h += bind_h;
    else if (rows[i].kind != SETTING_ROW_HEADING)
      rows_h += value_h;
    else if (rows[i].detail != NULL ||
             (muted && settings_heading_governs_levels(rows, row_count, i)))
      rows_h += heading_detail_h;
    else
      rows_h += heading_h;
  }

  /*
   * And if the page still does not fit the frame, the rows give the room back
   * rather than the plate hanging off the bottom of it.
   *
   * The sheet has always been sized from its table so that "a new section costs
   * no layout", and that held right up to the point where a new section was
   * bigger than the 36px the main page had spare. Squeezing here keeps the
   * promise the other way round: a tenth setting shortens the rows a little
   * instead of silently drawing the last of them off the plate, which is the
   * failure the manual's control sheet has already had once.
   */
  float panel_h = rows_top + rows_h + 38.0f;
  float frame_limit = (float)win_h - 16.0f;
  if (panel_h > frame_limit && rows_h > 0.0f)
  {
    float squeeze = (frame_limit - rows_top - 38.0f) / rows_h;
    if (squeeze < 0.6f)
      /* Past this the labels touch. A sheet that long is a bug in the table,
       * not something to keep shrinking around. */
      squeeze = 0.6f;
    value_h *= squeeze;
    bind_h *= squeeze;
    rows_h *= squeeze;
    panel_h = rows_top + rows_h + 38.0f;
  }
  float panel_x = ((float)win_w - panel_w) * 0.5f;
  float panel_y = ((float)win_h - panel_h) * 0.5f;

  draw_sheet_plate(r, panel_x, panel_y, panel_w, panel_h);

  draw_text(r, panel_x + 22.0f, panel_y + 18.0f, 2.0f,
            FX_CREAM.r, FX_CREAM.g, FX_CREAM.b,
            settings_page_title(game->settings_page));
  draw_text(r, panel_x + 22.0f, panel_y + 42.0f, 1.0f,
            FX_LABEL.r, FX_LABEL.g, FX_LABEL.b,
            settings_page_strap(game->settings_page));

  float y = panel_y + rows_top;
  for (int i = 0; i < row_count; ++i)
  {
    const SettingRow *row = &rows[i];

    if (row->kind == SETTING_ROW_HEADING)
    {
      /* A section rule: the name, and the hairline that carries it across the
       * plate so the three groups read as three groups. */
      float label_w = draw_text_width(row->label, 1.0f);
      draw_text(r, panel_x + 22.0f, y + 11.0f, 1.0f,
                FX_AMBER.r, FX_AMBER.g, FX_AMBER.b, row->label);
      SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
      set_rgba(r, FX_AMBER.r, FX_AMBER.g, FX_AMBER.b, 46);
      fill_rect(r, panel_x + 30.0f + label_w, y + 14.0f,
                panel_w - 52.0f - label_w, 1.0f);
      SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
      /*
       * The mute is the one thing on this sheet that is not a stored setting,
       * and the only thing that can make the two levels below it lie.
       *
       * Keeping M off this screen stopped the sheet *creating* that
       * contradiction and did nothing at all about a player who muted first
       * and opened the sheet second — one keystroke, and the more likely
       * order of the two. The ban is still right and still in force, which is
       * why the line says outright that the key is not answered here rather
       * than naming a binding this screen does not have.
       *
       * Set in the danger red rather than the label grey: it is not a
       * description of the section, it is a correction to it.
       */
      bool levels_are_silenced =
          muted && settings_heading_governs_levels(rows, row_count, i);
      if (levels_are_silenced)
      {
        draw_text(r, panel_x + 22.0f, y + 24.0f, 1.0f,
                  FX_RED.r, FX_RED.g, FX_RED.b,
                  SETTINGS_MUTED_LINE);
        y += heading_detail_h;
      }
      else if (row->detail != NULL)
      {
        draw_text(r, panel_x + 22.0f, y + 24.0f, 1.0f,
                  FX_LABEL.r, FX_LABEL.g, FX_LABEL.b, row->detail);
        y += heading_detail_h;
      }
      else
      {
        y += heading_h;
      }
      continue;
    }

    bool selected = game->settings_cursor == i;
    if (selected)
      draw_sheet_cursor(r, panel_x, y - 6.0f, panel_w, 36.0f, y);

    draw_text(r, panel_x + 34.0f, y, 1.0f,
              selected ? 236 : 200, selected ? 238 : 208,
              selected ? 224 : 196, row->label);
    if (row->detail != NULL)
      draw_text(r, panel_x + 34.0f, y + 15.0f, 1.0f,
                FX_LABEL.r, FX_LABEL.g, FX_LABEL.b, row->detail);

    /* Both controls end on the same right margin, so the column of switches
     * and the ends of the bars line up down the sheet. */
    float control_right = panel_x + panel_w - SETTINGS_CONTROL_INSET;
    if (row->kind == SETTING_ROW_SLIDER)
    {
      draw_setting_slider(r, control_right - SETTING_VALUE_GUTTER - 140.0f,
                          y + 1.0f, 140.0f,
                          settings_value_percent(&game->settings, row->id),
                          selected);
    }
    else if (row->kind == SETTING_ROW_TOGGLE)
    {
      draw_setting_toggle(r, control_right - 56.0f, y,
                          settings_value_bool(&game->settings, row->id));
    }
    else if (row->kind == SETTING_ROW_BINDING)
    {
      draw_setting_keys(game, control_right, y, row, selected);
    }
    /* SETTING_ROW_ACTION draws no control of its own: it is a label that
     * happens when it is pressed, and a switch beside it would say it holds a
     * state it does not. */

    y += row->kind == SETTING_ROW_BINDING ? bind_h : value_h;
  }

  /* Named exactly as bound: up and down walk, left and right change, and the
   * way out is whatever the player already reached for to open this. */
  char hint[96];
  /* An armed cap says what it is waiting for, and the two caps wait for
   * different things: a keyboard press into a key cap, a button into a pad
   * one. One line for both would name the wrong escape half the time — and
   * naming a way out the state does not accept is the thing this codebase
   * refuses everywhere else. Both are measured by
   * `test_every_word_on_the_options_sheet_fits_the_plate`. */
  const char *footer;
  if (game->settings_capturing)
    footer = game_settings_slot_is_pad(game)
                 ? SETTINGS_CAPTURE_PAD_LINE
                 : SETTINGS_CAPTURE_KEY_LINE;
  else
    footer = pad_hint(game_pad_hints(game), hint, sizeof(hint),
                      "LS/DPAD: SELECT AND CHANGE   $A: CHANGE   $B: DONE",
                      "ARROWS: SELECT AND CHANGE   ENTER: CHANGE   "
                      "ESC: DONE");
  draw_text(r, panel_x + 22.0f, panel_y + panel_h - 24.0f, 1.0f,
            FX_LABEL.r, FX_LABEL.g, FX_LABEL.b, footer);
}

#ifdef CHUCK_DEBUG
static void draw_debug_level_select(Game *game)
{
  SDL_Renderer *r = game->platform.renderer;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);

  char label[80];
  SDL_snprintf(label, sizeof(label),
               "DEBUG  </>: LEVEL %02d/%02d  F5: LOAD",
               game->debug_selected_level + 1, (int)EMBEDDED_LEVEL_COUNT);
  float text_w = draw_text_width(label, 1.0f);
  float panel_w = text_w + 24.0f;
  float panel_x = ((float)win_w - panel_w) * 0.5f;
  float panel_y = (float)win_h - 126.0f;

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  set_rgba(r, 5, 9, 15, 230);
  fill_rect(r, panel_x, panel_y, panel_w, 24.0f);
  set_rgba(r, FX_CYAN.r, FX_CYAN.g, FX_CYAN.b, 210);
  fill_rect(r, panel_x, panel_y, 3.0f, 24.0f);
  fill_rect(r, panel_x + panel_w - 3.0f, panel_y, 3.0f, 24.0f);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
  draw_text(r, panel_x + 12.0f, panel_y + 8.0f, 1.0f,
            FX_CYAN.r, FX_CYAN.g, FX_CYAN.b, label);
}
#endif

/*
 * The city the names roll over.
 *
 * A roll on flat black would be the one screen in the game that is not lit, so
 * the frame keeps a horizon: the skyline Chuck spent the night above, seen
 * from nowhere in particular, with Kessler Tower still the tallest thing on it
 * and its roof beacon still turning. Everything here is silhouette and every
 * lit window is dim on purpose — the type is what this screen is for, and a
 * backdrop that competes with it has stopped being a backdrop.
 */
static void draw_credits_skyline(SDL_Renderer *r, float view_w, float view_h,
                                 float time)
{
  const float base = view_h - 19.0f; /* the lower letterbox bar is the ground */
  const SDL_Color mass = {9, 12, 19, 255};
  const SDL_Color lit_edge = FX_SHADOW;
  float x = -22.0f;

  for (int i = 0; x < view_w; ++i)
  {
    unsigned h = fx_hash((unsigned)i * 2654435761u + 17u);
    float w = 34.0f + (float)(h % 5u) * 13.0f;
    float height = 38.0f + (float)((h >> 5) % 7u) * 15.0f;
    bool tower = (i == 9);
    if (tower)
      height = 226.0f;

    float top = base - height;
    color_rect(r, mass, x, top, w, height);
    color_rect(r, lit_edge, x, top, w, 1.0f);

    /* A handful of floors still working, the way the tower's own facade is
     * drawn on the title screen: asked for once, never twinkling. */
    for (float wy = top + 8.0f; wy < base - 6.0f; wy += 9.0f)
    {
      for (float wx = x + 5.0f; wx < x + w - 5.0f; wx += 8.0f)
      {
        /* Through `int`, because the row starts off the left edge of the frame:
         * the first block stands at x = -22, so `wx * 7` is negative for its
         * first three windows, and converting a negative float straight to
         * `unsigned` is undefined. It is also undefined *differently* on the
         * two slices of the shipped universal binary, which would have lit
         * different windows on Intel and on Apple Silicon. */
        unsigned cell = fx_hash((unsigned)(int)(wx * 7.0f) ^
                                (unsigned)(int)(wy * 131.0f));
        if ((cell % 9u) > 1u)
          continue;
        SDL_Color pane = (cell & 8u) ? FX_WARM : FX_LAMP;
        fx_rect_a(r, pane, 40, wx, wy, 3.0f, 4.0f);
      }
    }

    if (tower)
    {
      /* The roof they were never going to leave from. */
      float beacon_x = x + w * 0.5f;
      float pulse = 0.35f + 0.65f * (0.5f + 0.5f * sinf(time * 2.6f));
      fx_glow(r, beacon_x, top - 3.0f, 11.0f, FX_RED,
              (Uint8)(70.0f * pulse));
      color_rect(r, fx_dim(FX_RED, 0.45f + pulse * 0.55f),
                 beacon_x - 1.0f, top - 4.0f, 2.0f, 3.0f);
    }

    x += w + 5.0f;
  }
}

/* A row's colour, which is the one thing about it the table does not carry: the
 * name and the game's own name are what the screen is for, the job above a name
 * is an interface label, and a note is prose. */
static SDL_Color credit_ink(CreditLineKind kind)
{
  switch (kind)
  {
  case CREDIT_TITLE:
  case CREDIT_NAME:
    return FX_CREAM;
  case CREDIT_ROLE:
    return FX_LABEL;
  case CREDIT_NOTE:
    return FX_PALE;
  case CREDIT_RULE:
  case CREDIT_GAP:
    break;
  }
  return FX_CREAM;
}

/*
 * The roll itself: the table in [credits.c](credits.c), walked from the top,
 * every row placed by the height its own kind asks for. Nothing about the
 * layout is written here, which is the point — a line added to the table needs
 * no change to this function, and a line too long for the frame fails the test
 * suite rather than this screen.
 */
static void draw_credits_roll(Game *game)
{
  SDL_Renderer *r = game->platform.renderer;
  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);

  const CreditsRoll *roll = &game->presentation.credits;
  float view_w = (float)win_w;
  float view_h = (float)win_h;

  color_rect(r, FX_NIGHT, 0.0f, 0.0f, view_w, view_h);
  fx_vgrad(r, 0.0f, 0.0f, view_w, view_h,
           (SDL_Color){6, 9, 16, 255}, 255,
           (SDL_Color){16, 22, 33, 255}, 255);
  draw_credits_skyline(r, view_w, view_h, roll->time);

  int count = 0;
  const CreditLine *lines = credits_lines(&count);
  float scroll = credits_scroll(roll);
  float y = view_h - scroll;

  for (int i = 0; i < count; ++i)
  {
    float row_h = credits_line_height(lines[i].kind);
    /* Rows above and below the frame are stepped over rather than drawn: a
     * roll is a long strip and only a screen of it is ever on. */
    if (y > -row_h && y < view_h)
    {
      float center_x = view_w * 0.5f;
      if (lines[i].kind == CREDIT_RULE)
      {
        color_rect(r, FX_RUST, center_x - 56.0f, y + 8.0f, 112.0f, 2.0f);
      }
      else if (lines[i].kind != CREDIT_GAP)
      {
        SDL_Color ink = credit_ink(lines[i].kind);
        float scale = credits_line_scale(lines[i].kind);
        draw_text(r, center_x - draw_text_width(lines[i].text, scale) * 0.5f,
                  y, scale, ink.r, ink.g, ink.b, lines[i].text);
      }
    }
    y += row_h;
  }

  fx_grain(r, win_w, win_h, roll->time, FX_GRAIN_FILM);

  /* The same two bars the outro finishes on, drawn last so the roll runs under
   * them instead of stopping short of them. */
  color_rect(r, FX_INK, 0.0f, 0.0f, view_w, 19.0f);
  color_rect(r, FX_INK, 0.0f, view_h - 19.0f, view_w, 19.0f);

  char hint[40];
  const PadHints *pad = game_pad_hints(game);
  bool resting = credits_at_rest(roll);
  const char *prompt =
      resting ? pad_hint(pad, hint, sizeof(hint), "$A: MAIN MENU",
                         "SPACE / ENTER: MAIN MENU")
              : pad_hint(pad, hint, sizeof(hint), "$A: SKIP",
                         "SPACE / ENTER: SKIP");
  float pulse = 0.45f + 0.55f * sinf(roll->time * 2.0f);
  draw_text(r, view_w - draw_text_width(prompt, 1.0f) - 24.0f, view_h - 31.0f,
            1.0f, (Uint8)(101.0f + pulse * 40.0f), (Uint8)(109.0f + pulse * 40.0f),
            (Uint8)(108.0f + pulse * 38.0f), prompt);
}

void game_render(Game *game)
{
  SDL_Renderer *r = game->platform.renderer;
  SDL_SetRenderDrawColor(r, 8, 11, 17, 255);
  SDL_RenderClear(r);

  int win_w = 0, win_h = 0;
  game_get_view_size(game, &win_w, &win_h);

  /*
   * Every frame leaves through the bottom of this function, where the one
   * finishing pass lives. The screens the player is playing (the sector, the
   * chase) keep the light vignette so the playfield's edges stay readable;
   * the screens the player is watching (title, manual, cutscenes) close in
   * harder, the way a film frame does. No renderer applies its own finish:
   * when they did, the pause overlay over the chase floated on top of a
   * finished frame, and the assist sheet over the title was vignetted twice.
   */
  Uint8 vignette = FX_VIGNETTE_PLAY;

  if (game->state == STATE_CHASE ||
      (game->state == STATE_PAUSED &&
       game->pause_return_state == STATE_CHASE))
  {
    chase_render(r, &game->chase, win_w, win_h,
                 game->presentation.camera_shake_x,
                 game->presentation.camera_shake_y,
                 game->settings.reduced_motion,
                 game_pad_hints(game));
    if (game->state == STATE_PAUSED)
      draw_pause_menu(game);
  }
  else if (game->state == STATE_SETTINGS)
  {
    /* Over the title the sheet floats on the night; over the pause screen it
     * floats on the held frame — and "the held frame" has to mean whatever was
     * actually running, which during the prologue is the drive. Asking only
     * whether the sheet came from the title screen put the frozen first sector
     * behind a sheet opened from a paused drive, which is a road the player
     * has not reached yet. */
    if (game->settings_return_state == STATE_INTRO)
    {
      intro_render(r, &game->presentation.intro, win_w, win_h,
                   game_pad_hints(game));
      vignette = FX_VIGNETTE_SCENE;
    }
    else if (game->pause_return_state == STATE_CHASE)
    {
      chase_render(r, &game->chase, win_w, win_h,
                   game->presentation.camera_shake_x,
                   game->presentation.camera_shake_y,
                   game->settings.reduced_motion,
                   game_pad_hints(game));
    }
    else
    {
      render_world(game);
      render_hud(game);
    }
    draw_settings_sheet(game);
  }
  else if (game->state == STATE_ABDUCTION)
  {
    abduction_cutscene_render(r, &game->presentation.abduction_cutscene,
                              win_w, win_h, game_pad_hints(game));
    vignette = FX_VIGNETTE_SCENE;
  }
  else if (game->state == STATE_OPENING_CUTSCENE)
  {
    opening_cutscene_render(r, &game->presentation.opening_cutscene,
                            win_w, win_h, game_pad_hints(game));
    vignette = FX_VIGNETTE_SCENE;
  }
  else if (game->state == STATE_INTRO)
  {
    intro_render(r, &game->presentation.intro, win_w, win_h,
                 game_pad_hints(game));
#ifdef CHUCK_DEBUG
    draw_debug_level_select(game);
#endif
    vignette = FX_VIGNETTE_SCENE;
  }
  else if (game->state == STATE_MANUAL)
  {
    manual_render(r, &game->presentation.manual, win_w, win_h,
                  game_pad_hints(game));
    vignette = FX_VIGNETTE_SCENE;
  }
  else if (game->state == STATE_LEVEL_TRANSITION)
  {
    level_transition_render(r, &game->presentation.level_transition,
                            win_w, win_h, game_pad_hints(game));
    vignette = FX_VIGNETTE_SCENE;
  }
  else if (game->state == STATE_OUTRO)
  {
    outro_cutscene_render(r, &game->presentation.outro_cutscene,
                          win_w, win_h, game_pad_hints(game));
    vignette = FX_VIGNETTE_SCENE;
  }
  else if (game->state == STATE_CREDITS)
  {
    draw_credits_roll(game);
    vignette = FX_VIGNETTE_SCENE;
  }
  else
  {
    render_world(game);
    render_hud(game);
    render_crew_chatter(game, win_w);
    render_interaction_prompt(game, win_w, win_h);

    if (game->presentation.exit_unlocked_timer > 0.0f)
    {
      SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
      set_rgba(r, 7, 20, 20, 215);
      fill_rect(r, 226.0f, 47.0f, 348.0f, 31.0f);
      SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
      draw_text_centered(game, 63.0f, 2.0f, FX_GREEN.r, FX_GREEN.g, FX_GREEN.b,
                         "PURSUIT ROUTE OPEN");
      float exit_screen = game->gameplay.level.map.exit_col * (float)TILE_SIZE - game->presentation.cam_x;
      float exit_screen_y = game->gameplay.level.map.exit_row * (float)TILE_SIZE +
                            HUD_HEIGHT - game->presentation.cam_y;
      if (exit_screen + TILE_SIZE < 0.0f)
        draw_text(r, 199.0f, 52.0f, 2.0f, FX_AMBER.r, FX_AMBER.g, FX_AMBER.b, "<");
      else if (exit_screen > (float)win_w)
        draw_text(r, 584.0f, 52.0f, 2.0f, FX_AMBER.r, FX_AMBER.g, FX_AMBER.b, ">");
      else if (exit_screen_y + TILE_SIZE < HUD_HEIGHT)
        draw_text(r, 393.0f, 48.0f, 2.0f, FX_AMBER.r, FX_AMBER.g, FX_AMBER.b, "^");
      else if (exit_screen_y > (float)win_h)
        draw_text(r, 393.0f, 61.0f, 2.0f, FX_AMBER.r, FX_AMBER.g, FX_AMBER.b, "v");
    }

    if (game->state == STATE_LEVEL_CLEARED)
      draw_overlay_panel(game, 240.0f, FX_GREEN,
                         "THE TRAIL LEADS UP", NULL);
    else if (game->state == STATE_CONTINUE)
      draw_continue_overlay(game);
    else if (game->state == STATE_GAME_OVER)
      draw_game_over_panel(game);
    else if (game->state == STATE_PAUSED)
      draw_pause_menu(game);
  }

  /* The one finishing pass, and the one switch that turns it off. It is the
   * game's look and it stays on unless the player says otherwise, but it is a
   * filter over every pixel of every screen, so leaving it unremovable is the
   * one decision about it that is not ours to make. */
  if (game->settings.crt_filter)
  {
    fx_vignette(r, win_w, win_h, vignette);
    fx_scanlines(r, win_w, win_h, FX_SCANLINE_ALPHA);
  }

  SDL_RenderPresent(r);
}
