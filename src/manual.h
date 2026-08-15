#ifndef CHUCK_MANUAL_H
#define CHUCK_MANUAL_H

#include "common.h"
#include "pad_hint.h"

/* Which of the footer chips the pointer is over, if any. */
typedef enum
{
    MANUAL_HOT_NONE = 0,
    MANUAL_HOT_PREV,
    MANUAL_HOT_NEXT,
    MANUAL_HOT_BACK
} ManualHot;

typedef struct
{
    float time;   /* seconds since the manual was opened */
    int page;     /* 0 .. manual_page_count() - 1 */
    float settle; /* 1 -> 0 while a freshly turned sheet lands */
    int settle_dir;
    /* Footer chips, in logical render coordinates. They are laid out once per
     * frame in manual_update so the hit test and the drawing cannot disagree
     * about where they are. */
    SDL_FRect prev_chip;
    SDL_FRect next_chip;
    SDL_FRect back_chip;
    /* The way out, named for whatever is in the player's hands. Spelled once
     * per frame in manual_update, so the chip's width and its label are
     * decided by the same string. */
    char back_label[16];
    ManualHot hovered;
} Manual;

/* `pad` spells the control table and the way out for whatever is in the
 * player's hands; with none, the table keeps the Xbox lettering it is written
 * in and the way out is named for the keyboard. */
void manual_init(Manual *manual, int win_w, int win_h, const PadHints *pad);
void manual_update(Manual *manual, float dt, int win_w, int win_h,
                   float mouse_x, float mouse_y, const PadHints *pad);
void manual_render(SDL_Renderer *renderer, const Manual *manual,
                   int win_w, int win_h, const PadHints *pad);

/* Turn `delta` sheets, clamped at both ends. True when the page actually
 * changed, which is the shell's cue to sound the turn. */
bool manual_turn_page(Manual *manual, int delta);

/* Which chip (x, y) lands on, in logical render coordinates. */
ManualHot manual_hit_test(const Manual *manual, float x, float y);

int manual_page_count(void);

#endif /* CHUCK_MANUAL_H */
