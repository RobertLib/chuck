/*
 * One row of paint per theme. See [level_paint.h](level_paint.h) for the
 * rules the rows keep and why the table lives on this side of the boundary.
 */

#include "level_paint.h"

/* Positional and unsized, in `LevelTheme` order, so a theme added without a
 * row fails the assertion below rather than drawing as black on black. */
static const LevelThemePaint PAINT[] = {
    /* PLANT — the machine floor: red-oxide plate and yellow hazard nosing. */
    {BBC_RED, BBC_YELLOW, BBC_YELLOW, BBC_BLACK, BBC_YELLOW, BBC_BLUE,
     BBC_YELLOW, true},
    /* LOBBY — white stone veined in cyan, brass at the lip. */
    {BBC_WHITE, BBC_CYAN, BBC_YELLOW, BBC_WHITE, BBC_YELLOW, BBC_BLUE,
     BBC_YELLOW, true},
    /* OFFICE — cyan partitions with blue seams, a white skirting. */
    {BBC_CYAN, BBC_BLUE, BBC_WHITE, BBC_CYAN, BBC_WHITE, BBC_BLUE, BBC_WHITE,
     true},
    /* SERVER — cyan racking in green circuit lines, lit by its own status
     * lights. */
    {BBC_CYAN, BBC_GREEN, BBC_WHITE, BBC_GREEN, BBC_GREEN, BBC_BLUE,
     BBC_GREEN, true},
    /* CANTEEN — white tile with red grout, a red lip. */
    {BBC_WHITE, BBC_RED, BBC_RED, BBC_WHITE, BBC_YELLOW, BBC_BLUE, BBC_YELLOW,
     true},
    /* LAB — white tile in green grout, cyan at the lip: clinical, and not
     * the storm wall's cyan it follows. */
    {BBC_WHITE, BBC_GREEN, BBC_CYAN, BBC_WHITE, BBC_GREEN, BBC_BLUE, BBC_WHITE,
     true},
    /* ARCHIVE — yellow timber with red joints: the one wooden floor. */
    {BBC_YELLOW, BBC_RED, BBC_RED, BBC_YELLOW, BBC_RED, BBC_BLUE, BBC_YELLOW,
     true},
    /* SECURITY — red-painted steel under white rivets: the floor that is
     * itself the warning. */
    {BBC_RED, BBC_WHITE, BBC_WHITE, BBC_RED, BBC_YELLOW, BBC_BLUE, BBC_WHITE,
     true},
    /* DUCTS — galvanised cyan sheet with blue seams, yellow walkways. */
    {BBC_CYAN, BBC_BLUE, BBC_YELLOW, BBC_BLACK, BBC_YELLOW, BBC_BLUE,
     BBC_WHITE, true},
    /* PENTHOUSE — magenta stone veined white, gold at the lip. */
    {BBC_MAGENTA, BBC_WHITE, BBC_YELLOW, BBC_MAGENTA, BBC_YELLOW, BBC_BLUE,
     BBC_YELLOW, true},
    /* ROOF — red brick parapets in white mortar, the sky behind. */
    {BBC_RED, BBC_WHITE, BBC_WHITE, BBC_RED, BBC_YELLOW, BBC_BLUE, BBC_WHITE,
     true},
    /* VAULT — gold-yellow steel, white bolts, red for the locks. */
    {BBC_YELLOW, BBC_WHITE, BBC_WHITE, BBC_YELLOW, BBC_RED, BBC_BLUE,
     BBC_WHITE, true},
    /* RESTROOM — white tile, cyan grout. */
    {BBC_WHITE, BBC_CYAN, BBC_CYAN, BBC_WHITE, BBC_CYAN, BBC_BLUE, BBC_WHITE,
     true},
    /* FACADE_NIGHT — white ledges over a black face laid in blue courses,
     * yellow where somebody left a light on. */
    {BBC_WHITE, BBC_CYAN, BBC_WHITE, BBC_CYAN, BBC_RED, BBC_BLUE, BBC_YELLOW,
     false},
    /* FACADE_STORM — cyan ledges, wet; the face blue. */
    {BBC_CYAN, BBC_BLUE, BBC_CYAN, BBC_WHITE, BBC_RED, BBC_BLUE, BBC_YELLOW,
     false},
    /* FACADE_MOON — white stone, every window dark but a few. */
    {BBC_WHITE, BBC_BLUE, BBC_WHITE, BBC_BLUE, BBC_RED, BBC_BLUE, BBC_YELLOW,
     false},
    /* FACADE_HIGH — gilded cornices at the top of the tower, under the
     * penthouse's magenta. */
    {BBC_YELLOW, BBC_WHITE, BBC_YELLOW, BBC_WHITE, BBC_RED, BBC_BLUE,
     BBC_YELLOW, false},
    /* FACADE_SLEET — the storm's wall later: white ledges iced in cyan. */
    {BBC_WHITE, BBC_CYAN, BBC_CYAN, BBC_WHITE, BBC_RED, BBC_BLUE, BBC_YELLOW,
     false},
};

_Static_assert(sizeof(PAINT) / sizeof(PAINT[0]) == LEVEL_THEME_COUNT,
               "every LevelTheme needs a row of paint");

const LevelThemePaint *level_theme_paint(LevelTheme theme)
{
    if ((int)theme < 0 || (int)theme >= LEVEL_THEME_COUNT)
        return &PAINT[LEVEL_THEME_PLANT];
    return &PAINT[theme];
}
