#ifndef CHUCK_ENEMY_H
#define CHUCK_ENEMY_H

#include "game_config.h"
#include "level.h"
#include "rng.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    float x, y; /* top-left of the enemy box */
    float vx, vy;
    int dir; /* horizontal patrol direction: -1 or +1 */
    bool on_ground;
    int on_elevator; /* runtime elevator index while riding, otherwise -1 */
    bool climbing;
    int climb_dir;  /* -1 = up, +1 = down */
    int ladder_col; /* column the enemy is climbing */
    int climb_start_floor_row; /* floor row where this climb began */
    float climb_cooldown;
    /* Keeps a crate jump short enough to land on the box instead of sailing
     * over it. Cleared as soon as the guard is grounded again. */
    bool mounting_crate;
    /* Temporarily commits obstacle avoidance or a foreground crate route.
     * Without this, pursuit steering can undo the decision every frame. */
    float obstacle_avoid_timer;
    int hp; /* current hit points, 1..ENEMY_HP */
    bool dead;
    float shoot_cooldown; /* seconds until next shot */
    float aim_timer;      /* > 0 while enemy is standing still to aim */
    /* Stored target coordinates captured when aiming starts. */
    float aim_target_x;
    float aim_target_y;
    /* Last position the guard could actually see (or the alarm source).
     * Keeping this snapshot prevents tracking the live player through floors. */
    float pursuit_target_x;
    float pursuit_target_y;
    bool has_pursuit_target;
    /* A guard that survives a player bullet keeps hunting the shooter even
     * when the terminal alarm is inactive or the player is initially outside
     * the guard's normal sight and shooting range. */
    bool provoked;
    /* The first time a guard sees Chuck, it decides whether to fight or run
     * for an alarm switch. Losing sight for a while permits a new encounter. */
    bool encounter_decided;
    float encounter_lost_timer;
    bool raising_alarm;
    int alarm_switch_index;
    float alarm_use_timer;
    /* Suspicion state: a guard walks to a heard/seen disturbance, scans, and
     * returns to patrol. Escalates to real pursuit the moment it sees Chuck. */
    float investigate_timer;
    float investigate_x;
    float investigate_y;
    float investigate_scan_timer;
    /*
     * Which bodies this guard has already been over to look at.
     *
     * One bit per corpse rather than one flag per guard, and the difference is
     * the whole feature. A single latch stopped him re-triggering on the body
     * he was standing next to — which is what it was for — but it also made him
     * blind to every *other* corpse for the rest of the sector, so in a floor
     * with ten guards on it the rule that bodies can be read switched itself
     * off after the first kill. A bit is cleared again when the slot behind it
     * is handed to a live guard (`find_enemy_slot` / `find_dog_slot`), so a
     * recycled slot never arrives pre-investigated.
     *
     * Bits 0..MAX_ENEMIES-1 are guards; MAX_ENEMIES..MAX_ENEMIES+MAX_DOGS-1 are
     * dogs. `enemy_body_bit` is the one place that mapping is written down.
     */
    uint32_t bodies_investigated;
    /* How long this guard has held an unbroken line of sight on Chuck. A
     * fresh sighting is noticed for ENEMY_NOTICE_TIME before the aim starts,
     * so detection around a corner never fires below reaction time. */
    float sight_timer;
    /* Vertical firing direction for the pending shot: 0 = horizontal,
     * -1 = straight up, +1 = straight down. */
    int aim_vdir;
    bool talking;        /* true while chatting with another enemy */
    float talk_timer;    /* seconds remaining while talking */
    float talk_cooldown; /* seconds remaining before eligible to talk again */
    int talk_partner;    /* index of partner enemy, -1 if none */
    /* Counts down to this guard's next call in on the crew's own net. Only a
     * clock is needed: whether the call happens is decided by the same
     * conditions the chat uses, and the pose and the sound follow from
     * talking with no partner. */
    float radio_timer;
    float anim_time;     /* local procedural animation clock */
    float recoil_timer;  /* brief muzzle flash / firing follow-through */
} Enemy;

typedef enum
{
    DOG_GUARD,
    DOG_ROAM,
    DOG_RETURN,
    DOG_CHASE
} DogState;

typedef struct
{
    float x, y; /* top-left of the dog box */
    float vx, vy;
    int dir;
    bool on_ground;
    int hp;
    bool dead;
    int owner; /* index into Game.enemies[], -1 after the handler is gone */
    DogState state;
    float state_timer;
    float turn_cooldown; /* debounce before reversing at a ledge */
    float bite_cooldown;
    /* The announced crouch before a bite. Windup counts down while contact
     * holds; bite_ready marks the beat where the teeth actually land. */
    float bite_windup;
    bool bite_ready;
    float lost_timer;
    /* Dogs keep running to the last visible/alarm position, not to the
     * player's live position after line of sight has been lost. */
    float chase_target_x;
    bool has_chase_target;
    float guard_x;
    float guard_y;
    float roam_target_x;
    float vocal_timer; /* seconds until another chase bark or growl */
    float anim_time;    /* local procedural animation clock */
    float attack_timer; /* bite/lunge follow-through */
} Dog;

/*
 * A guard standing still and speaking with nobody beside him is speaking into
 * a handset. Deriving it rather than storing it means the pose can never
 * disagree with the state that stops him walking: every path that ends a chat
 * ends the radio check with it, and there is no second flag to forget.
 */
static inline bool enemy_on_radio(const Enemy *enemy)
{
    return enemy->talking && enemy->talk_partner < 0;
}

/*
 * Which bit of `Enemy.bodies_investigated` stands for a given corpse. Guards
 * are filed under their own slot and dogs after them, so one mask covers both
 * kinds and the two arrays can never be read as each other.
 */
_Static_assert(MAX_ENEMIES + MAX_DOGS <= 32,
               "the body-discovery mask has one bit per corpse");

static inline uint32_t enemy_body_bit(int slot, bool is_dog)
{
    return 1u << (unsigned)(is_dog ? MAX_ENEMIES + slot : slot);
}

void enemy_init(Enemy *enemy, float x, float y, Rng *rng);
/* speed_scale multiplies the guard's ground speed; 1.0 is the authored pace
 * and the assist option is the only caller that passes anything else. */
void enemy_update(Enemy *enemy, Level *level, float dt,
                  bool pursuing, bool alarmed,
                  float target_x, float target_y,
                  bool hemmed_in, float speed_scale, Rng *rng);
void dog_init(Dog *dog, float x, float y, int owner, Rng *rng);

#endif /* CHUCK_ENEMY_H */
