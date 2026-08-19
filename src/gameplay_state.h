#ifndef CHUCK_GAMEPLAY_STATE_H
#define CHUCK_GAMEPLAY_STATE_H

#include "enemy.h"
#include "game_event.h"
#include "level.h"
#include "player.h"
#include "rng.h"

typedef struct
{
    float x, y;
    float vx, vy;
    bool active;
} Bullet;

typedef struct
{
    float x, y;
    float vx, vy;
    bool active;
    float timer;
    float fuse_sound_timer;
    bool grounded;
} Grenade;

typedef struct
{
    float x, y;
    float vx, vy;
    bool active;
} Rocket;

/*
 * A bolt in the air. It hurts nobody, breaks nothing and is gone the moment it
 * lands — what it leaves behind is a noise at the place it landed, which is the
 * whole of the thing.
 *
 * No fuse, no owner and no count: see the note beside `MAX_DECOYS`. It is a
 * separate array from the grenades rather than a flag on one, because every
 * loop that walks the grenades is a loop about explosives — `apply_blast`
 * chains them, the fuse ticks them, a blast spends them — and a bolt belongs to
 * none of it.
 */
typedef struct
{
    float x, y;
    float vx, vy;
    bool active;
} Decoy;

typedef struct
{
    float x, y;
    float vx, vy;
    float angle;
    int variant;
    bool active;
} ThrownObject;

typedef struct
{
    float x, y;
    float vx, vy;
    float anim_time;
    bool active;
} Bird;

/* Facade wind runs as one building-wide cycle: quiet, an announced warning,
 * then the gust itself. Keeping it a single phase machine means the renderer,
 * the HUD and the simulation all read the same state. */
typedef enum
{
    FACADE_WIND_CALM = 0,
    FACADE_WIND_WARNING,
    FACADE_WIND_GUSTING
} FacadeWindPhase;

typedef struct
{
    float x, y;
    bool active;
    bool triggered;
    float timer;
} Mine;

/*
 * What a ceiling camera is *doing*, as opposed to where it is.
 *
 * The position is the map's (`SecurityCamera` in level.h) and this is the
 * simulation beside it, one per map camera and indexed the same way. Three
 * fields and each is a different clock: `sweep` is the pass across the floor,
 * `notice` is how long it has had Chuck in the beam, and `suspicion` is how
 * long the lens goes on flashing after it has lost him — which exists purely so
 * a player who got clear in time can see that they nearly did not.
 *
 * `working` is the one that is not a clock, and it is what a bullet or a blast
 * takes away. A destroyed camera stays destroyed for the visit, exactly as a
 * broken weak wall stays open: a lost life resumes at a checkpoint rather than
 * reloading, so the sector keeps what the player did to it.
 */
typedef struct
{
    float sweep;
    float notice;
    float suspicion;
    bool working;
} CameraState;

/* A magazine shaken loose from a guard downed in direct combat. */
typedef struct
{
    float x, y;
    float vy;
    bool active;
} AmmoDrop;

typedef enum
{
    JANITOR_WALK,
    JANITOR_MOP,
    JANITOR_PAUSE
} JanitorActivity;

typedef struct
{
    float x, y;
    float life;
    bool active;
} JanitorWetSpot;

typedef struct
{
    float x, y;
    float vx, vy;
    int dir;
    /* The direction for which the cart is currently trailing. It can lag
     * behind dir while the janitor makes room to turn beside a wall. */
    int cart_dir;
    bool on_ground;
    JanitorActivity activity;
    float activity_timer;
    float anim_time;
    float wet_timer;
    int next_wet_spot;
    JanitorWetSpot wet_spots[JANITOR_WET_SPOTS];
} Janitor;

/* A civilian caught in the lobby when the crew walked Ellen through it. The
 * whole part is four beats long: freeze, run, trip, gone. */
typedef enum
{
    CIVILIAN_STARTLED = 0,
    CIVILIAN_FLEEING,
    CIVILIAN_STUMBLING,
    CIVILIAN_GONE
} CivilianActivity;

typedef struct
{
    float x, y;
    float vx, vy;
    int dir;      /* facing: the danger while startled, the way out after */
    int flee_dir; /* which way the way out lies, decided once at spawn */
    bool on_ground;
    CivilianActivity activity;
    float activity_timer;
    /* Counts down to this person's one trip; zero once it has been used or
     * was never rolled. */
    float stumble_timer;
    /* How long the run has been going nowhere. A civilian walled in by an
     * unlucky map leaves the shot rather than running on the spot. */
    float stuck_timer;
    float anim_time;
    float speed;
    float fade;
    int variant;
} Civilian;

/* Front-desk staff. Unlike the janitor, who wanders wherever the floor lets
 * him, this one has a post and an errand and does nothing else: stand at the
 * counter, walk out to something, deal with it, walk back. */
typedef enum
{
    RECEPTIONIST_DESK = 0,
    RECEPTIONIST_WALK,
    RECEPTIONIST_ERRAND
} ReceptionistActivity;

typedef struct
{
    float x, y;
    float vx, vy;
    int dir;      /* facing */
    int desk_dir; /* the way the counter faces, decided once at spawn */
    bool on_ground;
    ReceptionistActivity activity;
    float activity_timer;
    /* The spawn tile, and the only place a walk ever ends up. Errand targets
     * are measured from here so a round trip cannot drift off the counter. */
    float post_x;
    float target_x;
    /* Latched when a walk starts. Deriving it from the target every frame
     * instead would make crossing the target unrepresentable: the sign flips
     * with the overshoot and the walk oscillates on the spot forever. */
    int walk_dir;
    bool heading_home;
    /* Counts down to the next glance away from the floor while on post, then
     * counts the glance itself out. */
    float glance_timer;
    bool glancing;
    float anim_time;
} Receptionist;

typedef struct
{
    int current_level;
    int lives;
    int continues_remaining;
    int score;
    float level_elapsed_time;
    int level_start_score;
    int level_deaths;
    /*
     * Whether this sector has already been paid for being finished.
     *
     * `try_finish_current_level` runs on every frame the player is standing in
     * the exit and is only *usually* the last thing that happens on a floor:
     * the window route can fail to load the sector above, and it returns
     * having changed nothing so the next frame tries again. With the pay-out
     * above that branch — which is where it has to be, because all three ways
     * out of a sector pass through it and only one of them draws a report —
     * that retry is a second pay-out, once per frame, for as long as the
     * player stands there. A latch rather than a rearranged control flow,
     * because the next way out of a sector will pass through the same place
     * and will not think about this.
     */
    bool sector_bonus_paid;
    /* The next score threshold that pays out an extra life. */
    int next_extra_life_score;
    float continue_timer;
    /*
     * How many of the crew this run has put down, across every sector of it.
     *
     * `GameplayState.hostiles_neutralized` is the *floor's* tally and is wiped
     * with the rest of the simulation at every sector, which is right for the
     * report between floors and wrong for the one other thing that asks: the
     * crew's own net says things like "I COUNTED ELEVEN OF US TONIGHT", and a
     * crew does not get its men back because the player opened a stair door.
     * Counted here, beside the score, because the score is the other number
     * that belongs to the run rather than to the floor.
     */
    int hostiles_down;
    /*
     * How many sheets off Meridian's docket this run has picked up.
     *
     * Beside the score and the crew tally because it belongs to the *run*
     * rather than to the floor — a sector's own copy would be wiped at every
     * doorway, and what this number is for is the state of the case at the end
     * of the night. It is the campaign's, and `Progress` keeps the best it has
     * ever been across runs; see progress.h.
     */
    int evidence_collected;
    /*
     * Which sectors' sheets are already in the folder, one bit to a sector.
     *
     * The count above needed this the day a retry was made unlimited and
     * nobody noticed. `campaign_accept_continue` says in as many words that
     * "retrying the sector is always on offer", and the retry goes through
     * `load_level`, which parses the map again — so the `*` is laid out again
     * while `evidence_collected` carries on from where it was. Twelve sheets
     * is a *set*, one to an interior, and the same sheet taken twice was two
     * of them: measured, five retries of sector 1 give `evidence_collected`
     * five, and `progress_note_evidence` banks it.
     *
     * A bitmask rather than a per-sector counter because the question is
     * membership and nothing else asks it, and it is on the campaign rather
     * than on the level for the reason the count beside it is: a sector's own
     * copy is wiped by the very reload this exists to survive.
     */
    uint32_t docket_sheets_held;
    /*
     * Whether any assist switch has been on at any point in this run.
     *
     * Sticky, and that is the whole of the rule: the assists take effect the
     * instant they are switched (`game_apply_assist_everywhere`), so a run's
     * records cannot be decided by what the sheet happened to say at the finish.
     * Turning infinite lives on for the one sector that keeps killing you and
     * off again for the walk to the roof is exactly the run this flag exists to
     * describe.
     *
     * What it costs is the three ratchets in [progress.h](progress.h) —
     * `best_score`, `best_sector_time` and `best_evidence` — because none of
     * them means anything measured across two different games. It deliberately
     * does **not** cost `furthest_sector`: that one is the title screen's resume
     * chip, which is navigation rather than a record, and a player who took the
     * assist to get to sector nine is still a player who has to start at sector
     * nine.
     *
     * Veteran does not set it. It is the same lever pulled the other way, and a
     * harder run has no reason to be kept off the ladder it is beating.
     */
    bool assisted;
    /*
     * Whether this is a veteran run, kept here because a continue has to ask.
     *
     * `campaign_reset` used to take the flag, spend it on the opening lives and
     * continues, and forget it — and `campaign_accept_continue`, which is the
     * one other place that hands lives out, had nothing to ask and so handed out
     * `PLAYER_LIVES`. A veteran run opens with `VETERAN_CONTINUES` of nought, so
     * its *first* death takes the branch that resets the score, and it came back
     * with three lives: the mode `VETERAN_LIVES` describes lasted exactly one
     * mistake, which on a one-life run is the whole of it. Two of the three
     * numbers the mode is survived the continue and the defining one did not.
     *
     * Not sticky like `assisted`, because it is not a record's business: it is
     * live difficulty, read the same way the assist switches are, and the sheet
     * can be reached from the pause menu mid-run. `apply_assist_to_state` keeps
     * it in step for exactly that reason — it is already the one function every
     * change to the run's difficulty passes through.
     */
    bool veteran;
} CampaignState;

/* `veteran` decides how many lives and continues the run opens with; see
 * VETERAN_LIVES. It is remembered on the campaign because a continue hands out
 * lives too; everything else about a campaign is the same either way. */
void campaign_reset(CampaignState *campaign, bool veteran);

/*
 * The sheet's answer about the veteran switch, which may be flipped mid-run.
 *
 * Beside `campaign_note_assist` and called from the same place, because these
 * are the same question asked of the two levers: what a run counts as, and how
 * many lives its next continue is worth.
 */
void campaign_note_veteran(CampaignState *campaign, bool veteran);

/*
 * An assist switch is on. Called with the sheet's answer whenever a run starts
 * and whenever one of the switches is touched mid-run, and it only ever sets:
 * see `CampaignState.assisted` for why a run cannot un-assist itself by
 * switching back.
 */
void campaign_note_assist(CampaignState *campaign, bool assist_on);

/*
 * Whether what this run does is worth writing to the player's disk.
 *
 * The one place the rule lives, so the four `progress_note_*` calls in the shell
 * cannot come to disagree about it — and it is here rather than in
 * [progress.c](progress.c) because it is a fact about the run, not about the
 * file. `progress_note_sector` is deliberately outside it; see the field.
 */
bool campaign_records_count(const CampaignState *campaign);
bool campaign_lose_life(CampaignState *campaign);
bool campaign_begin_continue(CampaignState *campaign);
bool campaign_update_continue(CampaignState *campaign, float dt);
bool campaign_accept_continue(CampaignState *campaign);
/* Returns true each time the score crosses an extra-life threshold, granting
 * the life; the shell turns the report into sound and HUD flash. */
bool campaign_check_extra_life(CampaignState *campaign);

/*
 * The docket as a set rather than as a tally.
 *
 * `campaign_holds_docket_sheet` is what makes this floor's sheet a wasted
 * pickup once the run already has it, and `campaign_take_docket_sheet` is the
 * only thing that raises `evidence_collected` — so the count and the set
 * cannot come apart, which is the whole reason the increment is not left at
 * the call site. A sector outside the campaign is held by nobody and takes
 * nothing: the mask is `CAMPAIGN_SECTORS` wide by assertion, so the only way
 * in is a caller with a sector it invented.
 */
bool campaign_holds_docket_sheet(const CampaignState *campaign, int sector);
void campaign_take_docket_sheet(CampaignState *campaign, int sector);

/*
 * What the clock pays for a sector finished in `elapsed_seconds`.
 *
 * Split out of `campaign_award_sector_bonus` because two other callers were
 * doing the arithmetic in their heads and one of them got it wrong: the
 * staged `--screen report`, `--screen cleared` and `--screen reveal` all
 * printed `+1200 TIME` beside a clock of 01:31, and 1200 is what 01:14 pays.
 * The press stills and the store page are cut from those frames.
 */
int campaign_time_bonus_for(float elapsed_seconds);

/*
 * What finishing a sector pays, and the two numbers the report prints beside
 * the time and the deaths it has always shown. See SECTOR_PAR_SECONDS for why
 * the par is the night clock's own allowance rather than a figure of its own.
 *
 * Both are handed back separately because the report shows them separately —
 * a bonus folded silently into SCORE is a bonus the player cannot learn to
 * play for — and the total is added to the score here, so the one caller
 * cannot bank one of the two and forget the other. Called on the way out of a
 * sector, whichever way out that is: the stair door, the window onto a climb,
 * and the last floor of all.
 *
 * **A second call for the same sector pays nothing and reports nothing**, and
 * that is this function's own guarantee rather than the caller's — see
 * `CampaignState.sector_bonus_paid`. The latch is cleared by
 * `campaign_begin_sector`.
 */
void campaign_award_sector_bonus(CampaignState *campaign,
                                 int *out_time_bonus, int *out_clean_bonus);

/*
 * The per-sector counters the report reads, and the pay-out latch, started
 * over. Called from the shell as a sector loads.
 *
 * One function rather than four assignments at the call site because the latch
 * is the fourth and arrived last: a new field that has to be reset alongside
 * the others is exactly what an open-coded reset forgets.
 */
void campaign_begin_sector(CampaignState *campaign);

typedef struct
{
    Rng rng;
    GameEventBuffer events;

    Level level;
    Player player;
    Enemy enemies[MAX_ENEMIES];
    int enemy_count;
    Dog dogs[MAX_DOGS];
    int dog_count;
    /* Kills counted as they happen rather than by scanning the `dead` flags at
     * the end of the sector. A reinforcement takes over the slot of a guard
     * already down (see `find_enemy_slot`), so the flags are the population
     * still on the floor and not the tally of what the player put there — read
     * that way, the report between sectors quietly lost one kill per door
     * spawn. */
    int hostiles_neutralized;
    Janitor janitors[MAX_JANITORS];
    int janitor_count;
    Civilian civilians[MAX_CIVILIANS];
    int civilian_count;
    Receptionist receptionists[MAX_RECEPTIONISTS];
    int receptionist_count;
    Mine mines[MAX_MINES];
    int mine_count;
    Grenade grenades[MAX_GRENADES];
    int grenade_count;
    /*
     * Flash charges in the air. Their own array rather than a flag on a
     * grenade, and for the reason the bolts have one: every loop that walks the
     * grenades is a loop about explosives — `apply_blast` chains them, a blast
     * spends them, they open weak walls — and a charge that hurts nobody
     * belongs to none of it.
     */
    Grenade flashbangs[MAX_FLASHBANGS];
    Decoy decoys[MAX_DECOYS];
    /*
     * One per map camera, and `cameras_initialized` is what says the array has
     * been matched to the map yet. A zeroed state has `working` false on every
     * one of them, which would be a sector full of dead cameras rather than an
     * untouched one — so they are turned on at the first update instead of
     * being trusted to a memset, the way the facade hazards already are.
     */
    CameraState cameras[MAX_CAMERAS];
    bool cameras_initialized;
    Rocket rockets[MAX_ROCKETS];
    Bullet bullets[MAX_BULLETS];
    Bullet enemy_bullets[MAX_ENEMY_BULLETS];
    ThrownObject thrown_objects[MAX_THROWN_OBJECTS];
    Bird birds[MAX_BIRDS];
    float facade_hazard_spawn_timers[MAX_FACADE_HAZARD_SPAWNS];
    float facade_hazard_windup_timers[MAX_FACADE_HAZARD_SPAWNS];
    bool facade_hazards_initialized;
    FacadeWindPhase facade_wind_phase;
    float facade_wind_timer;
    int facade_wind_dir;
    bool facade_wind_sheltered;
    /* Highest banked position on the wall; always somewhere the climber has
     * actually stood, so respawning there can never place him inside stone. */
    float facade_checkpoint_x;
    float facade_checkpoint_y;
    bool facade_has_checkpoint;
    /* The interior counterpart: banked at real progress (a card, a hacked
     * terminal, a used door), so a lost life resumes near the work instead of
     * back at the street entrance. */
    float interior_checkpoint_x;
    float interior_checkpoint_y;
    bool interior_has_checkpoint;

    AmmoDrop ammo_drops[MAX_AMMO_DROPS];

    /* Assist choices, handed in by the shell at level load. Both are false in
     * a zeroed state, so every plainly-initialised simulation — the game's
     * and the tests' — runs at the authored difficulty. */
    bool assist_slow_enemies;
    bool assist_more_hearts;
    /* The other direction, handed over the same way and read in the same two
     * places. A zeroed state is the authored difficulty, exactly as it is for
     * the two above. */
    bool veteran;

    float invuln_timer;
    int door_spawns[MAX_DOORS];
    float door_timers[MAX_DOORS];
    float teleport_cooldown;
    int player_on_elevator;
    int player_on_moving_platform;

    float terminal_hack_progress;
    float terminal_hack_tick_timer;
    bool terminal_in_range;
    bool terminal_hacking;
    /* Historical name: terminal breaches and guard-operated switches now
     * share this building-wide quiet-time countdown. */
    float terminal_alarm_timer;
    float alarm_target_x;
    float alarm_target_y;
    float alarm_siren_timer;
    int active_alarm_switch;
    float terminal_reinforcement_timer;
    int terminal_reinforcements_pending;
} GameplayState;

/* Clear all per-level simulation state while preserving the RNG stream. */
void gameplay_state_begin_level(GameplayState *state);

/*
 * The restroom door, as one rule rather than three assignments in the shell.
 *
 * A `U` swaps two whole `GameplayState`s, so everything that belongs to the
 * *man* rather than to the room has to be handed back across afterwards. The
 * shell used to do that by hand for two of the three — `player_carry_loadout`
 * and the hearts, under a comment saying "the door is not a heal" — and the
 * third, the mercy window a hit has just opened, stayed behind with the frozen
 * area. So a player who took a hit and stepped through arrived with no blink at
 * all and could be hit again on the first frame, in rooms holding two men and a
 * dog; and coming back out he was handed the sector's *old* window, banked at
 * the moment he went in, however long he had spent inside. The blink is a
 * property of the body, like the hearts, and it travels with them.
 *
 * It is here rather than in [game.c](game.c) for the reason
 * `player_carry_loadout` is in [player.c](player.c): a doorway rule kept on the
 * far side of the SDL boundary is a doorway rule no test can reach, which is
 * where the flash charge went missing for six sectors.
 */
void gameplay_carry_through_doorway(GameplayState *arriving,
                                    const Player *travelling,
                                    float travelling_invuln);

/* The assist choices as the numbers the simulation actually uses. */
int gameplay_player_max_hp(const GameplayState *state);
float gameplay_enemy_speed_scale(const GameplayState *state);

#endif /* CHUCK_GAMEPLAY_STATE_H */
