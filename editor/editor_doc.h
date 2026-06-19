#ifndef CHUCK_EDITOR_DOC_H
#define CHUCK_EDITOR_DOC_H

/*
 * The level editor's document: a map file as characters, before the game's
 * parser has made anything of them.
 *
 * The editor deliberately keeps the authored text rather than a parsed
 * `LevelMap`, because a map file says things a `LevelMap` cannot say back — the
 * difference between a space and a `.`, an unsupported decoration the parser
 * drops, a `THEME` line that is simply absent. Editing the text and handing it
 * to `level_load_data` for validation is what makes the editor's opinion of a
 * map the same as the game's.
 *
 * This module has no SDL in it so the test suite can link it and pin that
 * loading and saving every shipped level leaves the file byte-identical.
 */

#include "level.h"

#include <stddef.h>

#define ED_MAX_PATH 512
#define ED_UNDO_DEPTH 64
/* Metadata lines the editor does not understand are carried through untouched
 * rather than dropped on the next save. */
#define ED_MAX_EXTRA 8
#define ED_EXTRA_LEN 96

/* Everything a keystroke can change, and therefore everything undo restores. */
typedef struct
{
    char cells[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];
    int width;
    int height;
    bool facade; /* `MODE FACADE` */
    bool has_theme;
    LevelTheme theme;
    bool has_spawns;
    int spawn_count;
    int spawns[MAX_DOORS];
    int extra_count;
    char extra[ED_MAX_EXTRA][ED_EXTRA_LEN];
} EditorGrid;

typedef struct
{
    EditorGrid grid; /* the state on screen */
    char path[ED_MAX_PATH];
    bool dirty;

    EditorGrid undo[ED_UNDO_DEPTH];
    int undo_len;
    EditorGrid redo[ED_UNDO_DEPTH];
    int redo_len;
} EditorDoc;

/* The character an empty tile is written as. Facades pad with `.` because a
 * blank column on a wall reads as a hole in the file rather than as sky. */
char editor_doc_fill_char(const EditorDoc *doc);

void editor_doc_new(EditorDoc *doc, int width, int height, bool facade,
                    LevelTheme theme);

/* Replace the document with the contents of a map file. */
bool editor_doc_parse(EditorDoc *doc, const char *data, size_t size);
bool editor_doc_load(EditorDoc *doc, const char *path);

/* Write the document as a map file. Returns the number of bytes the text needs
 * (excluding the terminator), whether or not it fitted in `capacity`. */
size_t editor_doc_serialize(const EditorDoc *doc, char *out, size_t capacity);
bool editor_doc_save(EditorDoc *doc, const char *path);

char editor_doc_get(const EditorDoc *doc, int col, int row);
/* False when the write was refused (out of bounds) or changed nothing. */
bool editor_doc_set(EditorDoc *doc, int col, int row, char value);

/* Snapshot the current state so the next edit can be undone back to it. Call
 * once at the start of an edit, not once per painted tile. */
void editor_doc_checkpoint(EditorDoc *doc);
bool editor_doc_undo(EditorDoc *doc);
bool editor_doc_redo(EditorDoc *doc);
bool editor_doc_can_undo(const EditorDoc *doc);
bool editor_doc_can_redo(const EditorDoc *doc);

/* Geometry edits. All of them clamp to MAX_LEVEL_WIDTH / MAX_LEVEL_HEIGHT and
 * report false when the map is already at a limit. */
bool editor_doc_resize(EditorDoc *doc, int width, int height);
bool editor_doc_insert_row(EditorDoc *doc, int row);
bool editor_doc_delete_row(EditorDoc *doc, int row);
bool editor_doc_insert_col(EditorDoc *doc, int col);
bool editor_doc_delete_col(EditorDoc *doc, int col);
void editor_doc_mirror(EditorDoc *doc, int col0, int row0, int col1, int row1,
                       bool horizontal);

/* Parse the document through the game's own loader. `seed` fixes the choices
 * the loader makes (which card is live, which terminal is connected) so the
 * editor's view of a map does not flicker between frames. */
bool editor_doc_build_level(const EditorDoc *doc, Level *level, uint64_t seed);

/* The campaign index a path names (`levels/level7.txt` -> 7), or 0 for a file
 * that is not a numbered campaign level. */
int editor_path_level_number(const char *path);

#endif /* CHUCK_EDITOR_DOC_H */
