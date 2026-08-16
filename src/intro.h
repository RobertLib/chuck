#ifndef CHUCK_INTRO_H
#define CHUCK_INTRO_H

#include "common.h"
#include "game_config.h"
#include "pad_hint.h"

#define INTRO_STAR_COUNT 150

typedef struct
{
    float x, y;
    float speed; /* horizontal drift; faster stars form the near layer */
    float phase;
} IntroStar;

/* "RESUME SECTOR 15" and a terminator, with room to spare. */
#define INTRO_RESUME_LABEL_MAX 24

typedef struct
{
    float time; /* seconds elapsed since the intro screen appeared */
    IntroStar stars[INTRO_STAR_COUNT];
    SDL_FRect start_button;
    SDL_FRect manual_button;
    SDL_FRect options_button;
    /* The third chip only exists once a sector has been earned, so it has a
     * flag of its own rather than a zero-width rect: nothing may be hovered,
     * clicked or drawn when there is nothing to resume. */
    SDL_FRect resume_button;
    bool resume_offered;
    char resume_label[INTRO_RESUME_LABEL_MAX];
    /* The way out, and the last chip on the line for the same reason ABANDON
     * RUN is the last item on the pause sheet: it is the one thing here that
     * cannot be taken back. */
    SDL_FRect quit_button;
    /* Seconds B has been held, and the fill the chip draws. It decays as fast
     * as it grows, so letting go part way is an answer of its own. */
    float quit_hold;
    /* B has to be seen up before a hold counts. B is also what closes the
     * manual and the options sheet, both of which hand back to this screen —
     * so a thumb that stayed on the button after closing one would have
     * carried straight on into quitting the game, which is the exact accident
     * the hold was put here to prevent. Starts false, because `intro_init`
     * zeroes the struct and arrival is precisely when the button may already
     * be down. */
    bool quit_armed;
    bool start_hovered;
    bool manual_hovered;
    bool options_hovered;
    bool resume_hovered;
    bool quit_hovered;
} Intro;

/*
 * `pad` spells the chips' keycaps, and the row is measured *after* spelling,
 * because SELECT is six characters where R is one — the same rule the manual's
 * control table keeps. `resume_sector` is the 0-based sector the resume chip
 * offers, and 0 means there is nothing to offer and no chip.
 */
void intro_init(Intro *intro, int win_w, int win_h, const PadHints *pad,
                int resume_sector);
/*
 * `quit_held` is the pad's B, held. Returns true on the frame the hold has
 * gone the distance, which is the shell's cue to close the game; the keyboard
 * and the mouse never come through here, because ESC and the close box are
 * unambiguous and answer on the press.
 */
bool intro_update(Intro *intro, float dt, int win_w, int win_h, float mouse_x,
                  float mouse_y, const PadHints *pad, int resume_sector,
                  bool quit_held);
/* `pad` spells the prompts for whatever is in the player's hands, and is NULL
 * when that is the keyboard. */
void intro_render(SDL_Renderer *renderer, const Intro *intro,
                  int win_w, int win_h, const PadHints *pad);

/* True when (x, y), in logical render coordinates, lies within the START button. */
bool intro_hit_start_button(const Intro *intro, float x, float y);

/* The same, for the quieter plate that opens the field manual. */
bool intro_hit_manual_button(const Intro *intro, float x, float y);

/* And for its neighbour on the same line, the options sheet. */
bool intro_hit_options_button(const Intro *intro, float x, float y);

/* And for the resume chip, which answers nothing at all when the campaign has
 * never been past the lobby. */
bool intro_hit_resume_button(const Intro *intro, float x, float y);

/* And for the quit chip. A click is as deliberate as the window's own close
 * box, so the mouse needs no hold. */
bool intro_hit_quit_button(const Intro *intro, float x, float y);

#endif /* CHUCK_INTRO_H */
