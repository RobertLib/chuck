#ifndef CHUCK_DEMO_H
#define CHUCK_DEMO_H

#include "gameplay_state.h"
#include "player.h"

/*
 * A hand on the controls that is not a player's, and the coverage hole it was
 * written to fill.
 *
 * `make test` links no SDL and so reaches none of the renderers. `make smoke`
 * reaches them by booting the real binary — and it presses no key at all, which
 * is fine for a screen and useless for a *state*. `--scene` was added when that
 * distinction was first noticed and it only solved half of it: it can open the
 * report, the manual or a restroom, because each of those is a transition the
 * game makes on its own, but nothing in the tree could open a sector with a
 * body on the floor, an alarm ringing, a rocket in the air or Chuck on his
 * elbows. Measured with `llvm-cov` over a smoke run, twelve live drawing
 * functions were executed by nothing whatsoever:
 * `draw_downed_enemy`, `draw_downed_dog`, `draw_player_crawling`,
 * `draw_player_hacking`, `draw_bazooka_weapon`, `draw_vertical_bazooka_weapon`,
 * `draw_muzzle_flash`, `draw_rocket_sprite`, `draw_vertical_rocket_sprite`,
 * `vertical_rocket_rect`, `render_alarm_lighting` and
 * `heading_governs_the_levels` — the bodies the whole body-discovery rule is
 * built on among them. That is the same class of hole as the credits skyline,
 * the seven unread manual illustrations and the four restrooms, and it is the
 * fourth time this tree has found it.
 *
 * **So the fix is a hand rather than a state.** `--scene`'s rule is that every
 * name is a transition the game itself makes and never a state assembled by
 * hand, and this keeps it: the demo presses keys and the game does the rest, so
 * every state it reaches it reached by being played into. What it is not is an
 * attract mode or a replay — it does not need to look good and nothing asserts
 * on what it achieves. It needs to touch the controls a player touches, in a
 * sector the game loaded normally, so that ASan and UBSan get to watch the
 * drawing code run.
 *
 * The one thing it is handed rather than earning is the loadout, and that is
 * deliberate: a bazooka lies in one pocket of one map apiece and a scripted
 * hand that had to *find* one would be a pathfinder, would be level-specific,
 * and would silently stop covering the rocket the first time somebody moved a
 * pickup. What the run is for is the drawing, so the kit is granted and the
 * firing is played. See `demo_grant_loadout`.
 */

/* How the hand steers, which is the only decision in it that is not a clock. */
typedef struct
{
    /* Seconds since the sector opened, which is what indexes the script. */
    float time;
    /* Where the last steer sent it, so a sector with nothing to walk towards
     * still paces instead of standing still. */
    int wander_dir;
    float wander_timer;
} DemoHand;

void demo_hand_init(DemoHand *hand);

/*
 * One frame of the script, written into `input` in place of a player's.
 *
 * Every field is written, including the ones left false, because this replaces
 * a read of the keyboard rather than adding to it: a stale edge left over from
 * the frame before would be a press the script never made.
 */
void demo_hand_drive(DemoHand *hand, const GameplayState *state,
                     Input *input, float dt);

/*
 * The kit the hand is given on the way into a sector, for the reason above.
 * Separate from the driving so the shell can call it exactly where a real
 * arrival would have brought a loadout through the door.
 */
void demo_grant_loadout(GameplayState *state);

#endif /* CHUCK_DEMO_H */
