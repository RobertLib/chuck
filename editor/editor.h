#ifndef CHUCK_EDITOR_H
#define CHUCK_EDITOR_H

/*
 * The level editor's application state.
 *
 * The editor is a separate binary rather than a mode of the game, but it is not
 * a separate idea of what a level is: it links the game's own parser
 * (level.c), the game's own art direction (level_art.c) and the route model the
 * test suite judges a sector with (level_route.c). What it draws on the canvas
 * is what the game will draw, and what it says about a map is what `make test`
 * will say about it.
 */

#include "editor_doc.h"
#include "editor_legend.h"
#include "editor_validate.h"

#include <SDL3/SDL.h>

#define ED_MAX_FILES 64

/* Chrome metrics. The canvas gets whatever is left. */
#define ED_TOP_H 56
#define ED_LEFT_W 220
#define ED_RIGHT_W 352
#define ED_BOTTOM_H 26

typedef enum
{
    ED_TOOL_BRUSH = 0,
    ED_TOOL_LINE,
    ED_TOOL_RECT,   /* outline */
    ED_TOOL_BOX,    /* filled */
    ED_TOOL_FILL,   /* flood */
    ED_TOOL_PICK,   /* eyedropper */
    ED_TOOL_SELECT, /* rectangular selection */
    ED_TOOL_COUNT
} EdTool;

typedef struct
{
    char path[ED_MAX_PATH];
    char label[48];
    int number; /* campaign sector, 0 for a sublevel */
    bool sublevel;
} EdFile;

typedef struct
{
    char cells[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];
    int width, height;
} EdClipboard;

/*
 * The playtest's build, running beside the editor rather than inside it.
 *
 * `make` takes seconds, and it used to be run straight from the keypress that
 * asked for it — so the window stopped answering the moment the author pressed
 * playtest, drew nothing, and on macOS collected a spinning cursor. The status
 * line said `Building...` and was the one thing on screen that could not be
 * repainted to say it.
 *
 * The read stays blocking, on a thread of its own: draining the pipe as `make`
 * writes it is what keeps a build with a screenful of errors from filling the
 * buffer and wedging against a reader that is off drawing a frame. The thread
 * touches nothing of the app's — it fills this struct and sets `finished` as
 * its last act, and the main loop joins it and does everything that reaches the
 * screen or launches anything.
 */
typedef struct
{
    SDL_Thread *thread;
    SDL_AtomicInt finished;
    bool ok;
    char error[256];
    int level; /* the sector to open once the build succeeds */
} EdBuild;

typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *canvas_texture;
    int canvas_texture_w, canvas_texture_h;
    int win_w, win_h;
    bool quit;
    float time;

    EdFile files[ED_MAX_FILES];
    int file_count;
    int current_file; /* index into files, -1 when the map is unsaved */

    EditorDoc doc;
    Level level; /* the document, parsed by the game's own loader */
    bool level_ok;
    EdReport report;
    EdCampaign campaign;
    bool revalidate;

    /* View. */
    float cam_x, cam_y;
    float zoom;
    bool art_mode;
    bool show_grid;
    bool show_route;
    bool show_help;

    /* Editing. */
    EdTool tool;
    char brush;
    int cursor_col, cursor_row;
    bool painting;
    bool erasing;
    bool dragging;
    int drag_col, drag_row;
    bool panning;
    float pan_x, pan_y;
    bool has_selection;
    int sel_col0, sel_row0, sel_col1, sel_row1;
    EdClipboard clipboard;
    bool clipboard_valid;

    /* The playtest build, while one is running. */
    EdBuild build;

    /* Chrome. */
    SDL_FRect canvas;
    SDL_FRect left_panel;
    SDL_FRect right_panel;
    float mouse_x, mouse_y;
    bool clicked; /* a left press landed outside the canvas this frame */
    int findings_scroll;
    float palette_scroll; /* the left panel is taller than a short window */
    float palette_overflow;
    /* Which discard the author has already been warned about; doing the same
     * thing twice is what confirms it. */
    int confirm_token;
    char hint[192];   /* what the thing under the pointer is, rebuilt each frame */
    char status[192]; /* what the last action did */
    float status_timer;
    bool status_bad;
} EditorApp;

/* ---- editor_app.c ------------------------------------------------------ */

void ed_status(EditorApp *app, bool bad, const char *format, ...);
void ed_refresh_level(EditorApp *app);
void ed_scan_files(EditorApp *app);
bool ed_open_file(EditorApp *app, int index);
bool ed_save(EditorApp *app);
void ed_new_level(EditorApp *app, bool facade);
void ed_reload(EditorApp *app);
void ed_playtest(EditorApp *app);
/* Polled once a frame: joins a finished playtest build and launches the game,
 * or reports what `make` said. Does nothing while one is still running, and
 * nothing at all when none is. */
void ed_update_playtest(EditorApp *app);
/* Joins a build still running at shutdown, so the process does not close the
 * window out from under a thread still reading a pipe. */
void ed_cancel_playtest(EditorApp *app);
void ed_focus_tile(EditorApp *app, int col, int row);
void ed_center_view(EditorApp *app);
void ed_set_zoom(EditorApp *app, float zoom, float anchor_x, float anchor_y);
void ed_apply_undo(EditorApp *app, bool redo);
void ed_copy_selection(EditorApp *app, bool cut);
void ed_paste(EditorApp *app);
void ed_clear_selection_tiles(EditorApp *app);
void ed_mirror(EditorApp *app, bool horizontal);
void ed_cycle_theme(EditorApp *app, int step);
void ed_toggle_facade(EditorApp *app);
void ed_resize(EditorApp *app, int dw, int dh);
void ed_insert_row(EditorApp *app, bool insert);
void ed_insert_col(EditorApp *app, bool insert);
void ed_sync_spawns(EditorApp *app);

/* ---- editor_ui.c ------------------------------------------------------- */

void ed_text(EditorApp *app, float x, float y, float scale, SDL_Color color,
             const char *format, ...);
float ed_text_width(const char *text, float scale);
bool ed_button(EditorApp *app, SDL_FRect rect, const char *label, bool active,
               const char *tooltip);
void ed_draw_chrome(EditorApp *app);
void ed_layout(EditorApp *app);

/* ---- editor_render.c --------------------------------------------------- */

void ed_draw_canvas(EditorApp *app);
/* One legend character as the icon the palette and the canvas both use. */
void ed_draw_symbol(SDL_Renderer *renderer, char symbol, float x, float y,
                    float size, float time);
SDL_Color ed_symbol_color(char symbol);

#endif /* CHUCK_EDITOR_H */
