/*
 * The crew, and what comes off their net.
 *
 * Everything here is presentation: no line changes a cone of vision, a
 * cooldown or a route, and the simulation never reads a word of it. What it
 * does change is what the building sounds like from the inside — twelve men
 * running a schedule, saying twelve men's worth of things, in the one place
 * the story is otherwise never told.
 */

#include "crew.h"

#include <stddef.h>

/*
 * Twelve names, in the order they were badged in. Filed by slot rather than
 * drawn, so the man patrolling the archive is the same man every time that
 * sector loads and no callsign costs the seeded stream a draw.
 */
static const char *const CREW[CREW_SIZE] = {
    "KARL",  "THEO",   "TONY",     "HEINRICH", "MARCO", "KRISTOFF",
    "FRANCO", "FRITZ", "ALEXANDER", "ULI",     "EDDIE", "JAMES",
};

/*
 * A man on his own, with a handset, and nothing to report. These are the
 * lines that carry the plot, because a routine call is the only time in the
 * game somebody says out loud what the crew is actually doing here — the
 * locks, the cordon, the clock, and how little of the tally still answers.
 */
static const char *const RADIO_LINES[] = {
    "NET CHECK. THIRTY-SECOND FLOOR, NOTHING TO REPORT.",
    "THEO IS ON THE SIXTH LOCK. ONE MORE AND WE LOAD.",
    "THE SEVENTH LOCK IS NOT ON ANY DRAWING WE HAVE.",
    "TELL VOSS THE CIRCUITS CUT THEMSELVES AT MIDNIGHT.",
    "STILL NOTHING FROM MARCO. NOTHING FROM HEINRICH.",
    "I COUNTED ELEVEN OF US TONIGHT. IT WAS TWELVE.",
    "SOMEBODY TOOK A FIRE HOSE OFF THE THIRTY-FIRST.",
    "WHO BROKE THE GLASS ON THE STAIR? ALL OF IT.",
    "HE IS NOT A COP. A COP WOULD BE WEARING SHOES.",
    "ROOF CHARGES ARMED. NOBODY GOES UP BEFORE 01:00.",
    "THE CORDON IS FOUR BLOCKS OUT AND FACING AWAY.",
    "THE CONTROLLER STAYS BREATHING. VOSS WAS CLEAR.",
    "COPY. NOBODY MOVES UNTIL THE LAST BOLT DROPS.",
    "SIX HUNDRED AND FORTY MILLION. IN ONE CASE.",
    "IT IS ONE MAN. HOW MUCH TROUBLE IS ONE MAN?",
    "IF HE BLEEDS, HE CAN BE STOPPED. HE BLEEDS.",
    "MOVEMENT ON THE SERVICE LEVEL. SOMETHING BIG.",
    "HE CAME UP THROUGH THE DUCTS. THE DUCTS, VOSS.",
    "SAY AGAIN? YOU WANT ME TO SWEEP WHICH STAIR?",
    "STAIR CORE IS WELDED. NOTHING COMES DOWN IT.",
    "I AM GETTING TOO OLD FOR NIGHT WORK.",
    "TWELVE CASES IN, TWELVE OUT. THAT IS THE JOB.",
    "HE WILL BE BACK. THIS ONE IS ALWAYS BACK.",
    "DEAD OR ALIVE, HE IS COMING WITH US.",
    "I HEARD THIS MAN WAS DEAD. EVERYBODY HEARD IT.",
    "NO SIRENS OUT THERE. NOT ONE. LISTEN TO IT.",
    "SHE WROTE THIS ACCESS SYSTEM. OF COURSE SHE DID.",
    "VAULT IS DRY. LOAD IT AND GET IT ON THE ROOF.",
    "NEGATIVE. THE LIFT SHAFTS ARE OURS TONIGHT.",
    "KARL WANTS THE LIGHTS BACK ON. TELL KARL NO.",
};

/*
 * Two of them standing together with nothing to do. The chat already existed
 * as a pose and a sound; what it never had was a subject, and a pair of armed
 * men killing four seconds in silence is the one thing a night shift never
 * does.
 */
static const char *const TALK_LINES[] = {
    "TWELVE OF US, ONE OF HIM. DO THE ARITHMETIC.",
    "VOSS SAYS DO NOT SHOOT THE GLASS. HE LIKES GLASS.",
    "SWEEP THE STAIR. NO MERCY ON THE STAIR.",
    "IT IS ALL IN THE REFLEXES. THAT IS ALL IT IS.",
    "REMEMBER I SAID I WOULD RELIEVE YOU LAST? I LIED.",
    "BE NICE. UNTIL VOSS SAYS IT IS TIME NOT TO BE.",
    "THERE IS ONLY ONE OF HIM. THAT IS THE PROBLEM.",
    "I WOULD BUY THAT FOR A DOLLAR.",
    "HE DREW FIRST BLOOD, NOT US. REMEMBER THAT.",
    "COME WITH ME IF YOU WANT TO GET PAID.",
    "WELCOME TO THE PARTY. HE IS NOT ON THE LIST.",
    "SIX FORTY, THIRTEEN WAYS. DO NOT MAKE IT TWELVE.",
    "I AM ALL OUT OF PATIENCE AND ALL OUT OF GUM.",
    "HE STOPPED FOR COFFEE. I WATCHED HIM ORDER IT.",
    "WHO KEEPS TURNING THE LIGHTS OFF ON THIS FLOOR?",
    "WHO COMES UP A LIFT SHAFT? NOBODY COMES UP ONE.",
    "I NEED YOUR HANDSET, YOUR BOOTS AND YOUR CLIP.",
    "EVERY UNIT IN THE CITY IS OUTSIDE LOOKING AT US.",
    "TALK TO ME. NOBODY ON THIS NET EVER TALKS TO ME.",
    "THE NIGHT STAFF WERE POLITE. I LIKED THEM.",
    "TWO MINUTES AND THIS ROOF IS SOMEBODY ELSE'S.",
    "HE IS ONE FLOOR BEHIND US. HE IS ALWAYS ONE.",
};

/*
 * The beat a wall switch goes over. Short, loud and in the wrong order,
 * because nobody assembles a sentence while running at a switch.
 */
static const char *const ALARM_LINES[] = {
    "HE IS ON THIS FLOOR! I SAY AGAIN, THIS FLOOR!",
    "GAME OVER! THAT IS IT, THAT IS GAME OVER!",
    "SOMEBODY GET ON THE NET! WE HAVE A MAN INSIDE!",
    "THAT IS NOT SECURITY! THAT IS NOT ANYBODY!",
    "HE IS NOT A GUEST, VOSS! HE WAS NEVER A GUEST!",
    "NO SHOT! HE IS BEHIND THE SLAB! NO SHOT!",
    "KARL! KARL, ANSWER YOUR HANDSET!",
    "GET TO THE ROOF! EVERYBODY GET TO THE ROOF!",
    "IT IS THE MAN OFF THE PAVEMENT! IT IS HIM!",
    "HE HAS A CARD! HE IS COMING UP THE STAIR!",
    "TWELVE OF US! WHERE ARE THE OTHER ELEVEN?",
};

/*
 * Leaned out of a window with a piece of the cornice in both hands. Four
 * hundred feet of air and nothing else to say, so it is all shouted at Chuck
 * rather than into a handset — the one place on the net where the crew talk to
 * him instead of about him.
 */
static const char *const WALL_LINES[] = {
    "GET OFF THE WALL, COWBOY!",
    "FORTY FLOORS! COUNT THEM ON THE WAY DOWN!",
    "THERE IS NOTHING UP HERE BUT US!",
    "HEADS!",
    "LOOK UP! I SAID LOOK UP!",
    "THIS IS THE PART WHERE YOU LET GO!",
    "NOBODY CLIMBS OUT OF THIS BUILDING!",
    "SAY GOODNIGHT, WALL-CRAWLER!",
    "VOSS SAYS YOU ARE NOT EVEN REAL!",
    "STILL NO SHOES!",
};

/*
 * The lobby, ninety seconds after the crew walked Ellen through it. These are
 * the only witnesses in the campaign and the only people in the building who
 * are not armed, and they get about four seconds of screen time each — so the
 * lines are what somebody actually shouts while leaving a room, not what they
 * would say if asked.
 */
static const char *const PANIC_LINES[] = {
    "THEY HAVE GUNS! EVERYBODY OUT!",
    "THEY TOOK THE NIGHT CONTROLLER!",
    "THE STAIRS ARE WELDED! THE STAIRS ARE WELDED!",
    "DO NOT GO UP THERE!",
    "MAINTENANCE! THEY SAID THEY WERE MAINTENANCE!",
    "SOMEBODY CALL SOMEBODY!",
    "THERE IS NOBODY OUTSIDE! I LOOKED!",
    "I AM NOT PAID ENOUGH FOR THIS!",
    "GO! GO! DO NOT LOOK AT THEM!",
    "THAT IS NOT THE NIGHT SHIFT!",
};

#define COUNT_OF(table) ((int)(sizeof(table) / sizeof((table)[0])))

typedef struct
{
    const char *const *lines;
    int count;
} CrewTable;

/* Indexed by ChatterKind, so a new kind is a row here and a case at the emit
 * site, and never a chain of ifs in the renderer. */
static const CrewTable TABLES[CHATTER_KIND_COUNT] = {
    [CHATTER_RADIO] = {RADIO_LINES, COUNT_OF(RADIO_LINES)},
    [CHATTER_TALK] = {TALK_LINES, COUNT_OF(TALK_LINES)},
    [CHATTER_ALARM] = {ALARM_LINES, COUNT_OF(ALARM_LINES)},
    [CHATTER_WALL] = {WALL_LINES, COUNT_OF(WALL_LINES)},
    [CHATTER_PANIC] = {PANIC_LINES, COUNT_OF(PANIC_LINES)},
};

const char *crew_callsign(int speaker)
{
    /* A negative slot is not a caller's mistake to fix at the call site: the
     * facade has no enemy array at all, and the thrower behind a window is
     * identified by which window it is. Wrapping covers both. */
    if (speaker < 0)
        speaker = -speaker;
    return CREW[speaker % CREW_SIZE];
}

int crew_line_count(ChatterKind kind)
{
    if (kind < 0 || kind >= CHATTER_KIND_COUNT)
        return 0;
    return TABLES[kind].count;
}

const char *crew_line(ChatterKind kind, int roll)
{
    int count = crew_line_count(kind);
    if (count <= 0)
        return NULL;
    if (roll < 0)
        roll = -roll;
    return TABLES[kind].lines[roll % count];
}
