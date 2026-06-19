/*
 * The pause menu's three rows.
 *
 * Nothing here draws anything: the table is the sheet and `game_render.c` walks
 * it, the same split the options sheet and the credits roll use and for the same
 * reason — the suite links no SDL, and a row that outgrows its plate has to fail
 * a build rather than draw over the row beside it.
 *
 * Three rows because a paused run has exactly three things the player can want
 * from it, and the one that cannot be taken back is last and drawn in the danger
 * red by the renderer: a list where ABANDON RUN looks like RESUME is a list
 * somebody eventually loses a run to.
 */

#include "pause_sheet.h"

#define PAUSE_ROW_ENTRY(name, label, detail) {label, detail},

const PauseRow PAUSE_SHEET_ROWS[] = {PAUSE_ROWS(PAUSE_ROW_ENTRY)};

#undef PAUSE_ROW_ENTRY

int pause_sheet_row_count(void)
{
    return (int)(sizeof(PAUSE_SHEET_ROWS) / sizeof(PAUSE_SHEET_ROWS[0]));
}
