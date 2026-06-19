#ifndef CHUCK_MUSIC_ID_H
#define CHUCK_MUSIC_ID_H

/*
 * The tracks, named where a test can reach them.
 *
 * This is `sound_id.h`'s twin and it exists for the same reason: an id is not
 * a synthesiser. `audio.h` links SDL, so anything that names a track through
 * it is a file `make test` cannot compile — and the one table that maps a
 * theme to a track was therefore sitting in `level_art.c`, which the suite
 * links least of all.
 *
 * That mattered because the table is a designated-initializer array indexed by
 * `LevelTheme`. A theme added without a row does not fail to build; it gets a
 * zero-initialised entry, and the zero here is `MUSIC_INTRO` — so the missing
 * row plays the *title screen theme* over a sector, which is the loudest way a
 * silent table drift has ever been available to go wrong in this tree. The
 * mapping lives in `level.c` beside `level_theme_sublevel` now, for the reason
 * that file already gives: which score a floor gets is level data, not art
 * direction. `test_every_theme_names_a_score_of_its_own` is what holds it.
 */
typedef enum
{
    MUSIC_INTRO = 0, /* title screen, cutscenes and the game-over card */
    MUSIC_PURSUIT,   /* the prologue drive */
    MUSIC_LOBBY,
    MUSIC_OFFICE,
    MUSIC_SERVER,
    MUSIC_PLANT,
    MUSIC_CANTEEN,
    MUSIC_LAB,
    MUSIC_ARCHIVE,
    MUSIC_SECURITY,
    MUSIC_DUCTS,
    MUSIC_PENTHOUSE,
    MUSIC_ROOF,
    MUSIC_VAULT,
    MUSIC_RESTROOM,
    MUSIC_FACADE_NIGHT,
    MUSIC_FACADE_STORM,
    MUSIC_FACADE_MOON,
    MUSIC_FACADE_HIGH,
    MUSIC_FACADE_SLEET,
    MUSIC_TRACK_COUNT
} MusicTrack;

#endif /* CHUCK_MUSIC_ID_H */
