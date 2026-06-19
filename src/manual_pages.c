#include "manual_pages.h"

#include "game_config.h"

#include <string.h>

/* ---- The sheets ------------------------------------------------------- */

static const ManualLine PAGE_NIGHT[] = {
    {LINE_HEAD, "WHO YOU ARE"},
    {LINE_BODY, "Chuck Ross. Twelve years an Army sapper,"},
    {LINE_BODY, "now a two-man strip-out crew: you know"},
    {LINE_BODY, "charges, and which walls were bricked up."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "WHO THEY TOOK"},
    {LINE_BODY, "Ellen Ross. Your wife, and the night duty"},
    {LINE_BODY, "controller of Kessler Tower - she wrote"},
    {LINE_BODY, "its access system."},
    {LINE_BULLET, "Taken three blocks out and walked in"},
    {LINE_BODY, "the front door at 00:22, so it logged"},
    {LINE_BODY, "as normal."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "WHAT THIS ACTUALLY IS"},
    {LINE_BULLET, "Anton Voss and twelve men, badged in as"},
    {LINE_BODY, "the night maintenance contractor."},
    {LINE_BULLET, "The 00:04 demand is theatre: every unit"},
    {LINE_BODY, "on the cordon, nobody in the building."},
    {LINE_BULLET, "Six hundred and forty million in bearer"},
    {LINE_BODY, "bonds, and at 01:00 it leaves by air."},
    {LINE_BULLET, "The sub-vault runs on seven locks. The"},
    {LINE_BODY, "seventh needs the bank's key and the duty"},
    {LINE_BODY, "controller, alive and present. That is"},
    {LINE_BODY, "the only reason she still is."},
    /* The docket is on this sheet rather than on THE JOB because it is not a
     * rule of the sectors — it changes nothing about how one is played. It is
     * the answer to the paragraph above it: the city believes the broadcast,
     * and this is the only thing in the building that says otherwise. */
    {LINE_BULLET, "Their own DOCKET is loose in the tower,"},
    {LINE_BODY, "a sheet to a floor. Nobody outside has"},
    {LINE_BODY, "any of it, and the city believes them."},
};

/*
 * The sheet the net is explained on. It earns its place because the traffic
 * along the top of the screen is the one system in the game the player meets
 * without being told it exists: a line appears, a name is on it, and nothing
 * anywhere else says why. Everything on this sheet is the same twelve men the
 * strip is quoting.
 */
static const ManualLine PAGE_CREW[] = {
    {LINE_HEAD, "TWELVE MEN, ONE DOCKET"},
    {LINE_BODY, "Badged in on 14 March as the night"},
    {LINE_BODY, "maintenance contractor. Nobody has"},
    {LINE_BODY, "opened a case of theirs since."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "ANTON VOSS"},
    {LINE_BODY, "Not a soldier. He reads the settlement"},
    {LINE_BODY, "clock and nothing else, and he is on"},
    {LINE_BODY, "the roof at 00:57. Not before."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "THEIR NET"},
    {LINE_BULLET, "Alone, a guard calls in. Stand near"},
    {LINE_BODY, "enough and the strip prints what he"},
    {LINE_BODY, "said, and who said it."},
    {LINE_BULLET, "Two together talk instead. That is four"},
    {LINE_BODY, "seconds of a man facing the wrong way."},
    {LINE_BULLET, "A pulled alarm is shouted, not filed."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "THE TALLY"},
    {LINE_BULLET, "They count each other all night. They"},
    {LINE_BODY, "run out of names long before this"},
    {LINE_BODY, "building runs out of floors."},
};

static const ManualLine PAGE_MISSION[] = {
    {LINE_HEAD, "THE JOB"},
    {LINE_BODY, "Seventeen sectors, the lobby to the roof,"},
    {LINE_BODY, "and nobody else coming up after her."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "EVERY SECTOR IS THE SAME SHAPE"},
    {LINE_BULLET, "One security door leads up, and it"},
    {LINE_BODY, "starts LOCKED."},
    {LINE_BULLET, "Two ways to open it: pick up the one"},
    {LINE_BODY, "real KEY CARD, or hold $Y at the live"
                "|real KEY CARD, or hold E at the live"},
    {LINE_BODY, "TERMINAL for four seconds."},
    {LINE_BULLET, "Cards lie. A wrong one still scores and"},
    {LINE_BODY, "banks progress, but opens nothing."},
    {LINE_BULLET, "Hacking wakes the building: guards are"},
    {LINE_BODY, "sent to the terminal you used."},
    /* "No door at all" was the shorter sentence and the wrong one: the door is
     * there, welded, and the strip beside it reads BLOCKED rather than LOCKED
     * for exactly that reason. A player who goes looking for a door that the
     * manual says does not exist has been told the wrong thing about the room
     * they are standing in.
     *
     * The count is five and not four because sector 14 joined them when sector
     * 15 became a climb: a climb is entered through a window, so the floor below
     * one is a floor whose stair core is welded, whatever else it holds. It is
     * the only one of the five that still lays out cards and terminals — they
     * score and bank a checkpoint in any sector, which is what the "cards lie"
     * bullet three rows up is already telling the player. */
    {LINE_BULLET, "In five sectors the stair door is"},
    {LINE_BODY, "welded shut. The way on is the open"},
    {LINE_BODY, "WINDOW, out onto the wall itself."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "STAYING ALIVE"},
    {LINE_BULLET, "Three hearts a life. A hit costs one,"},
    {LINE_BODY, "a blast two; falls and crushes cost"},
    {LINE_BODY, "everything. Medkits refill the hearts."},
    {LINE_BULLET, "Cards, terminals and doors bank your"},
    {LINE_BODY, "progress. A lost life resumes there."},
    {LINE_BULLET, "Out of lives, a CONTINUE retries the"},
    {LINE_BODY, "same sector. The score survives three"},
    {LINE_BODY, "of those, then resets - the run never"},
    {LINE_BODY, "goes back to the lobby."},
};

/*
 * The pad column is written in `$` tokens rather than in letters, because the
 * letter is a property of the pad and not of the game: `$A` is whatever the
 * thing in the player's hands prints on its confirm button, which is A on an
 * Xbox pad, A on a Switch pad (the one on the *right*) and a cross on a
 * PlayStation. With nothing plugged in the sheet falls back to the Xbox
 * lettering it has always shown.
 */
static const ManualLine PAGE_CONTROLS[] = {
    {LINE_HEAD, "IN THE SECTORS"},
    {LINE_KEY, "WASD/ARROWS|LS/DPAD|MOVE - CLIMB - AIM"},
    {LINE_KEY, "W/UP/LSHIFT|$A|JUMP, LSHIFT ON LADDERS"},
    {LINE_KEY, "S or DOWN|DPAD|CRAWL"},
    {LINE_KEY, "SPACE|$B $X|ATTACK"},
    {LINE_KEY, "TAB / Q / Z|$LB $RB|CYCLE WEAPON, Z BACK"},
    {LINE_KEY, "E|$Y|USE DOOR / HOLD TO HACK"},
    /*
     * The sheet names the keys the game ships with, and the options sheet is
     * where they are actually decided — so it has to say so, or this becomes
     * the one page in the game that can be wrong about its own controls.
     *
     * Naming them here dynamically is what this row replaces, and it is not a
     * shortcut: the movement line above is four separate actions in one row,
     * and two six-character key names per action do not go in a column that is
     * eleven cells wide. What can name a rebound key is the prompt drawn over
     * the sector, which does; what cannot is a fixed table, which now points at
     * the thing that can.
     */
    /* No gap between the two sections either, for the reason the third heading
     * has never had one: this sheet is the tightest in the book and the
     * heading's own lead already separates it. It went over the bottom of the
     * text column once — silently, because the layout loop stops at
     * BODY_BOTTOM rather than drawing past it, so the line that fell off was
     * simply never seen. */
    {LINE_HEAD, "ANYWHERE"},
    {LINE_KEY, "ENTER|$A|CONFIRM - SKIP"},
    {LINE_KEY, "ESC|$START|PAUSE - RESUME"},
    {LINE_KEY, "BACKSPACE|$B|BACK OUT OF WHAT IS OPEN"},
    /* The action column starts 230px in and the sheet's text column ends at
     * TEXT_RIGHT, which leaves 24 cells of the 8x8 font. This row was the one
     * that spent 25 of them and hung out past the rules under every heading. */
    {LINE_KEY, "Q|$SELECT|ABANDON RUN, PRESS TWICE"},
    /* The other reading of the same button, and the two never overlap: one is
     * the way out of a paused run, this is the way back into an unpaused one.
     * A screen that answers a button nothing names is the same bug as a prompt
     * naming a button that does nothing, so it is named here. */
    {LINE_KEY, "R|$SELECT|RESUME FROM TITLE SCREEN"},
    /* The sector rows above name the keys the game ships with, and this is the
     * sheet where they stop being the keys the game ships with — so the row
     * that opens it has to say so. It is said here rather than up there
     * because that section is already at the bottom of the column: the
     * movement line is four actions in one row, and no fixed table can print
     * two six-character key names for each of them in eleven cells. The prompt
     * drawn over the sector is what names a rebound key while it is being
     * played, and it does. */
    {LINE_KEY, "J|$X|OPTIONS AND REBINDING"},
    /*
     * The one control that ends the session, and it earned a row of its own:
     * it spent a long time as the last bullet of this sheet, which is the
     * line the overflow above was quietly eating — so the only place the game
     * ever said how to close itself was the one line nobody could read.
     *
     * The pad holds B rather than pressing it, and that is the whole reason a
     * pad can quit at all. Every letter on the title screen is spoken for — A
     * starts, X and Y open the two sheets, SELECT takes the resume — and B is
     * the one left, which is also the button a thumb presses to back out of
     * the manual. A press of it landing on the title screen would close the
     * game on the rebound; a hold cannot be given by accident.
     */
    {LINE_KEY, "ESC|HOLD $B|CLOSE THE GAME (TITLE)"},
    /* The two keyboard-only toggles share a row, which is where the line for
     * the one above came from. They belong together: both are single keys with
     * no pad binding at all, because a pad reaches fullscreen and both volume
     * levels through the options sheet, and neither is a setting worth a row
     * apiece on the tightest sheet in the book. */
    {LINE_KEY, "F / M|-|FULLSCREEN - MUTE ALL"},
    /* No gap before the last heading: see the note on the first one. */
    {LINE_HEAD, "TWO THINGS WORTH KNOWING"},
    /* Both of these name buttons, so both carry the pad's wording and the
     * keyboard's. Written flat they spelled SPACE and UP at the one reader
     * with neither — the same thing the `$` tokens exist to prevent, said in
     * prose instead of in a row. */
    {LINE_BULLET, "$B attacks and $A jumps. Not one button.|"
                  "SPACE attacks and UP jumps. Not one key."},
    {LINE_BULLET, "On a ladder, the stick aims the shot.|"
                  "On a ladder, UP and DOWN aim the shot."},
};

static const ManualLine PAGE_MOVEMENT[] = {
    {LINE_HEAD, "GROUND"},
    /* The two plentiful answers only. A moving platform is a third one on
     * paper and a rarity in the building — sectors 5, 14 and 17, a tile each —
     * so listing it beside the ladder and the lift shaft told the player to
     * look for something that is almost never there. It keeps its mention
     * under GOING UP, worded as the rarity it is. */
    /* Two, not one, and the sheet said one for as long as it existed.
     *
     * A hole is landed *past* rather than cleared: `level_move` holds the box
     * up while any part of it is over solid tile, so the ground a gap asks for
     * is its width less `PLAYER_W`. The authoring page had the same arithmetic
     * a tile short in the same direction — see the jump-reach bullet in
     * [levels/LEGEND.md](../levels/LEGEND.md) — and this is the copy a player
     * reads, so it was the one telling somebody to go and find a ladder they do
     * not need. `test_a_jump_clears_a_wider_hole_than_the_model_will_route`
     * measures both widths.
     *
     * The third tile needs a second open row over the jump, which is the
     * difference between a corridor and a hall; "with headroom" is that
     * condition in the words a player can act on while standing in one. */
    {LINE_BULLET, "A jump clears a two-tile hole in the"},
    {LINE_BODY, "floor - three with headroom. Wider"},
    {LINE_BODY, "needs a ladder or a lift shaft."},
    {LINE_BULLET, "Hold DOWN to crawl. It is the only way"},
    {LINE_BODY, "under a one-tile gap, and the only way"},
    {LINE_BODY, "to hit something sitting on the floor."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "GOING UP"},
    {LINE_BULLET, "LADDERS: press UP or DOWN to grab one."},
    {LINE_BODY, "You can shuffle sideways on the rungs."},
    {LINE_BULLET, "LIFT SHAFTS carry you up and down, and"},
    {LINE_BODY, "so does the odd moving platform."},
    {LINE_BULLET, "CRACKED PANELS hold for a moment, then"},
    {LINE_BODY, "drop. They stay gone for the run."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "THINGS IN THE WAY"},
    {LINE_BULLET, "CRATES shove along the floor and make a"},
    {LINE_BODY, "step. Shots and blasts break them."},
    {LINE_BULLET, "SPIKES and CEILING FANS cost a heart"},
    {LINE_BODY, "and shove you back off them."},
    {LINE_BULLET, "PAIRED DOORS link up: stand in one and"},
    {LINE_BODY, "press $Y to come out of the other."
                "|press E to come out of the other."},
    /* The grenade is named because the campaign's own explosive budget counts
     * on it: four of the twenty a run can hold come out of these four doors.
     * Advertised as a medkit alone, the detour read as nothing at all to a
     * player whose hearts were full — which is most players most of the time,
     * and exactly the ones the balance assumes took it. */
    {LINE_BULLET, "A restroom door is a room of its own,"},
    {LINE_BODY, "with a medkit and a grenade up top. What"},
    {LINE_BODY, "you carry in comes back out with you."},
};

static const ManualLine PAGE_COMBAT[] = {
    {LINE_HEAD, "WHAT YOU CARRY"},
    {LINE_BODY, "Past the pistol, it is all theirs."},
    /* PLAYER_KNIFE_RANGE is 18px past the body, which is a good half tile
     * short of the tile the sheet used to promise. Named rather than measured,
     * because the number is the wrong unit for the thing being described. */
    {LINE_BULLET, "KNIFE: always with you, arm's length,"},
    {LINE_BODY, "and it makes no noise."},
    {LINE_BULLET, "PISTOL: six rounds, and a shot carries"},
    {LINE_BODY, "until it hits something. Ammo lies"},
    {LINE_BODY, "around, respawns, and the dead drop it."},
    {LINE_BULLET, "GRENADE: one at a time. Short fuse, it"},
    {LINE_BODY, "bounces, and it does not pick sides."},
    {LINE_BULLET, "BAZOOKA: one rocket, even sectors only."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "WHAT THEY DO"},
    /* "and nothing behind it" was the shorter sentence and a false one: a
     * guard's peripheral radius and the lane he covers straight up and down his
     * own column are both read without any facing test at all
     * (`enemy_sees_point`), so he notices whatever is right beside him and
     * whatever is on the ladder over his head. The caption still says where the
     * safe ground is; a rule stated more absolutely than it is kept is worse
     * than one stated loosely, because the next reader trusts it. */
    {LINE_BULLET, "A GUARD sees a cone seven tiles ahead,"},
    {LINE_BODY, "his own column up and down, and what is"},
    {LINE_BODY, "right beside him. His back is the rest."},
    /* The shot back is here because a stomp is the answer a player reaches for
     * the moment a floor gets busy, and until it was wired up it cost nothing
     * at all: a provoked guard aimed flat, and a flat round leaves the muzzle
     * at chest height while the boot that started it is two tiles over his
     * helmet. It costs a heart now, and a rule learned only by losing one is a
     * rule the game never taught. */
    {LINE_BULLET, "Land on a guard's head to knock him"},
    {LINE_BODY, "down and bounce clear - then he fires"},
    {LINE_BODY, "straight up. Other contact costs a heart."},
    /*
     * One row for the two kinds a boot bounces off, and one for what the vest
     * is actually worth.
     *
     * FIGHTING is the longest sheet in the book and its last row once put ink
     * three pixels under the column, silently, because the layout loop stops at
     * BODY_BOTTOM rather than drawing past it. So anything that joins it has to
     * pay for itself out of the rows above — which is where the dogs' "faster
     * and lower" went, and a line of the pistol's with it. The fit check is the
     * only reason either edit is known to be enough.
     */
    {LINE_BULLET, "DOGS and HEAVIES cannot be stomped, and"},
    {LINE_BODY, "a heavy takes twice the rounds."},
    {LINE_BULLET, "A guard who has seen you may run for a"},
    {LINE_BODY, "wall ALARM and wake the floor."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "BLASTS"},
    {LINE_BULLET, "Crawl and shoot a GAS CANISTER. A"},
    {LINE_BODY, "standing shot goes straight over it."},
    {LINE_BULLET, "A blast opens a blocked-up patch of"},
    {LINE_BODY, "wall for good, and that can be a route."},
};

/*
 * The ninth sheet, and the one the book was missing rather than the one it grew.
 *
 * Three things — the blade behind an unaware man, a bolt thrown to be heard
 * somewhere Chuck is not, and a body hauled out of the room it fell in — are
 * all answers to the *same* rule, which is that this building's guards read
 * what they see and hear. None of the three is announced anywhere on screen
 * except the drag's own prompt, and a mechanic nobody is told about is a
 * mechanic that does not exist for most of the people playing.
 *
 * They are a sheet of their own rather than three more bullets on FIGHTING for
 * an arithmetic reason as well as an editorial one: FIGHTING is the longest of
 * the sheets and has already lost a line to the column once. See the note there.
 */
static const ManualLine PAGE_QUIET[] = {
    {LINE_HEAD, "THE BLADE"},
    {LINE_BULLET, "Behind a man who has not seen you, the"},
    {LINE_BODY, "KNIFE takes him in one - and the man he"},
    {LINE_BODY, "was talking to hears nothing of it."},
    {LINE_BULLET, "Facing you it is three, and he shoots."},
    {LINE_BULLET, "A DOG has no back. It hears you. A"},
    {LINE_BODY, "HEAVY has one, and the vest is no help."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "BOLTS"},
    {LINE_BULLET, "A pocketful, and they never run out."},
    {LINE_BULLET, "Thrown, one makes a noise where it lands"},
    {LINE_BODY, "and calm guards walk to that, not you."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "THE BODY"},
    {LINE_BULLET, "Whoever finds one goes looking - often"},
    {LINE_BODY, "for a wall ALARM."},
    /* The one line on this sheet that names a rebindable key, and it takes the
     * same bar the other prose does: the pad reader is told to hold the button
     * the prompt in the sector is about to name at them. */
    {LINE_BULLET, "Hold $Y to drag it off|Hold USE to drag it off"},
    {LINE_BODY, "at half speed. No ladders, and where"},
    {LINE_BODY, "you let go is where it stays."},
    {LINE_GAP, NULL},
    {LINE_GAP, NULL},
    /* One heading for the two things that are not about staying unseen: the
     * fitting none of the above works on, and the charge for when none of the
     * above worked. They were two sections until the sheet ran out of column —
     * `test_manual_sheets_fit_the_column` is what says so, and a heading is the
     * most expensive row in the layout language. */
    {LINE_HEAD, "WHEN IT GOES WRONG"},
    {LINE_BULLET, "A CAMERA has no back and no ears, and"},
    {LINE_BODY, "it looks down - crawling is no help."},
    {LINE_BODY, "It sweeps: cross while it points away."},
    {LINE_BULLET, "A FLASH kills nobody. Everyone near it"},
    {LINE_BODY, "stops seeing for a few seconds, then"},
    {LINE_BODY, "carries on knowing what they knew."},
};

static const ManualLine PAGE_CLIMB[] = {
    {LINE_HEAD, "OUT THERE"},
    {LINE_BULLET, "No gravity and no ladders: you move"},
    {LINE_BODY, "four ways across the brickwork."},
    {LINE_BULLET, "Stone CORNICES are in the way, and they"},
    {LINE_BODY, "are also the only cover on the wall."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "WHAT THE WALL THROWS"},
    {LINE_BULLET, "WIND: a howl warns you, then the gust"},
    {LINE_BODY, "shoves. Get masonry upwind of you and"},
    {LINE_BODY, "it passes over."},
    {LINE_BULLET, "THROWERS lean out of a window and shout"},
    {LINE_BODY, "before they let go. A cornice between"},
    {LINE_BODY, "you shatters the brick."},
    {LINE_BULLET, "BIRDS cross at you. Masonry breaks them"},
    {LINE_BODY, "off as well."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "HEIGHT IS KEPT"},
    {LINE_BULLET, "Every three tiles of climb is banked."},
    {LINE_BULLET, "A lost life resumes at the last bank,"},
    {LINE_BODY, "not down on the pavement."},
    {LINE_BULLET, "Pickups out here are real detours: the"},
    {LINE_BODY, "loadout carries into the next sector."},
    {LINE_BULLET, "The way back inside is the WINDOW."},
};

static const ManualLine PAGE_CONSOLE[] = {
    {LINE_HEAD, "THE STRIP"},
    {LINE_BULLET, "VITAL: the hearts left in this life, and"},
    {LINE_BODY, "the lives left in the run beside them."},
    {LINE_BULLET, "The weapon named is the one the next"},
    {LINE_BODY, "attack uses; the rounds sit beside it."},
    {LINE_BULLET, "ACCESS: LOCKED until a card or a"},
    {LINE_BODY, "terminal says otherwise. BLOCKED means"},
    /* The same correction THE MISSION already carries, and it was missed here:
     * the door is there, welded, which is the whole reason the chip reads
     * BLOCKED rather than LOCKED. Two sheets of one book disagreeing about the
     * room the player is standing in is worse than either wording alone. */
    {LINE_BODY, "the stair door is welded -- the way"},
    {LINE_BODY, "on is the window."},
    {LINE_BULLET, "SCORE counts up beside the sector, and"},
    {LINE_BODY, "every 10000 points is a spare life."},
    {LINE_BULLET, "TRAIL is idle chatter. It becomes a red"},
    {LINE_BODY, "ALERT countdown while the building is"},
    {LINE_BODY, "actively looking for you."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "WHAT TO PICK UP"},
    {LINE_BULLET, "KEY CARD: one per sector is real."},
    {LINE_BULLET, "AMMO: fills the pistol back to six."},
    {LINE_BULLET, "MEDKIT: refills the hearts, or adds a"},
    {LINE_BODY, "life if they are already full."},
    /* All three carried things share the rule, so they share the bullet:
     * picking one up does not raise it. The sheet has no room to say it three
     * times, and the page clips at BODY_BOTTOM rather than reflowing, so this
     * section cannot grow — which is how the flash charge came to be left off
     * the one list in the book that says what is worth picking up, while the
     * sheet before this one teaches what it does. */
    {LINE_BULLET, "GRENADE, ROCKET and FLASH: one each, and"},
    {LINE_BODY, "none arms itself -- switch to it first."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "ON THE WALL"},
    {LINE_BULLET, "The climb has a strip of its own: height"},
    {LINE_BODY, "made, the wind, and what you carry. The"},
    {LINE_BODY, "amber mark on the bar is the last bank."},
};

/*
 * The sheet the records are read on, and the reason it is a sheet at all.
 *
 * `Progress` keeps four things and the game showed three of them only on the
 * card a *losing* run ends on, while the fourth — the per-sector best — was
 * visible for the one screen after the sector that set it and nowhere else ever
 * again. Seventeen numbers on the player's own disk, one readable at a time,
 * under a fiction that will not stop talking about the clock. The words here are
 * what the numbers mean; the grid of them is the illustration beside this text,
 * which reads `Progress` through `ManualRecords`.
 *
 * It is the last sheet in the sheaf on purpose: it is the only one that is about
 * the player rather than about the building, so it is what the book ends on
 * rather than something in the way of the briefing.
 */
static const ManualLine PAGE_RECORD[] = {
    {LINE_HEAD, "WHAT IS KEPT"},
    {LINE_BULLET, "The best score any run has finished on,"},
    {LINE_BODY, "and the most of the docket any single"},
    {LINE_BODY, "night has come away with."},
    {LINE_BULLET, "The quickest each sector has ever been"},
    {LINE_BODY, "cleared. A sector nobody has finished"},
    {LINE_BODY, "reads --:--."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "WHAT THE CLOCK PAYS"},
    {LINE_BULLET, "The night gives every sector the same"},
    {LINE_BODY, "slot. Finish inside it and the seconds"},
    {LINE_BODY, "handed back are paid as points."},
    {LINE_BULLET, "Clearing one without dying pays again."},
    {LINE_GAP, NULL},
    {LINE_HEAD, "WHAT IS NOT KEPT"},
    {LINE_BULLET, "A run with any ASSIST switch on banks"},
    {LINE_BODY, "no score, no time and no sheets. It is"},
    {LINE_BODY, "still your run; it is not a record."},
    {LINE_BULLET, "The VETERAN run does count."},
    {LINE_BULLET, "OPTIONS can clear all of it and keeps"},
    {LINE_BODY, "the sector you are resuming from."},
};

#define PAGE(lines) lines, (int)(sizeof(lines) / sizeof((lines)[0]))

const ManualPageText MANUAL_PAGES[] = {
    {"THE NIGHT", "KESSLER TOWER, 00:22, AND A CONTRACTOR NOBODY CHECKED",
     "WHAT THEY WHEELED IN AS TOOLS", PAGE(PAGE_NIGHT)},
    /* Straight after the night itself, because it is the same page seen from
     * the other side: who wheeled the cases in, and what they call each other
     * while the player is walking past them. */
    {"THE CREW", "TWELVE MEN ON ONE CONTRACTOR'S DOCKET",
     "NIGHT ACCESS LOG, SIGNED 14 MARCH", PAGE(PAGE_CREW)},
    {"THE MISSION", "SEVENTEEN SECTORS BETWEEN THE LOBBY AND THE ROOF",
     "FIVE OF THEM ARE ON THE OUTSIDE", PAGE(PAGE_MISSION)},
    {"CONTROLS", "KEYBOARD AND GAMEPAD ARE BOTH ALWAYS LIVE",
     "PICK ONE UP AND THE HINTS FOLLOW", PAGE(PAGE_CONTROLS)},
    /* The caption is a claim about the simulation rather than a label on the
     * picture — the distinction `MANUAL_SIGHT_CONE_LABEL` is already drawn on —
     * so it is held: `test_the_on_foot_sheet_spells_the_jump_it_draws`
     * measures the two widths off the body and requires this line to spell
     * them, with no list anywhere of which string is right. It read ONE and TWO, which is the same tile the bullet inside the
     * sheet was short by and the same tile `levels/LEGEND.md` was short by —
     * the fix landing on the words while the picture beside them kept the old
     * number is this file's own recurring shape, and it happened here.
     *
     * Two and four rather than two and three because both halves have to be
     * true in any room: three tiles is a jump only with a second open row over
     * it, and that condition belongs in the bullet, where there is room to say
     * it. Four is a ladder everywhere. */
    {"ON FOOT", "WHAT THE FLOOR PLAN WILL AND WILL NOT ALLOW",
     "TWO TILES IS A JUMP. FOUR IS A LADDER", PAGE(PAGE_MOVEMENT)},
    {"FIGHTING", "NOTHING IN THIS BUILDING IS FRIGHTENED OF YOU",
     "BEHIND HIM IS THE SAFEST PLACE", PAGE(PAGE_COMBAT)},
    {"GOING QUIET", "THE OTHER WAY THROUGH A FLOOR FULL OF THEM",
     "A ROOM THAT NEVER KNEW YOU WERE IN IT", PAGE(PAGE_QUIET)},
    {"THE CLIMB", "SECTORS 3, 7, 11, 13 AND 15 ARE CLIMBED, NOT WALKED",
     "THE WIND ANNOUNCES ITSELF FIRST", PAGE(PAGE_CLIMB)},
    {"THE CONSOLE", "READING THE STRIP ALONG THE TOP OF THE SCREEN",
     "LOCKED, GRANTED, BLOCKED", PAGE(PAGE_CONSOLE)},
    {"THE RECORD", "WHAT THE GAME REMEMBERS AFTER THE WINDOW CLOSES",
     "EVERY SECTOR, AND THE BEST IT HAS SEEN", PAGE(PAGE_RECORD)},
};

_Static_assert(sizeof(MANUAL_PAGES) / sizeof(MANUAL_PAGES[0]) ==
                   (size_t)MANUAL_PAGE_COUNT,
               "every sheet in the enum needs its words here");

/*
 * The climbs, for the sheet that both says them and draws them.
 *
 * `THE CLIMB`'s strap spells these numbers out and `THE MISSION`'s illustration
 * marks them in amber up the side of the tower. The drawing used to carry its own
 * copy, hard-coded, and that copy went stale silently — a strap can be measured
 * against its column and a picture cannot be measured against anything, so the
 * list has to come from one place that a test can reach. See the note in
 * [manual_pages.h](manual_pages.h).
 */
const int CAMPAIGN_CLIMB_SECTORS[] = {3, 7, 11, 13, 15};
const int CAMPAIGN_CLIMB_SECTOR_COUNT =
    (int)(sizeof(CAMPAIGN_CLIMB_SECTORS) / sizeof(CAMPAIGN_CLIMB_SECTORS[0]));

int campaign_docket_sheets_by(int sector)
{
    if (sector < 1)
        return 0;
    if (sector > CAMPAIGN_SECTORS)
        sector = CAMPAIGN_SECTORS;
    int climbs = 0;
    for (int i = 0; i < CAMPAIGN_CLIMB_SECTOR_COUNT; ++i)
        if (CAMPAIGN_CLIMB_SECTORS[i] <= sector)
            ++climbs;
    return sector - climbs;
}

int campaign_docket_sheets(void)
{
    /* The whole campaign is the general answer asked about the last sector, so
     * the two share one rule rather than agreeing by arithmetic. */
    return campaign_docket_sheets_by(CAMPAIGN_SECTORS);
}

/* ---- Does it fit? ----------------------------------------------------- */

/*
 * Both checks walk the kinds in the same order and off the same pitches the
 * renderer does, on purpose: two loops that disagree about where a line lands
 * would certify a sheet the frame then clips.
 *
 * Where the vertical one deliberately asks *more* than the renderer is the ink.
 * `render_text_column` clips on `y < MANUAL_BODY_BOTTOM`, which is a question
 * about where a line starts, so a line beginning a pixel above the bottom is
 * drawn whole below it — up to `MANUAL_INK_KEY` past, because a keycap is drawn
 * from above its own row and is eighteen pixels tall. Read that way the check
 * would wave through a control row sitting in the footer chips, which is the
 * same class of silent failure as the bullet `CONTROLS` used to lose off the
 * bottom, only upside down. So each kind is measured by what it actually puts
 * on the sheet, and the renderer's clip goes back to being what it is: a
 * backstop that drops a line rather than drawing it off the plate.
 */
bool manual_page_lines_fit(const ManualPageText *page)
{
    float y = MANUAL_BODY_Y;
    bool first_head = true;
    for (int i = 0; i < page->line_count; ++i)
    {
        float ink = 0.0f;
        float pitch = 0.0f;
        switch (page->lines[i].kind)
        {
        case LINE_HEAD:
            if (!first_head)
                y += MANUAL_HEAD_LEAD;
            first_head = false;
            ink = MANUAL_INK_HEAD;
            pitch = MANUAL_HEAD_PITCH;
            break;
        case LINE_KEY:
            ink = MANUAL_INK_KEY;
            pitch = MANUAL_KEY_PITCH;
            break;
        case LINE_GAP:
            /* A gap is space and nothing else, so it lays down no ink and can
             * legitimately be the thing that carries a sheet to its bottom. */
            pitch = MANUAL_GAP_PITCH;
            break;
        case LINE_BULLET:
        case LINE_BODY:
            ink = MANUAL_INK_TEXT;
            pitch = MANUAL_LINE_PITCH;
            break;
        }
        if (y + ink > MANUAL_BODY_BOTTOM)
            return false;
        y += pitch;
    }
    return true;
}

/* Field `index` of a bar-separated row, as a length in cells. */
static int field_length(const char *text, int index)
{
    const char *start = text;
    for (int field = 0; field < index; ++field)
    {
        const char *bar = strchr(start, '|');
        if (bar == NULL)
            return 0;
        start = bar + 1;
    }
    const char *end = strchr(start, '|');
    return end != NULL ? (int)(end - start) : (int)strlen(start);
}

int manual_line_widest_form(const char *text)
{
    if (text == NULL)
        return 0;
    const char *bar = strchr(text, '|');
    if (bar == NULL)
        return (int)strlen(text);
    int first = (int)(bar - text);
    int second = (int)strlen(bar + 1);
    return first > second ? first : second;
}

bool manual_page_lines_fit_width(const ManualPageText *page,
                                 float key_column_w, float pad_column_w)
{
    for (int i = 0; i < page->line_count; ++i)
    {
        const ManualLine *line = &page->lines[i];
        float right = 0.0f;
        switch (line->kind)
        {
        case LINE_GAP:
            continue;
        case LINE_HEAD:
            /* Letterspaced, and set flush with the column rather than indented
             * with the prose under it. */
            right = MANUAL_TEXT_X + (MANUAL_CH + MANUAL_HEAD_TRACK) *
                                        (float)manual_line_widest_form(line->text);
            break;
        case LINE_BULLET:
        case LINE_BODY:
            right = MANUAL_TEXT_X + MANUAL_BULLET_INDENT +
                    MANUAL_CH * (float)manual_line_widest_form(line->text);
            break;
        case LINE_KEY:
            /* The action column starts past both chips, whose widths belong to
             * the sheet and to the pad rather than to this row. */
            right = MANUAL_TEXT_X + key_column_w + pad_column_w +
                    MANUAL_KEY_ACTION_GAP +
                    MANUAL_CH * (float)field_length(line->text, 2);
            break;
        }
        if (right > MANUAL_TEXT_RIGHT)
            return false;
    }
    return true;
}
