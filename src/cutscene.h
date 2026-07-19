#ifndef CHUCK_CUTSCENE_H
#define CHUCK_CUTSCENE_H

#include "common.h"

#define OPENING_CUTSCENE_DURATION 12.4f
#define LEVEL_TRANSITION_DURATION 9.4f
#define OUTRO_CUTSCENE_DURATION 25.0f
#define OUTRO_FINAL_REVEAL_TIME 19.4f

typedef enum
{
    OPENING_CUE_RAIN = 1u << 0,
    OPENING_CUE_SUV_ENGINE = 1u << 1,
    OPENING_CUE_CAR_ENGINE = 1u << 2,
    OPENING_CUE_SUV_BRAKE = 1u << 3,
    OPENING_CUE_CAR_BRAKE = 1u << 4,
    OPENING_CUE_CAR_DOOR = 1u << 5,
    OPENING_CUE_ESCORT_STEP_A = 1u << 6,
    OPENING_CUE_ESCORT_STEP_B = 1u << 7,
    OPENING_CUE_CHUCK_STEP_A = 1u << 8,
    OPENING_CUE_CHUCK_STEP_B = 1u << 9,
    OPENING_CUE_BUILDING_DOOR = 1u << 10
} OpeningCutsceneCue;

typedef struct
{
    float time;
} OpeningCutscene;

typedef enum
{
    LEVEL_TRANSITION_CUE_STEP_A = 1u << 0,
    LEVEL_TRANSITION_CUE_STEP_B = 1u << 1,
    LEVEL_TRANSITION_CUE_DOOR_OPEN = 1u << 2,
    LEVEL_TRANSITION_CUE_DOOR_CLOSE = 1u << 3
} LevelTransitionCue;

typedef struct
{
    float time;
    float elapsed_seconds;
    int completed_level;
    int next_level;
    int level_score;
    int hostiles_neutralized;
} LevelTransition;

typedef enum
{
    OUTRO_CUE_DOOR = 1u << 0,
    OUTRO_CUE_STEP_A = 1u << 1,
    OUTRO_CUE_STEP_B = 1u << 2,
    OUTRO_CUE_HELICOPTER = 1u << 3,
    OUTRO_CUE_PLAYER_SHOT = 1u << 4,
    OUTRO_CUE_ENEMY_DOWN = 1u << 5,
    OUTRO_CUE_EXPLOSION = 1u << 6,
    OUTRO_CUE_WIN = 1u << 7
} OutroCutsceneCue;

typedef struct
{
    float time;
} OutroCutscene;

void opening_cutscene_init(OpeningCutscene *cutscene);

/*
 * Advances the cinematic, reports every sound cue crossed during this frame,
 * and returns true once it has reached its final fade. The caller owns audio
 * playback and the transition to the title screen.
 */
bool opening_cutscene_update(OpeningCutscene *cutscene, float dt,
                             Uint32 *out_cues);

void opening_cutscene_render(SDL_Renderer *renderer,
                             const OpeningCutscene *cutscene,
                             int win_w, int win_h);

/*
 * The between-level report and pursuit vignette share the opening's
 * procedural characters and cinematic presentation.
 */
void level_transition_init(LevelTransition *transition,
                           int completed_level, int next_level,
                           float elapsed_seconds, int level_score,
                           int hostiles_neutralized);
bool level_transition_update(LevelTransition *transition, float dt,
                             Uint32 *out_cues);
void level_transition_render(SDL_Renderer *renderer,
                             const LevelTransition *transition,
                             int win_w, int win_h);

/*
 * Final rooftop rescue. The scene remains on its thank-you frame after the
 * animation finishes so the player can take in the ending or replay it.
 */
void outro_cutscene_init(OutroCutscene *cutscene);
void outro_cutscene_update(OutroCutscene *cutscene, float dt,
                           Uint32 *out_cues);
void outro_cutscene_render(SDL_Renderer *renderer,
                           const OutroCutscene *cutscene,
                           int win_w, int win_h);

#endif /* CHUCK_CUTSCENE_H */
