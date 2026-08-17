#include "editor_legend.h"

#include <stddef.h>

const char *const ED_GROUP_NAMES[] = {
    "Terrain", "Route", "Items", "Enemies", "People",
    "Hazards", "Fittings", "Office props", "Front of house",
    "Restroom", "Plant props", "The night", "Facade"};

/* Unsized, so this measures the list against the enum; see the note on
 * `LEVEL_THEME_NAMES` in src/level.c. */
_Static_assert(sizeof(ED_GROUP_NAMES) / sizeof(ED_GROUP_NAMES[0]) ==
                   (size_t)ED_GROUP_COUNT,
               "every palette group needs a name");

/* Colours follow fx.h's vocabulary: cyan is security and technology, amber is
 * light and warning, red is danger, green is granted access. */
const EdSymbol ED_SYMBOLS[] = {
    {' ', "Air", "Empty space the player moves through", ED_GROUP_TERRAIN,
     30, 38, 52, false, false, false},
    {'.', "Padding", "Empty space written as a dot; how facades pad the sky",
     ED_GROUP_TERRAIN, 24, 30, 42, false, false, false},
    {'#', "Wall", "Solid tile. On a facade it is cornice masonry and cover",
     ED_GROUP_TERRAIN, 104, 121, 137, true, false, false},
    {'%', "Weak wall",
     "A blocked-up opening: solid until an explosion takes it out. A shortcut, never the only way",
     ED_GROUP_TERRAIN, 146, 118, 100, true, true, false},
    {'H', "Ladder", "Climbed up and down; stood on at the top",
     ED_GROUP_TERRAIN, 168, 116, 62, false, true, false},
    {'V', "Lift shaft", "Vertical elevator track; a run of two or more carries a platform",
     ED_GROUP_TERRAIN, 74, 222, 212, false, true, false},
    {'F', "Falling panel", "Falls away for the rest of the run once stepped on",
     ED_GROUP_TERRAIN, 168, 112, 40, false, true, false},
    {'P', "Moving platform", "Patrols the open run of its row",
     ED_GROUP_TERRAIN, 96, 230, 140, false, true, false},

    {'S', "Start", "Where the player comes in", ED_GROUP_ROUTE,
     70, 156, 180, false, false, false},
    {'E', "Exit door", "The stair door out. Welded shut when the map also has a window",
     ED_GROUP_ROUTE, 96, 230, 140, false, true, false},
    {'Y', "Window", "Traversable window: out onto the climb, or back inside",
     ED_GROUP_ROUTE, 74, 222, 212, false, false, false},
    {'D', "Paired door", "Doors teleport in pairs, matched 0-1, 2-3, ...",
     ED_GROUP_ROUTE, 168, 116, 62, false, true, false},
    {'U', "Restroom door", "Entrance to the sublevel", ED_GROUP_ROUTE,
     156, 173, 186, false, true, false},
    {'R', "Return door", "Way back out of a sublevel", ED_GROUP_ROUTE,
     156, 173, 186, false, true, false},

    {'C', "Key card", "One of them opens the exit; the seed decides which",
     ED_GROUP_ITEMS, 248, 188, 74, false, false, false},
    {'G', "Pistol", "Ammunition for the sidearm", ED_GROUP_ITEMS,
     156, 173, 186, false, false, false},
    {'N', "Grenade", "Thrown explosive", ED_GROUP_ITEMS, 122, 132, 88,
     false, false, false},
    {'K', "Medkit", "Restores health", ED_GROUP_ITEMS, 232, 74, 62,
     false, false, false},
    {'Z', "Bazooka", "One rocket, no respawn; interiors only",
     ED_GROUP_ITEMS, 248, 188, 74, false, true, false},
    {'!', "Flash charge",
     "Blinds guards and cameras for a few seconds. Hurts nobody and opens nothing; the seconds are for leaving",
     ED_GROUP_ITEMS, 156, 173, 186, false, true, false},
    {'*', "Docket sheet",
     "Proof of what tonight actually is. Scores, changes nothing else, and one belongs in every interior",
     ED_GROUP_ITEMS, 236, 238, 224, false, true, false},

    {'M', "Guard", "Patrolling enemy", ED_GROUP_ENEMIES, 232, 74, 62,
     false, true, false},
    {'W', "Guard and dog", "Enemy with a guard dog", ED_GROUP_ENEMIES,
     232, 110, 62, false, true, false},
    {'Q', "Heavy guard",
     "Plate carrier and helmet: twice the rounds, slower, and cannot be stomped. The blade behind him still works",
     ED_GROUP_ENEMIES, 156, 173, 186, false, true, false},

    {'J', "Janitor", "Ambient NPC, takes no part in the fight",
     ED_GROUP_PEOPLE, 70, 156, 180, false, true, false},
    {'f', "Civilian", "Freezes, shouts and runs for the player's start tile",
     ED_GROUP_PEOPLE, 216, 160, 110, false, true, false},
    {'k', "Receptionist", "Works a post at the counter and returns to it",
     ED_GROUP_PEOPLE, 216, 160, 110, false, true, false},

    {'X', "Mine", "Explodes when stepped on", ED_GROUP_HAZARDS,
     232, 74, 62, false, true, false},
    {'^', "Spike", "Area denial; a single one can be hopped with headroom",
     ED_GROUP_HAZARDS, 232, 74, 62, false, true, false},
    {'O', "Ceiling fan", "Blades cost a heart; hangs on a rod from the slab above",
     ED_GROUP_HAZARDS, 232, 74, 62, false, true, false},
    {'L', "Gas canister", "Crawl and shoot it to set off an explosion",
     ED_GROUP_HAZARDS, 96, 230, 140, false, true, false},
    {'B', "Crate", "Shovable, and destroyed by shots and blasts",
     ED_GROUP_HAZARDS, 168, 116, 62, false, true, false},

    {'T', "Terminal", "One of them is live and unlocks the exit",
     ED_GROUP_FITTINGS, 74, 222, 212, false, true, false},
    {'A', "Alarm switch", "A guard who has seen the player may run to it",
     ED_GROUP_FITTINGS, 248, 188, 74, false, true, false},
    {'I', "Camera",
     "Sweeps a beam across the floor and raises the alarm. Hangs from the slab above it, not the floor below",
     ED_GROUP_FITTINGS, 255, 76, 58, false, true, false},

    {'c', "Chair", "Office chair", ED_GROUP_OFFICE, 82, 100, 120,
     false, true, false},
    {'d', "Desk", "Office desk with a computer", ED_GROUP_OFFICE,
     82, 100, 120, false, true, false},
    {'i', "Equipment", "Filing cabinet, printer or server rack",
     ED_GROUP_OFFICE, 82, 100, 120, false, true, false},

    {'n', "Counter", "Reception counter; renders over whoever staffs it",
     ED_GROUP_LOBBY, 120, 104, 78, false, true, false},
    {'s', "Bench", "Waiting-area seat", ED_GROUP_LOBBY, 120, 104, 78,
     false, true, false},
    {'t', "Planter", "Palm in a stone planter", ED_GROUP_LOBBY,
     72, 128, 92, false, true, false},
    {'g', "Security gate", "Optical gate; why the door upstairs wants a card",
     ED_GROUP_LOBBY, 74, 222, 212, false, true, false},

    {'q', "Toilet", "Restroom fitting", ED_GROUP_RESTROOM, 156, 173, 186,
     false, true, false},
    {'b', "Washbasin", "Restroom fitting; a mirror is drawn above it",
     ED_GROUP_RESTROOM, 156, 173, 186, false, true, false},
    {'u', "Urinal", "Restroom fitting", ED_GROUP_RESTROOM, 156, 173, 186,
     false, true, false},
    {'p', "Partition", "Stall partition", ED_GROUP_RESTROOM, 140, 150, 160,
     false, true, false},
    {'o', "Open stall", "Stall with a visible toilet", ED_GROUP_RESTROOM,
     140, 150, 160, false, true, false},
    {'z', "Closed stall", "Stall with the door shut", ED_GROUP_RESTROOM,
     140, 150, 160, false, true, false},

    {'a', "Pallet",
     "A pallet of drums or sacks. The stacked thing a machine hall, a duct run or a strongroom is full of",
     ED_GROUP_PLANT, 132, 104, 68, false, true, false},
    {'e', "Cable reel",
     "Stood on its edge with the tail run off to one side; plant, roof deck and anywhere cable was pulled",
     ED_GROUP_PLANT, 140, 110, 74, false, true, false},
    {'j', "Pipe rail",
     "Two risers, a run across them and a hand wheel on the valve: the plumbing a plant room is made of",
     ED_GROUP_PLANT, 104, 121, 137, false, true, false},
    {'l', "Bollard",
     "A short post with a reflective band, for a service deck or a goods route something is driven along",
     ED_GROUP_PLANT, 248, 188, 74, false, true, false},

    {'m', "Flight case",
     "What Meridian wheeled in through the goods entrance; some of them stand shut and some lie open and empty",
     ED_GROUP_NIGHT, 146, 118, 100, false, true, false},
    {'w', "Wall clock",
     "Reads the sector's hour on the way to 01:00. Hangs from the slab above it, not the floor below",
     ED_GROUP_NIGHT, 198, 62, 50, false, true, false},

    {'r', "Thrower window", "Leans out and throws; telegraphed before release",
     ED_GROUP_FACADE, 232, 74, 62, false, false, true},
    {'v', "Bird entry", "Birds cross toward the climber from here",
     ED_GROUP_FACADE, 248, 188, 74, false, false, true},
};

const int ED_SYMBOL_COUNT = (int)(sizeof(ED_SYMBOLS) / sizeof(ED_SYMBOLS[0]));

const EdSymbol *editor_symbol(char symbol)
{
    for (int i = 0; i < ED_SYMBOL_COUNT; ++i)
    {
        if (ED_SYMBOLS[i].symbol == symbol)
            return &ED_SYMBOLS[i];
    }
    return NULL;
}

bool editor_symbol_hangs(char symbol)
{
    /* The two fittings that ask the tile *above* them for support. Everything
     * else in the legend stands on the one below, and the validator reports
     * either mistake in the same sentence. */
    return symbol == 'w' || symbol == 'I';
}
