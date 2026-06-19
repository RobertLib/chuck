#ifndef CHUCK_INTRO_H
#define CHUCK_INTRO_H

#include "common.h"

#define INTRO_STAR_COUNT 150

typedef struct
{
    float x, y;
    float speed; /* horizontal drift; faster stars form the near layer */
    float phase;
} IntroStar;

typedef struct
{
    float time; /* seconds elapsed since the intro screen appeared */
    IntroStar stars[INTRO_STAR_COUNT];
    SDL_FRect start_button;
    bool start_hovered;
} Intro;

void intro_init(Intro *intro, int win_w, int win_h);
void intro_update(Intro *intro, float dt, int win_w, int win_h, float mouse_x, float mouse_y);
void intro_render(SDL_Renderer *renderer, const Intro *intro, int win_w, int win_h);

/* True when (x, y), in logical render coordinates, lies within the START button. */
bool intro_hit_start_button(const Intro *intro, float x, float y);

#endif /* CHUCK_INTRO_H */
