#define SDL_MAIN_USE_CALLBACKS 1

#include "editor.h"

#include "fx.h"

#include <SDL3/SDL_main.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* The seed the editor parses with. Which card is live and which terminal is
 * connected is a per-run choice; fixing it here keeps the picture still while
 * a map is being drawn. */
#define ED_SEED 20260728u

#define ED_ZOOM_MIN 0.2f
#define ED_ZOOM_MAX 4.0f

void ed_status(EditorApp *app, bool bad, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    SDL_vsnprintf(app->status, sizeof(app->status), format, args);
    va_end(args);
    app->status_timer = 5.0f;
    app->status_bad = bad;
}

/* ---- Files ------------------------------------------------------------- */

static bool directory_exists(const char *path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

static bool has_suffix(const char *text, const char *suffix)
{
    size_t length = SDL_strlen(text);
    size_t want = SDL_strlen(suffix);
    return length >= want && SDL_strcmp(text + length - want, suffix) == 0;
}

/* The editor works in the repository, so every path it writes is the path the
 * Makefile and the tests use. */
static bool enter_repository(const char *hint)
{
    const char *candidates[4];
    int count = 0;
    if (hint != NULL)
        candidates[count++] = hint;
    candidates[count++] = ".";
    const char *base = SDL_GetBasePath();
    if (base != NULL)
        candidates[count++] = base;

    for (int i = 0; i < count; ++i)
    {
        char path[ED_MAX_PATH];
        SDL_snprintf(path, sizeof(path), "%s", candidates[i]);
        for (int up = 0; up < 4; ++up)
        {
            char levels[ED_MAX_PATH];
            SDL_snprintf(levels, sizeof(levels), "%s/levels", path);
            if (directory_exists(levels))
                return chdir(path) == 0;
            SDL_snprintf(levels, sizeof(levels), "%s/..", path);
            SDL_snprintf(path, sizeof(path), "%s", levels);
        }
    }
    return false;
}

static void sort_files(EditorApp *app)
{
    for (int i = 1; i < app->file_count; ++i)
    {
        EdFile key = app->files[i];
        int j = i - 1;
        while (j >= 0 &&
               ((app->files[j].sublevel && !key.sublevel) ||
                (app->files[j].sublevel == key.sublevel &&
                 app->files[j].number > key.number)))
        {
            app->files[j + 1] = app->files[j];
            j--;
        }
        app->files[j + 1] = key;
    }
}

static void add_files_from(EditorApp *app, const char *directory,
                           const char *pattern, bool sublevel)
{
    int count = 0;
    char **names = SDL_GlobDirectory(directory, pattern, 0, &count);
    if (names == NULL)
        return;
    for (int i = 0; i < count && app->file_count < ED_MAX_FILES; ++i)
    {
        EdFile *file = &app->files[app->file_count++];
        SDL_snprintf(file->path, sizeof(file->path), "%s/%s", directory,
                     names[i]);
        SDL_snprintf(file->label, sizeof(file->label), "%s", names[i]);
        /* The tab shows a sublevel by name, so drop the extension. */
        char *dot = SDL_strrchr(file->label, '.');
        if (dot != NULL)
            *dot = '\0';
        file->number = editor_path_level_number(file->path);
        file->sublevel = sublevel;
    }
    SDL_free(names);
}

static void scan_campaign(EditorApp *app)
{
    static Level level;
    SDL_zero(app->campaign);
    for (int i = 0; i < app->file_count; ++i)
    {
        const EdFile *file = &app->files[i];
        if (file->number <= 0)
            continue;
        size_t size = 0;
        void *data = SDL_LoadFile(file->path, &size);
        if (data == NULL)
            continue;
        Rng rng;
        rng_seed(&rng, ED_SEED);
        if (level_load_data(&level, file->path, (const char *)data, size, &rng))
            editor_campaign_record(&app->campaign, file->number, &level);
        SDL_free(data);
    }
}

void ed_scan_files(EditorApp *app)
{
    app->file_count = 0;
    add_files_from(app, "levels", "level*.txt", false);
    add_files_from(app, "levels/sublevels", "*.txt", true);
    sort_files(app);
    scan_campaign(app);

    /* Keep pointing at the same file after a rescan. */
    app->current_file = -1;
    for (int i = 0; i < app->file_count; ++i)
    {
        if (SDL_strcmp(app->files[i].path, app->doc.path) == 0)
            app->current_file = i;
    }
}

void ed_refresh_level(EditorApp *app)
{
    app->level_ok = editor_doc_build_level(&app->doc, &app->level, ED_SEED);
    editor_validate(&app->doc, &app->level, app->level_ok, &app->campaign,
                    &app->report);
    app->findings_scroll = 0;
    app->revalidate = false;
}

void ed_center_view(EditorApp *app)
{
    float world_w = (float)app->doc.grid.width * TILE_SIZE;
    float world_h = (float)app->doc.grid.height * TILE_SIZE;
    float zoom_x = app->canvas.w / (world_w + 32.0f);
    float zoom_y = app->canvas.h / (world_h + 32.0f);
    app->zoom = SDL_min(zoom_x, zoom_y);
    if (app->zoom < ED_ZOOM_MIN)
        app->zoom = ED_ZOOM_MIN;
    if (app->zoom > 2.0f)
        app->zoom = 2.0f;
    app->cam_x = (world_w - app->canvas.w / app->zoom) * 0.5f;
    app->cam_y = (world_h - app->canvas.h / app->zoom) * 0.5f;
}

void ed_focus_tile(EditorApp *app, int col, int row)
{
    app->cam_x = ((float)col + 0.5f) * TILE_SIZE - app->canvas.w / app->zoom * 0.5f;
    app->cam_y = ((float)row + 0.5f) * TILE_SIZE - app->canvas.h / app->zoom * 0.5f;
}

/*
 * Opening another map throws away whatever is unsaved in this one, so the
 * first attempt only says so. There is no dialog to click through: pressing
 * the same thing again is the confirmation.
 */
static bool confirm_discard(EditorApp *app, int token)
{
    if (!app->doc.dirty)
        return true;
    if (app->confirm_token == token)
    {
        app->confirm_token = 0;
        return true;
    }
    app->confirm_token = token;
    ed_status(app, true, "%s has unsaved changes - do that again to discard them",
              app->doc.path[0] != '\0' ? app->doc.path : "This map");
    return false;
}

bool ed_open_file(EditorApp *app, int index)
{
    if (index < 0 || index >= app->file_count)
        return false;
    if (index != app->current_file && !confirm_discard(app, 1000 + index))
        return false;
    if (!editor_doc_load(&app->doc, app->files[index].path))
    {
        ed_status(app, true, "Could not read %s", app->files[index].path);
        return false;
    }
    app->current_file = index;
    app->confirm_token = 0;
    app->has_selection = false;
    ed_refresh_level(app);
    ed_center_view(app);
    ed_status(app, false, "Opened %s", app->files[index].path);
    return true;
}

bool ed_save(EditorApp *app)
{
    if (app->doc.path[0] == '\0')
    {
        ed_status(app, true, "This map has no file yet");
        return false;
    }
    if (!editor_doc_save(&app->doc, app->doc.path))
    {
        ed_status(app, true, "Could not write %s", app->doc.path);
        return false;
    }
    scan_campaign(app);
    ed_refresh_level(app);
    app->confirm_token = 0;
    ed_status(app, false, "Saved %s", app->doc.path);
    return true;
}

void ed_reload(EditorApp *app)
{
    if (app->current_file < 0)
    {
        ed_status(app, true, "Nothing to reload");
        return;
    }
    if (!confirm_discard(app, 1))
        return;
    int index = app->current_file;
    app->current_file = -1;
    ed_open_file(app, index);
}

/* A new sector may not repeat a size the campaign already uses, so the editor
 * picks one that is free rather than leaving the author to find out from the
 * test suite. */
static void unique_size(const EdCampaign *campaign, int *width, int *height)
{
    for (int attempt = 0; attempt < 64; ++attempt)
    {
        bool taken = false;
        for (int i = 0; i < campaign->count; ++i)
        {
            if (!campaign->levels[i].loaded)
                continue;
            if (campaign->levels[i].width == *width &&
                campaign->levels[i].height == *height)
            {
                taken = true;
                break;
            }
        }
        if (!taken)
            return;
        (*width)++;
    }
}

void ed_new_level(EditorApp *app, bool facade)
{
    if (!confirm_discard(app, facade ? 3 : 2))
        return;

    int number = 1;
    for (int i = 0; i < app->file_count; ++i)
    {
        if (app->files[i].number >= number)
            number = app->files[i].number + 1;
    }

    int width = facade ? 25 : 46;
    int height = facade ? 40 : 18;
    if (facade)
    {
        /* Every climb is taller than the one before it. */
        for (int i = 0; i < app->campaign.count; ++i)
        {
            if (app->campaign.levels[i].loaded &&
                app->campaign.levels[i].facade &&
                app->campaign.levels[i].height >= height)
            {
                height = app->campaign.levels[i].height + 2;
            }
        }
    }
    if (height > MAX_LEVEL_HEIGHT)
        height = MAX_LEVEL_HEIGHT;
    unique_size(&app->campaign, &width, &height);

    LevelTheme theme = facade ? LEVEL_THEME_FACADE_NIGHT : LEVEL_THEME_PLANT;
    editor_doc_new(&app->doc, width, height, facade, theme);
    SDL_snprintf(app->doc.path, sizeof(app->doc.path), "levels/level%d.txt",
                 number);
    app->doc.dirty = true;

    if (!editor_doc_save(&app->doc, app->doc.path))
    {
        ed_status(app, true, "Could not create %s", app->doc.path);
        return;
    }
    ed_scan_files(app);
    ed_refresh_level(app);
    ed_center_view(app);
    ed_status(app, false, "Created %s as sector %d (%dx%d)", app->doc.path,
              number, width, height);
}

/* ---- Playtest ---------------------------------------------------------- */

/*
 * The build itself, on the playtest thread and touching nothing but its own
 * struct. Reading the pipe to the end as `make` writes it is the point: a build
 * that fails with a screenful of errors fills the buffer, and a reader that
 * only came back between frames would leave `make` blocked on a write.
 */
static int SDLCALL build_thread(void *data)
{
    EdBuild *build = (EdBuild *)data;

    FILE *pipe = popen("make 2>&1", "r");
    if (pipe == NULL)
    {
        SDL_snprintf(build->error, sizeof(build->error), "could not run make");
        build->ok = false;
        SDL_SetAtomicInt(&build->finished, 1);
        return 0;
    }

    char line[512];
    char last_error[256] = "";
    while (fgets(line, sizeof(line), pipe) != NULL)
    {
        if (SDL_strstr(line, "error") != NULL ||
            SDL_strstr(line, "Error") != NULL)
        {
            SDL_snprintf(last_error, sizeof(last_error), "%s", line);
        }
    }
    int status = pclose(pipe);

    char *newline = SDL_strchr(last_error, '\n');
    if (newline != NULL)
        *newline = '\0';
    SDL_snprintf(build->error, sizeof(build->error), "%s",
                 last_error[0] != '\0' ? last_error : "see the terminal");
    build->ok = status == 0;
    /* Last, and after everything above it: the main loop reads the rest of this
     * struct only once it has seen this. */
    SDL_SetAtomicInt(&build->finished, 1);
    return 0;
}

static void launch_playtest(EditorApp *app, int number)
{
    char level_arg[16];
    SDL_snprintf(level_arg, sizeof(level_arg), "%d", number);
    const char *args[] = {"./chuck", "--level", level_arg, NULL};
    SDL_Process *process = SDL_CreateProcess(args, false);
    if (process == NULL)
    {
        ed_status(app, true, "Could not launch the game: %s", SDL_GetError());
        return;
    }
    /* Destroying the handle leaves the game running on its own. */
    SDL_DestroyProcess(process);
    ed_status(app, false, "Playing sector %d", number);
}

void ed_playtest(EditorApp *app)
{
    if (app->build.thread != NULL)
    {
        ed_status(app, false, "Already building...");
        return;
    }
    if (!ed_save(app))
        return;

    int number = editor_path_level_number(app->doc.path);
    if (number <= 0)
    {
        /* A sublevel is entered through a sector's 'U', so play the first one
         * that has a door into it. */
        for (int i = 0; i < app->campaign.count && number <= 0; ++i)
        {
            if (app->campaign.levels[i].loaded &&
                app->campaign.levels[i].has_sublevel_entrance)
            {
                number = i + 1;
            }
        }
        if (number <= 0)
        {
            ed_status(app, true, "No sector opens onto this sublevel");
            return;
        }
    }

    app->build.level = number;
    app->build.ok = false;
    app->build.error[0] = '\0';
    SDL_SetAtomicInt(&app->build.finished, 0);
    app->build.thread = SDL_CreateThread(build_thread, "chuck-editor-build",
                                         &app->build);
    if (app->build.thread == NULL)
    {
        ed_status(app, true, "Could not start the build: %s", SDL_GetError());
        return;
    }
    ed_status(app, false, "Building sector %d...", number);
}

void ed_update_playtest(EditorApp *app)
{
    if (app->build.thread == NULL)
        return;

    if (SDL_GetAtomicInt(&app->build.finished) == 0)
    {
        /* Re-stamped every frame, because a status line stands for five seconds
         * and a full rebuild takes longer than that: left to expire, the one
         * thing on screen saying why nothing is happening would go out halfway
         * through the wait. The moving dots are the other half of the answer —
         * they are the frame being drawn, which is what says the window is
         * still alive rather than what it used to be, which was frozen. */
        static const char *const DOTS[] = {"", ".", "..", "..."};
        int step = (int)(app->time * 3.0f) & 3;
        ed_status(app, false, "Building sector %d%s", app->build.level,
                  DOTS[step]);
        return;
    }

    SDL_WaitThread(app->build.thread, NULL);
    app->build.thread = NULL;

    if (!app->build.ok)
    {
        ed_status(app, true, "make failed: %s", app->build.error);
        return;
    }
    launch_playtest(app, app->build.level);
}

void ed_cancel_playtest(EditorApp *app)
{
    if (app->build.thread == NULL)
        return;
    /* There is nothing to cancel — `make` is a child of this process and will
     * finish whatever happens. Waiting for it is what stops the window and the
     * renderer being torn down while the thread is still holding a pipe. */
    SDL_WaitThread(app->build.thread, NULL);
    app->build.thread = NULL;
}

/* ---- View -------------------------------------------------------------- */

void ed_set_zoom(EditorApp *app, float zoom, float anchor_x, float anchor_y)
{
    if (zoom < ED_ZOOM_MIN)
        zoom = ED_ZOOM_MIN;
    if (zoom > ED_ZOOM_MAX)
        zoom = ED_ZOOM_MAX;

    float world_x = app->cam_x + (anchor_x - app->canvas.x) / app->zoom;
    float world_y = app->cam_y + (anchor_y - app->canvas.y) / app->zoom;
    app->zoom = zoom;
    app->cam_x = world_x - (anchor_x - app->canvas.x) / zoom;
    app->cam_y = world_y - (anchor_y - app->canvas.y) / zoom;
}

/* ---- Edits ------------------------------------------------------------- */

static void begin_edit(EditorApp *app)
{
    editor_doc_checkpoint(&app->doc);
}

static void changed(EditorApp *app)
{
    app->revalidate = true;
}

void ed_apply_undo(EditorApp *app, bool redo)
{
    bool ok = redo ? editor_doc_redo(&app->doc) : editor_doc_undo(&app->doc);
    if (!ok)
    {
        ed_status(app, true, redo ? "Nothing to redo" : "Nothing to undo");
        return;
    }
    changed(app);
}

static void selection_bounds(const EditorApp *app, int *col0, int *row0,
                             int *col1, int *row1)
{
    if (app->has_selection)
    {
        *col0 = SDL_min(app->sel_col0, app->sel_col1);
        *col1 = SDL_max(app->sel_col0, app->sel_col1);
        *row0 = SDL_min(app->sel_row0, app->sel_row1);
        *row1 = SDL_max(app->sel_row0, app->sel_row1);
    }
    else
    {
        *col0 = 0;
        *row0 = 0;
        *col1 = app->doc.grid.width - 1;
        *row1 = app->doc.grid.height - 1;
    }
}

void ed_copy_selection(EditorApp *app, bool cut)
{
    if (!app->has_selection)
    {
        ed_status(app, true, "Select something first (tool 7)");
        return;
    }
    int col0, row0, col1, row1;
    selection_bounds(app, &col0, &row0, &col1, &row1);

    app->clipboard.width = col1 - col0 + 1;
    app->clipboard.height = row1 - row0 + 1;
    for (int row = 0; row < app->clipboard.height; ++row)
    {
        for (int col = 0; col < app->clipboard.width; ++col)
        {
            app->clipboard.cells[row][col] =
                editor_doc_get(&app->doc, col0 + col, row0 + row);
        }
    }
    app->clipboard_valid = true;

    if (cut)
    {
        begin_edit(app);
        char fill = editor_doc_fill_char(&app->doc);
        for (int row = row0; row <= row1; ++row)
            for (int col = col0; col <= col1; ++col)
                editor_doc_set(&app->doc, col, row, fill);
        changed(app);
    }
    ed_status(app, false, "%s %dx%d tiles", cut ? "Cut" : "Copied",
              app->clipboard.width, app->clipboard.height);
}

void ed_paste(EditorApp *app)
{
    if (!app->clipboard_valid)
    {
        ed_status(app, true, "The clipboard is empty");
        return;
    }
    begin_edit(app);
    for (int row = 0; row < app->clipboard.height; ++row)
    {
        for (int col = 0; col < app->clipboard.width; ++col)
        {
            editor_doc_set(&app->doc, app->cursor_col + col,
                           app->cursor_row + row,
                           app->clipboard.cells[row][col]);
        }
    }
    changed(app);
    ed_status(app, false, "Pasted %dx%d tiles", app->clipboard.width,
              app->clipboard.height);
}

void ed_clear_selection_tiles(EditorApp *app)
{
    if (!app->has_selection)
        return;
    int col0, row0, col1, row1;
    selection_bounds(app, &col0, &row0, &col1, &row1);
    begin_edit(app);
    char fill = editor_doc_fill_char(&app->doc);
    for (int row = row0; row <= row1; ++row)
        for (int col = col0; col <= col1; ++col)
            editor_doc_set(&app->doc, col, row, fill);
    changed(app);
}

void ed_mirror(EditorApp *app, bool horizontal)
{
    int col0, row0, col1, row1;
    selection_bounds(app, &col0, &row0, &col1, &row1);
    begin_edit(app);
    editor_doc_mirror(&app->doc, col0, row0, col1, row1, horizontal);
    changed(app);
    ed_status(app, false, "Mirrored %s",
              app->has_selection ? "the selection" : "the whole map");
}

void ed_cycle_theme(EditorApp *app, int step)
{
    begin_edit(app);
    if (!app->doc.grid.has_theme)
    {
        app->doc.grid.has_theme = true;
        app->doc.grid.theme = app->doc.grid.facade ? LEVEL_THEME_FACADE_NIGHT
                                                   : LEVEL_THEME_PLANT;
    }
    else
    {
        int theme = (int)app->doc.grid.theme + step;
        /* One step past either end drops the line, which is how a map asks for
         * its mode's default. */
        if (theme < 0 || theme >= LEVEL_THEME_COUNT)
            app->doc.grid.has_theme = false;
        else
            app->doc.grid.theme = (LevelTheme)theme;
    }
    app->doc.dirty = true;
    changed(app);
}

void ed_toggle_facade(EditorApp *app)
{
    begin_edit(app);
    app->doc.grid.facade = !app->doc.grid.facade;
    app->doc.dirty = true;
    changed(app);
    ed_status(app, false, "%s",
              app->doc.grid.facade
                  ? "MODE FACADE: climbed four ways, no gravity or ladders"
                  : "Interior: gravity, ladders, guards and doors");
}

void ed_resize(EditorApp *app, int dw, int dh)
{
    begin_edit(app);
    if (!editor_doc_resize(&app->doc, app->doc.grid.width + dw,
                           app->doc.grid.height + dh))
    {
        ed_status(app, true, "The map is already at that limit");
        return;
    }
    changed(app);
}

void ed_insert_row(EditorApp *app, bool insert)
{
    begin_edit(app);
    bool ok = insert ? editor_doc_insert_row(&app->doc, app->cursor_row)
                     : editor_doc_delete_row(&app->doc, app->cursor_row);
    if (!ok)
    {
        ed_status(app, true, "No room to %s a row here",
                  insert ? "insert" : "delete");
        return;
    }
    changed(app);
}

void ed_insert_col(EditorApp *app, bool insert)
{
    begin_edit(app);
    bool ok = insert ? editor_doc_insert_col(&app->doc, app->cursor_col)
                     : editor_doc_delete_col(&app->doc, app->cursor_col);
    if (!ok)
    {
        ed_status(app, true, "No room to %s a column here",
                  insert ? "insert" : "delete");
        return;
    }
    changed(app);
}

void ed_sync_spawns(EditorApp *app)
{
    int doors = 0;
    for (int row = 0; row < app->doc.grid.height; ++row)
        for (int col = 0; col < app->doc.grid.width; ++col)
            doors += editor_doc_get(&app->doc, col, row) == 'D';

    begin_edit(app);
    if (doors == 0)
    {
        app->doc.grid.has_spawns = false;
        app->doc.grid.spawn_count = 0;
        ed_status(app, false, "No doors, so no SPAWNS line");
    }
    else if (app->doc.grid.has_spawns && app->doc.grid.spawn_count == doors)
    {
        app->doc.grid.has_spawns = false;
        ed_status(app, false, "SPAWNS removed; the doors spawn nothing");
    }
    else
    {
        for (int i = app->doc.grid.spawn_count; i < doors && i < MAX_DOORS; ++i)
            app->doc.grid.spawns[i] = 1;
        app->doc.grid.spawn_count = doors < MAX_DOORS ? doors : MAX_DOORS;
        app->doc.grid.has_spawns = true;
        ed_status(app, false, "SPAWNS now lists %d doors", doors);
    }
    app->doc.dirty = true;
    changed(app);
}

/* ---- Tools ------------------------------------------------------------- */

static void paint(EditorApp *app, int col, int row, char value)
{
    if (editor_doc_set(&app->doc, col, row, value))
        changed(app);
}

static void paint_line(EditorApp *app, int c0, int r0, int c1, int r1,
                       char value)
{
    int dc = SDL_abs(c1 - c0);
    int dr = -SDL_abs(r1 - r0);
    int sc = c0 < c1 ? 1 : -1;
    int sr = r0 < r1 ? 1 : -1;
    int error = dc + dr;
    for (;;)
    {
        paint(app, c0, r0, value);
        if (c0 == c1 && r0 == r1)
            break;
        int doubled = 2 * error;
        if (doubled >= dr)
        {
            error += dr;
            c0 += sc;
        }
        if (doubled <= dc)
        {
            error += dc;
            r0 += sr;
        }
    }
}

static void flood_fill(EditorApp *app, int col, int row, char value)
{
    char target = editor_doc_get(&app->doc, col, row);
    if (target == value || target == '\0')
        return;

    /* Painted on the way in rather than on the way out, so a tile can never
     * be queued twice and the stack cannot outgrow the map. */
    static int stack[MAX_LEVEL_WIDTH * MAX_LEVEL_HEIGHT];
    int top = 0;
    paint(app, col, row, value);
    stack[top++] = row * MAX_LEVEL_WIDTH + col;
    while (top > 0)
    {
        int cell = stack[--top];
        int c = cell % MAX_LEVEL_WIDTH;
        int r = cell / MAX_LEVEL_WIDTH;
        static const int dc[4] = {-1, 1, 0, 0};
        static const int dr[4] = {0, 0, -1, 1};
        for (int i = 0; i < 4; ++i)
        {
            int nc = c + dc[i];
            int nr = r + dr[i];
            if (nc < 0 || nr < 0 || nc >= app->doc.grid.width ||
                nr >= app->doc.grid.height)
            {
                continue;
            }
            if (editor_doc_get(&app->doc, nc, nr) != target)
                continue;
            paint(app, nc, nr, value);
            stack[top++] = nr * MAX_LEVEL_WIDTH + nc;
        }
    }
}

static void apply_rect(EditorApp *app, int c0, int r0, int c1, int r1,
                       char value, bool filled)
{
    int col0 = SDL_min(c0, c1);
    int col1 = SDL_max(c0, c1);
    int row0 = SDL_min(r0, r1);
    int row1 = SDL_max(r0, r1);
    for (int row = row0; row <= row1; ++row)
    {
        for (int col = col0; col <= col1; ++col)
        {
            bool edge = row == row0 || row == row1 || col == col0 ||
                        col == col1;
            if (filled || edge)
                paint(app, col, row, value);
        }
    }
}

/* A selection that runs off the map would copy tiles that are not there. */
static void clamp_selection(EditorApp *app)
{
    int last_col = app->doc.grid.width - 1;
    int last_row = app->doc.grid.height - 1;
    int *values[4] = {&app->sel_col0, &app->sel_col1, &app->sel_row0,
                      &app->sel_row1};
    int limits[4] = {last_col, last_col, last_row, last_row};
    for (int i = 0; i < 4; ++i)
    {
        if (*values[i] < 0)
            *values[i] = 0;
        if (*values[i] > limits[i])
            *values[i] = limits[i];
    }
}

static void tool_press(EditorApp *app, bool erase, bool alt)
{
    char value = erase ? editor_doc_fill_char(&app->doc) : app->brush;
    int col = app->cursor_col;
    int row = app->cursor_row;
    if (col < 0 || row < 0)
        return;

    if (alt || app->tool == ED_TOOL_PICK)
    {
        char picked = editor_doc_get(&app->doc, col, row);
        if (picked != '\0')
        {
            app->brush = picked;
            const EdSymbol *symbol = editor_symbol(picked);
            ed_status(app, false, "Brush is now '%c' - %s",
                      picked == ' ' ? '_' : picked,
                      symbol != NULL ? symbol->name : "not in the legend");
        }
        return;
    }

    app->drag_col = col;
    app->drag_row = row;
    app->dragging = true;

    switch (app->tool)
    {
    case ED_TOOL_BRUSH:
        begin_edit(app);
        paint(app, col, row, value);
        break;
    case ED_TOOL_FILL:
        begin_edit(app);
        flood_fill(app, col, row, value);
        app->dragging = false;
        break;
    case ED_TOOL_SELECT:
        if (app->has_selection && (SDL_GetModState() & SDL_KMOD_SHIFT) != 0)
        {
            /* Shift reaches the far corner out to here rather than starting
             * again, which is how a selection is nudged wider. */
            app->sel_col1 = col;
            app->sel_row1 = row;
        }
        else
        {
            app->has_selection = true;
            app->sel_col0 = col;
            app->sel_row0 = row;
            app->sel_col1 = col;
            app->sel_row1 = row;
        }
        clamp_selection(app);
        break;
    default:
        break; /* line, rect and box commit on release */
    }
}

static void tool_motion(EditorApp *app, bool erase)
{
    if (!app->dragging)
        return;
    char value = erase ? editor_doc_fill_char(&app->doc) : app->brush;
    if (app->tool == ED_TOOL_BRUSH)
    {
        paint_line(app, app->drag_col, app->drag_row, app->cursor_col,
                   app->cursor_row, value);
        app->drag_col = app->cursor_col;
        app->drag_row = app->cursor_row;
    }
    else if (app->tool == ED_TOOL_SELECT)
    {
        app->sel_col1 = app->cursor_col;
        app->sel_row1 = app->cursor_row;
        clamp_selection(app);
    }
}

static void tool_release(EditorApp *app, bool erase)
{
    if (!app->dragging)
        return;
    char value = erase ? editor_doc_fill_char(&app->doc) : app->brush;
    switch (app->tool)
    {
    case ED_TOOL_LINE:
        begin_edit(app);
        paint_line(app, app->drag_col, app->drag_row, app->cursor_col,
                   app->cursor_row, value);
        break;
    case ED_TOOL_RECT:
        begin_edit(app);
        apply_rect(app, app->drag_col, app->drag_row, app->cursor_col,
                   app->cursor_row, value, false);
        break;
    case ED_TOOL_BOX:
        begin_edit(app);
        apply_rect(app, app->drag_col, app->drag_row, app->cursor_col,
                   app->cursor_row, value, true);
        break;
    default:
        break;
    }
    app->dragging = false;
}

/* ---- Input ------------------------------------------------------------- */

static void update_cursor(EditorApp *app, float x, float y)
{
    app->mouse_x = x;
    app->mouse_y = y;
    float world_x = app->cam_x + (x - app->canvas.x) / app->zoom;
    float world_y = app->cam_y + (y - app->canvas.y) / app->zoom;
    app->cursor_col = (int)SDL_floorf(world_x / TILE_SIZE);
    app->cursor_row = (int)SDL_floorf(world_y / TILE_SIZE);
}

static bool in_canvas(const EditorApp *app, float x, float y)
{
    return x >= app->canvas.x && x < app->canvas.x + app->canvas.w &&
           y >= app->canvas.y && y < app->canvas.y + app->canvas.h;
}

static void cycle_file(EditorApp *app, int step)
{
    if (app->file_count == 0)
        return;
    int index = app->current_file < 0 ? 0 : app->current_file + step;
    if (index < 0)
        index = app->file_count - 1;
    if (index >= app->file_count)
        index = 0;
    ed_open_file(app, index);
}

static void handle_key(EditorApp *app, const SDL_KeyboardEvent *key)
{
    bool ctrl = (key->mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
    bool shift = (key->mod & SDL_KMOD_SHIFT) != 0;

    if (ctrl)
    {
        switch (key->key)
        {
        case SDLK_S:
            ed_save(app);
            return;
        case SDLK_R:
            ed_reload(app);
            return;
        case SDLK_N:
            ed_new_level(app, shift);
            return;
        case SDLK_Z:
            ed_apply_undo(app, shift);
            return;
        case SDLK_Y:
            ed_apply_undo(app, true);
            return;
        case SDLK_C:
            ed_copy_selection(app, false);
            return;
        case SDLK_X:
            ed_copy_selection(app, true);
            return;
        case SDLK_V:
            ed_paste(app);
            return;
        case SDLK_H:
            ed_mirror(app, true);
            return;
        case SDLK_J:
            ed_mirror(app, false);
            return;
        case SDLK_UP:
            ed_insert_row(app, true);
            return;
        case SDLK_DOWN:
            ed_insert_row(app, false);
            return;
        case SDLK_LEFT:
            ed_insert_col(app, true);
            return;
        case SDLK_RIGHT:
            ed_insert_col(app, false);
            return;
        default:
            return;
        }
    }

    switch (key->key)
    {
    case SDLK_F1:
        app->show_help = !app->show_help;
        return;
    case SDLK_F2:
        app->art_mode = !app->art_mode;
        ed_status(app, false, "%s",
                  app->art_mode ? "Drawing the level as the game draws it"
                                : "Schematic");
        return;
    case SDLK_F3:
        app->show_grid = !app->show_grid;
        return;
    case SDLK_F4:
        app->show_route = !app->show_route;
        ed_status(app, false, "%s",
                  app->show_route
                      ? "Route overlay: green is reachable, red cannot get back out"
                      : "Route overlay off");
        return;
    case SDLK_F5:
        ed_playtest(app);
        return;
    case SDLK_F6:
        ed_toggle_facade(app);
        return;
    case SDLK_F7:
        ed_cycle_theme(app, -1);
        return;
    case SDLK_F8:
        ed_cycle_theme(app, 1);
        return;
    case SDLK_F9:
        ed_sync_spawns(app);
        return;
    case SDLK_ESCAPE:
        if (app->show_help)
            app->show_help = false;
        else
            app->has_selection = false;
        return;
    case SDLK_DELETE:
    case SDLK_BACKSPACE:
        ed_clear_selection_tiles(app);
        return;
    case SDLK_LEFTBRACKET:
        cycle_file(app, -1);
        return;
    case SDLK_RIGHTBRACKET:
        cycle_file(app, 1);
        return;
    case SDLK_0:
        ed_center_view(app);
        return;
    case SDLK_EQUALS:
    case SDLK_PLUS:
        ed_set_zoom(app, app->zoom * 1.25f,
                    app->canvas.x + app->canvas.w * 0.5f,
                    app->canvas.y + app->canvas.h * 0.5f);
        return;
    case SDLK_MINUS:
        ed_set_zoom(app, app->zoom / 1.25f,
                    app->canvas.x + app->canvas.w * 0.5f,
                    app->canvas.y + app->canvas.h * 0.5f);
        return;
    case SDLK_LEFT:
        app->cam_x -= TILE_SIZE * 2.0f;
        return;
    case SDLK_RIGHT:
        app->cam_x += TILE_SIZE * 2.0f;
        return;
    case SDLK_UP:
        app->cam_y -= TILE_SIZE * 2.0f;
        return;
    case SDLK_DOWN:
        app->cam_y += TILE_SIZE * 2.0f;
        return;
    default:
        break;
    }

    if (key->key >= SDLK_1 && key->key <= SDLK_7)
        app->tool = (EdTool)(key->key - SDLK_1);
}

/* ---- SDL callbacks ----------------------------------------------------- */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /*
     * `--soak N` closes the editor by itself after N seconds, for
     * [../tools/soak.sh](../tools/soak.sh). See `EditorApp.soaking`.
     *
     * Read off the line before the argument below rather than folded into it,
     * because the editor's one positional argument is a path and a switch is
     * not one; a soak that opened a map called `--soak` is the kind of joke
     * this would otherwise be.
     */
    float soak_seconds = 0.0f;
    const char *positional = NULL;
    for (int i = 1; i < argc; ++i)
    {
        if (SDL_strcmp(argv[i], "--soak") == 0)
        {
            if (i + 1 >= argc)
            {
                SDL_Log("--soak needs a number of seconds after it");
                continue;
            }
            double parsed = SDL_atof(argv[++i]);
            if (parsed > 0.0)
                soak_seconds = (float)parsed;
            else
                SDL_Log("--soak expects a positive number of seconds");
            continue;
        }
        if (positional == NULL)
            positional = argv[i];
    }

    /* An argument is either the repository to work in or the map to open in
     * it, because both are things you want to say when starting the editor
     * from a shell that is somewhere else. */
    const char *argument = positional;
    char open_directory[ED_MAX_PATH] = "";
    const char *open_file = NULL;
    if (argument != NULL && has_suffix(argument, ".txt"))
    {
        SDL_snprintf(open_directory, sizeof(open_directory), "%s", argument);
        char *slash = SDL_strrchr(open_directory, '/');
        if (slash != NULL)
        {
            *slash = '\0';
            open_file = argument + (slash - open_directory) + 1;
        }
        else
        {
            open_file = argument;
            SDL_snprintf(open_directory, sizeof(open_directory), ".");
        }
        argument = open_directory;
    }

    if (!enter_repository(argument))
    {
        SDL_Log("Could not find a 'levels' directory; run the editor from the "
                "repository, or pass its path (or a map file in it) as the "
                "first argument");
        return SDL_APP_FAILURE;
    }

    EditorApp *app = (EditorApp *)SDL_calloc(1, sizeof(EditorApp));
    if (app == NULL)
        return SDL_APP_FAILURE;

    if (!SDL_CreateWindowAndRenderer("Chuck level editor", 1500, 920,
                                     SDL_WINDOW_RESIZABLE, &app->window,
                                     &app->renderer))
    {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        SDL_free(app);
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderVSync(app->renderer, 1);
    SDL_StartTextInput(app->window);

    app->zoom = 1.0f;
    app->brush = '#';
    app->tool = ED_TOOL_BRUSH;
    app->art_mode = true;
    app->show_grid = true;
    app->show_route = false;
    app->current_file = -1;
    app->cursor_col = -1;
    app->cursor_row = -1;

    ed_layout(app);
    ed_scan_files(app);

    int start = app->file_count > 0 ? 0 : -1;
    if (open_file != NULL)
    {
        for (int i = 0; i < app->file_count; ++i)
        {
            if (SDL_strcmp(app->files[i].label, open_file) == 0 ||
                has_suffix(app->files[i].path, open_file))
            {
                start = i;
                break;
            }
        }
    }
    if (start >= 0)
        ed_open_file(app, start);
    else
        ed_new_level(app, false);

    if (soak_seconds > 0.0f)
    {
        app->soaking = true;
        app->soak_seconds_left = soak_seconds;
        /* Said out loud, because a soak that closed itself and one that
         * crashed look identical in a log otherwise. */
        SDL_Log("Soaking for %.1f seconds, then closing", (double)soak_seconds);
    }

    *appstate = app;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    EditorApp *app = (EditorApp *)appstate;

    switch (event->type)
    {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;

    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        ed_layout(app);
        break;

    case SDL_EVENT_MOUSE_MOTION:
        update_cursor(app, event->motion.x, event->motion.y);
        if (app->panning)
        {
            app->cam_x -= (event->motion.x - app->pan_x) / app->zoom;
            app->cam_y -= (event->motion.y - app->pan_y) / app->zoom;
            app->pan_x = event->motion.x;
            app->pan_y = event->motion.y;
        }
        else if (app->painting || app->erasing)
        {
            tool_motion(app, app->erasing);
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    {
        update_cursor(app, event->button.x, event->button.y);
        bool canvas = in_canvas(app, event->button.x, event->button.y) &&
                      !app->show_help;
        if (event->button.button == SDL_BUTTON_MIDDLE)
        {
            app->panning = true;
            app->pan_x = event->button.x;
            app->pan_y = event->button.y;
            break;
        }
        if (!canvas)
        {
            if (event->button.button == SDL_BUTTON_LEFT)
                app->clicked = true;
            break;
        }
        if (event->button.button == SDL_BUTTON_LEFT)
        {
            if ((SDL_GetModState() & SDL_KMOD_ALT) != 0)
            {
                tool_press(app, false, true);
                break;
            }
            app->painting = true;
            tool_press(app, false, false);
        }
        else if (event->button.button == SDL_BUTTON_RIGHT)
        {
            app->erasing = true;
            tool_press(app, true, false);
        }
        break;
    }

    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event->button.button == SDL_BUTTON_MIDDLE)
        {
            app->panning = false;
        }
        else if (event->button.button == SDL_BUTTON_LEFT && app->painting)
        {
            tool_release(app, false);
            app->painting = false;
        }
        else if (event->button.button == SDL_BUTTON_RIGHT && app->erasing)
        {
            tool_release(app, true);
            app->erasing = false;
        }
        break;

    case SDL_EVENT_MOUSE_WHEEL:
        if (in_canvas(app, app->mouse_x, app->mouse_y))
        {
            float factor = event->wheel.y > 0.0f ? 1.15f : 1.0f / 1.15f;
            ed_set_zoom(app, app->zoom * factor, app->mouse_x, app->mouse_y);
        }
        else if (app->mouse_x >= app->right_panel.x)
        {
            app->findings_scroll -= (int)event->wheel.y;
            if (app->findings_scroll < 0)
                app->findings_scroll = 0;
            if (app->findings_scroll >= app->report.count)
                app->findings_scroll = app->report.count > 0
                                           ? app->report.count - 1
                                           : 0;
        }
        else if (app->mouse_x < app->left_panel.x + app->left_panel.w)
        {
            app->palette_scroll -= event->wheel.y * 40.0f;
            if (app->palette_scroll < 0.0f)
                app->palette_scroll = 0.0f;
            if (app->palette_scroll > app->palette_overflow)
                app->palette_scroll = app->palette_overflow;
        }
        break;

    case SDL_EVENT_KEY_DOWN:
        if (!event->key.repeat || event->key.key == SDLK_LEFT ||
            event->key.key == SDLK_RIGHT || event->key.key == SDLK_UP ||
            event->key.key == SDLK_DOWN)
        {
            handle_key(app, &event->key);
        }
        break;

    case SDL_EVENT_TEXT_INPUT:
    {
        /* Typing a legend character picks it up as the brush, which is how a
         * map file is read and so how it is quickest to write. */
        if ((SDL_GetModState() & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0)
            break;
        char symbol = event->text.text[0];
        if (symbol == ' ' || symbol == '\0')
            break;
        if (editor_symbol(symbol) != NULL)
            app->brush = symbol;
        break;
    }

    default:
        break;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    EditorApp *app = (EditorApp *)appstate;

    Uint64 now = SDL_GetTicksNS();
    static Uint64 last = 0;
    Uint64 previous = last == 0 ? now : last;
    float dt = last == 0 ? 0.0f : (float)(now - last) / 1.0e9f;
    last = now;
    if (dt > 0.1f)
        dt = 0.1f;
    app->time += dt;
    if (app->status_timer > 0.0f)
        app->status_timer -= dt;

    /*
     * Spent before the frame is drawn, and out through `SDL_AppQuit` so the
     * teardown is sanitized too.
     *
     * Paid in **raw** elapsed time rather than the `dt` clamped above, which is
     * the same decision the game's loop makes and for the same reason: under a
     * sanitizer a frame can outlast the clamp, and a budget paid in clamped
     * time would turn `--soak 2` into minutes.
     */
    if (app->soaking)
    {
        app->soak_seconds_left -= (float)(now - previous) / 1.0e9f;
        if (app->soak_seconds_left <= 0.0f)
        {
            SDL_Log("Soak finished; closing");
            return SDL_APP_SUCCESS;
        }
    }

    /* A playtest build runs beside this loop rather than inside it, so the
     * window keeps answering while `make` works; this is where a finished one
     * is collected. */
    ed_update_playtest(app);

    /* A stroke is one edit, so the report waits for the mouse to come up
     * rather than re-running the route model on every pixel of a drag. */
    if (app->revalidate && !app->painting && !app->erasing)
        ed_refresh_level(app);

    ed_layout(app);

    fx_rect(app->renderer, FX_NIGHT, 0.0f, 0.0f, (float)app->win_w,
            (float)app->win_h);
    ed_draw_canvas(app);
    ed_draw_chrome(app);
    SDL_RenderPresent(app->renderer);

    app->clicked = false;
    return app->quit ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;
    EditorApp *app = (EditorApp *)appstate;
    if (app == NULL)
        return;
    /* Before the renderer and the window go, because a build thread outliving
     * them is a thread holding a pipe into a process that no longer has a
     * screen to report to. */
    ed_cancel_playtest(app);
    if (app->canvas_texture != NULL)
        SDL_DestroyTexture(app->canvas_texture);
    if (app->renderer != NULL)
        SDL_DestroyRenderer(app->renderer);
    if (app->window != NULL)
        SDL_DestroyWindow(app->window);
    SDL_free(app);
    SDL_Quit();
}
