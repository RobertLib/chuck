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
 *
 * **The roll is deliberately not a citation**, and five of these were changed to
 * make sure of it. The building is a homage and
 * [docs/story.md](../docs/story.md) says so; the rule it sets is that the homage
 * is paid by *describing* the films rather than naming them, and a roster
 * reproducing somebody else's crew name for name is the one thing on the page
 * that named them outright. What the twelve are for is the net and the manual's
 * `THE CREW` sheet — a man on a handset has a name, a man being shot at is
 * anonymous — and that works on any twelve names. A reader who recognises the
 * shape of the night is meant to; a reader who recognises the cast list was
 * reading a copy.
 */
static const char *const CREW[CREW_SIZE] = {
    "LENZ",   "BRUNO", "TONY",      "KASPAR", "MARCO", "MATTHIAS",
    "FRANCO", "FRITZ", "ALEXANDER", "MILO",   "EDDIE", "JAMES",
};

/*
 * A line, and the stretch of the night somebody may say it in.
 *
 * The three numbers after the text are the whole gate: the first sector it may
 * be heard in, the sector it stops being true at, how many of the crew have to
 * be down before it may be heard at all, and how many may be down before it
 * stops being true — two axes, each written floor-then-ceiling. **Nought is no
 * gate in all four**, so a row reading `0, 0, 0, 0` is ungated in every
 * direction,
 * which is most of them. They are spelled out rather than left to zero-fill
 * because `-Wextra` is right about that: a field nobody wrote is a field
 * nobody decided.
 *
 * It exists because these tables are rolled from without any idea where the
 * player is, and a good half of the writing is *about* where the player is. A
 * man in the lobby at 00:22 was free to report that the vault was already
 * empty, that the roof was somebody else's in two minutes, and that nothing had
 * come back from MARCO — before Chuck had climbed a floor or touched anybody.
 * The plot arrives in these lines whether or not the manual is ever opened, so
 * arriving out of order is the same failure as a report between sectors that
 * spoils its own ending, only shuffled.
 *
 * **A floor with no ceiling only fixed half of that.** Three of these lines
 * state an exact number of men still standing — `I COUNTED ELEVEN OF US
 * TONIGHT`, `WHERE ARE THE OTHER ELEVEN?`, two named men not answering — and
 * `after_down` alone let all three survive into a run with thirty guards down,
 * where they are exactly as false as they were in the lobby and a good deal
 * more conspicuous, because by then the player is the one who has been
 * counting. A line that names a number is true over a *window*, not from a
 * moment onward, and `until_down` is that window's far edge.
 *
 * **And that paragraph was written about the tally and left standing over the
 * sector, which is the half it names.** `until_down` was added, the sentence
 * explaining why a floor needs a ceiling was written directly above the field
 * it had just been given to — and `from_sector` went on having none, so the
 * fix landed on one of the two axes and the argument for it read as though it
 * covered both. Two lines were paying:
 *
 *   - `NINE MINUTES AND THIS ROOF IS SOMEBODY ELSE'S` is gated from 14, whose
 *     dial reads 00:51 against a night that ends at 01:00. With no ceiling it
 *     is also sayable on 15, 16 and 17, where the dial gives **seven, five and
 *     three** — so the one line on the net that states a remaining duration was
 *     right on one of the four sectors it could be heard on, and on the roof
 *     itself a man announced nine minutes standing beside another saying
 *     `THERE IS NO UPSTAIRS LEFT`.
 *   - `BRUNO IS ON THE SIXTH LOCK. ONE MORE AND WE LOAD` is gated from 8, and
 *     the seventh lock is open by 10 — `VAULT IS DRY` is gated there and says
 *     so. With no ceiling the same net reported the sixth lock still being
 *     worked, the vault already empty and, on 17, the cases already on the pad.
 *
 * **And the third one was found by the check written for the first two**, which
 * is the whole argument for asking the property rather than fixing the rows.
 * `VAULT IS DRY. LOAD IT AND GET IT ON THE ROOF` is an order with loading still
 * pending, and it outlived its own completion: on 17 it stood beside `CASES ARE
 * ON THE PAD. WE ARE WAITING ON THE BIRD`. It runs 10-16 now, and the three
 * stages of the job — the locks, the vault, the pad — hand over to each other in
 * order rather than piling up. `test_the_net_reports_one_stage_of_the_job_at_a_time`
 * is what holds that, and it finds the three by what they claim rather than by
 * index, so rewording one fails loudly instead of being checked no longer.
 *
 * `check_docs.py` holds the first of those, and could not see it: the pair it
 * checks is the number against the dial at the sector in the gate, and there
 * was no second number to read. A guard on a floor is a guard on a floor.
 *
 * A ceiling is deliberately only ever set on a line that *counts* or that
 * reports a **stage of the job**. Everything else here is a crew's opinion of
 * the man coming up the stairs, and an opinion does not go stale — `IT IS ONE
 * MAN. HOW MUCH TROUBLE IS ONE MAN?` gets funnier with the tally, not falser,
 * and pinning a ceiling to it would thin the late sectors for nothing. An
 * *event* does not go stale either, which is why `WHO BROKE THE GLASS ON THE
 * STAIR` and `HE CAME UP THROUGH THE DUCTS` keep their open tops: something
 * that happened goes on having happened.
 *
 * Gate for what a line *asserts*, never for flavour. `THE SEVENTH LOCK IS NOT
 * ON ANY DRAWING WE HAVE` is a crew that has been on the locks since midnight
 * and is true at 00:22; `VAULT IS DRY` is a fact about the climb the player has
 * not made yet. When in doubt the line goes ungated, because a thin table in
 * the early sectors is a worse failure than an eager line in a late one.
 */
typedef struct
{
    const char *text;
    short from_sector;
    /* Exclusive, and the mirror of `until_down` below: the line is dropped
     * once the run reaches this sector. Nought is no ceiling. The two axes are
     * written floor-then-ceiling, in pairs, because the whole of the defect
     * this field fixes was that one of them had a ceiling and the other did
     * not — and a reader of the four numbers could not see which. */
    short until_sector;
    short after_down;
    /* Exclusive: the line is dropped once this many of the crew are down.
     * Nought is no ceiling. */
    short until_down;
} CrewLine;

/*
 * A man on his own, with a handset, and nothing to report. These are the
 * lines that carry the plot, because a routine call is the only time in the
 * game somebody says out loud what the crew is actually doing here — the
 * locks, the cordon, the clock, and how little of the tally still answers.
 */
static const CrewLine RADIO_LINES[] = {
    /* No floor number, and for the same reason the cordon line below names no
     * distance. A net check is a man reporting his *own* post, `CHATTER_EARSHOT`
     * is eleven tiles against the 400px half of the viewport, and the strip
     * only ever prints a speaker who is on screen — so naming the thirty-second
     * floor, as this used to, put a guard standing beside Chuck in the
     * ground-floor lobby announcing he was thirty floors up. There is no
     * sector-to-storey mapping to gate it against either: seventeen sectors are
     * the route up a forty-floor building, not a floor count. What he can say
     * anywhere is the part that matters, which is that he has nothing. */
    {"NET CHECK. THIS FLOOR IS QUIET. NOTHING TO REPORT.", 0, 0, 0, 0},
    /* Six of seven locks is most of a night's work, so it may not be said
     * before the player knows there are locks at all. Sector 8's own report is
     * the beat that turns the job from a ransom into a door — `NOT FOR RANSOM.
     * THEY TOOK HER TO OPEN A DOOR` — and this is the first line on the net
     * that puts a number on how far down that door they have got. It used to
     * cite "the sector the report first says the word vault", which is sector
     * 13 and always was: the arithmetic was right and the reason written beside
     * it was somebody else's. */
    {"BRUNO IS ON THE SIXTH LOCK. ONE MORE AND WE LOAD.", 8, 10, 0, 0},
    {"THE SEVENTH LOCK IS NOT ON ANY DRAWING WE HAVE.", 0, 0, 0, 0},
    {"TELL VOSS THE CIRCUITS CUT THEMSELVES AT MIDNIGHT.", 0, 0, 0, 0},
    /* Two men not answering is two men down, and it used to be sayable in the
     * lobby before a shot had been fired. The ceiling is the other half of the
     * same thought: naming two absentees says the losses are still small
     * enough to be counted on a hand, and past that the crew has bigger
     * arithmetic than MARCO and KASPAR. */
    {"STILL NOTHING FROM MARCO. NOTHING FROM KASPAR.", 0, 0, 2, 5},
    /* The narrowest window on the net, and the most literal: this man has
     * counted, and the number he read out is eleven. It is true while exactly
     * one of the crew is down and false the moment a second goes. */
    {"I COUNTED ELEVEN OF US TONIGHT. IT WAS TWELVE.", 0, 0, 1, 2},
    {"SOMEBODY TOOK A FIRE HOSE OFF THE THIRTY-FIRST.", 6, 0, 0, 0},
    {"WHO BROKE THE GLASS ON THE STAIR? ALL OF IT.", 4, 0, 0, 0},
    /* He is not a cop because he came in alone and did not wait, which is the
     * one thing about him they can actually see. It used to say he was not
     * wearing shoes, which is a joke about a different man in a different
     * building: Chuck is drawn in boots (`boot_rear`/`boot_front` in
     * render_figures.c), so the line only worked as a nod and not as a
     * sentence about anybody on screen — which is the one thing the writing
     * on this net is not allowed to be. */
    {"HE IS NOT A COP. A COP WOULD HAVE WAITED FOR BACKUP.", 0, 0, 1, 0},
    {"ROOF CHARGES ARMED. NOBODY GOES UP BEFORE 01:00.", 0, 0, 0, 0},
    /* No distance in this line, deliberately. The ring is a spatial ramp that
     * thickens all the way to the kerb (`cordon_side` in chase.h), the
     * abduction happened on clean pavement three blocks out, and a man on a
     * handset naming a number contradicted one or the other of those whichever
     * number he named. What he can say is the part that matters: all of it is
     * pointing the wrong way. */
    {"THE WHOLE CORDON IS FACING AWAY FROM US. ALL OF IT.", 0, 0, 0, 0},
    {"THE CONTROLLER STAYS BREATHING. VOSS WAS CLEAR.", 0, 0, 0, 0},
    {"COPY. NOBODY MOVES UNTIL THE LAST BOLT DROPS.", 0, 0, 0, 0},
    {"SIX HUNDRED AND FORTY MILLION. IN ONE CASE.", 0, 0, 0, 0},
    {"IT IS ONE MAN. HOW MUCH TROUBLE IS ONE MAN?", 0, 0, 1, 0},
    {"IF HE BLEEDS, HE CAN BE STOPPED. HE BLEEDS.", 0, 0, 1, 0},
    {"MOVEMENT ON THE SERVICE LEVEL. SOMETHING BIG.", 2, 0, 0, 0},
    /* The ducts are sector 12, and the line is only funny standing in them. */
    {"HE CAME UP THROUGH THE DUCTS. THE DUCTS, VOSS.", 12, 0, 0, 0},
    {"SAY AGAIN? YOU WANT ME TO SWEEP WHICH STAIR?", 0, 0, 0, 0},
    {"STAIR CORE IS WELDED. NOTHING COMES DOWN IT.", 0, 0, 0, 0},
    {"I AM GETTING TOO OLD FOR NIGHT WORK.", 0, 0, 0, 0},
    {"TWELVE CASES IN, TWELVE OUT. THAT IS THE JOB.", 0, 0, 0, 0},
    {"HE WILL BE BACK. THIS ONE IS ALWAYS BACK.", 0, 0, 1, 0},
    {"DEAD OR ALIVE, HE IS COMING WITH US.", 0, 0, 1, 0},
    {"I HEARD THIS MAN WAS DEAD. EVERYBODY HEARD IT.", 0, 0, 1, 0},
    {"NO SIRENS OUT THERE. NOT ONE. LISTEN TO IT.", 0, 0, 0, 0},
    {"SHE WROTE THIS ACCESS SYSTEM. OF COURSE SHE DID.", 0, 0, 0, 0},
    /* The sub-vault is emptied during the climb, so this is the one line on
     * the net that reports the job actually being done. */
    {"VAULT IS DRY. LOAD IT AND GET IT ON THE ROOF.", 10, 17, 0, 0},
    /* The roof, and the reason it needed a line of its own. The report between
     * sectors is shown only after a stair door, the last of the six lands on the
     * vault at 16, and the net's highest gate was 14 — so sector 17, the floor
     * the whole night is played for, arrived with nothing new on either channel.
     * Every check passed: `test_no_two_sectors_in_a_row_go_quiet` allows one
     * quiet sector and the climbs were spending it, so the roof came in on the
     * allowance meant for a wordless wall.
     *
     * What it says is the only thing left to report by then: the cases are on the
     * pad and the crew is waiting on the aircraft rather than on the vault. It is
     * a fact about the last floor and false on every floor below it, which is
     * what the gate is for. */
    {"CASES ARE ON THE PAD. WE ARE WAITING ON THE BIRD.", 17, 0, 0, 0},
    {"NEGATIVE. THE LIFT SHAFTS ARE OURS TONIGHT.", 0, 0, 0, 0},
    {"LENZ WANTS THE LIGHTS BACK ON. TELL LENZ NO.", 0, 0, 0, 0},
};

/*
 * Two of them standing together with nothing to do. The chat already existed
 * as a pose and a sound; what it never had was a subject, and a pair of armed
 * men killing four seconds in silence is the one thing a night shift never
 * does.
 */
static const CrewLine TALK_LINES[] = {
    /* The boast only works while the twelve is still roughly true; once half
     * the crew is down, the arithmetic he is inviting is not the one he means.
     * Written off CREW_SIZE so the window moves if the roster ever does. */
    {"TWELVE OF US, ONE OF HIM. DO THE ARITHMETIC.", 0, 0, 1, CREW_SIZE / 2},
    {"VOSS SAYS DO NOT SHOOT THE GLASS. HE LIKES GLASS.", 0, 0, 0, 0},
    {"SWEEP THE STAIR. NO MERCY ON THE STAIR.", 0, 0, 1, 0},
    {"IT IS ALL IN THE REFLEXES. THAT IS ALL IT IS.", 0, 0, 0, 0},
    {"REMEMBER I SAID I WOULD RELIEVE YOU LAST? I LIED.", 0, 0, 0, 0},
    {"BE NICE. UNTIL VOSS SAYS IT IS TIME NOT TO BE.", 0, 0, 0, 0},
    {"THERE IS ONLY ONE OF HIM. THAT IS THE PROBLEM.", 0, 0, 1, 0},
    {"I WOULD BUY THAT FOR A DOLLAR.", 0, 0, 0, 0},
    {"HE DREW FIRST BLOOD, NOT US. REMEMBER THAT.", 0, 0, 1, 0},
    {"COME WITH ME IF YOU WANT TO GET PAID.", 0, 0, 0, 0},
    {"WELCOME TO THE PARTY. HE IS NOT ON THE LIST.", 0, 0, 1, 0},
    {"SIX FORTY, THIRTEEN WAYS. DO NOT MAKE IT TWELVE.", 0, 0, 0, 0},
    {"I AM ALL OUT OF PATIENCE AND ALL OUT OF GUM.", 0, 0, 0, 0},
    {"HE STOPPED FOR COFFEE. I WATCHED HIM ORDER IT.", 0, 0, 0, 0},
    {"WHO KEEPS TURNING THE LIGHTS OFF ON THIS FLOOR?", 0, 0, 0, 0},
    {"WHO COMES UP A LIFT SHAFT? NOBODY COMES UP ONE.", 2, 0, 0, 0},
    {"I NEED YOUR HANDSET, YOUR BOOTS AND YOUR CLIP.", 0, 0, 0, 0},
    {"EVERY UNIT IN THE CITY IS OUTSIDE LOOKING AT US.", 0, 0, 0, 0},
    {"TALK TO ME. NOBODY ON THIS NET EVER TALKS TO ME.", 0, 0, 0, 0},
    {"THE NIGHT STAFF WERE POLITE. I LIKED THEM.", 0, 0, 0, 0},
    /*
     * The second line on the net that states a *remaining duration*, and the
     * one nobody counted.
     *
     * It said `TWO MINUTES`, and the dial at the sector it is gated from reads
     * 00:51 — **nine** minutes to the helicopter, not two. The comment here
     * said "two minutes to 01:00 is the penthouse and the roof, and nowhere
     * else", which is where the number came from and is arithmetic on a
     * campaign divided fifteen ways. It is the twin of the intel table's
     * `THE SETTLEMENT CLOCK IS RUNNING` row, which said TEN for exactly the
     * same reason, was found, was corrected to the dial's own arithmetic, and
     * is held by [check_docs.py](../tools/check_docs.py) — whose docstring for
     * the helper that derives it opened by calling that row "the one line in
     * the game that states a remaining duration". This was the other one.
     * Fixing one half of a symmetric defect is the most reliable way to stop
     * anybody looking at the other half, and a comment claiming to have
     * enumerated something is the last place anybody recounts.
     *
     * **The number was wrong and the sector was not**, which took a wrong fix
     * to establish. The first attempt moved the line to 17, where the dial
     * gives three and "this roof" is the one under the speaker's boots — and
     * `test_no_two_sectors_in_a_row_go_quiet` refused it, because this gate is
     * what carries sector 14: emptied, 13, 14 and 15 go quiet in a row. The
     * beat coverage this table's own header is about was resting on the line
     * whose number was false, so the honest edit is one word. A duration in a
     * sentence is a fact about the clock; where the sentence is said is a fact
     * about the campaign, and only one of them was broken.
     *
     * `crew_duration_lines` in that script holds the pair — the number in the
     * words against the dial at the sector in the gate — and it holds *every*
     * row that counts minutes rather than this one by name, so a second clock
     * line is checked by having been written.
     */
    {"NINE MINUTES AND THIS ROOF IS SOMEBODY ELSE'S.", 14, 15, 0, 0},
    /* Gated with the radio line above, and for the same floor: there is no floor
     * above this one to be sent to. */
    {"THERE IS NO UPSTAIRS LEFT. THIS IS UPSTAIRS.", 17, 0, 0, 0},
    {"HE IS ONE FLOOR BEHIND US. HE IS ALWAYS ONE.", 0, 0, 1, 0},
};

/*
 * The beat a wall switch goes over. Short, loud and in the wrong order,
 * because nobody assembles a sentence while running at a switch.
 *
 * Barely gated, and that is the point of the kind: an alarm is only ever
 * raised by a man who has this second seen Chuck, so every line here already
 * has its own reason to be sayable. Only the two that count bodies, and the
 * one that is about the ending, ask for anything.
 */
static const CrewLine ALARM_LINES[] = {
    {"HE IS ON THIS FLOOR! I SAY AGAIN, THIS FLOOR!", 0, 0, 0, 0},
    {"GAME OVER! THAT IS IT, THAT IS GAME OVER!", 0, 0, 0, 0},
    {"SOMEBODY GET ON THE NET! WE HAVE A MAN INSIDE!", 0, 0, 0, 0},
    {"THAT IS NOT SECURITY! THAT IS NOT ANYBODY!", 0, 0, 0, 0},
    {"HE IS NOT A GUEST, VOSS! HE WAS NEVER A GUEST!", 0, 0, 0, 0},
    {"NO SHOT! HE IS BEHIND THE SLAB! NO SHOT!", 0, 0, 0, 0},
    {"LENZ! LENZ, ANSWER YOUR HANDSET!", 0, 0, 1, 0},
    {"GET TO THE ROOF! EVERYBODY GET TO THE ROOF!", 10, 0, 0, 0},
    {"IT IS THE MAN OFF THE PAVEMENT! IT IS HIM!", 0, 0, 0, 0},
    {"HE HAS A CARD! HE IS COMING UP THE STAIR!", 0, 0, 0, 0},
    /* Same claim as the boast in TALK_LINES and the same ceiling: a man
     * shouting that there are twelve of them has to be shouting it while there
     * are still about twelve of them. */
    {"TWELVE OF US! WHERE ARE THE OTHER ELEVEN?", 0, 0, 1, CREW_SIZE / 2},
};

/*
 * Leaned out of a window with a piece of the cornice in both hands. Four
 * hundred feet of air and nothing else to say, so it is all shouted at Chuck
 * rather than into a handset — the one place on the net where the crew talk to
 * him instead of about him.
 *
 * Ungated throughout: a thrower is only ever met on a climb, and the first
 * climb is sector 3, by which point everything here is true.
 */
static const CrewLine WALL_LINES[] = {
    {"GET OFF THE WALL, COWBOY!", 0, 0, 0, 0},
    {"FORTY FLOORS! COUNT THEM ON THE WAY DOWN!", 0, 0, 0, 0},
    {"THERE IS NOTHING UP HERE BUT US!", 0, 0, 0, 0},
    {"HEADS!", 0, 0, 0, 0},
    {"LOOK UP! I SAID LOOK UP!", 0, 0, 0, 0},
    {"THIS IS THE PART WHERE YOU LET GO!", 0, 0, 0, 0},
    {"NOBODY CLIMBS OUT OF THIS BUILDING!", 0, 0, 0, 0},
    {"SAY GOODNIGHT, WALL-CRAWLER!", 0, 0, 0, 0},
    {"VOSS SAYS YOU ARE NOT EVEN REAL!", 0, 0, 0, 0},
    /* The other half of the radio line above, and it stays the other half: the
     * running gag is that nobody is coming for him, which is the truest thing
     * anybody on this net says all night and is what the cordon bought. */
    {"STILL NO BACKUP!", 0, 0, 0, 0},
};

/*
 * The lobby, ninety seconds after the crew walked Ellen through it. These are
 * the only witnesses in the campaign and the only people in the building who
 * are not armed, and they get about four seconds of screen time each — so the
 * lines are what somebody actually shouts while leaving a room, not what they
 * would say if asked.
 *
 * Ungated, and structurally so: the evacuation is a one-shot part played at
 * `gameplay_ai_spawn_level_entities` in sector one, so the only night these
 * can be heard on is the one they are written for.
 */
static const CrewLine PANIC_LINES[] = {
    {"THEY HAVE GUNS! EVERYBODY OUT!", 0, 0, 0, 0},
    {"THEY TOOK THE NIGHT CONTROLLER!", 0, 0, 0, 0},
    {"THE STAIRS ARE WELDED! THE STAIRS ARE WELDED!", 0, 0, 0, 0},
    {"DO NOT GO UP THERE!", 0, 0, 0, 0},
    {"MAINTENANCE! THEY SAID THEY WERE MAINTENANCE!", 0, 0, 0, 0},
    {"SOMEBODY CALL SOMEBODY!", 0, 0, 0, 0},
    {"THERE IS NOBODY OUTSIDE! I LOOKED!", 0, 0, 0, 0},
    {"I AM NOT PAID ENOUGH FOR THIS!", 0, 0, 0, 0},
    {"GO! GO! DO NOT LOOK AT THEM!", 0, 0, 0, 0},
    {"THAT IS NOT THE NIGHT SHIFT!", 0, 0, 0, 0},
};

#define COUNT_OF(table) ((int)(sizeof(table) / sizeof((table)[0])))

typedef struct
{
    const CrewLine *lines;
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

static bool is_word_char(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9');
}

/* True when `line` uses `name` as a word of its own rather than as part of a
 * longer one, so a crew name can be looked for without a match inside some
 * other word ever counting. */
static bool line_names(const char *line, const char *name)
{
    if (line == NULL || name == NULL || name[0] == '\0')
        return false;
    for (int start = 0; line[start] != '\0'; ++start)
    {
        if (start > 0 && is_word_char(line[start - 1]))
            continue;
        int i = 0;
        while (name[i] != '\0' && line[start + i] == name[i])
            ++i;
        if (name[i] == '\0' && !is_word_char(line[start + i]))
            return true;
    }
    return false;
}

/* Whether the night has got far enough along for this to be said, and not so
 * far along that it has stopped being true. A situation nobody handed over is
 * every situation: `crew_line` and the tests ask about the table itself rather
 * than about a moment in a run.
 *
 * The ceiling is exclusive and nought means there is none, which is what lets
 * almost every row in the tables above go on writing two numbers instead of
 * three. */
static bool line_fits_the_night(const CrewLine *line,
                                const CrewSituation *situation)
{
    if (situation == NULL)
        return true;
    if (situation->sector < line->from_sector ||
        situation->hostiles_down < line->after_down)
        return false;
    if (line->until_sector != 0 && situation->sector >= line->until_sector)
        return false;
    return line->until_down == 0 ||
           situation->hostiles_down < line->until_down;
}

bool crew_line_allowed(ChatterKind kind, int index,
                       const CrewSituation *situation)
{
    int count = crew_line_count(kind);
    if (index < 0 || index >= count)
        return false;
    return line_fits_the_night(&TABLES[kind].lines[index], situation);
}

/*
 * How often a call leads with the newest thing the crew has to say, as one in
 * this many.
 *
 * **This exists because the middle third of the campaign had no guaranteed story
 * beat in it at all.** The report between sectors is the one channel the player
 * cannot miss, and it is shown only after a sector that leaves by its stair door
 * — so from sector 10 on, where the campaign alternates interior and climb and
 * every interior therefore needs a `Y`, the reports stop. They land after
 * sectors 1, 4, 5, 8, 9 and 16: five in the first nine sectors, then six
 * sectors of silence, then the payoff.
 *
 * What was supposed to cover that stretch is this table — `VAULT IS DRY` at 10,
 * `HE CAME UP THROUGH THE DUCTS` at 12, `NINE MINUTES AND THIS ROOF IS SOMEBODY
 * ELSE'S` at 14, and the pad at 17 — and it could not, because a gated line
 * competed on equal terms
 * with every ungated one. Fifteen-odd lines are sayable by then, so the odds of
 * any given call being the one beat that moves the plot were about one in
 * fifteen, on a call that also needs a live guard who is calm, on the ground and
 * inside `CHATTER_EARSHOT`. The plot was in the table and the player was not
 * going to hear it.
 *
 * A third rather than always: the freshest line is the one worth leading with,
 * but a net that says the same sentence every time somebody lifts a handset is a
 * net that reads as one line rather than as a crew. This applies at every tier,
 * not only the late ones — the newest beat is the right thing to lead with in the
 * lobby too.
 *
 * **And the gates have to reach the top of the building**, which they did not.
 * For a long time the highest was 14, so 15, 16 and 17 added nothing new: the
 * roof led with a line written for the penthouse, and sector 17 carried no fresh
 * beat on either channel at all. A ceiling on the gates is a ceiling on the plot,
 * and the sector it strands is always the last one — which is the one the whole
 * night is played for. `test_no_two_sectors_in_a_row_go_quiet` requires a quiet
 * sector to be a climb now, so a beat missing off an interior fails the build
 * rather than being spent out of the climbs' allowance.
 */
#define CREW_FRESH_LINE_EVERY 3

/*
 * The highest sector gate this situation has passed, or nought if the table's
 * sayable lines are all ungated. That is "the newest thing the crew has to say":
 * a gate is only ever put on a line because the line stopped being untrue at
 * that point in the night.
 */
static int newest_gate_reached(ChatterKind kind, const char *callsign,
                               const CrewSituation *situation)
{
    int count = crew_line_count(kind);
    int newest = 0;
    for (int i = 0; i < count; ++i)
    {
        const CrewLine *line = &TABLES[kind].lines[i];
        if (line->from_sector > newest &&
            !line_names(line->text, callsign) &&
            line_fits_the_night(line, situation))
            newest = line->from_sector;
    }
    return newest;
}

/*
 * The line, and the two reasons this man in this sector may not be the one
 * saying it.
 *
 * Several of these name a man: BRUNO is on the sixth lock, LENZ wants the
 * lights back on, nothing has come back from MARCO. The roll and the callsign
 * are drawn from different places — one off the RNG, one off the enemy slot —
 * so nothing stopped the two landing on the same man, and the strip printed
 * `MARCO: STILL NOTHING FROM MARCO.` And several more are only true once the
 * night has got somewhere: see `CrewLine` above.
 *
 * Stepping forward from the rolled index rather than re-rolling keeps both of
 * those out of the seeded stream entirely: the simulation still spends exactly
 * one number on a line however this resolves, so neither filter can move a
 * single seeded choice downstream of it. **The freshness pass below is held to
 * the same rule** — it reads a slice of the roll already drawn rather than
 * asking for a second one, so leading with the newest beat cannot shift a single
 * seeded choice either.
 */
const char *crew_line_in(ChatterKind kind, int roll, const char *callsign,
                         const CrewSituation *situation)
{
    int count = crew_line_count(kind);
    if (count <= 0)
        return NULL;
    if (roll < 0)
        roll = -roll;

    /*
     * Lead with the newest beat, one call in `CREW_FRESH_LINE_EVERY`.
     *
     * Only when a situation was handed over: `crew_line` and `crew_line_said_by`
     * pass NULL to ask about the table itself rather than about a moment in a
     * run, and the suite measures the table through them. A NULL situation is
     * every situation, which is no situation to be fresh in.
     */
    if (situation != NULL && roll % CREW_FRESH_LINE_EVERY == 0)
    {
        int newest = newest_gate_reached(kind, callsign, situation);
        if (newest > 0)
        {
            for (int step = 0; step < count; ++step)
            {
                const CrewLine *line =
                    &TABLES[kind].lines[(roll + step) % count];
                if (line->from_sector == newest &&
                    !line_names(line->text, callsign) &&
                    line_fits_the_night(line, situation))
                    return line->text;
            }
        }
        /* No gated line sayable here, or the one there is belongs to this
         * speaker: fall through to the ordinary walk rather than saying nothing.
         * Every line the walk can return was already allowed. */
    }

    for (int step = 0; step < count; ++step)
    {
        const CrewLine *line = &TABLES[kind].lines[(roll + step) % count];
        if (!line_names(line->text, callsign) &&
            line_fits_the_night(line, situation))
            return line->text;
    }
    /* Every line in the table ruled out for this man at this hour: say one
     * anyway rather than say nothing. Not reachable today —
     * `test_the_net_always_has_something_to_say` walks every kind through
     * every sector of the campaign with nobody down — and it must not become a
     * silent strip if somebody writes their way into it. */
    return TABLES[kind].lines[roll % count].text;
}

const char *crew_line_said_by(ChatterKind kind, int roll, const char *callsign)
{
    return crew_line_in(kind, roll, callsign, NULL);
}

const char *crew_line(ChatterKind kind, int roll)
{
    return crew_line_in(kind, roll, NULL, NULL);
}
