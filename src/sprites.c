#include "sprites.h"

static SDL_Texture *load_sheet(SDL_Renderer *renderer, const char *path)
{
  SDL_Surface *surf = SDL_LoadBMP(path);
  if (!surf)
  {
    SDL_Log("sprites: '%s' not found (%s) – using procedural fallback",
            path, SDL_GetError());
    return NULL;
  }
  SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
  SDL_DestroySurface(surf);
  if (!tex)
    SDL_Log("sprites: texture creation failed for '%s': %s", path, SDL_GetError());
  return tex;
}

bool sprites_load(Sprites *s, SDL_Renderer *renderer)
{
  SDL_zerop(s);
  s->tileset = load_sheet(renderer, "assets/tileset.bmp");
  s->player = load_sheet(renderer, "assets/player.bmp");
  s->enemy = load_sheet(renderer, "assets/enemy.bmp");
  s->items = load_sheet(renderer, "assets/items.bmp");
  return true;
}

void sprites_free(Sprites *s)
{
  if (s->tileset)
    SDL_DestroyTexture(s->tileset);
  if (s->player)
    SDL_DestroyTexture(s->player);
  if (s->enemy)
    SDL_DestroyTexture(s->enemy);
  if (s->items)
    SDL_DestroyTexture(s->items);
  SDL_zerop(s);
}
